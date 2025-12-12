/*
 * OTT Video Streaming Server - Main Entry Point
 * Multi-threaded TCP server implementation
 */

#include "common.h"

/* External function declarations */
extern void data_init(void);
extern void data_load(void);
extern void data_save(void);
extern int ffmpeg_check_available(void);
extern int ffmpeg_scan_videos(void);
extern void handle_request(SOCKET client, const char* raw_request);

/* Thread pool configuration */
#define THREAD_POOL_SIZE 8
#define MAX_QUEUE_SIZE 100

/* Connection queue */
typedef struct {
    SOCKET sockets[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
#if defined(_WIN32)
    HANDLE not_empty;
    HANDLE not_full;
#else
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
#endif
} ConnectionQueue;

static ConnectionQueue g_queue;
static int g_running = 1;

/* Initialize connection queue */
void queue_init(ConnectionQueue* q) {
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
#if defined(_WIN32)
    q->not_empty = CreateEvent(NULL, FALSE, FALSE, NULL);
    q->not_full = CreateEvent(NULL, FALSE, TRUE, NULL);
#else
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
#endif
}

/* Add socket to queue */
void queue_push(ConnectionQueue* q, SOCKET sock) {
    pthread_mutex_lock(&q->mutex);
    
#if defined(_WIN32)
    while (q->count >= MAX_QUEUE_SIZE) {
        pthread_mutex_unlock(&q->mutex);
        WaitForSingleObject(q->not_full, INFINITE);
        pthread_mutex_lock(&q->mutex);
    }
#else
    while (q->count >= MAX_QUEUE_SIZE) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
#endif
    
    q->sockets[q->rear] = sock;
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->count++;
    
#if defined(_WIN32)
    SetEvent(q->not_empty);
#else
    pthread_cond_signal(&q->not_empty);
#endif
    
    pthread_mutex_unlock(&q->mutex);
}

/* Get socket from queue */
SOCKET queue_pop(ConnectionQueue* q) {
    pthread_mutex_lock(&q->mutex);
    
#if defined(_WIN32)
    while (q->count == 0 && g_running) {
        pthread_mutex_unlock(&q->mutex);
        WaitForSingleObject(q->not_empty, 1000);
        pthread_mutex_lock(&q->mutex);
    }
#else
    while (q->count == 0 && g_running) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait(&q->not_empty, &q->mutex, &ts);
    }
#endif
    
    if (!g_running && q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }
    
    SOCKET sock = q->sockets[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->count--;
    
#if defined(_WIN32)
    SetEvent(q->not_full);
#else
    pthread_cond_signal(&q->not_full);
#endif
    
    pthread_mutex_unlock(&q->mutex);
    return sock;
}

/* Worker thread function */
#if defined(_WIN32)
unsigned __stdcall worker_thread(void* arg) {
#else
void* worker_thread(void* arg) {
#endif
    (void)arg;
    
    log_message(LOG_DEBUG, "Worker thread started");
    
    while (g_running) {
        SOCKET client = queue_pop(&g_queue);
        if (!ISVALIDSOCKET(client)) continue;
        
        /* Receive request */
        char request[MAX_REQUEST_SIZE + 1];
        int received = 0;
        int total = 0;
        
        /* Set receive timeout */
#if defined(_WIN32)
        DWORD timeout = 5000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
        
        while (total < MAX_REQUEST_SIZE) {
            received = recv(client, request + total, MAX_REQUEST_SIZE - total, 0);
            if (received <= 0) break;
            
            total += received;
            request[total] = '\0';
            
            /* Check for end of headers */
            if (strstr(request, "\r\n\r\n")) {
                /* Check if we need to read body */
                char* cl_header = strstr(request, "Content-Length:");
                if (cl_header) {
                    int content_length = atoi(cl_header + 15);
                    char* body_start = strstr(request, "\r\n\r\n");
                    if (body_start) {
                        body_start += 4;
                        int header_len = body_start - request;
                        int body_received = total - header_len;
                        
                        /* Read remaining body */
                        while (body_received < content_length && total < MAX_REQUEST_SIZE) {
                            received = recv(client, request + total, MAX_REQUEST_SIZE - total, 0);
                            if (received <= 0) break;
                            total += received;
                            body_received += received;
                        }
                        request[total] = '\0';
                    }
                }
                break;
            }
        }
        
        if (total > 0) {
            handle_request(client, request);
        }
        
        CLOSESOCKET(client);
    }
    
    log_message(LOG_DEBUG, "Worker thread exiting");
    
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

/* Create listening socket */
SOCKET create_server_socket(const char* port) {
    struct addrinfo hints;
    struct addrinfo* bind_address;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if (getaddrinfo(NULL, port, &hints, &bind_address) != 0) {
        log_message(LOG_ERROR, "getaddrinfo() failed");
        return -1;
    }
    
    SOCKET server = socket(bind_address->ai_family,
                          bind_address->ai_socktype,
                          bind_address->ai_protocol);
    
    if (!ISVALIDSOCKET(server)) {
        log_message(LOG_ERROR, "socket() failed: %d", GETSOCKETERRNO());
        freeaddrinfo(bind_address);
        return -1;
    }
    
    /* Allow address reuse */
    int opt = 1;
#if defined(_WIN32)
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    if (bind(server, bind_address->ai_addr, (int)bind_address->ai_addrlen) != 0) {
        log_message(LOG_ERROR, "bind() failed: %d", GETSOCKETERRNO());
        freeaddrinfo(bind_address);
        CLOSESOCKET(server);
        return -1;
    }
    
    freeaddrinfo(bind_address);
    
    if (listen(server, MAX_CLIENTS) != 0) {
        log_message(LOG_ERROR, "listen() failed: %d", GETSOCKETERRNO());
        CLOSESOCKET(server);
        return -1;
    }
    
    return server;
}

/* Signal handler */
#if !defined(_WIN32)
#include <signal.h>
void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
    log_message(LOG_INFO, "Shutdown signal received");
}
#endif

/* Main function */
int main(int argc, char* argv[]) {
    const char* port = SERVER_PORT;
    
    /* Parse command line */
    if (argc > 1) {
        port = argv[1];
    }
    
#if defined(_WIN32)
    /* Initialize Winsock */
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
    
    /* Create data directory */
    CreateDirectoryA(DATA_DIR, NULL);
    CreateDirectoryA(THUMBNAIL_DIR, NULL);
#else
    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    /* Create data directory */
    mkdir(DATA_DIR, 0755);
    mkdir(THUMBNAIL_DIR, 0755);
#endif
    
    log_message(LOG_INFO, "=================================");
    log_message(LOG_INFO, "OTT Video Streaming Server v1.0");
    log_message(LOG_INFO, "=================================");
    
    /* Initialize data storage */
    data_init();
    data_load();
    
    /* Check FFmpeg and scan videos */
    if (ffmpeg_check_available()) {
        log_message(LOG_INFO, "FFmpeg is available");
    } else {
        log_message(LOG_WARN, "FFmpeg not found - thumbnails will not be generated");
    }
    
    /* Always scan videos directory */
    ffmpeg_scan_videos();
    
    /* Initialize connection queue */
    queue_init(&g_queue);
    
    /* Create worker threads */
    pthread_t threads[THREAD_POOL_SIZE];
    
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
#if defined(_WIN32)
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, worker_thread, NULL, 0, NULL);
#else
        pthread_create(&threads[i], NULL, worker_thread, NULL);
#endif
    }
    
    log_message(LOG_INFO, "Started %d worker threads", THREAD_POOL_SIZE);
    
    /* Create server socket */
    SOCKET server = create_server_socket(port);
    if (!ISVALIDSOCKET(server)) {
        log_message(LOG_ERROR, "Failed to create server socket");
        return 1;
    }
    
    log_message(LOG_INFO, "Server listening on port %s", port);
    log_message(LOG_INFO, "Open http://localhost:%s in your browser", port);
    log_message(LOG_INFO, "Default users: admin/admin123, user1/password, test/test");
    
    /* Main accept loop */
    while (g_running) {
        struct sockaddr_storage client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        /* Use select for timeout */
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(server, &reads);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select((int)server + 1, &reads, NULL, NULL, &timeout);
        
        if (ready < 0) {
            if (g_running) {
                log_message(LOG_ERROR, "select() failed: %d", GETSOCKETERRNO());
            }
            break;
        }
        
        if (ready == 0) continue;
        
        SOCKET client = accept(server, (struct sockaddr*)&client_addr, &addr_len);
        
        if (!ISVALIDSOCKET(client)) {
            if (g_running) {
                log_message(LOG_WARN, "accept() failed: %d", GETSOCKETERRNO());
            }
            continue;
        }
        
        /* Add to queue for worker threads */
        queue_push(&g_queue, client);
    }
    
    /* Cleanup */
    log_message(LOG_INFO, "Shutting down...");
    
    g_running = 0;
    
    /* Wake up worker threads */
#if defined(_WIN32)
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        SetEvent(g_queue.not_empty);
    }
    
    WaitForMultipleObjects(THREAD_POOL_SIZE, threads, TRUE, 5000);
    
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        CloseHandle(threads[i]);
    }
#else
    pthread_cond_broadcast(&g_queue.not_empty);
    
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }
#endif
    
    /* Save data */
    data_save();
    
    CLOSESOCKET(server);
    
#if defined(_WIN32)
    WSACleanup();
#endif
    
    log_message(LOG_INFO, "Server stopped");
    return 0;
}
