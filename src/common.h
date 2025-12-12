/* 
 * OTT Video Streaming Server - Common Header
 * Cross-platform definitions for Windows and POSIX
 */

#ifndef COMMON_H
#define COMMON_H

#define _CRT_SECURE_NO_WARNINGS

#if defined(_WIN32)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #include <windows.h>
    #include <process.h>
    
    #define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
    #define CLOSESOCKET(s) closesocket(s)
    #define GETSOCKETERRNO() (WSAGetLastError())
    
    typedef HANDLE pthread_t;
    typedef CRITICAL_SECTION pthread_mutex_t;
    
    #define pthread_mutex_init(m, attr) InitializeCriticalSection(m)
    #define pthread_mutex_lock(m) EnterCriticalSection(m)
    #define pthread_mutex_unlock(m) LeaveCriticalSection(m)
    #define pthread_mutex_destroy(m) DeleteCriticalSection(m)
    
    #define sleep(s) Sleep((s) * 1000)
    #define usleep(us) Sleep((us) / 1000)
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    #include <sys/stat.h>
    #include <dirent.h>
    #include <pthread.h>
    #include <sys/sendfile.h>
    #include <sys/wait.h>
    #include <fcntl.h>
    
    #define ISVALIDSOCKET(s) ((s) >= 0)
    #define CLOSESOCKET(s) close(s)
    #define SOCKET int
    #define GETSOCKETERRNO() (errno)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>

/* Server Configuration */
#define SERVER_PORT "8080"
#define MAX_CLIENTS 100
#define MAX_REQUEST_SIZE 8192
#define BUFFER_SIZE 65536
#define MAX_PATH_LEN 512
#define MAX_SESSIONS 100
#define SESSION_TIMEOUT 3600  /* 1 hour */
#define MAX_VIDEOS 100
#define MAX_USERS 50

/* Directories */
#define STATIC_DIR "static"
#define VIDEO_DIR "videos"
#define THUMBNAIL_DIR "static/thumbnails"
#define DATA_DIR "data"

/* Log levels */
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

/* User structure */
typedef struct {
    int id;
    char username[64];
    char password_hash[256];
    int active;
} User;

/* Video structure */
typedef struct {
    int id;
    char title[256];
    char filename[256];
    char thumbnail[256];
    int duration_sec;
    char description[512];
} Video;

/* Watch history */
typedef struct {
    int user_id;
    int video_id;
    int last_pos_sec;
    time_t updated_at;
} WatchHistory;

/* Session structure */
typedef struct {
    char token[65];
    int user_id;
    time_t expires_at;
    int active;
} Session;

/* HTTP Request structure */
typedef struct {
    char method[16];
    char path[512];
    char query[512];
    char version[16];
    char host[256];
    char cookie[512];
    char content_type[128];
    int content_length;
    long range_start;
    long range_end;
    int has_range;
    char body[4096];
} HttpRequest;

/* HTTP Response helpers */
#define HTTP_200 "HTTP/1.1 200 OK\r\n"
#define HTTP_206 "HTTP/1.1 206 Partial Content\r\n"
#define HTTP_302 "HTTP/1.1 302 Found\r\n"
#define HTTP_400 "HTTP/1.1 400 Bad Request\r\n"
#define HTTP_401 "HTTP/1.1 401 Unauthorized\r\n"
#define HTTP_403 "HTTP/1.1 403 Forbidden\r\n"
#define HTTP_404 "HTTP/1.1 404 Not Found\r\n"
#define HTTP_500 "HTTP/1.1 500 Internal Server Error\r\n"

/* Function declarations */
void log_message(LogLevel level, const char* format, ...);
const char* get_content_type(const char* path);
void url_decode(char* dst, const char* src);
char* get_query_param(const char* query, const char* name, char* value, size_t value_size);
void generate_session_token(char* token, size_t len);
unsigned long simple_hash(const char* str);
int user_create(const char* username, const char* password);

#endif /* COMMON_H */
