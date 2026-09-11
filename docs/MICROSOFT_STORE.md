<!-- docs/MICROSOFT_STORE.md -->
# Microsoft Store 링크 배포

확인 기준일: 2026-09-11. 대상은 이 저장소의 C++23 / Dear ImGui 기반 Windows x64 LogGenerator이며, MSIX로 패키징하는 경로를 사용한다. 앱은 SIEM 검증용 샘플 로그를 생성하고 UDP, TCP, TLS 또는 로컬 FILE 방식으로 출력한다.

Partner Center에서 `LogGenerator` 이름을 예약했고 실제 앱 identity를 로컬 설정에 반영했다. 제품 ID `9NDZGLF6HDKR`의 Submission 1에 샘플 0개 패키지를 업로드했으며, 서버 검증 후 Packages 저장을 완료했다. 인증 제출과 게시는 아직 수행하지 않았다. 개발용 identity 패키지, 실제 identity의 초안 업로드 패키지, 인증 제출용 패키지는 아래 생성 모드로 구분한다.

모든 MSIX 생성 모드는 샘플 로그 **0개**를 기본값으로 사용한다. 패키지의 `Sample Logs/sample_logs.json`에는 `{"schema_version":1,"logs":[]}`만 넣고 CSV는 포함하지 않는다. 저장소 원본의 샘플 60개와 CSV는 보존하며 Store 패키지에 복사하지 않는다. 신규 설치 사용자는 `추가` 또는 `CSV 가져오기`로 자신의 샘플을 준비한다.

## 링크를 통한 설치

| 방식 | 접근 범위 | 이 작업에서의 용도 |
| --- | --- | --- |
| `Public audience` + 검색 숨김 + `Direct link only` | 직접 링크로 들어온 사용자가 취득 가능 | 이번 배포의 기본 경로 |
| MSIX `Private audience` | 지정 그룹의 개인 Microsoft 계정만 목록 조회 및 취득 | 특정인만 허용할 때 사용하는 별도 선택 |
| Intune MSIX LOB | 조직에서 배정한 Entra ID 사용자/기기 그룹 | 회사 계정만 허용해야 할 때의 별도 배포 경로 |

이번 배포는 모든 사람에게 링크를 전달할 수 있도록 `Public audience` → `Make this product available but not discoverable in the Store` → `Direct link only`를 선택한다. Store 검색·탐색에서는 숨겨지지만, 링크를 가진 사용자는 Windows 10/11에서 목록을 열고 설치할 수 있다. 링크 재전달을 막거나 수신자를 인증하는 설정은 아니므로 링크가 알려져도 되는 앱에 적용한다. [Microsoft: MSIX visibility options](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/visibility-options)

특정 사용자만 허용하려면 첫 제출부터 Private audience와 known user group을 사용해야 한다. Public으로 제출한 뒤 Private으로 바꿀 수 없다. Private 수신자는 등록된 개인 Microsoft 계정으로 로그인해야 하며 회사/학교 Entra ID 계정은 지원하지 않는다. 그룹에서 제거해도 기존 설치의 실행까지 차단하지는 않는다. [Microsoft: Private audience 제약](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/visibility-options)

Microsoft Store for Business / Education의 기존 Private Store 경로는 종료되었다. [Microsoft 종료 공지](https://learn.microsoft.com/en-us/lifecycle/announcements/microsoft-store-for-business-education-retiring) 회사 계정과 관리 기기만을 대상으로 하려면 서명한 MSIX를 Intune의 LOB 앱으로 올리고 조직 그룹에 할당한다. 이 경우 서명과 대상 기기의 인증서 신뢰 배포도 조직이 관리한다. [Microsoft: Intune MSIX 배포](https://learn.microsoft.com/en-us/windows/msix/desktop/managing-your-msix-deployment-mem-adminconsole)

## 계정 등록과 identity

이 제품의 계정 접근과 이름 예약은 완료했다. 아래는 신규 제품 설정 시의 절차이며, 이미 있는 `StoreConfig.local.psd1`을 예제로 덮어쓰지 않는다.

1. [Microsoft Store 개발자 등록](https://storedeveloper.microsoft.com/)에서 시작한다. 회사 명의 앱이면 Company 유형으로 사업자·담당자 검증을 완료한다. Individual에서 Company로의 계정 유형 변경은 지원하지 않으므로 등록할 때 명의를 확정한다. [Microsoft: 개발자 계정 개설](https://learn.microsoft.com/en-us/windows/apps/publish/partner-center/open-a-developer-account)
2. Partner Center의 Apps and games에서 새 앱을 만들고 사용 가능한 앱 이름을 예약한다.
3. 앱의 Product management → App identity에서 package identity 정보를 확인한다.
4. 아래 예제 파일을 복사하고 실제 값을 기입한다. 로컬 설정에는 인증서 비밀번호나 개인키를 기록하지 않는다.

```powershell
if (-not (Test-Path -LiteralPath .\packaging\windows\StoreConfig.local.psd1)) {
    Copy-Item -LiteralPath .\packaging\windows\StoreConfig.example.psd1 -Destination .\packaging\windows\StoreConfig.local.psd1
}
notepad .\packaging\windows\StoreConfig.local.psd1
```

| 설정 키 | 기입 값 |
| --- | --- |
| `IdentityName` | Partner Center의 `Package/Identity/Name` |
| `Publisher` | Partner Center의 `Package/Identity/Publisher` 전체 값 |
| `PublisherDisplayName` | Partner Center에 등록된 게시자 표시 이름 |
| `DisplayName` | 예약한 앱 표시 이름 |
| `Version` | 예: `1.0.0.0`; 업데이트 시 증가 |
| `PrivacyPolicyUrl` | 운영자 정보를 확정하여 게시한 개인정보처리방침의 공개 HTTPS URL; 개발 검증 및 `-DraftUpload`에서만 빈 문자열 허용 |

identity 값은 대소문자, 공백, 구두점까지 일치시킨다. 패키지 버전은 네 부분으로 구성하고 마지막 부분은 Store용 `0`으로 둔다. 앞 세 부분은 0~65535이며 첫 부분은 0보다 커야 한다. Store 제출용 MSIX는 유료 CA 인증서가 필요하지 않으며, Microsoft가 심사 후 재서명한다. Store 밖에서 설치할 패키지는 별도 서명과 신뢰가 필요하다. [Microsoft: MSIX package requirements](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/app-package-requirements)

## 패키지 생성

Windows x64 환경에 Visual Studio 2026 C++ 빌드 도구, CMake 3.26 이상, Git, Windows SDK의 MakeAppx·MakePri를 준비한다. 저장소 루트에서 실행한다. 기존 일반 빌드와 구분된 `build-store` 디렉터리를 사용한다.

| 생성 모드 | identity | 개인정보처리방침 URL | 출력 폴더와 용도 |
| --- | --- | --- | --- |
| `-ValidationOnly` | 로컬 개발 identity | 빈 문자열 허용 | `out/store/validation`, 로컬 검증 전용 |
| `-ConfigPath ... -DraftUpload` | 실제 Partner Center identity | 빈 문자열 허용 | `out/store/draft`, Packages 초안 업로드 |
| `-ConfigPath ...` | 실제 Partner Center identity | 공개 HTTPS URL 필수 | `out/store/submission`, 인증 제출 준비 |

현재처럼 실제 identity는 있고 공개 개인정보처리방침 URL은 준비 중인 경우, 초안 업로드용으로 다음 명령을 사용한다. `-DraftUpload`는 웹 방침이나 Store 목록을 완성하지 않은 상태에서 패키지를 초안에 업로드하기 위한 모드이며, 인증 제출 준비가 끝났다는 뜻은 아니다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-store.ps1 -ConfigPath .\packaging\windows\StoreConfig.local.psd1 -DraftUpload -RunWack
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-store-package.ps1 -PackagePath .\out\store\draft\LogGenerator-1.0.0.0-x64.msix
```

실제 identity 없이 로컬 검증만 할 때는 다음 명령으로 개발 identity 패키지를 만든다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-store.ps1 -ValidationOnly
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-store-package.ps1 -PackagePath .\out\store\validation\LogGenerator-1.0.0.0-x64.msix
```

`out/store/validation` 산출물은 실제 앱 예약 정보와 연결되지 않는다. 이 파일을 Partner Center에 제출하지 않는다. `-ValidationOnly` 또는 `-DraftUpload`에서 개인정보처리방침 URL을 비워 두면 앱의 `개인정보 안내`에는 로컬 처리·저장·전송 안내만 제공한다.

인증 제출 전에는 공개 웹 방침을 게시하고 실제 identity와 `PrivacyPolicyUrl` 설정을 사용해 `-DraftUpload` 없이 패키지를 다시 만든다. 이 제출 모드는 HTTPS 개인정보처리방침 URL이 없으면 실패한다. URL은 앱의 `개인정보 안내` → `온라인 방침 열기` 버튼에 적용한다. 스크립트의 URL 형식 검사는 웹 게시나 접근 가능성을 입증하지 않으므로, 운영자는 로그인 없는 브라우저에서도 방침이 열리는지 별도로 확인한다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-store.ps1 -ConfigPath .\packaging\windows\StoreConfig.local.psd1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-store-package.ps1 -PackagePath .\out\store\submission\LogGenerator-1.0.0.0-x64.msix
```

버전을 변경했다면 검사할 파일 이름도 변경한다. 서명 없는 제출 패키지를 더블 클릭하여 설치할 수 있는 것으로 해석하지 않는다. 로컬 설치용 서명이 필요하면 `-CertificateThumbprint`에 현재 사용자 `My` 인증서 저장소에 있는 코드 서명 인증서의 지문을 전달한다. 인증서의 Subject는 manifest의 Publisher와 일치해야 하며 대상 테스트 PC에서 신뢰되어야 한다. [Microsoft: MSIX 서명](https://learn.microsoft.com/en-us/windows/msix/package/signing-package-overview)

이 앱은 기존 Win32 실행 모델을 유지하며 manifest에 `Windows.Desktop`, x64와 `runFullTrust`를 사용한다. `runFullTrust`는 일반 데스크톱 앱 실행 권한이고 관리자 자동 승격 권한이 아니다. 현재 Partner Center에는 이 기능의 승인 필요 경고가 표시되므로 인증 제출 전에 사용 이유를 기입한다. 아래 심사 설명 초안을 사용할 수 있다. [Microsoft: 패키지 구성 요소](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion), [Microsoft: capability 설명](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/app-capability-declarations)

## 설치 후 확인

`test-store-package.ps1`의 검사는 패키지 구조와 포함 자산의 사전검증이다. 이를 Windows App Certification Kit(WACK) 통과, Store 심사 통과 또는 운영 수신기 검증으로 간주하지 않는다.

MSIX 실행 시 사용자 데이터의 기준 폴더는 `%LOCALAPPDATA%\Packages\<PackageFamilyName>\LocalState`다. 그 아래 `Sample Logs\sample_logs.json`, `logs`, `generated`를 사용한다. 신규 설치의 첫 실행에는 빈 카탈로그를 복사한다. 이미 있는 사용자 카탈로그는 업데이트 때 보존하므로, 샘플 0개 패키지로 업데이트해도 사용자가 저장한 샘플을 삭제하지 않는다. 기존 portable 데이터는 자동으로 검색·이관하지 않는다. 필요한 카탈로그는 먼저 별도로 백업한 뒤 해당 사용자 카탈로그로 옮긴다. 일반 portable 실행은 실행 파일 옆 저장 경로를 유지하고, CLI의 명시적 `--catalog`와 `--output-dir` 지정은 기본 경로보다 우선한다.

서명 및 인증서 신뢰가 준비된 테스트 PC/VM에 설치한 후 아래 항목을 확인한다.

| 확인 항목 | 기대 결과 |
| --- | --- |
| 표준 사용자 실행 | 관리자 권한 요청 없이 시작 메뉴에서 실행 |
| 한글 UI | 글꼴·아이콘이 정상이며 DPI 변경 후 잘림 없음 |
| 개인정보 안내 | 로컬 안내가 표시되고 제출 빌드의 온라인 방침 버튼이 올바른 HTTPS 페이지를 엶 |
| 카탈로그 | 신규 설치에서 샘플 0개, 직접 추가/CSV 가져오기/저장 후 재실행해 유지 |
| FILE | 지정한 제한만큼 로컬 파일 생성 후 정상 중단 |
| UDP/TCP/TLS | 승인된 테스트 수신기로 전송·중단·오류 표시 확인 |
| 읽기 전용 설치 경로 | 카탈로그·진단 로그·생성 파일이 설치 폴더에 쓰이지 않음 |
| 업데이트 | 높은 버전 설치 후 기존 사용자 카탈로그 유지 |
| 제거 | 실행 파일 제거와 사용자 데이터 보존/삭제 결과 기록 |

UDP 수신, TCP 프레이밍 및 TLS 신뢰 검증은 각각 수신기에서 확인한다. FILE 테스트만으로 네트워크 기능을 검증했다고 기록하지 않는다.

Windows SDK의 WACK를 설치한 테스트 환경에서 실제 앱 패키지를 검사한다. `package-store.ps1`에 `-RunWack`를 추가하면 패키지 생성 후 WACK를 실행하고, 선택 항목을 포함해 PASS가 아닌 결과가 있으면 실패로 보고한다. 앱을 배포하지 않은 패키지 검사와 설치 후 기능 검증은 구분한다. GUI로 Windows Store app 검사를 실행하거나 아래 CLI 흐름을 사용할 수도 있다. `-reportoutputpath`는 실제 보고서 경로로 지정한다. [Microsoft: Windows App Certification Kit](https://learn.microsoft.com/en-us/windows/uwp/debug-test-perf/windows-app-certification-kit)

```powershell
$appCert = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\App Certification Kit\appcert.exe'
$packageFile = (Resolve-Path -LiteralPath '.\out\store\submission\LogGenerator-1.0.0.0-x64.msix').Path
$reportFile = Join-Path (Get-Location) 'out\store\wack-report.xml'
& $appCert reset
& $appCert test -appxpackagepath $packageFile -reportoutputpath $reportFile
```

보고서의 실패 항목을 해결하고 재검사한다. 최소 지원 OS와 Windows 11에서 실제 설치·실행을 확인한다. 최종 확인 기록에는 패키지 SHA-256, 버전, OS 빌드, 검사 시각, WACK 결과, 수동 기능 결과를 함께 보관한다.

## Partner Center 제출

1. submission의 Pricing and availability → Visibility에서 Audience는 **Public audience**, Discoverability는 **Make this product available but not discoverable in the Store**, 취득 방식은 **Direct link only**로 설정한다. `Stop acquisition`은 신규 링크 설치 목적에 맞지 않는다.
2. Packages 초안을 준비할 때는 `out/store/draft`의 실제 identity MSIX를 업로드할 수 있다. 인증 제출 전에는 방침 URL을 포함해 다시 생성한 `out/store/submission` 패키지로 교체한다. 업로드 100%, 패키지 분석 완료, 초안 저장, 인증 제출 및 게시 완료는 서로 다른 상태다.
3. 표시 이름, 설명, 지원 연락처, 한국어 Store 목록, 연령 등급 설문 및 개인정보처리방침 URL을 입력한다. [개인정보처리방침 초안](PRIVACY_POLICY.ko.md)의 운영자·연락처·시행일을 확정하고 공개 HTTPS 웹페이지에 게시한다. 게시한 URL을 `StoreConfig.local.psd1`의 `PrivacyPolicyUrl`과 Partner Center에 동일하게 입력한다. 저장소의 로컬 파일 경로는 제출 URL이 아니다.
4. 실제 패키지 실행 화면을 PNG로 촬영한다. 데스크톱 스크린샷은 1366×768 이상, 파일당 50 MB 이하이며 최소 한 장이 필요하다. 내부 IP, 파일 경로, 실제 로그 등은 합성 테스트 데이터로 바꾼 상태에서 촬영한다. [Microsoft: Store 이미지 요구사항](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/screenshots-and-images)
5. Notes for certification에 아래 설명과 재현 가능한 테스트 절차를 넣고 제출한다. 회사 내부 서버가 심사자에게 열려 있다고 가정하지 않는다.
6. 심사 통과 및 게시 후 앱의 Store 직접 링크를 전달하고 링크 설치를 확인한다. 후속 업데이트도 같은 identity와 높은 버전으로 제출하며 검색 숨김 설정을 유지한다.

Win32 / Desktop Bridge 앱은 개인정보처리방침이 필요하다. 처리 데이터, 저장·전송 방식, 수신자, 사용자 통제 방법을 실제 동작과 일치시켜 작성한다. 앱과 설명은 심사자가 테스트할 수 있어야 하며, 서버가 필요한 기능은 테스트 가능한 서버 또는 충분한 검증 절차를 준비한다. [Microsoft Store 정책 10.3, 10.5](https://learn.microsoft.com/en-us/windows/apps/publish/store-policies)

심사 설명 초안:

> LogGenerator는 승인된 테스트 환경에서 SIEM 수집 성능과 로그 파싱을 검증하는 Windows x64 데스크톱 도구입니다. 설치 패키지에는 샘플 로그가 없습니다. 먼저 추가 버튼으로 테스트용 샘플을 저장하거나 CSV 가져오기로 샘플을 준비합니다. 사용자가 샘플을 선택하고 전송을 시작하면 지정한 UDP/TCP/TLS 수신기로 이벤트를 보내며, FILE 모드에서는 네트워크 연결 없이 로컬 파일을 생성합니다. 기존 C++ Win32 창, Direct3D 렌더링, 파일 선택 및 소켓 동작을 위해 runFullTrust를 사용합니다. 관리자 자동 승격이나 Windows 서비스 설치는 요구하지 않습니다. 앱 자체 계정 로그인은 필요하지 않습니다. FILE 모드에서 작은 파일 수 제한을 지정하고 생성·중단·재시작을 확인할 수 있습니다. 네트워크 검증은 심사자가 제어하는 로컬 수신기 또는 별도로 제공한 테스트 수신기를 사용합니다.

이 문서는 제출 절차와 확인 기준을 제공한다. Partner Center 계정 개설, 실제 identity 검증, 개인정보처리방침 웹 게시, WACK 실행, Store 인증과 직접 링크 설치는 각각 완료 증거가 있어야 완료로 기록한다.

## 2026-09-11 샘플 제외 패키지 검증

실제 앱 identity를 사용한 실행 명령은 `.\scripts\package-store.ps1 -ConfigPath .\packaging\windows\StoreConfig.local.psd1 -DraftUpload -RunWack`이다. `out/store/draft/LogGenerator-1.0.0.0-x64.msix`는 1,246,149 bytes이며, SHA-256은 `D45D1A150D77ED0BE2E0A1D4396375BE4188E7BE0DC2D2260A5AE4AF440D3E54`다.

| 확인 항목 | 결과 |
| --- | --- |
| Store x64 Release 빌드 및 CTest | 성공, CTest 4/4 PASS |
| 패키지 카탈로그 | canonical 빈 JSON, `SampleLogCount=0` |
| 포함 파일 검사 | CSV 없음, 정확한 파일 허용 목록과 identity 검사 PASS |
| 샘플 제외 회귀 검사 | PowerShell fixture 8개 PASS |
| WACK 패키지 검사 | Overall PASS, 개별 24/24 PASS |
| 저장소 원본 | 샘플 60개와 CSV 보존 |
| 앱 설치 후 기능 및 Store 인증 | 이 빌드·패키지 검사 결과에 포함되지 않음 |

같은 폴더의 `.msix.report.json`과 `.msix.wack.xml`이 해당 산출물의 로컬 검사 보고서다. 이 패키지는 공개 개인정보처리방침 URL이 없는 초안용이며, 방침과 Store 목록을 완성한 뒤 인증 제출용으로 다시 패키징한다. Partner Center 분석·저장 및 인증 진행 상태는 로컬 검사 보고서가 증명하지 않는다.

## 2026-09-11 Partner Center 업로드 결과

[제품 개요](https://partner.microsoft.com/ko-kr/dashboard/products/9NDZGLF6HDKR/overview)에서 Save 이후 확인한 결과다.

| 항목 | 확인 상태 |
| --- | --- |
| 제품 / 제출 | LogGenerator / Submission 1 초안 |
| 업로드 파일 | `LogGenerator-1.0.0.0-x64.msix`, 위 샘플 제외 패키지 해시와 동일 |
| 서버 패키지 검사 | `Validated` |
| 저장 후 패키지 상태 | `완료` |
| 저장 후 공개 범위 | `Public audience` + 검색 숨김 + `Direct link only`, 재조회로 저장 상태 확인 |
| 가격 | 미설정, 인증 제출 전에 유효한 가격 선택 필요 |
| 인증 제출 | 미수행, `인증을 위해 제출` 버튼 비활성화 |
| 게시 / 사용자 설치 | 미수행 |

공개 개인정보처리방침, 가격, Store 목록·이미지·연령 등급 등 미완성 항목과 `runFullTrust` 사용 이유를 준비해야 한다. 패키지 업로드 및 검증 완료는 Microsoft의 최종 인증 승인을 뜻하지 않는다. 게시 후 사용할 [Store 직접 링크](https://apps.microsoft.com/detail/9NDZGLF6HDKR)는 정해졌지만 현재 게시 전이므로 이 링크로 설치할 수 없다.

## 2026-09-11 이전 검증 기록

아래는 샘플 제외 적용 전의 1차 검증 기록이며 현재 초안 패키지의 결과가 아니다. 실행 명령은 `.\scripts\package-store.ps1 -ValidationOnly -RunWack`이다. Windows 11 Pro 빌드 26200, MSVC 19.51, Windows SDK 10.0.26100.0과 WACK 10.0.26100.8249 환경에서 수행했다.

| 확인 항목 | 결과 |
| --- | --- |
| Store x64 Release 빌드 | 성공, AVX2 필수 조건 해제 |
| CTest | 4/4 PASS; 카탈로그 경로·업데이트 보존·동시 초기화, 실행 리소스 검사 포함 |
| MakeAppx | 매니페스트 검증을 활성화한 MSIX 생성 성공 |
| 패키지 사전 검사 | 파일 허용 목록, identity, SHA256 블록맵 설정, PE 보안 플래그, 이미지 크기 PASS |
| Windows PowerShell 5.1 사전 검사 | PASS |
| 잘못된 설정 처리 | 미입력 Store identity와 불일치 패키지 identity 거부 확인 |
| 최종 WACK 패키지 검사 | Overall PASS, 개별 24/24 PASS, 실패 0 |
| 개발 identity 등록 시도 | 로컬 개발자/사이드로딩 정책에 의해 `0x80073CFF`로 제한됨 |
| 실제 설치 후 UI·업데이트·제거·Store 링크 설치 | 미검증 |
| 온라인 방침 열기 | SDK 코드 컴파일 확인; 실제 URL 및 브라우저 실행 미검증 |
| 당시 Partner Center 인증 | 당시 계정 미등록으로 미제출 |

WACK는 앱을 설치하지 않은 패키지 검사로 실행되었다. 이 결과는 설치 후 동작이나 Microsoft 심사 승인을 대신하지 않는다. 시스템의 개발자 모드·사이드로딩 보안 설정은 변경하지 않았다. 검증 과정에서의 Windows 등록 실패는 로컬 미서명 개발 패키지에 대한 결과이며, Store가 서명한 최종 앱의 설치 결과가 아니다.

당시 검증 패키지: `out/store/validation/LogGenerator-1.0.0.0-x64.msix` (1,260,302 bytes).

SHA-256: `B5B7409781ADB8DCB8627B0EBC26A8600D8EB54AE723D2859454549E1C71CCE5`.

당시 동일 디렉터리에 `.msix.report.json` 및 `.msix.wack.xml`을 생성했다. 같은 경로에 다시 패키징하면 파일과 보고서가 교체될 수 있다. 생성 패키지와 로컬 보고서는 Git에서 제외되며 이 과거 기록은 위 해시의 산출물에만 적용된다.

## GitHub 반영

기존 `scripts/publish.ps1`는 검증이 끝난 후 커밋하고 `origin HEAD`로 push한다. Store 검증을 포함하려면 `-StoreValidation`, 실제 설정으로 패키징하려면 `-StoreConfig`를 사용한다. 이 스크립트 실행은 GitHub에 코드를 반영하며 Microsoft Store 업로드는 수행하지 않는다. 로컬 Store 설정, 인증서 개인키 파일 및 생성 패키지는 Git에서 제외한다.

```powershell
.\scripts\publish.ps1 -Message 'Prepare Microsoft Store MSIX distribution' -StoreValidation
```
