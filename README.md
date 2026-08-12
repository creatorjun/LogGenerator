<!-- README.md -->
# LogGenerator

Windows 64비트 SIEM 스트레스 테스트를 위한 C++23 기반 고성능 로그 생성·전송기입니다. Dear ImGui Win32/DirectX 11 반응형 UI에서 UDP, TCP, TLS 전송을 선택하고 `Sample Logs` 폴더의 Excel 샘플을 실시간으로 변환해 전송합니다.

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
│  │  ├─ log_level.hpp
│  │  ├─ log_template.hpp
│  │  ├─ protocol.hpp
│  │  └─ transmission_stats.hpp
│  ├─ application/
│  │  ├─ ports/
│  │  │  ├─ log_catalog.hpp
│  │  │  ├─ logger.hpp
│  │  │  └─ log_transport.hpp
│  │  ├─ log_renderer.cpp
│  │  ├─ log_renderer.hpp
│  │  ├─ stress_test_service.cpp
│  │  └─ stress_test_service.hpp
│  ├─ infrastructure/
│  │  ├─ async_file_logger.cpp
│  │  ├─ async_file_logger.hpp
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
│  │  ├─ responsive_layout.hpp
│  │  ├─ ui_theme.cpp
│  │  └─ ui_theme.hpp
│  └─ main.cpp
└─ tests/
   ├─ async_file_logger_tests.cpp
   ├─ excel_log_catalog_tests.cpp
   ├─ log_renderer_tests.cpp
   ├─ responsive_layout_tests.cpp
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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
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

## 반응형 UI

창 너비에 따라 통계는 1열, 2열, 4열로 전환되고 전송 설정과 로그 설정은 1열 또는 2열로 재배치됩니다. 타임 오프셋 입력도 작은 창에서는 2열로 자동 줄바꿈됩니다. 최소 창 크기는 520×480 논리 픽셀이며 세로 공간이 부족하면 전체 화면을 스크롤할 수 있습니다.

모니터 DPI가 바뀌면 글꼴, 여백, 컨트롤 크기와 최소 창 크기가 함께 조정됩니다. 서로 다른 배율의 모니터 사이로 창을 이동해도 현재 모니터 배율을 즉시 반영합니다.

## 애플리케이션 로그

프로그램 자체의 실행 로그는 실행 파일 옆 `logs` 폴더에 UTF-8 텍스트로 저장됩니다.

```text
logs/LogGenerator_yyyyMMdd.log
```

파일 기록은 최대 8,192건의 제한된 큐와 전용 백그라운드 스레드를 사용합니다. 큐가 가득 차더라도 로그 전송 Worker를 대기시키지 않으며, 누락된 애플리케이션 로그 수는 다음 기록 시 경고로 남깁니다. 로그 파일은 1초 이내 주기로 flush되고 오류 및 정상 종료 시 즉시 flush됩니다.

기록 대상은 프로그램 시작·종료, UI 초기화, Excel 카탈로그 로딩, 스트레스 테스트 설정과 시작·중지, Worker 연결 완료 및 전송 오류입니다. 개별 보안 로그 전송 성공은 최대 EPS를 훼손하지 않도록 애플리케이션 로그에 기록하지 않습니다.

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
