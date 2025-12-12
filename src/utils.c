/*
 * OTT Video Streaming Server - Utility Functions
 */

#include "common.h"

/* Global log level */
static LogLevel g_log_level = LOG_INFO;

/* Logging function */
void log_message(LogLevel level, const char* format, ...) {
    if (level < g_log_level) return;
    
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    
    const char* level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    
    fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec,
            level_str[level]);
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
    fflush(stderr);
}

/* Get content type from file extension */
const char* get_content_type(const char* path) {
    const char* last_dot = strrchr(path, '.');
    if (last_dot) {
        if (strcmp(last_dot, ".css") == 0) return "text/css";
        if (strcmp(last_dot, ".html") == 0) return "text/html";
        if (strcmp(last_dot, ".htm") == 0) return "text/html";
        if (strcmp(last_dot, ".js") == 0) return "application/javascript";
        if (strcmp(last_dot, ".json") == 0) return "application/json";
        if (strcmp(last_dot, ".png") == 0) return "image/png";
        if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".gif") == 0) return "image/gif";
        if (strcmp(last_dot, ".ico") == 0) return "image/x-icon";
        if (strcmp(last_dot, ".svg") == 0) return "image/svg+xml";
        if (strcmp(last_dot, ".mp4") == 0) return "video/mp4";
        if (strcmp(last_dot, ".webm") == 0) return "video/webm";
        if (strcmp(last_dot, ".mp3") == 0) return "audio/mpeg";
        if (strcmp(last_dot, ".wav") == 0) return "audio/wav";
        if (strcmp(last_dot, ".txt") == 0) return "text/plain";
    }
    return "application/octet-stream";
}

/* URL decode */
void url_decode(char* dst, const char* src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* Get query parameter value */
char* get_query_param(const char* query, const char* name, char* value, size_t value_size) {
    if (!query || !name || !value) return NULL;
    
    char search[128];
    snprintf(search, sizeof(search), "%s=", name);
    
    const char* start = strstr(query, search);
    if (!start) return NULL;
    
    start += strlen(search);
    const char* end = strchr(start, '&');
    
    size_t len = end ? (size_t)(end - start) : strlen(start);
    if (len >= value_size) len = value_size - 1;
    
    strncpy(value, start, len);
    value[len] = '\0';
    
    /* URL decode the value */
    char decoded[512];
    url_decode(decoded, value);
    strncpy(value, decoded, value_size - 1);
    value[value_size - 1] = '\0';
    
    return value;
}

/* Generate random session token */
void generate_session_token(char* token, size_t len) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static int seeded = 0;
    
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        seeded = 1;
    }
    
    for (size_t i = 0; i < len - 1; i++) {
        token[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    token[len - 1] = '\0';
}

/* Simple hash function for passwords (DJB2) */
unsigned long simple_hash(const char* str) {
    unsigned long hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash;
}

/* Parse HTTP request */
int parse_http_request(const char* raw, HttpRequest* req) {
    memset(req, 0, sizeof(HttpRequest));
    req->range_start = -1;
    req->range_end = -1;
    
    /* Parse request line */
    char* line_end = strstr(raw, "\r\n");
    if (!line_end) return -1;
    
    char request_line[1024];
    size_t line_len = line_end - raw;
    if (line_len >= sizeof(request_line)) return -1;
    
    strncpy(request_line, raw, line_len);
    request_line[line_len] = '\0';
    
    /* Parse method, path, version */
    char* token = strtok(request_line, " ");
    if (!token) return -1;
    strncpy(req->method, token, sizeof(req->method) - 1);
    
    token = strtok(NULL, " ");
    if (!token) return -1;
    
    /* Split path and query */
    char* query_start = strchr(token, '?');
    if (query_start) {
        *query_start = '\0';
        strncpy(req->query, query_start + 1, sizeof(req->query) - 1);
    }
    strncpy(req->path, token, sizeof(req->path) - 1);
    
    token = strtok(NULL, " ");
    if (token) {
        strncpy(req->version, token, sizeof(req->version) - 1);
    }
    
    /* Parse headers */
    const char* header_start = line_end + 2;
    const char* body_start = strstr(raw, "\r\n\r\n");
    
    while (header_start < body_start) {
        line_end = strstr(header_start, "\r\n");
        if (!line_end) break;
        
        char header_line[1024];
        line_len = line_end - header_start;
        if (line_len >= sizeof(header_line)) {
            header_start = line_end + 2;
            continue;
        }
        
        strncpy(header_line, header_start, line_len);
        header_line[line_len] = '\0';
        
        /* Parse specific headers */
        if (strncasecmp(header_line, "Host:", 5) == 0) {
            char* value = header_line + 5;
            while (*value == ' ') value++;
            strncpy(req->host, value, sizeof(req->host) - 1);
        }
        else if (strncasecmp(header_line, "Cookie:", 7) == 0) {
            char* value = header_line + 7;
            while (*value == ' ') value++;
            strncpy(req->cookie, value, sizeof(req->cookie) - 1);
        }
        else if (strncasecmp(header_line, "Content-Type:", 13) == 0) {
            char* value = header_line + 13;
            while (*value == ' ') value++;
            strncpy(req->content_type, value, sizeof(req->content_type) - 1);
        }
        else if (strncasecmp(header_line, "Content-Length:", 15) == 0) {
            req->content_length = atoi(header_line + 15);
        }
        else if (strncasecmp(header_line, "Range:", 6) == 0) {
            /* Parse Range: bytes=start-end */
            char* range_val = header_line + 6;
            while (*range_val == ' ') range_val++;
            
            if (strncmp(range_val, "bytes=", 6) == 0) {
                req->has_range = 1;
                char* range_spec = range_val + 6;
                char* dash = strchr(range_spec, '-');
                
                if (dash) {
                    *dash = '\0';
                    if (*range_spec) {
                        req->range_start = atol(range_spec);
                    }
                    if (*(dash + 1)) {
                        req->range_end = atol(dash + 1);
                    }
                }
            }
        }
        
        header_start = line_end + 2;
    }
    
    /* Copy body if present */
    if (body_start && req->content_length > 0) {
        body_start += 4;
        size_t body_len = req->content_length;
        if (body_len >= sizeof(req->body)) {
            body_len = sizeof(req->body) - 1;
        }
        strncpy(req->body, body_start, body_len);
        req->body[body_len] = '\0';
    }
    
    return 0;
}

/* Get cookie value */
char* get_cookie_value(const char* cookies, const char* name, char* value, size_t value_size) {
    if (!cookies || !name || !value) return NULL;
    
    char search[128];
    snprintf(search, sizeof(search), "%s=", name);
    
    const char* start = strstr(cookies, search);
    if (!start) return NULL;
    
    start += strlen(search);
    const char* end = strchr(start, ';');
    
    size_t len = end ? (size_t)(end - start) : strlen(start);
    if (len >= value_size) len = value_size - 1;
    
    strncpy(value, start, len);
    value[len] = '\0';
    
    return value;
}

#if defined(_WIN32)
/* Windows strncasecmp equivalent */
int strncasecmp(const char* s1, const char* s2, size_t n) {
    return _strnicmp(s1, s2, n);
}
#endif
