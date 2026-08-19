<!-- README.md -->
# LogGenerator

Windows, Linux 및 macOS 64비트에서 실행되는 C++23 기반 SIEM 로그 생성·전송기입니다. 데스크톱에서는 Dear ImGui 반응형 UI를 사용하고, 디스플레이 서버가 없는 Linux에서는 GUI 의존성이 전혀 없는 CLI 모드로 UDP, TCP, TLS 또는 FILE 전송을 실행할 수 있습니다. macOS Apple Silicon에서는 Cocoa와 OpenGL을 사용하는 arm64 GUI 및 CLI를 빌드할 수 있습니다.

## 프로젝트 구조

```text
LogGenerator/
├─ CMakeLists.txt
├─ BUILD.md
├─ PERFORMANCE.md
├─ README.md
├─ requirements.txt
├─ resources/
│  └─ windows/
│     ├─ log.ico
│     └─ log_generator.rc
├─ Sample Logs/
│  └─ sample_logs.json
├─ scripts/
│  ├─ build-cli-linux.sh
│  ├─ build-cli-windows.cmd
│  ├─ build.ps1
│  ├─ build.sh
│  └─ publish.ps1
├─ src/
│  ├─ domain/
│  ├─ application/
│  │  ├─ ports/
│  │  ├─ log_catalog_service.cpp
│  │  ├─ log_renderer.cpp
│  │  ├─ privacy_anonymizer.cpp
│  │  └─ stress_test_service.cpp
│  ├─ infrastructure/
│  │  ├─ async_file_logger.cpp
│  │  ├─ file_transport.cpp
│  │  ├─ json_log_catalog.cpp
│  │  ├─ openssl_transport.cpp
│  │  ├─ posix_execution_runtime.cpp
│  │  ├─ schannel_transport.cpp
│  │  ├─ socket_support.cpp
│  │  ├─ tcp_transport.cpp
│  │  ├─ transport_factory.cpp
│  │  ├─ udp_transport.cpp
│  │  └─ windows_execution_runtime.cpp
│  ├─ presentation/
│  │  ├─ app.cpp
│  │  ├─ app.hpp
│  │  ├─ cli_app.cpp
│  │  ├─ cli_app.hpp
│  │  ├─ d3d11_context.cpp
│  │  ├─ responsive_layout.hpp
│  │  ├─ ui_theme.cpp
│  │  └─ windows_icon.cpp
│  ├─ cli_main.cpp
│  └─ main.cpp
└─ tests/
   ├─ architecture_tests.cmake
   ├─ async_file_logger_tests.cpp
   ├─ cli_app_tests.cpp
   ├─ cli_smoke.cmake
   ├─ file_transport_tests.cpp
   ├─ json_log_catalog_tests.cpp
   ├─ log_catalog_service_tests.cpp
   ├─ log_renderer_tests.cpp
   ├─ responsive_layout_tests.cpp
   ├─ stress_test_service_tests.cpp
   └─ windows_icon_tests.cpp
```

## 플랫폼 구성

공통 Domain과 Application 코드는 운영체제 API를 사용하지 않습니다. `src/main.cpp`와 `src/cli_main.cpp`가 각각 GUI와 CLI Composition Root로서 플랫폼별 Infrastructure와 Presentation 어댑터를 선택합니다.

| 구분 | Windows | Linux | macOS Apple Silicon |
|---|---|---|---|
| 창·입력 | Win32 | GLFW 3.4 / X11 | GLFW 3.4 / Cocoa |
| 렌더링 | DirectX 11 | OpenGL 3.2 | Apple OpenGL 3.2 |
| 소켓 | Winsock2 | POSIX socket | POSIX socket |
| TLS | Schannel | OpenSSL | Homebrew OpenSSL 3 |
| 실행 환경 | Win32 고해상도 타이머 | POSIX 스레드 실행 환경 | POSIX 스레드 실행 환경 |
| CLI 전용 빌드 | `LogGenerator.exe` | `LogGenerator` | `LogGenerator` |

Linux UI는 X11 또는 Wayland 데스크톱의 XWayland에서 실행됩니다. `LOGGEN_BUILD_GUI=OFF`에서는 Dear ImGui, GLFW, OpenGL, X11을 구성하거나 링크하지 않습니다. TLS 인증서 검증은 기본으로 활성화되며 Windows는 운영체제 인증서 저장소, Linux와 macOS는 OpenSSL 시스템 CA 저장소를 사용합니다.

## 요구 환경

공통 요구사항은 64비트 환경, CMake 3.26 이상, C++23 컴파일러, Git입니다. Dear ImGui 1.92.9, GLFW 3.4, nlohmann/json 3.12.0은 CMake가 고정 태그에서 가져옵니다.

### Windows

- Windows 10 또는 11 64비트
- Visual Studio 2026 이상
- Desktop development with C++ 워크로드

### Linux GUI

Ubuntu 24.04 기준 의존성 설치 명령은 다음과 같습니다.

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake git pkg-config \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxi-dev libssl-dev fonts-noto-cjk
```

`fonts-noto-cjk`가 없으면 기본 글꼴로 실행되지만 한글 글리프가 표시되지 않을 수 있습니다.

### Linux 헤드리스

GUI가 없는 Ubuntu 24.04 서버에서는 다음 의존성만 필요합니다. X11, OpenGL, GLFW 패키지는 설치하지 않습니다.

```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config libssl-dev
```

Python 런타임은 사용하지 않으므로 `requirements.txt`에는 설치할 Python 패키지가 없습니다.

### macOS Apple Silicon

- M1, M2, M3 또는 M4 Mac
- Xcode Command Line Tools
- Homebrew
- CMake 3.26 이상과 OpenSSL 3

```bash
xcode-select --install
brew install cmake openssl@3
```

운영체제별 캐시 없는 원샷 빌드 명령은 [`BUILD.md`](BUILD.md)를 참고합니다.
성능 벤치마크 실행 방법과 M4 측정 결과는 [`PERFORMANCE.md`](PERFORMANCE.md)를 참고합니다.

## 빌드와 테스트

### Windows

GUI와 CLI를 함께 빌드합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
```

GUI 실행 파일은 `build\bin\Release\LogGenerator.exe`, CLI 실행 파일은 `build\bin\Release\LogGeneratorCli.exe`에 생성됩니다.

CLI만 간단하게 빌드하려면 다음 명령을 실행합니다.

```powershell
.\scripts\build-cli-windows.cmd
```

Debug 빌드는 `.\scripts\build-cli-windows.cmd Debug`으로 실행합니다. CLI 전용 실행 파일은 `build-windows-cli\bin\Release\LogGenerator.exe`에 생성됩니다.

### Linux GUI

```bash
bash scripts/build.sh Release --gui
```

실행 파일은 `build-linux/bin/LogGenerator`에 생성됩니다.

```bash
./build-linux/bin/LogGenerator
```

스크립트는 구성, 병렬 빌드, 전체 CTest를 순서대로 실행합니다. 직접 실행하려면 다음 명령을 사용할 수 있습니다.

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DLOGGEN_BUILD_GUI=ON
cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
```

### Linux 헤드리스

```bash
bash scripts/build-cli-linux.sh
./build-linux-headless/bin/LogGenerator --help
```

Debug 빌드는 `bash scripts/build-cli-linux.sh Debug`으로 실행합니다. 헤드리스 빌드는 `build-linux-headless/bin/LogGenerator`를 CLI 전용 실행 파일로 만듭니다. 기존 통합 스크립트의 `bash scripts/build.sh Release --headless` 명령도 계속 사용할 수 있습니다. CMake를 직접 실행하려면 다음 명령을 사용합니다.

```bash
cmake -S . -B build-linux-headless -DCMAKE_BUILD_TYPE=Release -DLOGGEN_BUILD_GUI=OFF
cmake --build build-linux-headless --parallel
ctest --test-dir build-linux-headless --output-on-failure
```

### macOS Apple Silicon GUI와 CLI

```bash
bash scripts/build.sh Release --gui
./build-macos/bin/LogGenerator
```

헤드리스 CLI만 빌드하려면 다음 명령을 사용합니다.

```bash
bash scripts/build.sh Release --headless
./build-macos-headless/bin/LogGenerator --help
```

모든 플랫폼에서 빌드 후 실행 파일 옆에 `Sample Logs/sample_logs.json`이 자동 복사됩니다.

## 사용 방법

1. UDP, TCP, TLS, FILE 중 전송 방식을 선택합니다.
2. 네트워크 방식에서는 대상 Host와 Port를 입력합니다.
3. TCP/TLS에서는 Newline 또는 RFC 6587 Octet Counting 프레이밍을 선택합니다.
4. TLS 인증서 이름이 대상 Host와 다르면 TLS 서버 이름을 입력합니다.
5. 샘플을 검색하거나 전체 순환·단일 샘플 생성을 선택합니다.
6. `src_ip`, `dst_ip`, 날짜 범위 또는 현재 시각 오프셋을 설정합니다.
7. Worker 수와 목표 EPS를 설정합니다. 목표 EPS가 0이면 최대 처리량 모드입니다.
8. `전송 시작`을 누르고 현재 EPS, 평균 EPS, 총 로그 수, 총 바이트를 확인합니다.

FILE 방식은 네트워크 객체를 생성하지 않으며 실행 파일 옆 `generated` 디렉터리에 로그 이벤트 하나당 파일 하나를 기록합니다. 총 바이트, 파일 수, 실행 시간 제한을 0으로 두면 해당 제한은 비활성화됩니다.

## CLI 사용 방법

### Windows CLI

도움말과 샘플 목록을 확인합니다.

```powershell
.\build-windows-cli\bin\Release\LogGenerator.exe --help
.\build-windows-cli\bin\Release\LogGenerator.exe list
```

FILE 로그 100개를 생성합니다.

```powershell
.\build-windows-cli\bin\Release\LogGenerator.exe run `
  --protocol file `
  --sample-id 0001 `
  --file-max-count 100 `
  --output-dir .\generated
```

전체 샘플을 전역 Round-Robin으로 한 번씩 저장합니다.

```powershell
.\build-windows-cli\bin\Release\LogGenerator.exe run `
  --all `
  --protocol file `
  --file-max-count 60 `
  --output-dir .\generated
```

60초 동안 UDP로 목표 1,000 EPS를 전송합니다.

```powershell
.\build-windows-cli\bin\Release\LogGenerator.exe run `
  --all `
  --protocol udp `
  --host 192.0.2.10 `
  --port 514 `
  --eps 1000 `
  --duration 60
```

GUI와 함께 빌드한 경우에는 같은 옵션으로 `build\bin\Release\LogGeneratorCli.exe`를 사용할 수 있습니다.

### Linux CLI

도움말과 샘플 목록을 확인합니다.

```bash
./build-linux-headless/bin/LogGenerator --help
./build-linux-headless/bin/LogGenerator list
```

FILE 로그 100개를 생성합니다.

```bash
./build-linux-headless/bin/LogGenerator run \
  --protocol file \
  --sample-id 0001 \
  --file-max-count 100 \
  --output-dir ./generated
```

전체 샘플을 전역 Round-Robin으로 한 번씩 저장합니다.

```bash
./build-linux-headless/bin/LogGenerator run \
  --all \
  --protocol file \
  --file-max-count 60 \
  --output-dir ./generated
```

60초 동안 UDP로 목표 1,000 EPS를 전송합니다.

```bash
./build-linux-headless/bin/LogGenerator run \
  --all \
  --protocol udp \
  --host 192.0.2.10 \
  --port 514 \
  --eps 1000 \
  --duration 60
```

샘플 ID는 `0001`처럼 숫자로만 지정합니다. 전체 샘플은 `--all`, 일부 샘플은 반복 가능한 `--sample-id`로 선택하며 두 옵션은 함께 사용할 수 없습니다. 실행 시간을 생략하거나 0으로 설정하면 `Ctrl+C` 또는 FILE 제한에 도달할 때까지 실행합니다. 여러 Worker는 하나의 전역 Round-Robin 커서를 공유하므로 샘플 선택이 Worker별로 분리되지 않습니다. TCP/TLS는 `--framing newline|octet`, TLS는 `--tls-server-name`을 지원합니다. 전체 옵션은 `--help`에서 확인할 수 있습니다.

## 데이터와 로그

샘플 카탈로그는 `Sample Logs/sample_logs.json`에 저장됩니다. UI에서 카탈로그를 저장할 때 임시 파일을 만든 뒤 원자적으로 교체합니다.

프로그램 자체 로그는 실행 파일 옆 `logs` 디렉터리에 기록됩니다. CLI 로그 파일의 기본 이름은 `LogGeneratorCli_yyyyMMdd.log`입니다.

```text
logs/LogGenerator_yyyyMMdd.log
logs/LogGeneratorCli_yyyyMMdd.log
```

비동기 로거는 최대 8,192건의 제한된 큐를 사용하며 오류와 종료 시 즉시 flush합니다. FILE 생성 결과는 다음 형식을 사용합니다.

```text
generated/yyyyMMdd_HHmmss_SSS.log
generated/yyyyMMdd_HHmmss_SSS_0002.log
```

## 개인정보 치환

카탈로그 저장 시 필드명과 보조 패턴을 기준으로 사람, 점포, 계정, 사번, 부서, 조직, 이메일, 전화번호, 주소, IP, MAC, 호스트, 식별자, 비밀값, 파일 경로를 토큰으로 치환합니다. 생성 시 50개의 사전 생성 합성 프로필 중 하나를 이벤트마다 선택해 관련 필드 간 일관성을 유지합니다.

`{{SRC_IP}}`와 `{{DST_IP}}`는 UI 또는 CLI 입력값으로 치환되며 일반 개인정보 IP와 구분됩니다. 이메일, 전화번호, 주민등록번호 형태, MAC 주소, Windows 사용자 경로는 필드명이 없어도 보조 패턴으로 처리됩니다.

## 아키텍처 검증

의존성 방향은 `Presentation/Infrastructure → Application → Domain`입니다. `LogGeneratorArchitecture` CTest가 금지된 역방향 include와 Application 계층의 플랫폼·프레임워크 API 유입을 검사합니다.

네트워크 소켓, TLS 세션, 파일 핸들, 백그라운드 스레드, 창과 렌더링 컨텍스트는 RAII 수명으로 관리됩니다. CLI Presentation은 Infrastructure를 직접 참조하지 않으며 Composition Root에서 필요한 포트를 주입받습니다. Windows 전용 소스는 Windows 빌드에만, OpenSSL·POSIX 소스는 Linux 빌드에만 포함됩니다.

## GitHub 게시

GitHub 원격 저장소와 권한이 구성된 Windows 환경에서는 검증 후 다음 스크립트로 현재 브랜치를 커밋하고 push할 수 있습니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\publish.ps1 -Message "feat: update log generator"
```
