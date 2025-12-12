# OTT Video Streaming Server - 상세 프로젝트 문서

---

# 목차

1. [프로젝트 개요](#1-프로젝트-개요)
2. [파일 구조](#2-파일-구조)
3. [소스 코드 상세](#3-소스-코드-상세)
4. [데이터베이스 설계](#4-데이터베이스-설계)
5. [HTTP API 명세](#5-http-api-명세)
6. [프론트엔드 구성](#6-프론트엔드-구성)
7. [핵심 알고리즘](#7-핵심-알고리즘)
8. [실행 환경](#8-실행-환경)

---

# 1. 프로젝트 개요

## 1.1 프로젝트 정보
| 항목 | 내용 |
|------|------|
| 프로젝트명 | OTT Video Streaming Server |
| 개발 언어 | C (C99) |
| 데이터베이스 | PostgreSQL 15 |
| 프로토콜 | HTTP/1.1 |
| 대상 플랫폼 | Windows 10/11 |

## 1.2 주요 기능
- 사용자 로그인/회원가입 (세션 기반 인증)
- 비디오 목록 조회 및 메타데이터 관리
- HTTP Range Request 기반 비디오 스트리밍
- 시청 기록 저장 및 이어보기
- 자동 썸네일 생성 (FFmpeg 연동)

## 1.3 기술 스택

| 계층 | 기술 | 설명 |
|------|------|------|
| 네트워크 | WinSock2 | Windows 소켓 API |
| 멀티스레딩 | Win32 Threads | _beginthreadex, CRITICAL_SECTION |
| 데이터베이스 | libpq | PostgreSQL C 클라이언트 라이브러리 |
| 프론트엔드 | HTML5, CSS3, JavaScript | 반응형 웹 UI |
| 미디어 처리 | FFmpeg/FFprobe | 썸네일 추출, 영상 길이 분석 |

---

# 2. 파일 구조

## 2.1 전체 디렉토리 트리

```
video_streaming/
│
├── src/                           # 소스 코드 디렉토리
│   ├── common.h                   # 공통 헤더 (158줄)
│   ├── main.c                     # 서버 진입점 (411줄)
│   ├── http_handler.c             # HTTP 요청 처리 (598줄)
│   ├── db.c                       # PostgreSQL 연동 (433줄)
│   ├── utils.c                    # 유틸리티 함수 (280줄)
│   ├── ffmpeg_helper.c            # FFmpeg 연동 (250줄)
│   ├── data.c                     # 파일 기반 저장 (미사용, 레거시)
│   ├── libpq-fe.h                 # PostgreSQL 헤더
│   ├── postgres_ext.h             # PostgreSQL 확장 헤더
│   └── pg_config_ext.h            # PostgreSQL 설정 헤더
│
├── static/                        # 정적 파일 (프론트엔드)
│   ├── index.html                 # 랜딩 페이지 (1,335 bytes)
│   ├── login.html                 # 로그인 페이지 (3,221 bytes)
│   ├── register.html              # 회원가입 페이지 (2,358 bytes)
│   ├── list.html                  # 비디오 목록 (6,359 bytes)
│   ├── player.html                # 비디오 플레이어 (10,860 bytes)
│   ├── css/
│   │   └── style.css              # 스타일시트
│   ├── js/
│   │   └── (JavaScript 파일)
│   └── thumbnails/                # 자동 생성된 썸네일
│
├── sql/
│   └── schema.sql                 # DB 스키마 (53줄)
│
├── videos/                        # 비디오 파일 저장소
│   └── *.mp4                      # MP4 비디오 파일들
│
├── build_vs.bat                   # Visual Studio 빌드 스크립트
├── docker-compose.yml             # Docker Compose 설정
├── Makefile                       # Linux용 Makefile
└── README.md                      # 프로젝트 설명서
```

## 2.2 파일별 상세 설명

### 2.2.1 src/common.h (158줄)

**역할**: 프로젝트 전역 헤더. 모든 소스 파일에서 include.

**주요 내용**:

| 구분 | 내용 |
|------|------|
| 플랫폼 분기 | `#ifdef _WIN32` 로 Windows/POSIX 분리 |
| 소켓 매크로 | `ISVALIDSOCKET`, `CLOSESOCKET`, `GETSOCKETERRNO` |
| 스레드 호환 | Windows CRITICAL_SECTION → pthread_mutex_t 매핑 |

**상수 정의**:
```c
#define SERVER_PORT     "8080"      // 서버 포트
#define MAX_CLIENTS     100         // 최대 동시 접속
#define MAX_REQUEST_SIZE 8192       // 요청 버퍼 크기
#define BUFFER_SIZE     65536       // I/O 버퍼 (64KB)
#define MAX_PATH_LEN    512         // 경로 최대 길이
#define SESSION_TIMEOUT 3600        // 세션 만료 (1시간)
```

**자료구조**:

```c
// 사용자 정보
typedef struct {
    int id;
    char username[64];
    char password_hash[256];
    int active;
} User;

// 비디오 정보
typedef struct {
    int id;
    char title[256];
    char filename[256];
    char thumbnail[256];
    int duration_sec;
    char description[512];
} Video;

// 세션 정보
typedef struct {
    char token[65];
    int user_id;
    time_t expires_at;
    int active;
} Session;

// 시청 기록
typedef struct {
    int user_id;
    int video_id;
    int last_pos_sec;
    time_t updated_at;
} WatchHistory;

// HTTP 요청 파싱 결과
typedef struct {
    char method[16];        // GET, POST 등
    char path[512];         // 요청 경로
    char query[512];        // 쿼리스트링
    char cookie[512];       // Cookie 헤더
    int content_length;     // Content-Length
    long range_start;       // Range 시작
    long range_end;         // Range 끝
    int has_range;          // Range 요청 여부
    char body[4096];        // 요청 본문
} HttpRequest;
```

---

### 2.2.2 src/main.c (411줄)

**역할**: 프로그램 진입점. 서버 초기화, 멀티스레드 관리, 메인 루프.

**함수 목록**:

| 함수명 | 라인 | 설명 |
|--------|------|------|
| `queue_init` | 39-52 | 연결 큐 초기화 |
| `queue_push` | 54-81 | 큐에 소켓 추가 (Producer) |
| `queue_pop` | 83-119 | 큐에서 소켓 꺼내기 (Consumer) |
| `worker_thread` | 125-198 | Worker 스레드 함수 |
| `create_server_socket` | 201-249 | 리스닝 소켓 생성 |
| `signal_handler` | 254-258 | 시그널 핸들러 (Linux용) |
| `main` | 262-410 | 메인 함수 |

**핵심 로직 (main 함수)**:
```
1. WSAStartup() - WinSock 초기화
2. data_init() - DB 연결
3. ffmpeg_scan_videos() - 비디오 폴더 스캔
4. create_server_socket() - 포트 8080 바인딩
5. Worker Thread 8개 생성
6. accept() 루프 - 클라이언트 연결 수락
7. queue_push() - 연결을 큐에 추가
```

**Connection Queue 구조**:
```c
typedef struct {
    SOCKET sockets[MAX_CLIENTS];  // 소켓 배열
    int front, rear, count;        // 큐 인덱스
    pthread_mutex_t mutex;         // 동기화
    HANDLE not_empty;              // 조건 변수
    HANDLE not_full;               // 조건 변수
} ConnectionQueue;
```

---

### 2.2.3 src/http_handler.c (598줄)

**역할**: HTTP 요청 라우팅 및 응답 생성.

**함수 목록**:

| 함수명 | 라인 | 설명 |
|--------|------|------|
| `send_response` | 30-52 | HTTP 응답 전송 |
| `send_json` | 54-57 | JSON 응답 전송 |
| `send_redirect` | 59-84 | 302 리다이렉트 |
| `send_static_file` | 86-149 | 정적 파일 전송 |
| `stream_video` | 151-263 | 비디오 스트리밍 (Range 지원) |
| `handle_login` | 265-294 | POST /login 처리 |
| `handle_register` | 296-324 | POST /register 처리 |
| `handle_logout` | 326-336 | 로그아웃 처리 |
| `api_get_videos` | 338-369 | GET /api/videos |
| `api_get_video` | 371-391 | GET /api/videos/:id |
| `api_update_history` | 393-409 | POST /api/history/:id |
| `api_get_history` | 411-436 | GET /api/history |
| `api_get_user` | 438-449 | GET /api/user |
| `handle_request` | 451-597 | 메인 라우터 |

**라우팅 테이블 (`handle_request` 내부)**:

| 경로 | 메소드 | 핸들러 |
|------|--------|--------|
| `/` | GET | 로그인 상태에 따라 리다이렉트 |
| `/login.html` | GET | `send_static_file` |
| `/register.html` | GET | `send_static_file` |
| `/list.html` | GET | `send_static_file` (인증 필요) |
| `/player.html` | GET | `send_static_file` (인증 필요) |
| `/login` | POST | `handle_login` |
| `/register` | POST | `handle_register` |
| `/logout` | GET | `handle_logout` |
| `/video/:id` | GET | `stream_video` |
| `/api/videos` | GET | `api_get_videos` |
| `/api/videos/:id` | GET | `api_get_video` |
| `/api/history` | GET | `api_get_history` |
| `/api/history/:id` | POST | `api_update_history` |
| `/api/user` | GET | `api_get_user` |
| `/css/*`, `/js/*` | GET | `send_static_file` |

---

### 2.2.4 src/db.c (433줄)

**역할**: PostgreSQL 데이터베이스 연동 (CRUD 연산).

**DB 설정**:
```c
#define DB_HOST "127.0.0.1"
#define DB_PORT "5432"
#define DB_NAME "ott_streaming"
#define DB_USER "ott"
#define DB_PASS "ott123"
```

**함수 목록**:

| 함수명 | 라인 | 설명 |
|--------|------|------|
| **내부 함수** | | |
| `db_query` | 31-47 | 단순 SQL 실행 |
| `db_query_params` | 49-65 | 파라미터화 쿼리 (SQL Injection 방지) |
| **초기화** | | |
| `data_init` | 67-91 | DB 연결 |
| `data_save` | 93-101 | DB 연결 해제 |
| `data_load` | 103-106 | No-op (호환성) |
| **User** | | |
| `user_find_by_username` | 110-139 | 사용자명으로 조회 |
| `user_find_by_id` | 141-165 | ID로 조회 |
| `user_verify_password` | 167-172 | 비밀번호 검증 |
| `user_create` | 174-191 | 회원가입 |
| **Video** | | |
| `video_add` | 195-217 | 비디오 추가/업데이트 |
| `video_find_by_id` | 219-254 | ID로 조회 |
| `video_get_all` | 256-288 | 전체 목록 |
| `video_count` | 290-297 | 개수 조회 |
| **Session** | | |
| `session_create` | 301-326 | 세션 생성 |
| `session_find` | 328-351 | 토큰으로 조회 |
| `session_destroy` | 353-358 | 세션 삭제 |
| **History** | | |
| `history_find` | 362-387 | 시청 기록 조회 |
| `history_update` | 389-407 | 시청 기록 저장 |
| `history_get_user_history` | 409-432 | 사용자 전체 기록 |

---

### 2.2.5 src/utils.c (280줄)

**역할**: 유틸리티 함수 모음.

**함수 목록**:

| 함수명 | 라인 | 설명 |
|--------|------|------|
| `log_message` | 10-31 | 로그 출력 (DEBUG/INFO/WARN/ERROR) |
| `get_content_type` | 33-55 | 파일 확장자 → MIME 타입 |
| `url_decode` | 57-80 | URL 디코딩 (%XX → 문자) |
| `get_query_param` | 82-108 | 쿼리스트링 파싱 |
| `generate_session_token` | 110-124 | 64자 랜덤 토큰 생성 |
| `simple_hash` | 126-136 | DJB2 해시 알고리즘 |
| `parse_http_request` | 138-250 | HTTP 요청 파싱 |
| `get_cookie_value` | 252-272 | Cookie 헤더 파싱 |
| `strncasecmp` | 275-278 | Windows용 호환 함수 |

**DJB2 해시 알고리즘**:
```c
unsigned long simple_hash(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}
```

**MIME 타입 매핑**:
```c
// get_content_type 함수 내부 매핑
.html → text/html
.css  → text/css
.js   → application/javascript
.json → application/json
.png  → image/png
.jpg  → image/jpeg
.mp4  → video/mp4
```

---

### 2.2.6 src/ffmpeg_helper.c (250줄)

**역할**: FFmpeg 외부 프로세스 연동.

**함수 목록**:

| 함수명 | 라인 | 설명 |
|--------|------|------|
| `ffmpeg_check_available` | 8-16 | FFmpeg 설치 확인 |
| `ffmpeg_extract_thumbnail` | 18-43 | 썸네일 추출 |
| `ffmpeg_get_duration` | 45-83 | 영상 길이 분석 |
| `ffmpeg_scan_videos` | 85-235 | videos/ 폴더 스캔 |
| `ffmpeg_create_default_thumbnail` | 237-249 | 기본 썸네일 생성 |

**썸네일 추출 명령**:
```bash
ffmpeg -i <video> -ss 10 -vframes 1 -vf scale=320:-1 <output.jpg>
```

**영상 길이 분석 명령**:
```bash
ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 <video>
```

---

# 3. 소스 코드 상세

## 3.1 프로그램 실행 흐름

```
main()
  │
  ├──► WSAStartup()              // WinSock 초기화
  │
  ├──► data_init()               // PostgreSQL 연결
  │         │
  │         └──► PQconnectdb()   // libpq 연결 함수
  │
  ├──► ffmpeg_scan_videos()      // videos/ 폴더 스캔
  │         │
  │         ├──► FindFirstFile() // 파일 검색
  │         ├──► ffmpeg_get_duration()
  │         ├──► ffmpeg_extract_thumbnail()
  │         └──► video_add()     // DB에 등록
  │
  ├──► create_server_socket()    // TCP 소켓 생성
  │         │
  │         ├──► getaddrinfo()
  │         ├──► socket()
  │         ├──► bind()
  │         └──► listen()
  │
  ├──► Worker Thread 생성 (8개)
  │         │
  │         └──► worker_thread()
  │                   │
  │                   └──► queue_pop()
  │                   └──► recv()
  │                   └──► handle_request()
  │                   └──► CLOSESOCKET()
  │
  └──► Accept Loop
            │
            ├──► accept()
            └──► queue_push()    // Worker에게 전달
```

## 3.2 HTTP 요청 처리 흐름

```
handle_request(client, raw_request)
  │
  ├──► parse_http_request()      // 요청 파싱
  │         │
  │         ├── method (GET/POST)
  │         ├── path (/login, /api/videos 등)
  │         ├── headers (Cookie, Range 등)
  │         └── body (form data)
  │
  ├──► 세션 확인
  │         │
  │         ├──► get_cookie_value("session")
  │         └──► session_find(token)
  │
  └──► 라우팅 분기
            │
            ├── /login (POST)  ──► handle_login()
            ├── /register (POST) ──► handle_register()
            ├── /api/videos (GET) ──► api_get_videos()
            ├── /video/:id (GET) ──► stream_video()
            └── 기타 ──► send_static_file()
```

## 3.3 비디오 스트리밍 흐름

```
stream_video(client, video_id, req)
  │
  ├──► video_find_by_id()        // DB에서 비디오 정보 조회
  │
  ├──► fopen(video_path, "rb")   // 파일 열기
  │
  ├──► Range 헤더 파싱
  │         │
  │         └── Range: bytes=0-1048575
  │                     ↓
  │              range_start=0, range_end=1048575
  │
  ├──► HTTP 헤더 전송
  │         │
  │         ├── HTTP/1.1 206 Partial Content
  │         ├── Content-Range: bytes 0-1048575/10000000
  │         └── Content-Length: 1048576
  │
  └──► 데이터 전송 (청크 단위)
            │
            └── while (bytes_remaining > 0)
                    fread() → send()
```

---

# 4. 데이터베이스 설계

## 4.1 ER 다이어그램

```
┌─────────────────┐
│     users       │
├─────────────────┤
│ PK  id          │──────────┐
│     username    │          │ 1:N
│     password_hash          │
│     active      │          │
│     created_at  │          │
└─────────────────┘          │
                             │
    ┌────────────────────────┤
    │                        │
    ▼                        ▼
┌─────────────────┐   ┌─────────────────┐
│    sessions     │   │  watch_history  │
├─────────────────┤   ├─────────────────┤
│ PK  id          │   │ PK  id          │
│ FK  user_id     │   │ FK  user_id     │
│     token       │   │ FK  video_id    │◄──────┐
│     expires_at  │   │     last_pos_sec│       │
│     created_at  │   │     updated_at  │       │
└─────────────────┘   └─────────────────┘       │
                                                │
                      ┌─────────────────┐       │
                      │     videos      │       │
                      ├─────────────────┤       │
                      │ PK  id          │───────┘
                      │     title       │
                      │     filename    │ UNIQUE
                      │     thumbnail   │
                      │     duration_sec│
                      │     description │
                      │     created_at  │
                      └─────────────────┘
```

## 4.2 테이블 DDL

### users 테이블
```sql
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(64) UNIQUE NOT NULL,
    password_hash VARCHAR(256) NOT NULL,
    active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### videos 테이블
```sql
CREATE TABLE IF NOT EXISTS videos (
    id SERIAL PRIMARY KEY,
    title VARCHAR(256) NOT NULL,
    filename VARCHAR(256) UNIQUE NOT NULL,
    thumbnail VARCHAR(256),
    duration_sec INTEGER DEFAULT 0,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### sessions 테이블
```sql
CREATE TABLE IF NOT EXISTS sessions (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    token VARCHAR(64) UNIQUE NOT NULL,
    expires_at TIMESTAMP NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### watch_history 테이블
```sql
CREATE TABLE IF NOT EXISTS watch_history (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    video_id INTEGER REFERENCES videos(id) ON DELETE CASCADE,
    last_pos_sec INTEGER DEFAULT 0,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_id, video_id)
);
```

## 4.3 인덱스
```sql
CREATE INDEX idx_sessions_token ON sessions(token);
CREATE INDEX idx_sessions_user_id ON sessions(user_id);
CREATE INDEX idx_history_user_video ON watch_history(user_id, video_id);
```

## 4.4 초기 데이터
```sql
INSERT INTO users (username, password_hash) VALUES 
    ('admin', '407908580'),   -- 비밀번호: admin123
    ('user1', '402054200'),   -- 비밀번호: password
    ('test', '2090756197');   -- 비밀번호: test
```

---

# 5. HTTP API 명세

## 5.1 인증 API

### POST /login
로그인 처리

**Request**:
```
Content-Type: application/x-www-form-urlencoded

username=admin&password=admin123
```

**Response (성공)**:
```
HTTP/1.1 302 Found
Location: /list.html
Set-Cookie: session=<64자 토큰>; Path=/; HttpOnly
```

**Response (실패)**:
```
HTTP/1.1 302 Found
Location: /login.html?error=invalid
```

---

### POST /register
회원가입 처리

**Request**:
```
Content-Type: application/x-www-form-urlencoded

username=newuser&password=newpass123
```

**Response (성공)**:
```
HTTP/1.1 302 Found
Location: /login.html?registered=true
```

**Response (실패 - 중복)**:
```
HTTP/1.1 302 Found
Location: /register.html?error=exists
```

---

### GET /logout
로그아웃

**Response**:
```
HTTP/1.1 302 Found
Location: /login.html
Set-Cookie: session=; Path=/; Max-Age=0
```

---

## 5.2 비디오 API

### GET /api/videos
전체 비디오 목록 조회

**Response**:
```json
[
    {
        "id": 1,
        "title": "Sample Video",
        "thumbnail": "thumbnails/sample.jpg",
        "duration": 180,
        "last_pos": 45
    },
    ...
]
```

---

### GET /api/videos/:id
단일 비디오 정보

**Response**:
```json
{
    "id": 1,
    "title": "Sample Video",
    "thumbnail": "thumbnails/sample.jpg",
    "duration": 180,
    "last_pos": 45,
    "filename": "sample.mp4"
}
```

---

### GET /video/:id
비디오 스트리밍

**Request Header**:
```
Range: bytes=0-1048575
```

**Response**:
```
HTTP/1.1 206 Partial Content
Content-Type: video/mp4
Content-Length: 1048576
Content-Range: bytes 0-1048575/10000000
Accept-Ranges: bytes

<바이너리 데이터>
```

---

## 5.3 시청 기록 API

### GET /api/history
내 시청 기록 조회

**Response**:
```json
[
    {
        "video_id": 1,
        "title": "Sample Video",
        "last_pos": 45,
        "duration": 180
    }
]
```

---

### POST /api/history/:video_id
시청 위치 저장

**Request**:
```
Content-Type: application/x-www-form-urlencoded

position=67
```

**Response**:
```json
{"success": true}
```

---

### GET /api/user
현재 로그인 사용자 정보

**Response**:
```json
{
    "id": 1,
    "username": "admin"
}
```

---

# 6. 프론트엔드 구성

## 6.1 페이지 목록

| 파일 | 설명 | 인증 필요 |
|------|------|----------|
| `index.html` | 랜딩 페이지 | X |
| `login.html` | 로그인 페이지 | X |
| `register.html` | 회원가입 페이지 | X |
| `list.html` | 비디오 목록 | O |
| `player.html` | 비디오 플레이어 | O |

## 6.2 주요 UI 기능

### login.html
- 아이디/비밀번호 입력 폼
- 오류 메시지 표시 (invalid, missing)
- 회원가입 성공 메시지 표시
- 테스트 계정 안내

### register.html
- 아이디/비밀번호 입력 폼
- 중복 아이디 오류 표시
- 로그인 페이지 링크

### list.html
- 비디오 카드 그리드 레이아웃
- 썸네일, 제목, 재생시간 표시
- 이어보기 진행률 표시
- 로그아웃 버튼

### player.html
- HTML5 Video 플레이어
- 5초마다 재생 위치 자동 저장
- 이어보기 시작점 자동 설정
- 목록으로 돌아가기 버튼

---

# 7. 핵심 알고리즘

## 7.1 멀티스레드 Connection Pool

**패턴**: Producer-Consumer

```
Main Thread (Producer)         Worker Threads (Consumer)
       │                              │
       │     ┌──────────────┐         │
       └────►│    Queue     │◄────────┘
             │ [sock1]      │
             │ [sock2]      │
             │   ...        │
             └──────────────┘
```

**동기화**:
- Mutex: 큐 접근 동기화
- Event (Windows): not_empty, not_full 조건 변수

## 7.2 HTTP Range Request

**용도**: 대용량 비디오 스트리밍, 탐색(Seek) 지원

**처리 과정**:
```
1. 클라이언트: Range: bytes=5000000-
2. 서버: 파일 크기 확인 (예: 10MB)
3. 서버: 청크 크기 제한 (1MB)
4. 응답:
   - 206 Partial Content
   - Content-Range: bytes 5000000-5999999/10000000
5. 데이터 전송
```

## 7.3 세션 기반 인증

**흐름**:
```
1. 로그인 성공
   └─► session_create(user_id)
       └─► 64자 랜덤 토큰 생성
       └─► DB에 저장 (expires_at = now + 1시간)
       └─► Set-Cookie: session=<token>

2. 이후 요청
   └─► Cookie: session=<token>
   └─► session_find(token)
       └─► 유효성 검사
       └─► user_id 추출
```

## 7.4 DJB2 비밀번호 해싱

**알고리즘**:
```
hash = 5381
for each character c in password:
    hash = hash * 33 + c
return hash
```

**특징**:
- 단순하고 빠름
- 플랫폼 독립적
- ⚠️ 보안용으로는 부적합 (학습 목적)

---

# 8. 실행 환경

## 8.1 필수 요구사항

| 항목 | 요구사항 |
|------|----------|
| OS | Windows 10/11 (64-bit) |
| 컴파일러 | Visual Studio 2022 |
| 데이터베이스 | PostgreSQL 15+ |
| 브라우저 | Chrome, Firefox, Edge |

## 8.2 선택 요구사항

| 항목 | 용도 |
|------|------|
| FFmpeg | 썸네일 자동 생성 |
| Docker | PostgreSQL 컨테이너 실행 |

## 8.3 설치 및 실행

### 방법 1: 직접 설치

```bash
# 1. PostgreSQL 설치 및 설정
createdb -U postgres ott_streaming
psql -U postgres -d ott_streaming -f sql/schema.sql

# 2. 빌드
build_vs.bat

# 3. 실행
ott_server.exe
```

### 방법 2: Docker 사용

```bash
# 1. PostgreSQL 컨테이너 실행
docker-compose up -d

# 2. 빌드
build_vs.bat

# 3. 실행
ott_server.exe
```

## 8.4 설정 변경

### 포트 변경
`src/common.h`:
```c
#define SERVER_PORT "8080"  // 원하는 포트로 변경
```

### DB 연결 정보 변경
`src/db.c`:
```c
#define DB_HOST "127.0.0.1"
#define DB_PORT "5432"
#define DB_NAME "ott_streaming"
#define DB_USER "ott"
#define DB_PASS "ott123"
```

---

# 부록: 코드 통계

| 파일 | 라인 수 | 함수 수 | 크기 |
|------|--------|--------|------|
| common.h | 158 | - | 4,010 bytes |
| main.c | 411 | 7 | 11,478 bytes |
| http_handler.c | 598 | 14 | 18,425 bytes |
| db.c | 433 | 18 | 14,518 bytes |
| utils.c | 280 | 9 | 8,958 bytes |
| ffmpeg_helper.c | 250 | 5 | 8,514 bytes |
| **총계** | **2,130** | **53** | **65,903 bytes** |
