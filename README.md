<!-- README.md -->
# LogGenerator

Windows, Linux 및 macOS 64비트에서 실행되는 C++23 기반 SIEM 로그 생성·전송기입니다. Linux의 기본 빌드 기준은 Oracle Linux 8.10과 9.8입니다. 데스크톱에서는 Dear ImGui 반응형 UI를 사용하고, 디스플레이 서버가 없는 Linux에서는 GUI 의존성이 전혀 없는 CLI 모드로 UDP, TCP, TLS 또는 FILE 전송을 실행할 수 있습니다. macOS Apple Silicon에서는 Cocoa와 OpenGL을 사용하는 arm64 GUI 및 CLI를 빌드할 수 있습니다.

## 프로젝트 구조

```text
LogGenerator/
├─ CMakeLists.txt
├─ BUILD.md
├─ PERFORMANCE.md
├─ README.md
├─ requirements.txt
├─ docs/
│  ├─ MICROSOFT_STORE.md
│  └─ PRIVACY_POLICY.ko.md
├─ packaging/
│  ├─ windows/
│  │  ├─ AppxManifest.xml.in
│  │  └─ StoreConfig.example.psd1
│  └─ linux/
│     ├─ install-shortcuts.sh
│     └─ run-loggenerator.sh
├─ resources/
│  └─ windows/
│     ├─ log.ico
│     └─ log_generator.rc
├─ Sample Logs/
│  ├─ sample_logs.json
│  └─ security_device_scenarios.csv
├─ scripts/
│  ├─ build-cli-linux.sh
│  ├─ build-cli-windows.cmd
│  ├─ build.ps1
│  ├─ build.sh
│  ├─ package-linux.cmake
│  ├─ package-store.ps1
│  ├─ test-store-package.ps1
│  └─ publish.ps1
├─ src/
│  ├─ domain/
│  ├─ application/
│  │  ├─ models/
│  │  ├─ ports/
│  │  ├─ use_cases/
│  │  ├─ log_catalog_service.cpp
│  │  ├─ log_preparation_cache.cpp
│  │  ├─ log_renderer.cpp
│  │  ├─ privacy_anonymizer.cpp
│  │  ├─ sample_log_import_service.cpp
│  │  └─ stress_test_service.cpp
│  ├─ infrastructure/
│  │  ├─ async_file_logger.cpp
│  │  ├─ csv_sample_log_source.cpp
│  │  ├─ file_transport.cpp
│  │  ├─ json_log_catalog.cpp
│  │  ├─ openssl_transport.cpp
│  │  ├─ posix_execution_runtime.cpp
│  │  ├─ runtime_paths.cpp
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
   ├─ log_preparation_cache_tests.cpp
   ├─ log_renderer_tests.cpp
   ├─ linux_package_tests.cmake
   ├─ responsive_layout_tests.cpp
   ├─ sample_log_import_tests.cpp
   ├─ stress_test_service_tests.cpp
   └─ windows_icon_tests.cpp
```

## Microsoft Store 링크 배포

Windows x64 앱은 MSIX로 패키징할 수 있습니다. Partner Center에서 `Public audience` → `Make this product available but not discoverable in the Store` → `Direct link only`를 선택하면 Store 검색에서 숨기고 링크를 받은 누구나 설치하도록 제공할 수 있습니다. 링크 재전달은 제한하지 않습니다.

모든 MSIX 패키지는 샘플 로그 **0개**로 시작합니다. 패키지에는 빈 `sample_logs.json`만 넣으며 저장소의 샘플 60개와 CSV는 포함하지 않습니다. 저장소 원본은 보존됩니다. 사용자는 첫 실행 후 `추가` 또는 `CSV 가져오기`로 샘플을 준비합니다.

Partner Center 제품 `9NDZGLF6HDKR`의 Submission 1 초안에 샘플 0개 패키지를 업로드했고, 서버 검사 `Validated`와 저장 후 `패키지 상태: 완료`를 확인했습니다. Public/검색 숨김/Direct link only 설정도 저장했습니다. 가격과 개인정보처리방침·Store 목록을 완성하기 전이며, 인증 제출과 게시는 아직 수행하지 않았습니다. 초안 업로드용 패키지는 다음 명령으로 생성합니다.

```powershell
.\scripts\package-store.ps1 -ConfigPath .\packaging\windows\StoreConfig.local.psd1 -DraftUpload -RunWack
```

이 명령은 별도 `build-store`에서 AVX2 필수 조건을 해제한 Release 빌드와 테스트를 실행하고, `out/store/draft`에 실제 identity의 MSIX와 로컬 검사 보고서를 생성합니다. `-DraftUpload`는 개인정보처리방침 URL이 없는 Packages 초안 업로드를 허용합니다. 인증 제출 전에는 공개 HTTPS 방침 URL을 `packaging/windows/StoreConfig.local.psd1`의 `PrivacyPolicyUrl`에 기입하고 Store 목록을 완성한 뒤, `-DraftUpload` 없이 다시 패키징합니다. 이때 산출물은 `out/store/submission`에 생성되며, URL이 비어 있으면 실패합니다. 업로드·분석·초안 저장은 Store 심사 승인이나 게시 완료와 구분합니다.

실제 identity가 필요 없는 로컬 검증은 `.\scripts\package-store.ps1 -ValidationOnly -RunWack`로 실행합니다. `out/store/validation`의 개발 identity 패키지는 Partner Center에 업로드하지 않습니다.

Store 신규 설치에서는 빈 카탈로그를 사용자별 `LocalState`에 복제하고, 사용자가 추가한 샘플·진단 로그·기본 FILE 출력도 그곳에 저장합니다. 업데이트는 기존 사용자 카탈로그를 보존하므로 새 패키지의 샘플이 0개여도 사용자의 기존 샘플을 삭제하지 않습니다. 일반 실행 파일 배포의 저장 위치는 유지합니다.

전체 설정, 서명과 설치 검증, 심사 준비 및 링크 발급 절차는 [Microsoft Store 배포 안내](docs/MICROSOFT_STORE.md), 공개 전 운영자 정보를 채워야 하는 방침 초안은 [개인정보처리방침](docs/PRIVACY_POLICY.ko.md)을 확인하세요.

## 플랫폼 구성

공통 Domain과 Application 코드는 운영체제 API를 사용하지 않습니다. `src/main.cpp`와 `src/cli_main.cpp`가 각각 GUI와 CLI Composition Root로서 플랫폼별 Infrastructure와 Presentation 어댑터를 선택합니다.

| 구분 | Windows | Oracle Linux 8.10/9.8 | macOS Apple Silicon |
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

### Oracle Linux 8.10/9.8 GUI

Oracle Linux 8.10과 9.8의 AppStream 저장소를 기준으로 합니다. `/etc/os-release`에서 실제 버전을 확인한 뒤 root 권한으로 다음 패키지를 설치합니다. 일반 사용자 셸에서는 `dnf` 앞에 `sudo`를 붙입니다.

```bash
cat /etc/os-release
dnf install -y \
  cmake git make pkgconf-pkg-config openssl-devel \
  gcc-toolset-15-gcc gcc-toolset-15-gcc-c++ \
  mesa-libGL-devel libX11-devel libXrandr-devel libXinerama-devel \
  libXcursor-devel libXi-devel
```

Oracle Linux 8의 기본 GCC 8.5와 Oracle Linux 9의 기본 GCC 11은 이 프로젝트의 C++23 `<format>` 구현 기준을 충족하지 않으므로 GCC Toolset 15를 사용합니다. `scripts/build.sh`는 Oracle Linux 8 또는 9를 감지하면 `/opt/rh/gcc-toolset-15/root/usr/bin/gcc`와 `g++`를 자동 선택합니다. `CC`와 `CXX`를 직접 지정하는 경우에는 두 값을 모두 지정해야 합니다.

```bash
cmake --version
/opt/rh/gcc-toolset-15/root/usr/bin/g++ --version
```

Linux GUI 구성 단계는 Noto Sans KR Regular/Bold와 OFL 라이선스를 고정 커밋에서 내려받아 SHA-256으로 검증합니다. 실행 파일 옆 `fonts` 디렉터리에 함께 배치되며 Oracle Linux의 시스템 글꼴보다 먼저 로드되므로 별도 한글 글꼴 패키지가 없어도 UI 한글을 표시합니다.

### Oracle Linux 8.10/9.8 헤드리스

GUI가 없는 서버에서는 다음 의존성만 필요합니다. X11, OpenGL, GLFW 패키지는 설치하지 않습니다.

```bash
dnf install -y \
  cmake git make pkgconf-pkg-config openssl-devel \
  gcc-toolset-15-gcc gcc-toolset-15-gcc-c++
```

빌드 도구가 없는 실행 전용 서버에는 `libstdc++`, `openssl-libs`, `zlib`, `ca-certificates`만 설치하면 됩니다.

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

### Oracle Linux 8.10/9.8 GUI

```bash
bash scripts/build.sh Release --gui --clean
```

실행 파일은 `build-linux/bin/LogGenerator`와 `build-linux/bin/LogGeneratorCli`에 생성됩니다. 빌드가 끝나면 GUI, CLI, 샘플 로그, 한글 글꼴, OFL 라이선스, 아이콘과 바로가기 설치기를 포함한 단일 ZIP 파일이 자동 생성됩니다.

```text
build-linux/dist/LogGenerator-1.0.0-oracle-linux-x86_64.zip
```

aarch64 빌드에서는 파일명의 아키텍처가 `aarch64`로 생성됩니다. 같은 빌드 과정에서 현재 사용자의 `${XDG_DATA_HOME:-$HOME/.local/share}/loggenerator`에 앱을 설치하고 앱 서랍 항목을 생성합니다. XDG Desktop 디렉터리가 존재하면 `LogGenerator.desktop` 바로가기도 복사합니다. root로 빌드하면 `/root`에 등록되므로 실제 데스크톱 로그인 계정의 앱 서랍에 표시하려면 해당 계정으로 빌드합니다.

ZIP을 빌드 PC와 다른 Oracle Linux GUI 환경에서 실행할 때는 다음 시스템 런타임이 필요합니다. 애플리케이션 실행 파일, 샘플 로그, 한글 글꼴과 아이콘은 ZIP에 포함되지만 glibc, OpenSSL, X11과 OpenGL은 운영체제 패키지를 사용합니다.

```bash
dnf install -y \
  libstdc++ openssl-libs zlib ca-certificates \
  libglvnd-glx libglvnd-opengl mesa-dri-drivers \
  libX11 libXcursor libXi libXinerama libXrandr
```

```bash
./build-linux/bin/LogGenerator
```

스크립트는 구성, 병렬 빌드, 전체 CTest를 순서대로 실행합니다. 직접 실행하려면 다음 명령을 사용할 수 있습니다.

```bash
export CC=/opt/rh/gcc-toolset-15/root/usr/bin/gcc
export CXX=/opt/rh/gcc-toolset-15/root/usr/bin/g++
cmake --fresh -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DLOGGEN_BUILD_GUI=ON -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX"
cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
```

ZIP만 생성하고 현재 사용자에게 바로가기를 자동 설치하지 않으려면 다음과 같이 실행합니다.

```bash
LOGGEN_INSTALL_LINUX_SHORTCUTS=OFF bash scripts/build.sh Release --gui --clean
```

ZIP을 다른 PC에서 푼 뒤 해당 PC의 앱 서랍과 Desktop에 등록하려면 압축 해제 디렉터리에서 다음 명령을 실행합니다.

```bash
bash LogGenerator/install-shortcuts.sh
```

### Oracle Linux 8.10/9.8 헤드리스

```bash
bash scripts/build.sh Release --headless --clean
./build-linux-headless/bin/LogGenerator --help
```

Debug 빌드는 `bash scripts/build-cli-linux.sh Debug`으로 실행합니다. 헤드리스 빌드는 `build-linux-headless/bin/LogGenerator`를 CLI 전용 실행 파일로 만듭니다. `bash scripts/build-cli-linux.sh` 명령도 계속 사용할 수 있습니다. CMake를 직접 실행하려면 다음 명령을 사용합니다.

```bash
export CC=/opt/rh/gcc-toolset-15/root/usr/bin/gcc
export CXX=/opt/rh/gcc-toolset-15/root/usr/bin/g++
cmake --fresh -S . -B build-linux-headless -DCMAKE_BUILD_TYPE=Release -DLOGGEN_BUILD_GUI=OFF -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX"
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

일반 빌드에서는 모든 플랫폼의 실행 파일 옆에 `Sample Logs/sample_logs.json`이 자동 복사됩니다. Store MSIX를 생성할 때는 별도의 패키지 준비 폴더에 빈 카탈로그를 기록하여 원본 샘플과 CSV를 제외합니다. Linux GUI 빌드는 검증된 Noto Sans KR 글꼴도 `fonts` 디렉터리에 복사합니다.

## 사용 방법

1. UDP, TCP, TLS, FILE 중 전송 방식을 선택합니다.
2. UDP에서는 프로토콜과 대상 IP 사이의 `통합 전송 허용 여부`를 선택합니다. 기본값은 `불가`입니다.
3. 네트워크 방식에서는 대상 Host와 Port를 입력합니다.
4. TCP/TLS에서는 Newline 또는 RFC 6587 Octet Counting 프레이밍을 선택합니다.
5. TLS 인증서 이름이 대상 Host와 다르면 TLS 서버 이름을 입력합니다.
6. 샘플을 검색하거나 전체 순환·단일 샘플 생성을 선택합니다.
7. `src_ip`, `dst_ip`, 날짜 범위 또는 현재 시각 오프셋을 설정합니다.
8. 순차 전송 또는 병렬 전송을 선택하고 목표 EPS를 설정합니다. 목표 EPS가 0이면 최대 처리량 모드입니다.
9. `전송 시작`을 누르고 현재 EPS, 평균 EPS, 총 로그 수, 총 바이트를 확인합니다.

FILE 방식은 네트워크 객체를 생성하지 않으며 로그 이벤트 하나당 파일 하나를 기록합니다. 기본 위치는 일반 배포에서 실행 파일 옆 `generated`, Store 설치에서 사용자별 `LocalState\generated`입니다. 총 바이트, 파일 수, 실행 시간 제한을 0으로 두면 해당 제한은 비활성화됩니다.

UDP 통계는 로그 이벤트 EPS와 실제 데이터그램 DPS를 분리해 표시합니다. `통합 전송 허용 여부`의 기본값인 `불가`는 순차·병렬 모드와 관계없이 로그 1개를 데이터그램 1개로 전송합니다. `허용`은 로그를 개행으로 구분해 최대 60 KiB 데이터그램에 패킹하고 여러 데이터그램을 최대 600 KiB 작업 단위로 벡터 전송하므로 수신기가 데이터그램 내부의 개행 단위 이벤트 분리를 지원해야 합니다. 단일 UDP payload의 프로토콜 상한은 65,507바이트이므로 600 KiB를 하나의 데이터그램으로 만들지 않습니다. 로그 내부의 실제 CR/LF는 `\\r`, `\\n`으로 이스케이프됩니다. 60 KiB UDP는 일반 MTU에서 IP 단편화되므로 loopback, jumbo frame 또는 충분한 대역폭의 전용망을 권장합니다. 수신 포트가 없을 때 나중에 도착하는 ICMP Port Unreachable은 UDP 송신을 중단시키지 않으며, 권한 거부·라우팅 실패·버퍼 오류 등 실제 로컬 송신 오류는 계속 실패로 보고합니다.

순차 전송은 항상 Worker 1개를 사용합니다. 병렬 전송은 Worker 수를 직접 입력받지 않고 실행 시 OS, 프로토콜 및 CPU 수에 따라 자동 결정합니다. macOS UDP는 XNU 출력 경합 실측 결과에 따라 Worker 2개와 `sendmsg_x` 데이터그램 벡터 송신을 사용하고, Windows/Linux UDP는 CPU 수에 따라 2~8개를 사용합니다. Linux는 `sendmmsg`, TCP/TLS는 최대 16개의 자동 Worker를 사용합니다. FILE은 순차 전송으로 고정됩니다.

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
  --udp-integration allow `
  --mode parallel `
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
  --udp-integration allow \
  --mode parallel \
  --eps 1000 \
  --duration 60
```

샘플 ID는 `0001`처럼 숫자로만 지정합니다. 전체 샘플은 `--all`, 일부 샘플은 반복 가능한 `--sample-id`로 선택하며 두 옵션은 함께 사용할 수 없습니다. 실행 시간을 생략하거나 0으로 설정하면 `Ctrl+C` 또는 FILE 제한에 도달할 때까지 실행합니다. 전송 방식은 `--mode sequential|parallel`로 선택하며 병렬 모드의 Worker 수는 자동 결정됩니다. UDP 통합 전송은 `--udp-integration deny|allow`로 선택하며 기본값은 `deny`입니다. 병렬 Worker는 하나의 전역 Round-Robin 커서를 공유하므로 샘플 선택이 Worker별로 분리되지 않습니다. TCP/TLS는 `--framing newline|octet`, TLS는 `--tls-server-name`을 지원합니다. 전체 옵션은 `--help`에서 확인할 수 있습니다.

## 데이터와 로그

샘플 카탈로그는 `Sample Logs/sample_logs.json`에 저장됩니다. 일반 배포는 실행 파일 옆 경로를 사용하고, Store 설치는 `%LOCALAPPDATA%\Packages\<PackageFamilyName>\LocalState` 아래 경로를 사용합니다. Store 패키지의 초기 카탈로그에는 샘플이 없습니다. UI에서 카탈로그를 저장할 때 임시 파일을 만든 뒤 원자적으로 교체합니다.

UI의 기존 `추가` 버튼과 직접 입력 편집 방식은 그대로 사용할 수 있습니다. `CSV 가져오기`는 정확히 1개 컬럼인 `.csv` 파일을 읽고 각 행을 하나의 새 샘플 로그로 추가한 뒤 같은 JSON 카탈로그에 저장합니다. 첫 행의 `sample`, `sample_log`, `샘플로그`, `샘플 로그` 헤더는 선택 사항이며 헤더가 없으면 첫 행부터 샘플 로그로 처리합니다. 로그에 쉼표, 따옴표 또는 개행이 있으면 표준 CSV 따옴표 규칙을 사용해야 합니다. 가져온 로그도 직접 입력한 로그와 동일하게 개인정보·시간·IP 토큰화와 전송용 캐시 준비를 거칩니다.

`Sample Logs/security_device_scenarios.csv`는 저장소에 보존된 합성 데이터 기반 고객 시연용 파일이며 Store MSIX에는 포함하지 않습니다. 메일 분할 유출, 개인정보 포함 메일, DB 개인정보 조회, 개인정보 출력, PC DLP 반출의 5개 케이스와 34개 장비 이벤트를 포함합니다. 각 이벤트의 `case_id`, `correlation_id`, `seq`, `device_type`으로 동일 케이스의 시간순 흐름과 장비 간 연관관계를 확인할 수 있습니다.

프로그램 자체 로그는 일반 배포의 실행 파일 옆 `logs` 디렉터리, Store 설치의 사용자별 `LocalState\logs`에 기록됩니다. CLI 로그 파일의 기본 이름은 `LogGeneratorCli_yyyyMMdd.log`입니다.

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

샘플 편집 중에는 120ms 디바운스 뒤 전용 백그라운드 스레드가 샘플과 테스트 케이스 값을 자동 토큰화하고, 변경된 최신 버전만 UI에 반영합니다. 편집 창의 변환 미리보기와 `캐시 준비 완료` 상태를 확인한 뒤 저장할 수 있습니다. 원본 날짜는 `{{TIMESTAMP:포맷:원본값}}` 형태의 자체 설명 토큰으로 저장되므로 JSON을 다시 불러와도 날짜 포맷과 오프셋 기능이 유지됩니다.

토큰화와 동시에 불변 렌더 청사진을 미리 컴파일합니다. 원본·토큰화 샘플은 동일한 캐시 항목을 공유하며, 전송 또는 FILE 생성을 시작할 때는 정규식 분석을 다시 수행하지 않고 실행 시각 오프셋과 `src_ip`/`dst_ip`만 바인딩합니다. 각 worker는 이 청사진을 공유하고 worker별 렌더 캐시만 가지므로 병렬 실행에서도 컴파일 비용과 잠금 경합이 hot path에 들어가지 않습니다.

## 아키텍처 검증

의존성 방향은 `Presentation/Infrastructure → Application → Domain`입니다. GUI와 CLI Presentation은 Application의 입력 경계(`use_cases`)와 출력 포트만 참조하고, 구체 서비스와 Infrastructure 구현은 각 Composition Root에서 조립합니다. GUI Presentation도 독립 정적 라이브러리 타깃으로 분리되어 Infrastructure에 링크할 수 없습니다.

`LogGeneratorArchitecture` CTest는 금지된 역방향 include, Presentation의 구체 유스케이스 결합, Application 계층의 플랫폼·프레임워크 API 유입, 조립용 Infrastructure 헤더의 OS 타입 노출과 어댑터 타깃의 역방향 링크를 검사합니다.

네트워크 소켓, TLS 세션, 파일 핸들, 백그라운드 스레드, 창과 렌더링 컨텍스트는 RAII 수명으로 관리됩니다. Presentation은 Infrastructure를 직접 참조하지 않으며 Composition Root에서 필요한 포트를 주입받습니다. Windows 전용 소스는 Windows 빌드에만, OpenSSL·POSIX 소스는 Linux와 macOS 빌드에만 포함됩니다.
