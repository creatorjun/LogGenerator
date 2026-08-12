<!-- README.md -->
# LogGenerator

Windows 64비트 SIEM 스트레스 테스트를 위한 C++23 기반 고성능 로그 생성·전송기입니다. Dear ImGui Win32/DirectX 11 반응형 UI에서 UDP, TCP, TLS, FILE 전송을 선택하고 UTF-8 JSON 샘플 카탈로그를 직접 추가·수정·삭제할 수 있습니다.

## 프로젝트 구조

```text
LogGenerator/
├─ CMakeLists.txt
├─ README.md
├─ requirements.txt
├─ Sample Logs/
│  └─ sample_logs.json
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
│  │  ├─ file_transport.cpp
│  │  ├─ file_transport.hpp
│  │  ├─ json_log_catalog.cpp
│  │  ├─ json_log_catalog.hpp
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
   ├─ file_transport_tests.cpp
   ├─ json_log_catalog_tests.cpp
   ├─ log_renderer_tests.cpp
   ├─ responsive_layout_tests.cpp
   ├─ stress_test_service_tests.cpp
   ├─ test_main.cpp
   └─ test_support.hpp
```

## 요구 환경

- Windows 10/11 64비트
- Visual Studio 2026 이상, Desktop development with C++ 워크로드
- CMake 3.28 이상
- Git

Dear ImGui 1.92.9와 nlohmann/json 3.12.0은 CMake 구성 시 고정 태그에서 자동으로 가져옵니다. TLS는 외부 런타임 없이 Windows Schannel을 사용하며 운영체제의 TLS 정책과 인증서 저장소를 따릅니다.

Python 런타임 의존성은 없으므로 `requirements.txt`에는 설치할 패키지가 없습니다.

## 빌드와 테스트

PowerShell 실행 정책과 무관하게 빌드하려면 다음 명령을 사용합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
```

현재 PowerShell 프로세스에서만 스크립트 실행을 허용한 뒤 실행할 수도 있습니다.

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\scripts\build.ps1 -Configuration Release
```

스크립트는 x64 구성, Release 빌드, CTest를 순서대로 실행합니다. 실행 파일은 `build\bin\Release\LogGenerator.exe`에 생성되고 `Sample Logs\sample_logs.json`이 같은 실행 디렉터리로 복사됩니다.

## 사용법

1. UDP, TCP, TLS, FILE 중 전송 방식을 선택합니다. 네트워크 방식은 대상 IP 또는 Host, Port를 입력합니다.
2. TCP/TLS에서는 Newline 또는 RFC 6587 Octet Counting 프레이밍을 선택합니다.
3. TLS 인증서 이름이 대상 IP와 다르면 `TLS 서버 이름`을 입력합니다.
4. 샘플 로그를 검색하고 단일 로그 또는 검색 결과 전체 순환을 선택합니다.
5. `추가`, `수정`, `삭제`로 샘플을 편집합니다. 저장 결과는 JSON 카탈로그에 즉시 반영됩니다.
6. `src_ip`, `dst_ip`를 지정하고 `현재 시간 + 오프셋` 또는 `기간 지정` 날짜 생성 방식을 선택합니다.
7. Worker 수와 목표 EPS를 설정합니다. 목표 EPS가 0이면 제한 없는 최대 성능 모드입니다.
8. `전송 시작`을 누르고 현재 EPS, 평균 EPS, 총 로그 수, 총 바이트를 확인합니다.

UDP 통계는 수신 장비의 처리 성공이 아니라 로컬 Winsock `send` 완료를 기준으로 집계합니다. TCP/TLS는 지속 연결과 최대 256 KiB 배치 전송을 사용합니다. FILE은 실행 파일 옆 `generated` 폴더에 기록합니다. `전송 중지`는 UI 스레드에서 연결 종료를 기다리지 않고 즉시 중지 요청만 전달합니다.

## FILE 전송

FILE을 선택하고 전송을 시작하면 다음 폴더가 자동으로 생성됩니다.

```text
실행파일 경로/generated/
```

첫 파일은 로컬 시작 시각을 사용한 `yyyyMMdd_HHmmss_SSS.log` 형식입니다. 같은 실행에서 파일이 회전되면 `_0002`, `_0003` 순번이 추가됩니다.

```text
generated/20260812_173245_123.log
generated/20260812_173245_123_0002.log
```

각 파일은 배치 경계를 유지하면서 약 1 MiB 단위로 회전합니다. 로그 이벤트 중간 절단을 피하기 때문에 파일 크기는 1 MiB보다 조금 작을 수 있습니다. FILE은 디스크 순차 처리량과 생성 순서를 유지하기 위해 Worker를 단일 writer로 고정하며 목표 EPS 설정은 동일하게 적용됩니다.

## JSON 샘플 카탈로그

샘플은 `Sample Logs\sample_logs.json`에 UTF-8 JSON으로 저장됩니다. 앱은 임시 파일 기록 후 원자적 교체를 사용해 저장 중 원본 손상 가능성을 줄입니다.

```json
{
  "schema_version": 1,
  "logs": [
    {
      "id": "sample-0001",
      "name": "장비와 파서 이름",
      "sample": "timestamp=2025-07-05T13:53:53Z src_ip=192.0.2.1 dst_ip=192.0.2.2",
      "source": "기본 내장 샘플"
    }
  ]
}
```

`id`, `name`, `sample`은 비어 있으면 안 되며 `id`는 전체 카탈로그에서 고유해야 합니다. UI에서 새 로그를 만들면 충돌하지 않는 `user-...` 식별자를 자동 생성합니다. JSON을 외부 코드 편집기로 직접 바꾼 뒤에는 UI의 `새로고침`을 누르면 됩니다.

## 자동 날짜와 IP 파싱

편집창은 로그 본문을 정규식으로 분석해 인식한 시간 토큰, `src_ip`, `dst_ip` 개수와 날짜 포맷을 보여줍니다. 전송 시작 시 같은 파서를 한 번 컴파일해 지정된 IP와 날짜 생성 설정을 적용합니다.

날짜 생성 방식은 다음 두 가지입니다.

- `현재 시간 + 오프셋`: 현재 시각에 `+/- 일·시간·분`을 적용합니다.
- `기간 지정`: `yyyy-MM-dd` 시작일 00:00:00부터 종료일 23:59:59까지 이벤트마다 1초씩 진행하고 범위 끝에서 시작일로 순환합니다. 원본 로그의 타임존 접미사는 유지하되 사용자가 입력한 달력 날짜가 다른 타임존으로 이동하지 않도록 처리합니다.

지원 시간 형식은 다음과 같습니다.

- ISO 8601 및 Year First: `yyyy-MM-dd HH:mm:ss`, `yyyy/MM/ddTHH:mm:ss`, 소수초, `Z`, `+09:00`
- Syslog: `MMM dd HH:mm:ss`, `MMM dd HH:mm:ss yyyy`
- Month First: `MMM dd yyyy HH:mm:ss`, 선택적 `GMT` 또는 `UTC`
- Apache: `dd/MMM/yyyy:HH:mm:ss +0900`
- Compact: `yyyyMMddHHmmss`
- 분리 필드: `date=yyyy-MM-dd` 또는 `date=yyyy/MM/dd`, `time=HH:mm:ss[.fraction]`

소스 IP는 `src`, `srcip`, `src_ip`, `srp_ip`, `src-ip`, `srcaddr`, `source_ip`, `source-address`, `clientip`, `clientipaddr`, `sip` 계열을 인식합니다. 목적지 IP는 `dst`, `dstip`, `dst_ip`, `dest_ip`, `dstn_ip`, `dstaddr`, `destination_ip`, `destination-address`, `server_ip`, `dip` 계열을 인식합니다. `IP:Port -> IP:Port` 형식도 소스와 목적지로 자동 인식합니다.

## 반응형 UI

창 너비에 따라 통계는 1열, 2열, 4열로 전환되고 전송 설정과 로그 설정은 1열 또는 2열로 재배치됩니다. 타임 오프셋 입력도 작은 창에서는 2열로 자동 줄바꿈됩니다. 최소 창 크기는 520×480 논리 픽셀이며 세로 공간이 부족하면 전체 화면을 스크롤할 수 있습니다.

모니터 DPI가 바뀌면 글꼴, 여백, 컨트롤 크기와 최소 창 크기가 함께 조정됩니다. 서로 다른 배율의 모니터 사이로 창을 이동해도 현재 모니터 배율을 즉시 반영합니다.

## 애플리케이션 로그

프로그램 자체의 실행 로그는 실행 파일 옆 `logs` 폴더에 UTF-8 텍스트로 저장됩니다.

```text
logs/LogGenerator_yyyyMMdd.log
```

파일 기록은 최대 8,192건의 제한된 큐와 전용 백그라운드 스레드를 사용합니다. 큐가 가득 차더라도 전송 Worker를 대기시키지 않으며, 누락된 애플리케이션 로그 수는 다음 기록 시 경고로 남깁니다. 로그 파일은 1초 이내 주기로 flush되고 오류 및 정상 종료 시 즉시 flush됩니다.

기록 대상은 프로그램 시작·종료, UI 초기화, JSON 카탈로그 로드·저장, 스트레스 테스트 설정과 시작·중지, Worker 연결 완료 및 전송 오류입니다. 개별 보안 로그 전송 성공은 최대 EPS를 훼손하지 않도록 애플리케이션 로그에 기록하지 않습니다.

## 성능 설계

- JSON 로드·저장·정규식 사전 분석은 UI 외부 백그라운드 스레드에서 실행합니다.
- 검색용 소문자 이름, 미리보기, 분석 결과와 통계 표시 문자열을 캐시합니다.
- 통계는 실행 중 100 ms, 대기 중 250 ms 주기로만 갱신합니다.
- 전송 시작 시 템플릿 준비와 Worker 생성을 supervisor 스레드에서 수행합니다.
- 각 템플릿은 Worker별 파티션으로 이동해 모든 Worker가 전체 로그를 복제하지 않습니다.
- Worker별 독립 소켓과 연결로 전송 경로의 전역 잠금을 제거합니다.
- UDP는 connected socket과 4 MiB 송신 버퍼를 사용합니다.
- TCP/TLS는 최대 256 KiB 단위로 여러 로그를 배치합니다.
- FILE은 최대 64 KiB 배치와 단일 순차 writer로 약 1 MiB 회전 파일을 생성합니다.
- 정적 로그는 완성 문자열을 재사용하고 시간 포함 로그는 초 단위 결과를 캐시합니다.
- 고속 UDP의 시스템 시각 조회는 256건 단위로 줄이고 통계는 Worker 로컬 누적 후 일괄 반영합니다.
- TLS 암호화 버퍼는 재사용하며 DirectX 11 Flip Model 스왑 체인을 사용합니다.
- Release 빌드는 최적화, LTCG, AVX2를 활성화합니다.

## GitHub 게시

검증 후 커밋과 현재 브랜치 push를 연속 수행하려면 다음 스크립트를 사용합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\publish.ps1 -Message "feat: update log generator"
```
