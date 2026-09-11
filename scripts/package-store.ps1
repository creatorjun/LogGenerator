# scripts/package-store.ps1
[CmdletBinding(DefaultParameterSetName = 'Submission')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Submission')]
    [ValidateNotNullOrEmpty()]
    [string]$ConfigPath,
    [Parameter(Mandatory = $true, ParameterSetName = 'Validation')]
    [switch]$ValidationOnly,
    [Parameter(ParameterSetName = 'Submission')]
    [switch]$DraftUpload,
    [switch]$RunWack,
    [ValidatePattern('^[A-Fa-f0-9]{40}$')]
    [string]$CertificateThumbprint
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDirectory = Join-Path $ProjectRoot 'build-store'

function Invoke-Checked {
    param([string]$FilePath, [string[]]$ArgumentList)
    & $FilePath @ArgumentList | ForEach-Object { $_.ToString().Replace([string][char]0, '').TrimEnd("`r") }
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

if ($env:OS -ne 'Windows_NT') {
    throw 'Microsoft Store packaging requires Windows and the Windows SDK.'
}
if ($ValidationOnly) {
    $Config = @{
        IdentityName = 'LogGenerator.LocalValidation'
        Publisher = 'CN=LogGenerator Local Validation'
        PublisherDisplayName = 'Local validation only'
        DisplayName = 'LogGenerator Validation'
        Version = '1.0.0.0'
        PrivacyPolicyUrl = ''
    }
} else {
    $Config = Import-PowerShellDataFile -LiteralPath (Resolve-Path -LiteralPath $ConfigPath).Path
}
foreach ($Key in @('IdentityName', 'Publisher', 'PublisherDisplayName', 'DisplayName', 'Version')) {
    if (-not $Config.ContainsKey($Key) -or $Config[$Key] -isnot [string] -or [string]::IsNullOrWhiteSpace($Config[$Key])) {
        throw "Missing $Key. Copy its exact value from Partner Center into StoreConfig.local.psd1."
    }
    if ($Config[$Key] -ne $Config[$Key].Trim()) {
        throw "$Key must not have leading or trailing whitespace."
    }
}
if ($Config.IdentityName -notmatch '^[A-Za-z0-9.-]{3,50}$' -or $Config.IdentityName -match '^(?i)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$') {
    throw 'IdentityName is not a valid package identity.'
}
if (-not $ValidationOnly -and $Config.IdentityName -eq 'LogGenerator.LocalValidation') {
    throw 'The local validation identity cannot be used for a Store submission.'
}
if ($Config.Publisher -notmatch '^CN=') {
    throw 'Publisher must contain the exact Partner Center distinguished name, starting with CN=.'
}
if ($Config.Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+\.0$') {
    throw 'Store Version must have four numeric components and a final component of 0.'
}
$PackageVersion = [version]$Config.Version
if ($PackageVersion.Major -lt 1 -or $PackageVersion.Major -gt 65535 -or $PackageVersion.Minor -gt 65535 -or $PackageVersion.Build -gt 65535) {
    throw 'Store version components must fit in 16 bits and the major component must be nonzero.'
}
if (-not $Config.ContainsKey('PrivacyPolicyUrl') -or $Config.PrivacyPolicyUrl -isnot [string]) {
    throw 'PrivacyPolicyUrl must be a string in the Store configuration.'
}
if ([string]::IsNullOrEmpty($Config.PrivacyPolicyUrl)) {
    if (-not $ValidationOnly -and -not $DraftUpload) {
        throw 'PrivacyPolicyUrl must be the published HTTPS privacy policy URL before creating a submission package.'
    }
} elseif ($Config.PrivacyPolicyUrl -notmatch '^https://[A-Za-z0-9][A-Za-z0-9./_~%?&=+#:-]*$') {
    throw 'PrivacyPolicyUrl must be the published HTTPS privacy policy URL before creating a submission package.'
}
$WindowsBuild = [Environment]::OSVersion.Version.Build
if ($WindowsBuild -lt 19041) {
    throw 'Packaging and validation require Windows 10 build 19041 or later.'
}
$SdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
$SdkDirectory = Get-ChildItem -LiteralPath $SdkRoot -Directory |
    Where-Object { $_.Name -match '^10\.0\.[0-9]+\.0$' -and (Test-Path -LiteralPath (Join-Path $_.FullName 'x64\makeappx.exe')) } |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if ($null -eq $SdkDirectory) {
    throw 'Install the Windows SDK with MakeAppx, MakePri and SignTool.'
}
$SdkBin = Join-Path $SdkDirectory.FullName 'x64'
$MakeAppx = Join-Path $SdkBin 'makeappx.exe'
$MakePri = Join-Path $SdkBin 'makepri.exe'
$SignTool = Join-Path $SdkBin 'signtool.exe'
foreach ($Tool in @($MakeAppx, $MakePri)) {
    if (-not (Test-Path -LiteralPath $Tool)) { throw "Windows SDK tool is missing: $Tool" }
}
if ($CertificateThumbprint) {
    $Certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$CertificateThumbprint"
    if (-not $Certificate.HasPrivateKey -or $Certificate.Subject -cne $Config.Publisher -or $Certificate.NotAfter -le (Get-Date) -or $Certificate.NotBefore -gt (Get-Date)) {
        throw 'The signing certificate must have a private key, be valid, and match Publisher exactly.'
    }
}

Invoke-Checked 'cmake' @('-S', $ProjectRoot, '-B', $BuildDirectory, '-G', 'Visual Studio 18 2026', '-A', 'x64', '-DLOGGEN_BUILD_GUI=ON', '-DLOGGEN_STORE_BUILD=ON', '-DLOGGEN_ENABLE_AVX2=OFF', "-DLOGGEN_PRIVACY_POLICY_URL=$($Config.PrivacyPolicyUrl)")
Invoke-Checked 'cmake' @('--build', $BuildDirectory, '--config', 'Release', '--parallel')
Invoke-Checked 'ctest' @('--test-dir', $BuildDirectory, '-C', 'Release', '--output-on-failure')

$OutputKind = if ($ValidationOnly) { 'validation' } elseif ($DraftUpload) { 'draft' } else { 'submission' }
$OutputDirectory = Join-Path $ProjectRoot "out\store\$OutputKind"
$StageDirectory = Join-Path $BuildDirectory ("package\" + [guid]::NewGuid().ToString('N'))
$AssetDirectory = Join-Path $StageDirectory 'Assets'
$LicenseDirectory = Join-Path $StageDirectory 'Licenses'
New-Item -ItemType Directory -Force -Path $OutputDirectory, $AssetDirectory, $LicenseDirectory, (Join-Path $StageDirectory 'Sample Logs') | Out-Null
foreach ($Executable in @('LogGenerator.exe', 'LogGeneratorCli.exe')) {
    Copy-Item -LiteralPath (Join-Path $BuildDirectory "bin\Release\$Executable") -Destination $StageDirectory
}
[IO.File]::WriteAllText((Join-Path $StageDirectory 'Sample Logs\sample_logs.json'), '{"schema_version":1,"logs":[]}', [Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $BuildDirectory '_deps\imgui-src\LICENSE.txt') -Destination (Join-Path $LicenseDirectory 'DearImGui.txt')
Copy-Item -LiteralPath (Join-Path $BuildDirectory '_deps\nlohmann_json-src\LICENSE.MIT') -Destination (Join-Path $LicenseDirectory 'nlohmann-json.txt')

Add-Type -AssemblyName System.Drawing
$Icon = [System.Drawing.Icon]::new((Join-Path $ProjectRoot 'resources\windows\log.ico'), 256, 256)
try {
    $SourceBitmap = $Icon.ToBitmap()
    try {
        foreach ($Asset in @(@{Name='Square44x44Logo.png'; Size=44}, @{Name='Square150x150Logo.png'; Size=150}, @{Name='StoreLogo.png'; Size=50})) {
            $Bitmap = [System.Drawing.Bitmap]::new($Asset.Size, $Asset.Size)
            try {
                $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
                try {
                    $Graphics.Clear([System.Drawing.Color]::Transparent)
                    $Graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                    $Graphics.DrawImage($SourceBitmap, 0, 0, $Asset.Size, $Asset.Size)
                } finally { $Graphics.Dispose() }
                $Bitmap.Save((Join-Path $AssetDirectory $Asset.Name), [System.Drawing.Imaging.ImageFormat]::Png)
            } finally { $Bitmap.Dispose() }
        }
    } finally { $SourceBitmap.Dispose() }
} finally { $Icon.Dispose() }

$ManifestText = [IO.File]::ReadAllText((Join-Path $ProjectRoot 'packaging\windows\AppxManifest.xml.in'))
$ManifestValues = @{} + $Config
$ManifestValues['MaxVersionTested'] = "10.0.$WindowsBuild.0"
foreach ($Key in $ManifestValues.Keys) {
    $ManifestText = $ManifestText.Replace("@$Key@", [System.Security.SecurityElement]::Escape([string]$ManifestValues[$Key]))
}
if ($ManifestText -match '@[A-Za-z]+@') { throw 'The manifest contains unresolved placeholders.' }
$ManifestPath = Join-Path $StageDirectory 'AppxManifest.xml'
[IO.File]::WriteAllText($ManifestPath, $ManifestText, [Text.UTF8Encoding]::new($false))
$PriConfig = Join-Path $BuildDirectory 'priconfig.xml'
Invoke-Checked $MakePri @('createconfig', '/cf', $PriConfig, '/dq', 'ko-KR', '/o')
Invoke-Checked $MakePri @('new', '/pr', $StageDirectory, '/cf', $PriConfig, '/mn', $ManifestPath, '/of', (Join-Path $StageDirectory 'resources.pri'), '/o')

$PackagePath = Join-Path $OutputDirectory "LogGenerator-$($Config.Version)-x64.msix"
if (Test-Path -LiteralPath "$PackagePath.report.json") {
    Remove-Item -LiteralPath "$PackagePath.report.json" -Force
}
Invoke-Checked $MakeAppx @('pack', '/o', '/h', 'SHA256', '/d', $StageDirectory, '/p', $PackagePath)
if ($CertificateThumbprint) {
    Invoke-Checked $SignTool @('sign', '/fd', 'SHA256', '/sha1', $CertificateThumbprint, '/s', 'My', $PackagePath)
    Invoke-Checked $SignTool @('verify', '/pa', '/v', $PackagePath)
}
& (Join-Path $PSScriptRoot 'test-store-package.ps1') -PackagePath $PackagePath -ExpectedIdentityName $Config.IdentityName -ExpectedPublisher $Config.Publisher
$WackResult = 'Not run'
if ($RunWack) {
    $AppCert = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\App Certification Kit\appcert.exe'
    if (-not (Test-Path -LiteralPath $AppCert)) { throw 'Install Windows App Certification Kit to use RunWack.' }
    Invoke-Checked $AppCert @('reset')
    $WackPath = "$PackagePath.wack.xml"
    Invoke-Checked $AppCert @('test', '-appxpackagepath', $PackagePath, '-reportoutputpath', $WackPath)
    [xml]$WackReport = [IO.File]::ReadAllText($WackPath)
    $FailedChecks = @($WackReport.SelectNodes('//TEST/RESULT') | Where-Object { $_.InnerText -ne 'PASS' })
    if ($WackReport.REPORT.OVERALL_RESULT -ne 'PASS' -or $FailedChecks.Count -gt 0) {
        throw "Review the WACK report before submission: $WackPath ($($FailedChecks.Count) non-passing checks)."
    }
    $WackResult = "PASS (package inspection; verify installed behavior separately): $WackPath"
}
$Report = [ordered]@{
    PackagePath = $PackagePath
    Sha256 = (Get-FileHash -LiteralPath $PackagePath -Algorithm SHA256).Hash
    Mode = $OutputKind
    IdentityName = $Config.IdentityName
    Publisher = $Config.Publisher
    Version = $Config.Version
    SampleLogCount = 0
    StageDirectory = $StageDirectory
    WindowsBuild = $WindowsBuild
    WindowsSdk = $SdkDirectory.Name
    Signed = [bool]$CertificateThumbprint
    CTest = 'Passed'
    MakeAppxValidation = 'Passed'
    PackagePreflight = 'Passed'
    PackagedRuntime = 'Not tested by this script'
    WindowsAppCertificationKit = $WackResult
    PartnerCenterCertification = 'Not submitted'
    CreatedUtc = [DateTime]::UtcNow.ToString('o')
}
$Report | ConvertTo-Json | Set-Content -LiteralPath "$PackagePath.report.json" -Encoding UTF8
Write-Host "Package: $PackagePath"
Write-Host "Report: $PackagePath.report.json"
if ($ValidationOnly) {
    Write-Warning 'LOCAL VALIDATION ONLY: this identity is not registered in Partner Center. Do not submit this package.'
} elseif ($DraftUpload) {
    Write-Host 'Draft upload only; complete privacy/listing before certification.'
} else {
    Write-Host 'Confirm the Partner Center identity, Public/Hidden/Direct link only visibility, privacy URL, listing and WACK results before submission.'
}
