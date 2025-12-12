# 🎬 OTT Video Streaming Server

C언어로 만든 비디오 스트리밍 서버입니다.

---

# 📥 설치 가이드 (처음부터 끝까지)

## 🔧 1단계: 필요한 프로그램 설치하기

### 1-1. Visual Studio 2022 설치

1. https://visualstudio.microsoft.com/ko/downloads/ 접속
2. **"Community 2022"** 클릭해서 다운로드
3. 다운로드된 파일 실행
4. 설치 화면에서 **"C++를 사용한 데스크톱 개발"** 체크 ✅
5. **"설치"** 버튼 클릭
6. 설치 완료까지 기다리기 (30분~1시간 소요)

### 1-2. PostgreSQL 설치

1. https://www.postgresql.org/download/windows/ 접속
2. **"Download the installer"** 클릭
3. **Windows x86-64** 최신 버전 다운로드
4. 다운로드된 파일 실행
5. **Next** 계속 클릭
6. 비밀번호 설정 화면에서: **postgres** 입력 (기억해두기!)
7. 포트 번호: **5432** (기본값 그대로)
8. **Next** → **Install** 클릭
9. 설치 완료!

---

## 📦 2단계: GitHub에서 프로젝트 다운로드

1. https://github.com/swsw12/ott-streaming-server 접속
2. 초록색 **"Code"** 버튼 클릭
3. **"Download ZIP"** 클릭
4. 다운로드 완료되면 ZIP 파일을 **바탕화면**에 압축 풀기
5. 폴더 이름이 `ott-streaming-server-main`이면 성공!

---

## 📂 3단계: PostgreSQL 라이브러리 파일 복사하기

**이 단계가 중요합니다! 빠뜨리면 빌드가 안 됩니다.**

### 3-1. 파일 탐색기 두 개 열기

- 창 1: `C:\Program Files\PostgreSQL\15\` (PostgreSQL 설치 폴더)
- 창 2: 바탕화면의 `ott-streaming-server-main` 폴더

### 3-2. 다음 파일들을 복사하기

**lib 폴더에서 (1개):**
```
C:\Program Files\PostgreSQL\15\lib\libpq.lib
```
↓ 복사해서 ↓
```
바탕화면\ott-streaming-server-main\libpq.lib
```

**bin 폴더에서 (6개):**
```
C:\Program Files\PostgreSQL\15\bin\libpq.dll
C:\Program Files\PostgreSQL\15\bin\libssl-3-x64.dll
C:\Program Files\PostgreSQL\15\bin\libcrypto-3-x64.dll
C:\Program Files\PostgreSQL\15\bin\libiconv-2.dll
C:\Program Files\PostgreSQL\15\bin\libintl-9.dll
C:\Program Files\PostgreSQL\15\bin\libwinpthread-1.dll
```
↓ 전부 복사해서 ↓
```
바탕화면\ott-streaming-server-main\ (프로젝트 폴더)
```

### 3-3. 확인하기

프로젝트 폴더에 다음 파일들이 있으면 성공:
- ✅ libpq.lib
- ✅ libpq.dll
- ✅ libssl-3-x64.dll
- ✅ libcrypto-3-x64.dll
- ✅ libiconv-2.dll
- ✅ libintl-9.dll
- ✅ libwinpthread-1.dll

---

## 🗄️ 4단계: 데이터베이스 만들기

### 4-1. SQL Shell 열기

1. 윈도우 시작 메뉴 클릭
2. **"SQL Shell"** 또는 **"psql"** 검색
3. 클릭해서 실행 (검은 창이 뜹니다)

### 4-2. 접속하기

검은 창에 다음이 나타납니다:
```
Server [localhost]: 
```
**그냥 Enter 누르기**

```
Database [postgres]: 
```
**그냥 Enter 누르기**

```
Port [5432]: 
```
**그냥 Enter 누르기**

```
Username [postgres]: 
```
**그냥 Enter 누르기**

```
Password for user postgres: 
```
**PostgreSQL 설치할 때 정한 비밀번호 입력** (화면에 안 보여도 정상)

```
postgres=#
```
이렇게 나오면 접속 성공!

### 4-3. 사용자와 데이터베이스 만들기

다음 명령어를 **한 줄씩** 입력하고 Enter:

```sql
CREATE USER ott WITH PASSWORD 'ott123';
```
(Enter)

```sql
CREATE DATABASE ott_streaming OWNER ott;
```
(Enter)

```sql
\q
```
(Enter - 이건 나가기 명령어)

### 4-4. 테이블 만들기

1. 윈도우 **시작 메뉴** 클릭
2. **"cmd"** 검색해서 **명령 프롬프트** 실행
3. 다음 명령어 입력:

```
cd 바탕화면\ott-streaming-server-main
```
(Enter)

```
"C:\Program Files\PostgreSQL\15\bin\psql" -U ott -d ott_streaming -f sql\schema.sql
```
(Enter)

4. 비밀번호 물어보면: **ott123** 입력

---

## 🔨 5단계: 서버 빌드하기 (exe 파일 만들기)

### 5-1. Developer Command Prompt 열기

1. 윈도우 시작 메뉴 클릭
2. **"Developer Command Prompt for VS 2022"** 검색
3. 클릭해서 실행

### 5-2. 프로젝트 폴더로 이동

```
cd 바탕화면\ott-streaming-server-main
```
(Enter)

### 5-3. 빌드 실행

```
build_vs.bat
```
(Enter)

### 5-4. 확인

화면에 **"Build Success!"** 가 나오면 성공!

폴더에 **ott_server.exe** 파일이 생겼는지 확인하세요.

---

## 🎬 6단계: 비디오 파일 넣기

1. 프로젝트 폴더 안에 **videos** 폴더 열기
2. 원하는 **MP4 파일**을 여기에 복사
3. (테스트용으로 아무 MP4 파일이나 넣으면 됩니다)

---

## ▶️ 7단계: 서버 실행하기

### 7-1. 서버 시작

명령 프롬프트에서:
```
ott_server.exe
```
(Enter)

### 7-2. 확인

다음과 비슷한 메시지가 나오면 성공:
```
[INFO] OTT Video Streaming Server v1.0
[INFO] Connected to PostgreSQL database successfully
[INFO] Server listening on port 8080
```

---

## 🌐 8단계: 웹사이트 접속하기

1. 인터넷 브라우저 열기 (크롬, 엣지 등)
2. 주소창에 입력: **http://localhost:8080**
3. Enter

### 로그인 테스트

| 아이디 | 비밀번호 |
|--------|----------|
| admin | admin123 |
| user1 | password |
| test | test |

---

## ❌ 문제가 생겼을 때

### "libpq.dll을 찾을 수 없습니다" 오류
→ 3단계에서 DLL 파일들을 프로젝트 폴더에 복사했는지 확인

### "connection refused" 또는 DB 연결 실패
→ 4단계에서 PostgreSQL 사용자와 데이터베이스를 만들었는지 확인

### "cl is not recognized" 오류
→ 일반 cmd가 아니라 **Developer Command Prompt for VS 2022**에서 실행해야 함

### 빌드는 됐는데 서버 실행 시 오류
→ PostgreSQL이 실행 중인지 확인 (서비스에서 postgresql 검색)

---

## 📝 기능

- ✅ 사용자 로그인/회원가입
- ✅ 비디오 목록 조회
- ✅ 비디오 스트리밍 (이어보기 지원)
- ✅ 시청 기록 저장

---

## 🔐 기본 계정

| 아이디 | 비밀번호 |
|--------|----------|
| admin | admin123 |
| user1 | password |
| test | test |
