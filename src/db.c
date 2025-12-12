/*
 * OTT Video Streaming Server - PostgreSQL Database Module
 * Replaces data.c to provide DB storage
 */

#include "common.h"
#include "libpq-fe.h"

/* Thread-local storage for return buffers */
#if defined(_WIN32)
    #define THREAD_LOCAL __declspec(thread)
#else
    #define THREAD_LOCAL __thread
#endif

/* Database connection */
static PGconn* g_db_conn = NULL;
static pthread_mutex_t g_db_mutex;
static int g_db_initialized = 0;

/* Database configuration - Docker PostgreSQL */
#define DB_HOST "127.0.0.1"
#define DB_PORT "5432"
#define DB_NAME "ott_streaming"
#define DB_USER "ott"
#define DB_PASS "ott123"

/* Forward declaration */
unsigned long simple_hash(const char* str);

/* Internal: Execute query and return result */
static PGresult* db_query(const char* query) {
    if (!g_db_conn) return NULL;
    
    pthread_mutex_lock(&g_db_mutex);
    PGresult* result = PQexec(g_db_conn, query);
    pthread_mutex_unlock(&g_db_mutex);
    
    ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        log_message(LOG_ERROR, "Query failed: %s", PQerrorMessage(g_db_conn));
        PQclear(result);
        return NULL;
    }
    
    return result;
}

/* Internal: Execute parameterized query */
static PGresult* db_query_params(const char* query, int nParams, const char* const* paramValues) {
    if (!g_db_conn) return NULL;
    
    pthread_mutex_lock(&g_db_mutex);
    PGresult* result = PQexecParams(g_db_conn, query, nParams, NULL, paramValues, NULL, NULL, 0);
    pthread_mutex_unlock(&g_db_mutex);
    
    ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        log_message(LOG_ERROR, "Query failed: %s", PQerrorMessage(g_db_conn));
        PQclear(result);
        return NULL;
    }
    
    return result;
}

/* Initialize database connection (Renamed to match main.c expectation) */
void data_init(void) {
    if (g_db_initialized) return;
    
    pthread_mutex_init(&g_db_mutex, NULL);
    
    char conninfo[512];
    snprintf(conninfo, sizeof(conninfo),
        "host=%s port=%s dbname=%s user=%s password=%s",
        DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASS);
    
    log_message(LOG_INFO, "Connecting to PostgreSQL: %s:%s/%s", DB_HOST, DB_PORT, DB_NAME);
    
    g_db_conn = PQconnectdb(conninfo);
    
    if (PQstatus(g_db_conn) != CONNECTION_OK) {
        log_message(LOG_ERROR, "PostgreSQL connection failed: %s", PQerrorMessage(g_db_conn));
        PQfinish(g_db_conn);
        g_db_conn = NULL;
        return;
    }
    
    g_db_initialized = 1;
    log_message(LOG_INFO, "Connected to PostgreSQL database successfully");
}

/* Cleanup (Called by main.c:data_save) */
void data_save(void) {
    if (g_db_conn) {
        PQfinish(g_db_conn);
        g_db_conn = NULL;
        log_message(LOG_INFO, "PostgreSQL connection closed");
    }
    g_db_initialized = 0;
}

/* Legacy: Load data (No-op for DB) */
void data_load(void) {
    // Database is already loaded
}

/* ==================== USER FUNCTIONS ==================== */

User* user_find_by_username(const char* username) {
    fprintf(stderr, "DEBUG: user_find_by_username called for '%s'\n", username);
    static THREAD_LOCAL User user;
    memset(&user, 0, sizeof(User));
    
    const char* params[1] = { username };
    PGresult* result = db_query_params(
        "SELECT id, username, password_hash, active FROM users WHERE username = $1",
        1, params);
    
    if (!result) {
        fprintf(stderr, "DEBUG: db_query_params returned NULL\n");
        return NULL;
    }

    if (PQntuples(result) == 0) {
        fprintf(stderr, "DEBUG: No user found for '%s'\n", username);
        if (result) PQclear(result);
        return NULL;
    }
    
    user.id = atoi(PQgetvalue(result, 0, 0));
    strncpy(user.username, PQgetvalue(result, 0, 1), sizeof(user.username) - 1);
    strncpy(user.password_hash, PQgetvalue(result, 0, 2), sizeof(user.password_hash) - 1);
    user.active = (PQgetvalue(result, 0, 3)[0] == 't') ? 1 : 0;
    
    fprintf(stderr, "DEBUG: User found: %s\n", username);
    PQclear(result);
    return &user;
}

User* user_find_by_id(int id) {
    static THREAD_LOCAL User user;
    memset(&user, 0, sizeof(User));
    
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%d", id);
    
    const char* params[1] = { id_str };
    PGresult* result = db_query_params(
        "SELECT id, username, password_hash, active FROM users WHERE id = $1",
        1, params);
    
    if (!result || PQntuples(result) == 0) {
        if (result) PQclear(result);
        return NULL;
    }
    
    user.id = atoi(PQgetvalue(result, 0, 0));
    strncpy(user.username, PQgetvalue(result, 0, 1), sizeof(user.username) - 1);
    strncpy(user.password_hash, PQgetvalue(result, 0, 2), sizeof(user.password_hash) - 1);
    user.active = (PQgetvalue(result, 0, 3)[0] == 't') ? 1 : 0;
    
    PQclear(result);
    return &user;
}

int user_verify_password(User* user, const char* password) {
    char hash_str[256];
    snprintf(hash_str, sizeof(hash_str), "%lu", simple_hash(password));
    log_message(LOG_INFO, "Login Verify: Username=%s, StoredHash=%s, ComputedHash=%s", user->username, user->password_hash, hash_str);
    return strcmp(user->password_hash, hash_str) == 0;
}

int user_create(const char* username, const char* password) {
    char hash_str[256];
    snprintf(hash_str, sizeof(hash_str), "%lu", simple_hash(password));

    const char* params[2] = { username, hash_str };
    PGresult* result = db_query_params(
        "INSERT INTO users (username, password_hash) VALUES ($1, $2)",
        2, params);

    if (!result) {
        log_message(LOG_WARN, "Failed to create user: %s (Error: %s)", username, PQerrorMessage(g_db_conn));
        return -1;
    }

    PQclear(result);
    log_message(LOG_INFO, "Created new user: %s", username);
    return 0;
}

/* ==================== VIDEO FUNCTIONS ==================== */

int video_add(const char* title, const char* filename, const char* thumbnail, int duration, const char* description) {
    char duration_str[16];
    snprintf(duration_str, sizeof(duration_str), "%d", duration);
    
    const char* params[5] = { title, filename, thumbnail, duration_str, description ? description : "" };
    PGresult* result = db_query_params(
        "INSERT INTO videos (title, filename, thumbnail, duration_sec, description) "
        "VALUES ($1, $2, $3, $4, $5) "
        "ON CONFLICT (filename) DO UPDATE SET title=$1, thumbnail=$3, duration_sec=$4 "
        "RETURNING id",
        5, params);
    
    if (!result) {
        log_message(LOG_ERROR, "Failed to add video to DB: %s (Error: %s)", title, PQerrorMessage(g_db_conn));
        return -1;
    }
    
    int id = atoi(PQgetvalue(result, 0, 0));
    PQclear(result);
    
    log_message(LOG_INFO, "Added/updated video in DB: %s (id=%d)", title, id);
    return id;
}

Video* video_find_by_id(int id) {
    static THREAD_LOCAL Video video;
    memset(&video, 0, sizeof(Video));
    
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%d", id);
    
    const char* params[1] = { id_str };
    PGresult* result = db_query_params(
        "SELECT id, title, filename, thumbnail, duration_sec, description FROM videos WHERE id = $1",
        1, params);
    
    if (!result || PQntuples(result) == 0) {
        if (result) PQclear(result);
        return NULL;
    }
    
    video.id = atoi(PQgetvalue(result, 0, 0));
    strncpy(video.title, PQgetvalue(result, 0, 1), sizeof(video.title) - 1);
    strncpy(video.filename, PQgetvalue(result, 0, 2), sizeof(video.filename) - 1);
    
    char* thumb = PQgetvalue(result, 0, 3);
    if (thumb && !PQgetisnull(result, 0, 3)) {
        strncpy(video.thumbnail, thumb, sizeof(video.thumbnail) - 1);
    }
    
    video.duration_sec = atoi(PQgetvalue(result, 0, 4));
    
    char* desc = PQgetvalue(result, 0, 5);
    if (desc && !PQgetisnull(result, 0, 5)) {
        strncpy(video.description, desc, sizeof(video.description) - 1);
    }
    
    PQclear(result);
    return &video;
}

int video_get_all(Video** videos) {
    static THREAD_LOCAL Video video_buffer[MAX_VIDEOS];
    
    PGresult* result = db_query(
        "SELECT id, title, filename, thumbnail, duration_sec, description FROM videos ORDER BY id");
    
    if (!result) return 0;
    
    int count = PQntuples(result);
    if (count > MAX_VIDEOS) count = MAX_VIDEOS;
    
    for (int i = 0; i < count; i++) {
        video_buffer[i].id = atoi(PQgetvalue(result, i, 0));
        strncpy(video_buffer[i].title, PQgetvalue(result, i, 1), sizeof(video_buffer[i].title) - 1);
        strncpy(video_buffer[i].filename, PQgetvalue(result, i, 2), sizeof(video_buffer[i].filename) - 1);
        
        char* thumb = PQgetvalue(result, i, 3);
        if (thumb && !PQgetisnull(result, i, 3)) {
            strncpy(video_buffer[i].thumbnail, thumb, sizeof(video_buffer[i].thumbnail) - 1);
        }
        
        video_buffer[i].duration_sec = atoi(PQgetvalue(result, i, 4));
        
        char* desc = PQgetvalue(result, i, 5);
        if (desc && !PQgetisnull(result, i, 5)) {
            strncpy(video_buffer[i].description, desc, sizeof(video_buffer[i].description) - 1);
        }
    }
    
    PQclear(result);
    *videos = video_buffer;
    return count;
}

int video_count(void) {
    PGresult* result = db_query("SELECT COUNT(*) FROM videos");
    if (!result) return 0;
    
    int count = atoi(PQgetvalue(result, 0, 0));
    PQclear(result);
    return count;
}

/* ==================== SESSION FUNCTIONS ==================== */

Session* session_create(int user_id) {
    static THREAD_LOCAL Session session;
    memset(&session, 0, sizeof(Session));
    
    char user_id_str[16];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    
    /* Generate random token */
    generate_session_token(session.token, sizeof(session.token));
    
    const char* params[2] = { user_id_str, session.token };
    PGresult* result = db_query_params(
        "INSERT INTO sessions (user_id, token, expires_at) "
        "VALUES ($1, $2, NOW() + INTERVAL '24 hours') RETURNING id",
        2, params);
    
    if (!result) return NULL;
    
    session.user_id = user_id;
    session.active = 1;
    session.expires_at = time(NULL) + SESSION_TIMEOUT;
    
    PQclear(result);
    log_message(LOG_INFO, "Created session for user_id=%d", user_id);
    return &session;
}

Session* session_find(const char* token) {
    static THREAD_LOCAL Session session;
    memset(&session, 0, sizeof(Session));
    
    if (!token || !*token) return NULL;
    
    const char* params[1] = { token };
    PGresult* result = db_query_params(
        "SELECT user_id, token FROM sessions "
        "WHERE token = $1 AND expires_at > NOW()",
        1, params);
    
    if (!result || PQntuples(result) == 0) {
        if (result) PQclear(result);
        return NULL;
    }
    
    session.user_id = atoi(PQgetvalue(result, 0, 0));
    strncpy(session.token, PQgetvalue(result, 0, 1), sizeof(session.token) - 1);
    session.active = 1;
    
    PQclear(result);
    return &session;
}

void session_destroy(const char* token) {
    const char* params[1] = { token };
    PGresult* result = db_query_params("DELETE FROM sessions WHERE token = $1", 1, params);
    if (result) PQclear(result);
    log_message(LOG_INFO, "Destroyed session");
}

/* ==================== WATCH HISTORY FUNCTIONS ==================== */

WatchHistory* history_find(int user_id, int video_id) {
    static THREAD_LOCAL WatchHistory history;
    memset(&history, 0, sizeof(WatchHistory));
    
    char user_id_str[16], video_id_str[16];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(video_id_str, sizeof(video_id_str), "%d", video_id);
    
    const char* params[2] = { user_id_str, video_id_str };
    PGresult* result = db_query_params(
        "SELECT user_id, video_id, last_pos_sec FROM watch_history "
        "WHERE user_id = $1 AND video_id = $2",
        2, params);
    
    if (!result || PQntuples(result) == 0) {
        if (result) PQclear(result);
        return NULL;
    }
    
    history.user_id = atoi(PQgetvalue(result, 0, 0));
    history.video_id = atoi(PQgetvalue(result, 0, 1));
    history.last_pos_sec = atoi(PQgetvalue(result, 0, 2));
    
    PQclear(result);
    return &history;
}

int history_update(int user_id, int video_id, int position) {
    char user_id_str[16], video_id_str[16], pos_str[16];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(video_id_str, sizeof(video_id_str), "%d", video_id);
    snprintf(pos_str, sizeof(pos_str), "%d", position);
    
    const char* params[3] = { user_id_str, video_id_str, pos_str };
    PGresult* result = db_query_params(
        "INSERT INTO watch_history (user_id, video_id, last_pos_sec, updated_at) "
        "VALUES ($1, $2, $3, NOW()) "
        "ON CONFLICT (user_id, video_id) DO UPDATE SET last_pos_sec = $3, updated_at = NOW()",
        3, params);
    
    if (!result) return -1;
    
    PQclear(result);
    // log_message(LOG_DEBUG, "Updated history: user=%d video=%d pos=%d", user_id, video_id, position);
    return 0;
}

int history_get_user_history(int user_id, WatchHistory* out_history, int max_count) {
    char user_id_str[16];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    
    const char* params[1] = { user_id_str };
    PGresult* result = db_query_params(
        "SELECT user_id, video_id, last_pos_sec FROM watch_history "
        "WHERE user_id = $1 ORDER BY updated_at DESC",
        1, params);
    
    if (!result) return 0;
    
    int count = PQntuples(result);
    if (count > max_count) count = max_count;
    
    for (int i = 0; i < count; i++) {
        out_history[i].user_id = atoi(PQgetvalue(result, i, 0));
        out_history[i].video_id = atoi(PQgetvalue(result, i, 1));
        out_history[i].last_pos_sec = atoi(PQgetvalue(result, i, 2));
    }
    
    PQclear(result);
    return count;
}
