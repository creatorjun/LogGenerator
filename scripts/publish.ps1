# scripts/publish.ps1
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Message
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$ArgumentList
    )
    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

& (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release
Invoke-Checked -FilePath 'git' -ArgumentList @('-C', $ProjectRoot, 'add', '--all')
& git -C $ProjectRoot diff --cached --quiet
if ($LASTEXITCODE -eq 0) {
    throw 'There are no staged changes to publish.'
}
if ($LASTEXITCODE -ne 1) {
    throw "git diff failed with exit code $LASTEXITCODE"
}
Invoke-Checked -FilePath 'git' -ArgumentList @('-C', $ProjectRoot, 'commit', '-m', $Message)
Invoke-Checked -FilePath 'git' -ArgumentList @('-C', $ProjectRoot, 'push', 'origin', 'HEAD')
