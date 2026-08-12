# scripts/publish.ps1
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Message
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot

& (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release
git -C $ProjectRoot add --all
git -C $ProjectRoot diff --cached --quiet
if ($LASTEXITCODE -eq 0) {
    throw 'There are no staged changes to publish.'
}
git -C $ProjectRoot commit -m $Message
git -C $ProjectRoot push origin HEAD
