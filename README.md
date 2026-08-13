<!-- README.md -->
# LogGenerator

Windows 64비트 SIEM 스트레스 테스트를 위한 C++23 기반 고성능 로그 생성·전송기입니다. Dear ImGui Win32/DirectX 11 반응형 UI에서 UDP, TCP, TLS 네트워크 전송 또는 FILE 로컬 생성을 선택하고 UTF-8 JSON 샘플 카탈로그를 직접 추가·수정·삭제할 수 있습니다.

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
│  │  ├─ privacy_anonymizer.cpp
│  │  ├─ privacy_anonymizer.hpp
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

1. UDP, TCP, TLS, FILE 중 실행 방식을 선택합니다. 네트워크 방식만 대상 IP 또는 Host, Port를 입력합니다.
2. TCP/TLS에서는 Newline 또는 RFC 6587 Octet Counting 프레이밍을 선택합니다.
3. TLS 인증서 이름이 대상 IP와 다르면 `TLS 서버 이름`을 입력합니다.
4. 샘플 이름·본문·개인정보 범주를 검색합니다. 검색 결과 전체 순환이 기본으로 활성화되며 필요하면 단일 로그 생성으로 전환할 수 있습니다. `이름`, `계정`, `부서`, `호스트`, `IP` 같은 범주 검색어도 사용할 수 있습니다.
5. `추가`, `수정`, `삭제`로 샘플을 편집합니다. 저장 시 인식된 개인정보는 익명화 토큰으로 바뀌고 JSON 카탈로그에 즉시 반영됩니다.
6. `src_ip`, `dst_ip`를 지정하고 `현재 시간 + 오프셋` 또는 `기간 지정` 날짜 생성 방식을 선택합니다.
7. Worker 수와 목표 EPS를 설정합니다. 목표 EPS가 0이면 제한 없는 최대 성능 모드입니다. FILE에서는 총 생성량·파일 개수·실행 시간 안전 제한을 함께 설정합니다.
8. `전송 시작`을 누르고 현재 EPS, 평균 EPS, 총 로그 수, 총 바이트를 확인합니다. FILE에서는 각각 파일 기록 완료 EPS, 로그 수, 바이트입니다.

UDP 통계는 수신 장비의 처리 성공이 아니라 로컬 Winsock `send` 완료를 기준으로 집계합니다. TCP/TLS는 지속 연결과 최대 256 KiB 배치 전송을 사용합니다. FILE은 소켓이나 네트워크 전송 객체를 만들지 않고 실행 파일 옆 `generated` 폴더에만 기록하며 별도 어댑터가 해당 파일을 수집할 수 있습니다. `전송 중지`는 UI 스레드에서 종료를 기다리지 않고 즉시 중지 요청만 전달합니다.

## FILE 로컬 생성

FILE을 선택하고 시작하면 다음 폴더가 자동으로 생성됩니다. 이 모드에서는 대상 Host와 Port를 사용하지 않으며 네트워크로 로그를 보내지 않습니다.

```text
실행파일 경로/generated/
```

첫 파일은 로컬 시작 시각을 사용한 `yyyyMMdd_HHmmss_SSS.log` 형식입니다. 같은 실행에서 다음 로그 파일을 만들 때 `_0002`, `_0003` 순번이 추가됩니다.

```text
generated/20260812_173245_123.log
generated/20260812_173245_123_0002.log
```

각 로그 이벤트는 크기와 관계없이 독립된 파일 하나로 저장됩니다. FILE은 최대 256개 로그를 묶어 파일명 순번과 로그 생성 순서의 대응을 유지하면서 순차 기록하며 목표 EPS 설정은 동일하게 적용됩니다. 총 생성량, 파일 개수, 실행 시간 제한의 기본값은 모두 0(무제한)입니다. 필요한 제한만 0보다 큰 값으로 지정할 수 있으며 먼저 도달한 제한에서 오류 없이 정상 종료합니다. 제한을 설정한 경우에는 경계를 정확히 지키기 위해 로그를 한 건씩 검사한 후 기록합니다.

## JSON 샘플 카탈로그

샘플은 `Sample Logs\sample_logs.json`에 UTF-8 JSON으로 저장됩니다. 앱은 임시 파일 기록 후 원자적 교체를 사용해 저장 중 원본 손상 가능성을 줄입니다.

```json
{
  "schema_version": 1,
  "logs": [
    {
      "id": "woori-0001",
      "name": "[MONITORAPP]_AIWAF_Traffic_v1_cef_01",
      "test_case": {
        "FILE_PATH": ["security_20250705.log"]
      },
      "sample": "file={{FILE_PATH}} timestamp=2025-07-05T13:53:53Z src_ip={{SRC_IP}} dst_ip={{DST_IP}} user_name={{PERSON}} store_name={{STORE}}"
    }
  ]
}
```

`id`, `name`, `sample`은 비어 있으면 안 되며 `id`는 전체 카탈로그에서 고유해야 합니다. `test_case`는 샘플별 값 객체이며 토큰 발생 순서와 같은 개수의 값을 가집니다. 값은 렌더러 준비 단계에서 주입되므로 파일명, URI, S3 키, 애플리케이션 경로와 같은 필드 의미를 유지합니다. 개수가 맞지 않으면 실행을 거부해 잘못된 로그를 생성하지 않습니다. UI에서 새 로그를 만들면 충돌하지 않는 `user-...` 식별자를 자동 생성합니다. JSON을 외부 코드 편집기로 직접 바꾼 뒤에는 UI의 `새로고침`을 누르면 됩니다.

현재 기본 카탈로그는 `woori_poc_로그파서수정_20260812.csv`의 최신 60개 샘플로 기존 견본 전체를 교체한 결과입니다. 원본 CSV와 Parser XML의 필드명·위치 정보를 함께 분석해 실제 값 대신 익명화 토큰만 저장했으며 원본 CSV 파일 자체와 출처 메타데이터는 배포 카탈로그에 포함하지 않습니다.

## 개인정보 익명화

회사 문자열은 대소문자 구분 없이 긴 문자열부터 `lottermart → Your-Company`, `lotte → Your`, `mart → company` 순서로 치환하며 최신 샘플의 `test123` 회사 자리표시자도 `Your-Company`로 정규화합니다. 필드명으로 사람, 점포, 계정, 사번, 부서, 조직, 이메일, 전화번호, 주소, 일반 IP, MAC, 호스트, 식별자, 비밀값, 파일 경로를 판별합니다. 이메일·전화번호·주민등록번호 형태·MAC·Windows 사용자 경로는 필드명이 없어도 보조 패턴으로 제거합니다.

카탈로그에는 `{{PERSON}}`, `{{STORE}}`, `{{STORE_CODE}}`, `{{USER_ID}}`, `{{EMPLOYEE_ID}}`, `{{DEPARTMENT}}`, `{{EMAIL}}` 등의 토큰을 저장합니다. 점포 표시명과 숫자 코드 필드를 구분하므로 SQL과 숫자 필드에 `호점` 문자열이 삽입되지 않습니다. 프로그램은 시작 시 50개의 연관된 합성 프로필을 한 번만 준비합니다. 로그 한 건마다 프로필 하나를 선택하므로 같은 이벤트의 계정·성명·부서·호스트·IP는 모두 같은 프로필 번호를 공유합니다.

- 사람 이름: `홍길동 1` ~ `홍길동 50`
- 점포명: `1호점` ~ `50호점`
- 점포 코드: `1` ~ `50`
- 계정·사번·부서·이메일·전화·호스트 등: 종류별 테스트 전용 합성값
- 파일명·URI·경로: 기본 카탈로그의 샘플별 `test_case` 값으로 정확하게 생성
- 일반 개인정보 IP: 문서용 대역 `198.51.100.1` ~ `198.51.100.50`
- 소스·목적지 IP: `{{SRC_IP}}`, `{{DST_IP}}`를 UI에 입력한 값으로 치환

UI에서 새 샘플을 저장하거나 기존 샘플을 수정해도 같은 필드 기반 익명화가 적용됩니다. 편집창과 선택 미리보기에는 인식된 개인정보 토큰 개수가 표시됩니다. 개인정보 범주 비트마스크와 검색 동의어는 카탈로그 로드 시 검색 인덱스에 함께 캐시되므로 검색할 때 본문을 다시 파싱하지 않습니다.

## 자동 날짜와 IP 파싱

편집창은 로그 본문을 정규식으로 분석해 인식한 시간 토큰, `src_ip`, `dst_ip` 개수와 날짜 포맷을 보여줍니다. 전송 시작 시 같은 파서를 한 번 컴파일해 지정된 IP와 날짜 생성 설정을 적용합니다.

날짜 생성 방식은 다음 두 가지입니다.

- `현재 시간 + 오프셋`: 현재 시각에 `+/- 일·시간·분`을 적용합니다.
- `기간 지정`: `yyyy-MM-dd` 시작일 00:00:00부터 종료일 23:59:59까지 이벤트마다 1초씩 진행하고 범위 끝에서 시작일로 순환합니다. 원본 로그의 타임존 접미사는 유지하되 사용자가 입력한 달력 날짜가 다른 타임존으로 이동하지 않도록 처리합니다.

지원 시간 형식은 다음과 같습니다.

- ISO 8601 및 Year First: `yyyy-MM-dd HH:mm:ss`, `yyyy-MM-dd H:mm`, `yyyy/MM/ddTHH:mm:ss`, 소수초, `Z`, `+09:00`
- Syslog: `MMM dd HH:mm:ss`, `MMM dd HH:mm:ss yyyy`
- Month First: `MMM dd yyyy HH:mm:ss`, 선택적 `GMT` 또는 `UTC`
- Apache: `dd/MMM/yyyy:HH:mm:ss +0900`
- Compact: `yyyyMMddHHmmss`, `yyyyMMddTHHmmss[.fraction]`, 파일명의 `yyyyMMdd`와 `yyyyMM`, 필드형 `HHmmss`
- 분리 필드: 모든 `yyyy-MM-dd` 또는 `yyyy/MM/dd`, `yyyy-MM`, `time=HH:mm:ss[.fraction]`

소스 IP는 `src`, `srcip`, `src_ip`, `srp_ip`, `src-ip`, `srcaddr`, `source_ip`, `source-address`, `clientip`, `clientipaddr`, `sip` 계열을 인식합니다. 목적지 IP는 `dst`, `dstip`, `dst_ip`, `dest_ip`, `dstn_ip`, `dstaddr`, `destination_ip`, `destination-address`, `server_ip`, `dip` 계열을 인식합니다. `IP:Port -> IP:Port` 형식도 소스와 목적지로 자동 인식합니다.

## 반응형 UI

라이트 테마를 기본으로 사용합니다. 창 너비에 따라 통계는 1열, 2열, 4열로 전환되고 전송 설정과 로그 설정은 1열 또는 2열로 재배치됩니다. 넓은 화면에서는 두 설정 패널이 남은 세로 공간을 동일하게 채우고 패널별 스크롤을 제공하며, 타임 오프셋 입력도 작은 창에서는 2열로 자동 줄바꿈됩니다. 최소 창 크기는 520×480 논리 픽셀입니다.

모니터 DPI와 현재 창의 논리 해상도가 바뀌면 맑은 고딕 Regular/Bold 글꼴 크기와 굵기, 여백, 컨트롤 크기 및 최소 창 크기가 함께 조정됩니다. 최대화 크기는 현재 모니터의 실제 작업 영역을 사용하며 서로 다른 배율의 모니터 사이로 창을 이동해도 즉시 다시 계산합니다.

## 애플리케이션 로그

프로그램 자체의 실행 로그는 실행 파일 옆 `logs` 폴더에 UTF-8 텍스트로 저장됩니다.

```text
logs/LogGenerator_yyyyMMdd.log
```

파일 기록은 최대 8,192건의 제한된 큐와 전용 백그라운드 스레드를 사용합니다. 큐가 가득 차더라도 전송 Worker를 대기시키지 않으며, 누락된 애플리케이션 로그 수는 다음 기록 시 경고로 남깁니다. 로그 파일은 1초 이내 주기로 flush되고 오류 및 정상 종료 시 즉시 flush됩니다.

기록 대상은 프로그램 시작·종료, UI 초기화, JSON 카탈로그 로드·저장, 스트레스 테스트 설정과 시작·중지, Worker 연결 완료 및 전송 오류입니다. 개별 보안 로그 전송 성공은 최대 EPS를 훼손하지 않도록 애플리케이션 로그에 기록하지 않습니다.

## 성능 설계

- JSON 로드·저장·정규식 사전 분석은 UI 외부 백그라운드 스레드에서 실행합니다.
- 검색용 이름·본문·개인정보 범주 인덱스, 미리보기, 분석 결과와 통계 표시 문자열을 캐시합니다.
- 통계는 실행 중 100 ms, 대기 중 250 ms 주기로만 갱신합니다.
- 전송 시작 시 템플릿 준비와 Worker 생성을 supervisor 스레드에서 수행합니다.
- 각 템플릿은 Worker별 파티션으로 이동해 모든 Worker가 전체 로그를 복제하지 않습니다.
- Worker별 독립 소켓과 연결로 전송 경로의 전역 잠금을 제거합니다.
- UDP는 connected socket과 4 MiB 송신 버퍼를 사용합니다.
- TCP/TLS는 최대 256 KiB 단위로 여러 로그를 배치합니다.
- FILE은 최대 256개 로그를 묶어 독립 파일들을 순차 생성하고 호출·프레이밍 오버헤드를 줄이며, 안전 제한을 설정한 경우에는 로그마다 총량·개수·시간 제한을 검사합니다.
- 정적 로그는 완성 문자열을 재사용하고 시간 포함 로그는 초 단위 결과를 캐시합니다. 개인정보 토큰은 정규식을 반복 실행하지 않으며 이벤트당 PRNG를 한 번만 호출한 뒤 50개 사전 생성 프로필의 문자열을 바로 추가합니다.
- 고속 UDP의 시스템 시각 조회는 256건 단위로 줄이고 통계는 Worker 로컬 누적 후 일괄 반영합니다.
- TLS 암호화 버퍼는 재사용하며 DirectX 11 Flip Model 스왑 체인을 사용합니다.
- Release 빌드는 최적화, LTCG, AVX2를 활성화합니다.

## GitHub 게시

검증 후 커밋과 현재 브랜치 push를 연속 수행하려면 다음 스크립트를 사용합니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\publish.ps1 -Message "feat: update log generator"
```
