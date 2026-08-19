# 성능 벤치마크

`LogGeneratorBenchmarks`는 디스크와 실제 네트워크 수신기 성능을 제외하고 로그 렌더링, Round-Robin 선택, 프레이밍, 배치 및 worker 오케스트레이션의 최대 처리량을 측정합니다. 결과는 실제 UDP/TCP/TLS 전송 EPS의 보장값이 아니라 애플리케이션 엔진의 상한선입니다.

## 빌드와 실행

macOS Apple Silicon 예시:

```bash
cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Release -DLOGGEN_BUILD_GUI=ON -DLOGGEN_BUILD_BENCHMARKS=ON -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-macos --target LogGeneratorBenchmarks --parallel
./build-macos/bin/LogGeneratorBenchmarks
```

## M4 측정 결과

AppleClang 21, Release, 14 worker에서 측정한 대표값입니다.

| 시나리오 | 변경 전 | 변경 후 | 배율 |
|---|---:|---:|---:|
| 동일 초 개인정보 렌더링 | 6.17M EPS | 385.56M EPS | 62.5x |
| 스트림 엔진, worker 1개 | 1.12M EPS | 6.72M EPS | 6.0x |
| 스트림 엔진, worker 14개 | 10.68M EPS | 74.24M EPS | 6.9x |
| 60개 템플릿 스트림 선택 | 12.82M EPS | 58.55M EPS | 4.6x |
| 60개 템플릿 데이터그램 선택 | 12.71M EPS | 73.05M EPS | 5.7x |

측정값은 전원 모드, 열 상태, 백그라운드 프로세스와 컴파일러 버전에 따라 달라질 수 있습니다.

동일 M4에서 60개 실제 샘플, 14 worker, 로컬 Python 수신기와 3초간 통신한 결과는 TCP 약 13.90M EPS, UDP 로컬 `send` 기준 약 578K EPS였습니다. UDP 수치는 애플리케이션이 수신 확인을 하지 않으므로 실제 수신 EPS가 아니라 로컬 소켓이 받아들인 전송 호출 수입니다.

### macOS UDP 3M end-to-end 검증

UDP 엔진은 XNU `sendmsg_x` 벡터 송신을 지원하며, 3M 병렬 경로는 최대 60 KiB 개행 패킹으로 물리 데이터그램 수를 추가로 줄입니다. 2026-08-19 M4에서 목표 3,000,000 EPS, 5초, loopback C++ 수신기로 측정한 결과입니다.

| 항목 | 결과 |
|---|---:|
| 순환 샘플 | 전체 60종 |
| 송신 로그 | 15,015,293 |
| 수신 로그 | 15,015,293 |
| UDP 데이터그램 | 251,317 |
| 송신 평균 | 2,999,914.76 EPS |
| 수신 평균 | 3,000,200.86 EPS |
| 수신 데이터그램 처리량 | 50,215.57 DPS |
| UDP payload 처리량 | 약 2.85 GB/s |
| 수신 비율 | 100.000000% |

목표 경계의 측정 오차를 배제하기 위해 3.1M EPS로 3초간 추가 검증했으며, 전체 60종에서 송·수신 9,315,998건, 수신 3,100,087.06 EPS, 손실 0%를 기록했습니다.

재현 명령은 다음과 같습니다. 이 벤치마크는 실제 loopback UDP 소켓에서 송신한 데이터그램을 수신하고 내부 개행 이벤트를 계수하며, 수신 3M EPS와 99.9% 이상의 수신 비율을 만족하지 못하면 실패 코드로 종료합니다.

```bash
cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Release -DLOGGEN_BUILD_GUI=ON -DLOGGEN_BUILD_BENCHMARKS=ON -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-macos --target LogGeneratorMacosUdpBenchmark --parallel
./build-macos/bin/LogGeneratorMacosUdpBenchmark 3000000 5
```

이 결과의 EPS는 개행으로 분리되는 로그 이벤트 수입니다. 표준 호환 모드인 순차 UDP는 로그당 데이터그램 1개를 유지하며 동일 장비에서 약 0.74M PPS, `sendmsg_x`와 Worker 2개를 사용하는 비패킹 데이터그램 경로의 상한은 약 1.21M PPS였습니다. 전체 샘플의 평균 크기에서는 3M EPS가 약 22.8 Gbit/s의 UDP payload이므로 원격 전송에는 프로토콜 오버헤드를 포함해 최소 25GbE급 링크와 동급 수신기가 필요합니다. 60 KiB 데이터그램은 1,500-byte MTU에서 IP 단편화되므로 loopback, jumbo frame 또는 신뢰할 수 있는 전용망이 아니면 손실률이 커질 수 있습니다.

## 적용된 hot-path 최적화

- macOS 연결형 UDP에서 최대 256개 데이터그램을 한 syscall로 넘기는 XNU `sendmsg_x` 경로 추가
- Linux UDP에서 최대 256개 데이터그램을 묶는 `sendmmsg` 경로 추가
- 병렬 UDP에서 개행 이벤트를 최대 60 KiB로 패킹하고 로그 EPS와 데이터그램 DPS를 분리 집계
- 모든 Worker 연결 후 동시에 전송을 시작해 초기 단일 Worker 구간과 연결 시간을 EPS에서 제외
- 카탈로그 로드·편집 시 토큰화된 불변 렌더 청사진을 미리 컴파일하고 원본/토큰 샘플 키로 공유
- 전송 및 FILE 생성 시작 시 정규식 재분석 없이 시각 오프셋과 송수신 IP만 캐시 청사진에 바인딩
- 동일 초의 개인정보 로그를 50개 합성 프로필별 완성 문자열로 캐시
- UTC 및 기간 지정 시간 변환에서 전역 `gmtime` mutex 제거
- 고속 모드에서 전역 Round-Robin 순번을 worker별 연속 블록으로 예약
- 단일 템플릿 실행에서 Round-Robin 원자 연산 제거
- Newline 프레이밍에 줄바꿈이 없는 일반 로그의 일괄 복사 경로 추가
- 최대 스트림 배치를 1,024 이벤트 및 1 MiB까지 확장
- supervisor의 1ms polling을 stop callback 기반 대기로 교체
- 부하 중 UI와 worker의 OS 스케줄링 우선순위를 분리하고 UI 렌더 주기를 절반으로 조정
