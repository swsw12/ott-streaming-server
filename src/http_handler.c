/*
 * OTT Video Streaming Server - HTTP Response Handlers
 */

#include "common.h"

/* External declarations */
extern int parse_http_request(const char* raw, HttpRequest* req);
extern char* get_cookie_value(const char* cookies, const char* name, char* value, size_t value_size);

extern void data_init(void);
extern void data_load(void);
extern void data_save(void);
extern User* user_find_by_username(const char* username);
extern User* user_find_by_id(int id);
extern int user_verify_password(User* user, const char* password);
extern int user_create(const char* username, const char* password);
extern Session* session_create(int user_id);
extern Session* session_find(const char* token);
extern void session_destroy(const char* token);
extern Video* video_find_by_id(int id);
extern int video_get_all(Video** videos);
extern int video_count(void);
extern WatchHistory* history_find(int user_id, int video_id);
extern int history_update(int user_id, int video_id, int position);
extern int history_get_user_history(int user_id, WatchHistory* out_history, int max_count);

extern int ffmpeg_scan_videos(void);

/* Send HTTP response helper */
static void send_response(SOCKET client, const char* status, const char* content_type, 
                          const char* extra_headers, const char* body, size_t body_len) {
    char header[2048];
    int header_len;
    
    header_len = snprintf(header, sizeof(header),
        "%s"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "%s"
        "Connection: close\r\n"
        "\r\n",
        status,
        content_type,
        body_len,
        extra_headers ? extra_headers : "");
    
    send(client, header, header_len, 0);
    if (body && body_len > 0) {
        send(client, body, (int)body_len, 0);
    }
}

/* Send JSON response */
static void send_json(SOCKET client, const char* status, const char* json) {
    send_response(client, status, "application/json", NULL, json, strlen(json));
}

/* Send redirect */
static void send_redirect(SOCKET client, const char* location, const char* cookie) {
    char header[1024];
    int len;
    
    if (cookie) {
        len = snprintf(header, sizeof(header),
            HTTP_302
            "Location: %s\r\n"
            "Set-Cookie: %s\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n",
            location, cookie);
    } else {
        len = snprintf(header, sizeof(header),
            HTTP_302
            "Location: %s\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n",
            location);
    }
    
    send(client, header, len, 0);
}

/* Send static file */
static void send_static_file(SOCKET client, const char* path, HttpRequest* req) {
    char full_path[MAX_PATH_LEN];
    
    /* Security check */
    if (strstr(path, "..")) {
        send_response(client, HTTP_403, "text/plain", NULL, "Forbidden", 9);
        return;
    }
    
    /* Build full path */
    if (path[0] == '/') path++;
    snprintf(full_path, sizeof(full_path), "%s/%s", STATIC_DIR, path);
    
#if defined(_WIN32)
    /* Windows path separator */
    for (char* p = full_path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#endif
    
    FILE* fp = fopen(full_path, "rb");
    if (!fp) {
        /* Try index.html for directory */
        char index_path[MAX_PATH_LEN];
        snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);
        fp = fopen(index_path, "rb");
        
        if (!fp) {
            const char* msg = "Not Found";
            send_response(client, HTTP_404, "text/plain", NULL, msg, strlen(msg));
            return;
        }
        strcpy(full_path, index_path);
    }
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    const char* content_type = get_content_type(full_path);
    
    /* Send header */
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        HTTP_200
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        content_type, file_size);
    
    send(client, header, header_len, 0);
    
    /* Send file content */
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send(client, buffer, (int)bytes_read, 0);
    }
    
    fclose(fp);
}

/* Stream video with Range support */
static void stream_video(SOCKET client, int video_id, HttpRequest* req, int user_id) {
    Video* video = video_find_by_id(video_id);
    if (!video) {
        const char* msg = "Video not found";
        send_response(client, HTTP_404, "text/plain", NULL, msg, strlen(msg));
        return;
    }
    
    char video_path[MAX_PATH_LEN];
    snprintf(video_path, sizeof(video_path), "%s/%s", VIDEO_DIR, video->filename);
    
#if defined(_WIN32)
    for (char* p = video_path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#endif
    
    FILE* fp = fopen(video_path, "rb");
    if (!fp) {
        log_message(LOG_ERROR, "Cannot open video file: %s", video_path);
        const char* msg = "Video file not found";
        send_response(client, HTTP_404, "text/plain", NULL, msg, strlen(msg));
        return;
    }
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* Handle start parameter */
    char start_param[32] = {0};
    if (get_query_param(req->query, "start", start_param, sizeof(start_param))) {
        int start_sec = atoi(start_param);
        if (start_sec > 0 && video->duration_sec > 0) {
            /* Approximate byte position (rough estimate) */
            long byte_pos = (long)((double)start_sec / video->duration_sec * file_size);
            req->range_start = byte_pos;
            req->has_range = 1;
        }
    }
    
    long range_start = 0;
    long range_end = file_size - 1;
    
    if (req->has_range) {
        if (req->range_start >= 0) {
            range_start = req->range_start;
        }
        if (req->range_end >= 0 && req->range_end < file_size) {
            range_end = req->range_end;
        }
        
        /* Limit chunk size */
        if (range_end - range_start > 1024 * 1024) {
            range_end = range_start + 1024 * 1024 - 1;
        }
    }
    
    long content_length = range_end - range_start + 1;
    
    const char* content_type = get_content_type(video->filename);
    char header[512];
    int header_len;
    
    if (req->has_range) {
        header_len = snprintf(header, sizeof(header),
            HTTP_206
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Content-Range: bytes %ld-%ld/%ld\r\n"
            "Accept-Ranges: bytes\r\n"
            "Connection: close\r\n"
            "\r\n",
            content_type, content_length, range_start, range_end, file_size);
    } else {
        header_len = snprintf(header, sizeof(header),
            HTTP_200
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Accept-Ranges: bytes\r\n"
            "Connection: close\r\n"
            "\r\n",
            content_type, file_size);
    }
    
    send(client, header, header_len, 0);
    
    /* Seek to start position */
    fseek(fp, range_start, SEEK_SET);
    
    /* Send video data */
    char buffer[BUFFER_SIZE];
    long bytes_remaining = content_length;
    
    while (bytes_remaining > 0) {
        size_t to_read = bytes_remaining > sizeof(buffer) ? sizeof(buffer) : bytes_remaining;
        size_t bytes_read = fread(buffer, 1, to_read, fp);
        
        if (bytes_read == 0) break;
        
        int sent = send(client, buffer, (int)bytes_read, 0);
        if (sent <= 0) break;
        
        bytes_remaining -= bytes_read;
    }
    
    fclose(fp);
    
    log_message(LOG_DEBUG, "Streamed %ld bytes of %s (range: %ld-%ld)", 
                content_length, video->filename, range_start, range_end);
}

/* Handle login POST */
static void handle_login(SOCKET client, HttpRequest* req) {
    char username[64] = {0};
    char password[64] = {0};
    
    /* Parse form data */
    get_query_param(req->body, "username", username, sizeof(username));
    get_query_param(req->body, "password", password, sizeof(password));
    
    if (!username[0] || !password[0]) {
        send_redirect(client, "/login.html?error=missing", NULL);
        return;
    }
    
    User* user = user_find_by_username(username);
    if (!user || !user_verify_password(user, password)) {
        log_message(LOG_WARN, "Login failed for user: %s", username);
        send_redirect(client, "/login.html?error=invalid", NULL);
        return;
    }
    
    /* Create session */
    Session* session = session_create(user->id);
    
    char cookie[256];
    snprintf(cookie, sizeof(cookie), "session=%s; Path=/; HttpOnly", session->token);
    
    log_message(LOG_INFO, "User logged in: %s", username);
    send_redirect(client, "/list.html", cookie);
}

/* Handle register POST */
static void handle_register(SOCKET client, HttpRequest* req) {
    char username[64] = {0};
    char password[64] = {0};

    /* Parse form data */
    get_query_param(req->body, "username", username, sizeof(username));
    get_query_param(req->body, "password", password, sizeof(password));

    if (!username[0] || !password[0]) {
        send_redirect(client, "/register.html?error=missing", NULL);
        return;
    }

    if (user_find_by_username(username)) {
        log_message(LOG_WARN, "Registration failed: user already exists: %s", username);
        send_redirect(client, "/register.html?error=exists", NULL);
        return;
    }

    if (user_create(username, password) != 0) {
        log_message(LOG_ERROR, "Registration failed for user: %s", username);
        send_redirect(client, "/register.html?error=failed", NULL);
        return;
    }

    log_message(LOG_INFO, "User registered: %s", username);
    send_redirect(client, "/login.html?registered=true", NULL);
}

/* Handle logout */
static void handle_logout(SOCKET client, HttpRequest* req) {
    char token[65] = {0};
    get_cookie_value(req->cookie, "session", token, sizeof(token));
    
    if (token[0]) {
        session_destroy(token);
    }
    
    send_redirect(client, "/login.html", "session=; Path=/; Max-Age=0");
}

/* API: Get video list */
static void api_get_videos(SOCKET client, int user_id) {
    Video* videos;
    int count = video_get_all(&videos);
    
    char json[16384] = "[";
    char* p = json + 1;
    size_t remaining = sizeof(json) - 2;
    
    for (int i = 0; i < count; i++) {
        Video* v = &videos[i];
        
        /* Check watch history */
        int last_pos = 0;
        WatchHistory* h = history_find(user_id, v->id);
        if (h) {
            last_pos = h->last_pos_sec;
        }
        
        int written = snprintf(p, remaining,
            "%s{\"id\":%d,\"title\":\"%s\",\"thumbnail\":\"%s\",\"duration\":%d,\"last_pos\":%d}",
            i > 0 ? "," : "",
            v->id, v->title, v->thumbnail, v->duration_sec, last_pos);
        
        p += written;
        remaining -= written;
    }
    
    strcat(p, "]");
    
    send_json(client, HTTP_200, json);
}

/* API: Get single video info */
static void api_get_video(SOCKET client, int video_id, int user_id) {
    Video* v = video_find_by_id(video_id);
    if (!v) {
        send_json(client, HTTP_404, "{\"error\":\"Video not found\"}");
        return;
    }
    
    int last_pos = 0;
    WatchHistory* h = history_find(user_id, v->id);
    if (h) {
        last_pos = h->last_pos_sec;
    }
    
    char json[1024];
    snprintf(json, sizeof(json),
        "{\"id\":%d,\"title\":\"%s\",\"thumbnail\":\"%s\",\"duration\":%d,\"last_pos\":%d,\"filename\":\"%s\"}",
        v->id, v->title, v->thumbnail, v->duration_sec, last_pos, v->filename);
    
    send_json(client, HTTP_200, json);
}

/* API: Update watch history */
static void api_update_history(SOCKET client, int video_id, int user_id, HttpRequest* req) {
    char pos_str[32] = {0};
    get_query_param(req->body, "position", pos_str, sizeof(pos_str));
    
    if (!pos_str[0]) {
        get_query_param(req->query, "position", pos_str, sizeof(pos_str));
    }
    
    int position = atoi(pos_str);
    
    if (history_update(user_id, video_id, position) == 0) {
        send_json(client, HTTP_200, "{\"success\":true}");
    } else {
        send_json(client, HTTP_500, "{\"error\":\"Failed to update history\"}");
    }
}

/* API: Get watch history */
static void api_get_history(SOCKET client, int user_id) {
    WatchHistory history[MAX_VIDEOS];
    int count = history_get_user_history(user_id, history, MAX_VIDEOS);
    
    char json[8192] = "[";
    char* p = json + 1;
    size_t remaining = sizeof(json) - 2;
    
    for (int i = 0; i < count; i++) {
        Video* v = video_find_by_id(history[i].video_id);
        if (!v) continue;
        
        int written = snprintf(p, remaining,
            "%s{\"video_id\":%d,\"title\":\"%s\",\"last_pos\":%d,\"duration\":%d}",
            i > 0 ? "," : "",
            history[i].video_id, v->title, history[i].last_pos_sec, v->duration_sec);
        
        p += written;
        remaining -= written;
    }
    
    strcat(p, "]");
    
    send_json(client, HTTP_200, json);
}

/* API: Get current user */
static void api_get_user(SOCKET client, int user_id) {
    User* user = user_find_by_id(user_id);
    if (!user) {
        send_json(client, HTTP_401, "{\"error\":\"Not authenticated\"}");
        return;
    }
    
    char json[256];
    snprintf(json, sizeof(json), "{\"id\":%d,\"username\":\"%s\"}", user->id, user->username);
    send_json(client, HTTP_200, json);
}

/* Main request handler */
void handle_request(SOCKET client, const char* raw_request) {
    HttpRequest req;
    
    if (parse_http_request(raw_request, &req) < 0) {
        const char* msg = "Bad Request";
        send_response(client, HTTP_400, "text/plain", NULL, msg, strlen(msg));
        return;
    }
    
    log_message(LOG_INFO, "%s %s", req.method, req.path);
    
    /* Get session */
    int user_id = 0;
    char token[65] = {0};
    get_cookie_value(req.cookie, "session", token, sizeof(token));
    
    Session* session = session_find(token);
    if (session) {
        user_id = session->user_id;
    }
    
    /* Route request */
    
    /* Static files */
    if (strncmp(req.path, "/css/", 5) == 0 ||
        strncmp(req.path, "/js/", 4) == 0 ||
        strncmp(req.path, "/thumbnails/", 12) == 0) {
        send_static_file(client, req.path, &req);
        return;
    }
    
    /* Root - redirect based on auth */
    if (strcmp(req.path, "/") == 0) {
        if (user_id > 0) {
            send_redirect(client, "/list.html", NULL);
        } else {
            send_redirect(client, "/login.html", NULL);
        }
        return;
    }
    
    /* Login page */
    if (strcmp(req.path, "/login.html") == 0) {
        send_static_file(client, "login.html", &req);
        return;
    }

    /* Register page */
    if (strcmp(req.path, "/register.html") == 0) {
        send_static_file(client, "register.html", &req);
        return;
    }
    
    /* Index page */
    if (strcmp(req.path, "/index.html") == 0) {
        if (user_id > 0) {
            send_redirect(client, "/list.html", NULL);
        } else {
            send_static_file(client, "index.html", &req);
        }
        return;
    }
    
    /* Login POST */
    if (strcmp(req.path, "/login") == 0 && strcmp(req.method, "POST") == 0) {
        handle_login(client, &req);
        return;
    }

    /* Register POST */
    if (strcmp(req.path, "/register") == 0 && strcmp(req.method, "POST") == 0) {
        handle_register(client, &req);
        return;
    }
    
    /* Logout */
    if (strcmp(req.path, "/logout") == 0) {
        handle_logout(client, &req);
        return;
    }
    
    /* Protected pages - require login */
    if (user_id == 0) {
        if (strcmp(req.path, "/list.html") == 0 ||
            strcmp(req.path, "/player.html") == 0 ||
            strncmp(req.path, "/api/", 5) == 0 ||
            strncmp(req.path, "/video/", 7) == 0) {
            send_redirect(client, "/login.html", NULL);
            return;
        }
    }
    
    /* List page */
    if (strcmp(req.path, "/list.html") == 0) {
        send_static_file(client, "list.html", &req);
        return;
    }
    
    /* Player page */
    if (strcmp(req.path, "/player.html") == 0) {
        send_static_file(client, "player.html", &req);
        return;
    }
    
    /* Video streaming */
    if (strncmp(req.path, "/video/", 7) == 0) {
        int video_id = atoi(req.path + 7);
        stream_video(client, video_id, &req, user_id);
        return;
    }
    
    /* API endpoints */
    if (strcmp(req.path, "/api/videos") == 0) {
        api_get_videos(client, user_id);
        return;
    }
    
    if (strncmp(req.path, "/api/videos/", 12) == 0) {
        int video_id = atoi(req.path + 12);
        api_get_video(client, video_id, user_id);
        return;
    }
    
    if (strcmp(req.path, "/api/history") == 0) {
        if (strcmp(req.method, "GET") == 0) {
            api_get_history(client, user_id);
        }
        return;
    }
    
    if (strncmp(req.path, "/api/history/", 13) == 0) {
        int video_id = atoi(req.path + 13);
        if (strcmp(req.method, "POST") == 0) {
            api_update_history(client, video_id, user_id, &req);
        }
        return;
    }
    
    if (strcmp(req.path, "/api/user") == 0) {
        api_get_user(client, user_id);
        return;
    }
    
    /* Try static file */
    send_static_file(client, req.path, &req);
}
