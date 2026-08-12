# scripts/build.ps1
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDirectory = Join-Path $ProjectRoot 'build'

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

Invoke-Checked -FilePath 'cmake' -ArgumentList @('-S', $ProjectRoot, '-B', $BuildDirectory, '-G', 'Visual Studio 18 2026', '-A', 'x64')
Invoke-Checked -FilePath 'cmake' -ArgumentList @('--build', $BuildDirectory, '--config', $Configuration, '--parallel')
Invoke-Checked -FilePath 'ctest' -ArgumentList @('--test-dir', $BuildDirectory, '-C', $Configuration, '--output-on-failure')
