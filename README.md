# SFTP TUI 클라이언트

`ncurses`와 `libssh2`를 사용하여 C 언어로 작성된 간단한 터미널 기반 SFTP 클라이언트입니다.

## 주요 기능
- 접속 정보(호스트, 사용자명, 비밀번호) 및 다운로드 경로를 `config/settings.conf`에 저장 및 자동 로드.
- 로컬 및 원격 패널로 구성된 듀얼 패널 TUI 인터페이스.
- 단일 키 입력을 통한 파일 업로드 및 다운로드.
- **업로드/다운로드 시 진행률(Progress Bar) 표시**.
- **Enter 키를 통한 로컬 및 원격 디렉토리 이동 기능**.
- 고정된 다운로드 디렉토리 지정 기능.
- 리눅스 및 macOS(크로스 플랫폼) 지원.

## 사전 요구 사항

### macOS (Homebrew 이용)
```bash
brew install libssh2 ncurses
```

### 리눅스 (Ubuntu/Debian 기준)
```bash
sudo apt-get update
sudo apt-get install -y libssh2-1-dev libncurses-dev
```

## 컴파일 및 테스트 방법

### 컴파일
```bash
make clean
make
```
컴파일 완료 시 실행 파일은 `bin/sftp-tui`에 생성됩니다.

### 단위 테스트 실행
프로그램의 핵심 기능(설정 로드, 로컬 파일 탐색 등)을 검증하기 위해 단위 테스트를 제공합니다:
```bash
make test
```

## 사용 방법

1. **접속 정보 설정**:
   프로그램을 처음 실행하거나, `config/settings.conf` 파일을 수동으로 생성하여 정보를 입력하세요:
   ```ini
   HOST=서버.IP.주소
   USER=사용자명
   PASSWORD=비밀번호
   DOWNLOAD_DIR=./downloads
   ```

2. **프로그램 실행**:
   ```bash
   ./bin/sftp-tui
   ```

3. **주요 단축키**:
   - `TAB`: 로컬 패널과 원격 패널 사이의 포커스 전환.
   - `방향키(위/아래)`: 파일 목록 탐색.
   - `ENTER`: 선택한 디렉토리로 이동.
   - `U`: 선택한 로컬 파일을 원격 서버로 업로드.
   - `D`: 선택한 원격 파일을 로컬의 지정된 경로로 다운로드.
   - `Q`: 프로그램 종료.

## 프로젝트 구조
- `src/`: 소스 코드 파일 (.c)
- `include/`: 헤더 파일 (.h)
- `tests/`: 단위 테스트 파일 (.c)
- `config/`: 설정 파일 저장 디렉토리
- `bin/`: 컴파일된 실행 파일 및 테스트 파일 출력 디렉토리
- `downloads/`: 기본 다운로드 디렉토리
