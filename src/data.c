/*
 * OTT Video Streaming Server - Data Storage Module
 * File-based storage for users, videos, sessions, and watch history
 */

#include "common.h"

/* Global storage */
static User g_users[MAX_USERS];
static int g_user_count = 0;

static Video g_videos[MAX_VIDEOS];
static int g_video_count = 0;

static Session g_sessions[MAX_SESSIONS];
static int g_session_count = 0;

static WatchHistory g_history[MAX_USERS * MAX_VIDEOS];
static int g_history_count = 0;

/* Mutex for thread safety */
static pthread_mutex_t g_data_mutex;
static int g_mutex_initialized = 0;

/* Initialize data storage */
void data_init(void) {
    if (!g_mutex_initialized) {
        pthread_mutex_init(&g_data_mutex, NULL);
        g_mutex_initialized = 1;
    }
    
    /* Initialize counters - data will be loaded from PostgreSQL */
    g_user_count = 0;
    g_video_count = 0;
    g_session_count = 0;
    g_history_count = 0;
    
    log_message(LOG_INFO, "Data storage initialized (using PostgreSQL database)");
}

/* Save data to file */
void data_save(void) {
    FILE* fp;
    
#if defined(_WIN32)
    #define PATH_SEP "\\"
#else
    #define PATH_SEP "/"
#endif
    
    /* Save users */
    fp = fopen(DATA_DIR PATH_SEP "users.dat", "wb");
    if (fp) {
        fwrite(&g_user_count, sizeof(int), 1, fp);
        fwrite(g_users, sizeof(User), g_user_count, fp);
        fclose(fp);
        log_message(LOG_DEBUG, "Saved %d users", g_user_count);
    } else {
        log_message(LOG_ERROR, "Failed to save users.dat");
    }
    
    /* Save videos */
    fp = fopen(DATA_DIR PATH_SEP "videos.dat", "wb");
    if (fp) {
        fwrite(&g_video_count, sizeof(int), 1, fp);
        fwrite(g_videos, sizeof(Video), g_video_count, fp);
        fclose(fp);
        log_message(LOG_DEBUG, "Saved %d videos", g_video_count);
    } else {
        log_message(LOG_ERROR, "Failed to save videos.dat");
    }
    
    /* Save history */
    fp = fopen(DATA_DIR PATH_SEP "history.dat", "wb");
    if (fp) {
        fwrite(&g_history_count, sizeof(int), 1, fp);
        fwrite(g_history, sizeof(WatchHistory), g_history_count, fp);
        fclose(fp);
    }
}

/* Load data from file */
void data_load(void) {
    FILE* fp;
    
    /* Load users */
    fp = fopen(DATA_DIR PATH_SEP "users.dat", "rb");
    if (fp) {
        fread(&g_user_count, sizeof(int), 1, fp);
        if (g_user_count > MAX_USERS) g_user_count = MAX_USERS;
        fread(g_users, sizeof(User), g_user_count, fp);
        fclose(fp);
        log_message(LOG_INFO, "Loaded %d users from file", g_user_count);
    }
    
    /* Load videos */
    fp = fopen(DATA_DIR PATH_SEP "videos.dat", "rb");
    if (fp) {
        fread(&g_video_count, sizeof(int), 1, fp);
        if (g_video_count > MAX_VIDEOS) g_video_count = MAX_VIDEOS;
        fread(g_videos, sizeof(Video), g_video_count, fp);
        fclose(fp);
        log_message(LOG_INFO, "Loaded %d videos from file", g_video_count);
    }
    
    /* Load history */
    fp = fopen(DATA_DIR PATH_SEP "history.dat", "rb");
    if (fp) {
        fread(&g_history_count, sizeof(int), 1, fp);
        if (g_history_count > MAX_USERS * MAX_VIDEOS) g_history_count = MAX_USERS * MAX_VIDEOS;
        fread(g_history, sizeof(WatchHistory), g_history_count, fp);
        fclose(fp);
        log_message(LOG_INFO, "Loaded %d watch history records from file", g_history_count);
    }
}

/* User functions */
User* user_find_by_username(const char* username) {
    pthread_mutex_lock(&g_data_mutex);
    for (int i = 0; i < g_user_count; i++) {
        if (strcmp(g_users[i].username, username) == 0 && g_users[i].active) {
            pthread_mutex_unlock(&g_data_mutex);
            return &g_users[i];
        }
    }
    pthread_mutex_unlock(&g_data_mutex);
    return NULL;
}

User* user_find_by_id(int id) {
    pthread_mutex_lock(&g_data_mutex);
    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i].id == id && g_users[i].active) {
            pthread_mutex_unlock(&g_data_mutex);
            return &g_users[i];
        }
    }
    pthread_mutex_unlock(&g_data_mutex);
    return NULL;
}

int user_verify_password(User* user, const char* password) {
    char hash_str[256];
    snprintf(hash_str, sizeof(hash_str), "%lu", simple_hash(password));
    return strcmp(user->password_hash, hash_str) == 0;
}

/* Session functions */
Session* session_create(int user_id) {
    pthread_mutex_lock(&g_data_mutex);
    
    /* Find empty slot or reuse expired */
    int slot = -1;
    time_t now = time(NULL);
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!g_sessions[i].active || g_sessions[i].expires_at < now) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        /* Overwrite oldest */
        slot = 0;
        for (int i = 1; i < MAX_SESSIONS; i++) {
            if (g_sessions[i].expires_at < g_sessions[slot].expires_at) {
                slot = i;
            }
        }
    }
    
    Session* session = &g_sessions[slot];
    generate_session_token(session->token, sizeof(session->token));
    session->user_id = user_id;
    session->expires_at = now + SESSION_TIMEOUT;
    session->active = 1;
    
    if (slot >= g_session_count) {
        g_session_count = slot + 1;
    }
    
    pthread_mutex_unlock(&g_data_mutex);
    return session;
}

Session* session_find(const char* token) {
    if (!token || !*token) return NULL;
    
    pthread_mutex_lock(&g_data_mutex);
    time_t now = time(NULL);
    
    for (int i = 0; i < g_session_count; i++) {
        if (g_sessions[i].active && 
            strcmp(g_sessions[i].token, token) == 0 &&
            g_sessions[i].expires_at > now) {
            pthread_mutex_unlock(&g_data_mutex);
            return &g_sessions[i];
        }
    }
    pthread_mutex_unlock(&g_data_mutex);
    return NULL;
}

void session_destroy(const char* token) {
    pthread_mutex_lock(&g_data_mutex);
    for (int i = 0; i < g_session_count; i++) {
        if (strcmp(g_sessions[i].token, token) == 0) {
            g_sessions[i].active = 0;
            break;
        }
    }
    pthread_mutex_unlock(&g_data_mutex);
}

/* Video functions */
int video_add(const char* title, const char* filename, const char* thumbnail, int duration, const char* description) {
    pthread_mutex_lock(&g_data_mutex);
    
    if (g_video_count >= MAX_VIDEOS) {
        pthread_mutex_unlock(&g_data_mutex);
        return -1;
    }
    
    Video* v = &g_videos[g_video_count];
    v->id = g_video_count + 1;
    strncpy(v->title, title, sizeof(v->title) - 1);
    strncpy(v->filename, filename, sizeof(v->filename) - 1);
    strncpy(v->thumbnail, thumbnail, sizeof(v->thumbnail) - 1);
    v->duration_sec = duration;
    if (description) {
        strncpy(v->description, description, sizeof(v->description) - 1);
    }
    
    g_video_count++;
    pthread_mutex_unlock(&g_data_mutex);
    
    data_save();
    return v->id;
}

Video* video_find_by_id(int id) {
    pthread_mutex_lock(&g_data_mutex);
    for (int i = 0; i < g_video_count; i++) {
        if (g_videos[i].id == id) {
            pthread_mutex_unlock(&g_data_mutex);
            return &g_videos[i];
        }
    }
    pthread_mutex_unlock(&g_data_mutex);
    return NULL;
}

int video_get_all(Video** videos) {
    *videos = g_videos;
    return g_video_count;
}

int video_count(void) {
    return g_video_count;
}

/* Watch history functions */
WatchHistory* history_find(int user_id, int video_id) {
    pthread_mutex_lock(&g_data_mutex);
    for (int i = 0; i < g_history_count; i++) {
        if (g_history[i].user_id == user_id && g_history[i].video_id == video_id) {
            pthread_mutex_unlock(&g_data_mutex);
            return &g_history[i];
        }
    }
    pthread_mutex_unlock(&g_data_mutex);
    return NULL;
}

int history_update(int user_id, int video_id, int position) {
    pthread_mutex_lock(&g_data_mutex);
    
    /* Find existing record */
    for (int i = 0; i < g_history_count; i++) {
        if (g_history[i].user_id == user_id && g_history[i].video_id == video_id) {
            g_history[i].last_pos_sec = position;
            g_history[i].updated_at = time(NULL);
            pthread_mutex_unlock(&g_data_mutex);
            data_save();
            return 0;
        }
    }
    
    /* Create new record */
    if (g_history_count >= MAX_USERS * MAX_VIDEOS) {
        pthread_mutex_unlock(&g_data_mutex);
        return -1;
    }
    
    WatchHistory* h = &g_history[g_history_count];
    h->user_id = user_id;
    h->video_id = video_id;
    h->last_pos_sec = position;
    h->updated_at = time(NULL);
    g_history_count++;
    
    pthread_mutex_unlock(&g_data_mutex);
    data_save();
    return 0;
}

int history_get_user_history(int user_id, WatchHistory* out_history, int max_count) {
    pthread_mutex_lock(&g_data_mutex);
    int count = 0;
    
    for (int i = 0; i < g_history_count && count < max_count; i++) {
        if (g_history[i].user_id == user_id) {
            out_history[count++] = g_history[i];
        }
    }
    
    pthread_mutex_unlock(&g_data_mutex);
    return count;
}
