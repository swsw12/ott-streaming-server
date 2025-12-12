# 🎬 OTT Video Streaming Server

C언어로 구현한 비디오 스트리밍 서버입니다.

## 📋 요구사항

- Windows 10/11
- Visual Studio 2022 (C 컴파일러)
- PostgreSQL 15+
- FFmpeg (선택사항 - 썸네일 생성용)

## 🚀 설치 및 실행

### 1. PostgreSQL 설치

[PostgreSQL 다운로드](https://www.postgresql.org/download/windows/)에서 설치 후:

```bash
# 1. PostgreSQL 접속
psql -U postgres

# 2. 사용자 및 데이터베이스 생성 (psql 안에서 실행)
CREATE USER ott WITH PASSWORD 'ott123';
CREATE DATABASE ott_streaming OWNER ott;
\q

# 3. 스키마 적용
psql -U ott -d ott_streaming -f sql/schema.sql
# (비밀번호 입력: ott123)
```

**또는 Docker 사용 (더 간단):**
```bash
docker-compose up -d
```

### 2. 서버 빌드

```bash
# Visual Studio Developer Command Prompt에서 실행
build_vs.bat
```

### 3. 서버 실행

```bash
ott_server.exe
```

브라우저에서 http://localhost:8080 접속

## 🔐 테스트 계정

| 아이디 | 비밀번호 |
|--------|----------|
| admin | admin123 |
| user1 | password |
| test | test |

## 📁 프로젝트 구조

```
├── src/           # C 소스코드
├── sql/           # 데이터베이스 스키마
├── static/        # HTML, CSS, JS
├── videos/        # 비디오 파일 (MP4)
└── build_vs.bat   # 빌드 스크립트
```

## ⚙️ 데이터베이스 설정

`sql/schema.sql` 파일에 테이블 구조와 초기 데이터가 포함되어 있습니다.

PostgreSQL 연결 정보 (기본값):
- Host: 127.0.0.1
- Port: 5432
- Database: ott_streaming
- User: ott
- Password: ott123

## 📝 기능

- ✅ 사용자 로그인/회원가입
- ✅ 비디오 목록 조회
- ✅ 비디오 스트리밍 (Range 요청 지원)
- ✅ 시청 기록 저장
- ✅ 썸네일 자동 생성 (FFmpeg)
