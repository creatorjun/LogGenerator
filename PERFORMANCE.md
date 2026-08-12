<!-- PERFORMANCE.md -->
# LogGenerator Performance

## Audit scope

성능 검토 범위는 전체 Domain, Application, Infrastructure, Presentation, 테스트, CMake 및 PowerShell 스크립트입니다. 생성 핫루프의 문자열 할당과 시간 변환, worker 간 원자 연산, EPS 페이싱, UDP/TCP/TLS/FILE 시스템 호출, 파일 로거 큐, JSON 및 정규식 처리, Dear ImGui 프레임과 D3D11 리사이즈 경로를 확인했습니다.

## Applied optimizations

- 소수점 타임스탬프는 초 단위 달력 문자열을 캐시하고 소수점 숫자만 갱신합니다.
- 날짜 숫자는 일반 경로에서 CRT 서식 함수를 호출하지 않고 고정 자릿수로 직접 기록합니다.
- 개인정보 프로필은 이벤트당 한 번 선택하고 모든 토큰이 같은 사전 생성 세트를 직접 참조합니다.
- 기간 지정 커서는 이벤트마다 나머지 연산을 하지 않고 덧셈과 경계 분기만 사용합니다.
- 목표 EPS 간격은 worker 시작 시 한 번 계산합니다.
- 무제한 TCP/TLS는 최대 4,096건 또는 256KiB, FILE은 최대 16,384건 또는 1MiB를 한 번에 처리합니다.
- TLS는 Schannel 헤더, 본문, 트레일러를 별도 문자열로 복사하지 않고 `WSASend` scatter/gather로 전송합니다.
- Schannel 암호화 버퍼는 최대 레코드 크기로 한 번 할당해 재사용합니다.
- supervisor는 1ms polling 대신 stop-token 기반 대기를 사용합니다.
- Windows worker의 실행 속도 power throttling을 비활성화합니다.
- 편집기 정규식 분석은 UI와 분리된 background worker에서 실행합니다.
- Win32 resize 메시지는 마지막 크기만 프레임 경계에서 적용하며 D3D11 최대 frame latency를 1로 제한합니다.

## Renderer benchmark

2026-08-12 동일 PC의 x64 Release, LTCG, AVX2 빌드에서 이벤트 2,000,000건을 측정했습니다. 수정 전은 3회, 수정 후는 7회 실행의 중앙값입니다.

| Scenario | Before | After | Change |
|---|---:|---:|---:|
| Static | 108,424,591 events/s | 115,917,166 events/s | +6.9% |
| Integer timestamp | 86,628,464 events/s | 93,026,285 events/s | +7.4% |
| Fractional timestamp | 1,996,124 events/s | 17,756,911 events/s | 8.9x |
| Timestamp and privacy tokens | 10,358,052 events/s | 11,477,670 events/s | +10.8% |

실행 방법은 다음과 같습니다.

```powershell
cmake --build build --config Release --target LogGeneratorBenchmarks --parallel
.\build\bin\Release\LogGeneratorBenchmarks.exe
```

## Engine benchmark

전송 엔진 자체의 상한을 보기 위해 실제 시스템 호출을 하지 않는 adapter와 개인정보 및 소수점 타임스탬프가 포함된 로그를 사용해 750ms씩 5회 측정했습니다.

| Scenario | Median |
|---|---:|
| UDP semantics, 1 worker | 8,723,550 events/s |
| Stream semantics, 1 worker | 8,201,119 events/s |
| Stream semantics, 4 workers | 24,168,485 events/s |

```powershell
cmake --build build --config Release --target LogGeneratorEngineBenchmarks --parallel
.\build\bin\Release\LogGeneratorEngineBenchmarks.exe
```

이 수치는 생성기 내부의 CPU 상한이며 실제 UDP, TCP, TLS EPS를 의미하지 않습니다. 실제 결과는 평균 로그 크기, NIC, Windows network stack, TLS cipher, 수신 장비의 socket buffer와 처리량, 패킷 손실 및 디스크에 따라 달라집니다.

## Production measurement

실제 SIEM 장비에서는 프로토콜별로 worker 수를 1, 2, 4, 8 순서로 올리며 송신 PC CPU, NIC bytes/packets, UDP discard, TCP retransmit, TLS CPU, 수신 장비 EPS를 함께 기록해야 합니다. FILE은 생성 폴더와 수집 adapter가 같은 디스크를 경합하는 조건에서도 별도로 측정해야 합니다. 최대값은 30초 이상의 warm-up 이후 안정 구간의 중앙값으로 결정하고 오류 또는 손실이 발생하기 직전 값을 사용합니다.

TLS scatter/gather의 컴파일 및 정적 경로는 검증했지만 이번 로컬 검증에는 실제 TLS 수신 endpoint와 인증서가 포함되지 않았습니다. Release GUI의 D3D11/ImGui 초기화, 84개 카탈로그 로드와 정상 종료 smoke는 통과했습니다. 실제 창 조작 latency 측정은 운영 PC에서 추가로 수행해야 합니다.
