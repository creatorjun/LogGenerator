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

## 적용된 hot-path 최적화

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
