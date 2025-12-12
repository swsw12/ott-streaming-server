# OTT Video Streaming Server

C언어로 구현한 HTTP 비디오 스트리밍 서버

## 요구사항

- Windows 10/11
- Visual Studio 2022
- PostgreSQL 15

## 설치

### 1. PostgreSQL 데이터베이스 설정

PostgreSQL 설치 후 SQL Shell(psql) 실행:

```sql
CREATE USER ott WITH PASSWORD 'ott123';
CREATE DATABASE ott_streaming OWNER ott;
\q
```

스키마 적용 (cmd에서):
```
"C:\Program Files\PostgreSQL\15\bin\psql" -U ott -d ott_streaming -f sql/schema.sql
```
비밀번호: ott123

### 2. 빌드

`build_vs.bat` 실행 (더블클릭 또는 Developer Command Prompt에서)

### 3. 실행

```
ott_server.exe
```

브라우저에서 http://localhost:8080 접속

## 테스트 계정

| ID | PW |
|----|-----|
| admin | admin123 |
| user1 | password |
| test | test |

## 프로젝트 구조

```
src/          C 소스코드
sql/          DB 스키마
static/       프론트엔드 (HTML/CSS/JS)
videos/       MP4 파일 저장 폴더
```

## 기능

- 로그인/회원가입
- 비디오 스트리밍 (Range 요청 지원)
- 시청 기록 저장 (이어보기)
- 썸네일 자동 생성 (FFmpeg 필요)

## DB 연결 정보

```
Host: 127.0.0.1
Port: 5432
Database: ott_streaming
User: ott
Password: ott123
```
