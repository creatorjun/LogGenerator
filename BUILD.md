<!-- BUILD.md -->
# LogGenerator 빌드

아래 명령은 저장소 루트에서 실행합니다. 각 명령은 해당 OS의 전용 빌드 디렉터리를 먼저 삭제하여 CMake 및 FetchContent 캐시를 사용하지 않고, 별도 확인 입력 없이 구성·Release 빌드·전체 테스트를 한 번에 수행합니다.

## Windows 10/11 x64

요구 사항: Visual Studio 2026의 `Desktop development with C++`, CMake 3.26 이상, Git이 PATH에 있어야 합니다. PowerShell에서 실행합니다.

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release -Mode Desktop -Clean
```

완료 후 GUI는 `build\bin\Release\LogGenerator.exe`, CLI는 `build\bin\Release\LogGeneratorCli.exe`입니다.

## Oracle Linux 8.10/9.8 x86_64/arm64

Linux의 기본 빌드 기준은 Oracle Linux 8.10과 9.8입니다. 먼저 실제 운영체제 버전을 확인합니다.

```bash
cat /etc/os-release
```

GUI 빌드에는 CMake 3.26 이상, Git, Make, OpenSSL 개발 패키지, OpenGL 및 X11 개발 패키지와 GCC Toolset 15가 필요합니다. root 권한으로 다음 명령을 실행합니다. 일반 사용자 셸에서는 `dnf` 앞에 `sudo`를 붙입니다.

```bash
dnf install -y \
  cmake git make pkgconf-pkg-config openssl-devel \
  gcc-toolset-15-gcc gcc-toolset-15-gcc-c++ \
  mesa-libGL-devel libX11-devel libXrandr-devel libXinerama-devel \
  libXcursor-devel libXi-devel
```

`scripts/build.sh`는 Oracle Linux 8 또는 9를 감지하면 `/opt/rh/gcc-toolset-15/root/usr/bin/gcc`와 `g++`를 자동 선택합니다. Oracle Linux의 기본 GCC를 사용했던 CMake 캐시가 남지 않도록 최초 빌드에는 `--clean`을 사용합니다.

```bash
bash scripts/build.sh Release --gui --clean
```

완료 후 GUI는 `build-linux/bin/LogGenerator`, CLI는 `build-linux/bin/LogGeneratorCli`입니다.

디스플레이 서버가 없는 Oracle Linux에서 CLI만 빌드하려면 GUI 패키지를 제외한 다음 의존성만 설치합니다.

```bash
dnf install -y \
  cmake git make pkgconf-pkg-config openssl-devel \
  gcc-toolset-15-gcc gcc-toolset-15-gcc-c++
```

헤드리스 빌드와 전체 테스트는 다음 명령으로 실행합니다.

```bash
bash scripts/build.sh Release --headless --clean
```

완료 후 CLI는 `build-linux-headless/bin/LogGenerator`입니다. 빌드 도구가 없는 실행 전용 Oracle Linux에는 `libstdc++`, `openssl-libs`, `zlib`, `ca-certificates`가 필요합니다.

`CC`와 `CXX`를 직접 지정해 자동 선택을 재정의할 수 있으며, 이 경우 두 값을 모두 지정합니다.

```bash
CC=/opt/rh/gcc-toolset-15/root/usr/bin/gcc \
CXX=/opt/rh/gcc-toolset-15/root/usr/bin/g++ \
bash scripts/build.sh Release --gui --clean
```

## macOS Apple Silicon(M1–M4)

요구 사항: Xcode Command Line Tools, Homebrew, CMake 3.26 이상, Git, OpenSSL 3가 필요합니다. 최초 한 번 `brew install cmake openssl@3`으로 준비할 수 있습니다. GLFW와 Dear ImGui는 CMake가 고정 버전으로 내려받습니다.

```bash
PATH="/opt/homebrew/bin:$PATH" bash scripts/build.sh Release --gui --clean
```

완료 후 arm64 GUI는 `build-macos/bin/LogGenerator`, CLI는 `build-macos/bin/LogGeneratorCli`입니다. GUI는 다음 명령으로 실행합니다.

```bash
./build-macos/bin/LogGenerator
```

macOS에서 CLI만 빌드하려면 다음 한 줄을 사용합니다.

```bash
PATH="/opt/homebrew/bin:$PATH" bash scripts/build.sh Release --headless --clean
```

## 캐시를 유지하는 반복 빌드

반복 개발 중 다운로드 및 컴파일 결과를 재사용하려면 마지막 `--clean`만 제거합니다.
