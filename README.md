<!-- README.md -->
# LogGenerator

Windows 64비트 SIEM 스트레스 테스트를 위한 C++23 기반 고성능 로그 생성·전송기입니다. Dear ImGui Win32/DirectX 11 UI에서 UDP, TCP, TLS 전송을 선택하고 `Sample Logs` 폴더의 Excel 샘플을 실시간으로 변환해 전송합니다.

## 프로젝트 구조

```text
LogGenerator/
├─ CMakeLists.txt
├─ README.md
├─ requirements.txt
├─ Sample Logs/
│  └─ Sample_Log_20260626.xlsx
├─ scripts/
│  ├─ build.ps1
│  └─ publish.ps1
├─ src/
│  ├─ domain/
│  │  ├─ generator_config.hpp
│  │  ├─ log_template.hpp
│  │  ├─ protocol.hpp
│  │  └─ transmission_stats.hpp
│  ├─ application/
│  │  ├─ ports/
│  │  │  ├─ log_catalog.hpp
│  │  │  └─ log_transport.hpp
│  │  ├─ log_renderer.cpp
│  │  ├─ log_renderer.hpp
│  │  ├─ stress_test_service.cpp
│  │  └─ stress_test_service.hpp
│  ├─ infrastructure/
│  │  ├─ excel_log_catalog.cpp
│  │  ├─ excel_log_catalog.hpp
│  │  ├─ schannel_transport.cpp
│  │  ├─ schannel_transport.hpp
│  │  ├─ tcp_transport.cpp
│  │  ├─ tcp_transport.hpp
│  │  ├─ transport_factory.cpp
│  │  ├─ transport_factory.hpp
│  │  ├─ udp_transport.cpp
│  │  ├─ udp_transport.hpp
│  │  ├─ winsock_support.cpp
│  │  └─ winsock_support.hpp
│  ├─ presentation/
│  │  ├─ app.cpp
│  │  ├─ app.hpp
│  │  ├─ d3d11_context.cpp
│  │  ├─ d3d11_context.hpp
│  │  ├─ ui_theme.cpp
│  │  └─ ui_theme.hpp
│  └─ main.cpp
└─ tests/
   ├─ excel_log_catalog_tests.cpp
   ├─ log_renderer_tests.cpp
   ├─ test_main.cpp
   └─ test_support.hpp
```

## 요구 환경

- Windows 10/11 64비트
- Visual Studio 2026 이상, Desktop development with C++ 워크로드
- CMake 3.28 이상
- Git

Dear ImGui 1.92.9와 OpenXLSX 0.5.1은 CMake가 구성 시 고정된 태그에서 자동으로 가져옵니다. TLS는 외부 런타임 없이 Windows Schannel을 사용하며 운영체제의 TLS 정책과 인증서 저장소를 따릅니다.

Python 런타임 의존성은 없으므로 `requirements.txt`에는 패키지가 없습니다.

## 빌드와 테스트

```powershell
.\scripts\build.ps1 -Configuration Release
```

실행 파일은 `build\bin\Release\LogGenerator.exe`에 생성되고 `Sample Logs` 폴더가 같은 위치로 복사됩니다.

## 사용법

1. 프로토콜과 대상 IP 또는 Host, Port를 입력합니다.
2. TCP/TLS에서는 Newline 또는 RFC 6587 Octet Counting 프레이밍을 선택합니다.
3. TLS 인증서 이름이 대상 IP와 다르면 `TLS 서버 이름`을 입력합니다.
4. Excel 로그를 검색하고 단일 로그 또는 검색 결과 전체 순환을 선택합니다.
5. `src_ip`, `dst_ip`, 현재 시각 기준 `+/- 일·시간·분` 오프셋을 지정합니다.
6. Worker 수와 목표 EPS를 설정합니다. 목표 EPS가 0이면 제한 없는 최대 성능 모드입니다.
7. `전송 시작`을 누르고 현재 EPS, 평균 EPS, 총 로그 수, 총 바이트를 확인합니다.

UDP 전송 통계는 수신 장비의 처리 성공이 아니라 로컬 Winsock `send` 완료를 기준으로 집계합니다. TCP/TLS는 지속 연결과 배치 전송을 사용합니다.

## Excel 형식

프로그램은 `Sample Logs` 폴더의 모든 `.xlsx` 파일을 읽고 시트 이름과 무관하게 다음 헤더를 탐지합니다.

- `Log Parser Name`
- `Sample Log`

헤더는 첫 20개 행에서 검색합니다. Excel 파일을 변경한 뒤 UI의 `새로고침` 버튼으로 다시 불러올 수 있습니다.

## 성능 설계

- Worker별 독립 소켓과 연결로 전송 경로의 전역 잠금을 제거합니다.
- UDP는 connected socket과 4 MiB 송신 버퍼를 사용합니다.
- TCP/TLS는 최대 256 KiB 단위로 여러 로그를 배치합니다.
- 로그 IP 및 타임스탬프 위치는 시작 시 한 번만 컴파일합니다.
- 생성된 타임스탬프 문자열은 Worker별로 초 단위 캐시합니다.
- 통계는 Worker 로컬 누적 후 완화된 원자 연산으로 일괄 반영합니다.
- Release 빌드는 최적화, LTCG, AVX2를 활성화합니다.

## GitHub 게시

검증 후 커밋과 현재 브랜치 push를 연속 수행하려면 다음 스크립트를 사용합니다.

```powershell
.\scripts\publish.ps1 -Message "feat: implement high-performance log generator"
```
