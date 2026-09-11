# scripts/publish.ps1
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Message,
    [string]$StoreConfig,
    [switch]$StoreValidation
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot

function Invoke-GitChecked {
    param([string[]]$ArgumentList)
    & git -C $ProjectRoot @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "Git operation failed with exit code $LASTEXITCODE"
    }
}

if ($StoreConfig -and $StoreValidation) {
    throw 'Choose either StoreConfig or StoreValidation.'
}
if ($StoreConfig) {
    & (Join-Path $PSScriptRoot 'package-store.ps1') -ConfigPath $StoreConfig
} elseif ($StoreValidation) {
    & (Join-Path $PSScriptRoot 'package-store.ps1') -ValidationOnly
} else {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release
}
Invoke-GitChecked @('add', '--all')
git -C $ProjectRoot diff --cached --quiet
if ($LASTEXITCODE -eq 0) {
    throw 'There are no staged changes to publish.'
}
if ($LASTEXITCODE -ne 1) {
    throw 'Unable to inspect staged changes.'
}
Invoke-GitChecked @('commit', '-m', $Message)
Invoke-GitChecked @('push', 'origin', 'HEAD')
