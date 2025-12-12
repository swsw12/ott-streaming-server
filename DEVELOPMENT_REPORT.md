# 프로젝트 보고서

## 프로젝트명: OTT Video Streaming Server

---

# 1. 시스템 구조도

## 1.1 전체 아키텍처

```
┌─────────────────────────────────────────────────────────────────────┐
│                           Client (Browser)                          │
│                    HTML5 Video Player + JavaScript                  │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ HTTP/TCP (Port 8080)
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         OTT Server (C)                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌────────────┐ │
│  │   main.c    │  │http_handler │  │    db.c     │  │   utils.c  │ │
│  │  (서버 코어) │  │  (HTTP처리) │  │  (DB연동)   │  │  (유틸리티)│ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └────────────┘ │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    Thread Pool (8 Workers)                   │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ libpq (TCP 5432)
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         PostgreSQL Database                         │
│         users / videos / sessions / watch_history                   │
└─────────────────────────────────────────────────────────────────────┘
```

## 1.2 요청 처리 흐름

```
Client → accept() → Queue → Worker Thread → parse_http_request()
                                                    │
                    ┌───────────────────────────────┤
                    ▼                               ▼
             [Static File]                   [API/Dynamic]
                    │                               │
          send_static_file()              handle_login() 등
                    │                               │
                    └───────────────────────────────┤
                                                    ▼
                                              HTTP Response
```

---

# 2. 개발 보고서

## 2.1 개발환경 및 컴파일 환경

### 개발 환경

| 항목 | 내용 |
|------|------|
| 운영체제 | Windows 11 |
| 개발 도구 | Visual Studio 2022 Community |
| 언어 | C (C99) |
| 데이터베이스 | PostgreSQL 15 |
| 버전 관리 | Git / GitHub |

### 컴파일 환경

| 항목 | 내용 |
|------|------|
| 컴파일러 | MSVC (cl.exe) 19.44 |
| 링커 | Microsoft Incremental Linker 14.44 |
| 타겟 아키텍처 | x64 |
| 빌드 방식 | 배치 스크립트 (build_vs.bat) |

### 사용 라이브러리

| 라이브러리 | 용도 |
|------------|------|
| WinSock2 (ws2_32.lib) | TCP 소켓 통신 |
| libpq | PostgreSQL 클라이언트 |
| Win32 API | 스레드, 파일 I/O |

---

## 2.2 설계 명세서

### 모듈 구성

| 모듈 | 파일 | 라인수 | 역할 |
|------|------|--------|------|
| Main | main.c | 411 | 서버 초기화, 스레드 풀, 연결 수락 |
| HTTP Handler | http_handler.c | 598 | 요청 파싱, 라우팅, 응답 생성 |
| Database | db.c | 433 | PostgreSQL CRUD 연산 |
| Utility | utils.c | 280 | 로깅, 해싱, 문자열 처리 |
| FFmpeg | ffmpeg_helper.c | 250 | 썸네일 추출, 영상 분석 |

### 데이터베이스 설계

**users**
| 컬럼 | 타입 | 설명 |
|------|------|------|
| id | SERIAL | PK |
| username | VARCHAR(64) | UNIQUE |
| password_hash | VARCHAR(256) | DJB2 해시 |
| active | BOOLEAN | 활성 상태 |

**videos**
| 컬럼 | 타입 | 설명 |
|------|------|------|
| id | SERIAL | PK |
| title | VARCHAR(256) | 제목 |
| filename | VARCHAR(256) | 파일명 (UNIQUE) |
| thumbnail | VARCHAR(256) | 썸네일 경로 |
| duration_sec | INTEGER | 재생시간(초) |

**sessions**
| 컬럼 | 타입 | 설명 |
|------|------|------|
| id | SERIAL | PK |
| user_id | INTEGER | FK → users |
| token | VARCHAR(64) | 세션 토큰 |
| expires_at | TIMESTAMP | 만료 시간 |

**watch_history**
| 컬럼 | 타입 | 설명 |
|------|------|------|
| id | SERIAL | PK |
| user_id | INTEGER | FK → users |
| video_id | INTEGER | FK → videos |
| last_pos_sec | INTEGER | 마지막 재생 위치 |

---

## 2.3 요구사항에 대한 구현 명세서

### 요구사항 충족 요약표

| 번호 | 요구사항 | 구현 | 비고 |
|------|----------|------|------|
| 1 | 멀티스레드 서버 | ✓ | Worker Thread Pool 8개 |
| 2 | HTTP 프로토콜 | ✓ | GET/POST, Range 지원 |
| 3 | 사용자 인증 | ✓ | 로그인/회원가입/세션 |
| 4 | 비디오 스트리밍 | ✓ | HTTP Range Request |
| 5 | 데이터베이스 연동 | ✓ | PostgreSQL + libpq |
| 6 | 시청 기록 저장 | ✓ | 이어보기 기능 |
| 7 | 비디오 목록 API | ✓ | JSON 응답 |
| 8 | 정적 파일 서빙 | ✓ | HTML/CSS/JS/이미지 |
| 9 | 썸네일 생성 | ✓ | FFmpeg 연동 |
| 10 | 보안 | ✓ | 해시, SQL Injection 방지 |

---

## 2.4 요구사항에 대한 세부 설명서

### 2.4.1 멀티스레드 서버

**구현 방식**: Producer-Consumer 패턴

```c
#define THREAD_POOL_SIZE 8
#define MAX_QUEUE_SIZE 100

typedef struct {
    SOCKET sockets[MAX_QUEUE_SIZE];
    int front, rear, count;
    pthread_mutex_t mutex;
    HANDLE not_empty, not_full;
} ConnectionQueue;
```

- Main Thread: accept() → 큐에 추가
- Worker Thread 8개: 큐에서 꺼내 처리
- 동기화: CRITICAL_SECTION + Event

### 2.4.2 HTTP 프로토콜

**지원 메소드**: GET, POST

**Range 요청**:
- Request: `Range: bytes=0-1048575`
- Response: `206 Partial Content` + `Content-Range`

**라우팅**:
| 경로 | 메소드 | 핸들러 |
|------|--------|--------|
| /login | POST | handle_login() |
| /register | POST | handle_register() |
| /api/videos | GET | api_get_videos() |
| /video/:id | GET | stream_video() |

### 2.4.3 사용자 인증

**비밀번호**: DJB2 해시
```c
unsigned long simple_hash(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}
```

**세션**: 64자 랜덤 토큰, Cookie 저장, 1시간 만료

### 2.4.4 비디오 스트리밍

1. 클라이언트가 Range 헤더와 함께 요청
2. 서버가 파일의 해당 구간만 읽음
3. 206 Partial Content로 응답
4. 브라우저가 버퍼링하며 재생

### 2.4.5 데이터베이스 연동

**라이브러리**: libpq (PostgreSQL C API)

**SQL Injection 방지**: Parameterized Query
```c
const char* params[1] = { username };
PGresult* result = db_query_params(
    "SELECT * FROM users WHERE username = $1", 1, params);
```

### 2.4.6 시청 기록

- 클라이언트에서 5초마다 현재 위치 전송
- API: `POST /api/history/:video_id`
- DB에 UPSERT 처리
- 다음 재생 시 마지막 위치부터 시작

---

# 3. 주요 함수 / 모듈 설명

## main.c
| 함수 | 설명 |
|------|------|
| main() | 서버 초기화, 메인 루프 |
| queue_push() | 연결 큐에 추가 |
| queue_pop() | 연결 큐에서 꺼냄 |
| worker_thread() | 워커 스레드 함수 |

## http_handler.c
| 함수 | 설명 |
|------|------|
| handle_request() | 메인 라우터 |
| handle_login() | 로그인 처리 |
| stream_video() | 비디오 스트리밍 |
| api_get_videos() | 비디오 목록 API |

## db.c
| 함수 | 설명 |
|------|------|
| user_find_by_username() | 사용자 조회 |
| user_verify_password() | 비밀번호 검증 |
| session_create() | 세션 생성 |
| history_update() | 시청 기록 저장 |

---

# 4. 구현 기능 및 테스트 결과

## 4.1 테스트 결과

### 테스트 1: 로그인
- 방법: admin/admin123으로 로그인
- 결과: 성공, list.html로 이동

### 테스트 2: 회원가입
- 방법: 새 계정 생성 후 로그인
- 결과: 성공

### 테스트 3: 비디오 재생
- 방법: 비디오 선택 후 재생, 탐색
- 결과: 성공, Range 요청 정상 동작

### 테스트 4: 이어보기
- 방법: 중간까지 시청 후 종료, 재접속
- 결과: 성공, 마지막 위치에서 재생

### 테스트 5: 동시 접속
- 방법: 여러 브라우저에서 동시 접속
- 결과: 성공, 각 세션 독립 동작

---

# 5. 팀원 담당 업무 및 기여도

혼자서 진행 하였습니다!

**세부 내역**:
- 서버 아키텍처 설계
- 멀티스레드 구현
- HTTP 프로토콜 구현
- PostgreSQL 연동
- 프론트엔드 개발
- 테스트 및 디버깅

---

# 6. 소스코드 목록

| 파일 | 라인 | 설명 |
|------|------|------|
| src/main.c | 411 | 서버 메인 |
| src/http_handler.c | 598 | HTTP 처리 |
| src/db.c | 433 | DB 연동 |
| src/utils.c | 280 | 유틸리티 |
| src/ffmpeg_helper.c | 250 | FFmpeg |
| src/common.h | 158 | 헤더 |
| sql/schema.sql | 53 | DB 스키마 |
| **총계** | **~2,200** | |
