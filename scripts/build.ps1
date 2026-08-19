# scripts/build.ps1
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('Desktop', 'Cli')]
    [string]$Mode = 'Desktop',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildName = if ($Mode -eq 'Cli') { 'build-windows-cli' } else { 'build' }
$BuildGui = if ($Mode -eq 'Cli') { 'OFF' } else { 'ON' }
$BuildDirectory = Join-Path $ProjectRoot $BuildName

if ($Clean -and (Test-Path -LiteralPath $BuildDirectory)) {
    Remove-Item -LiteralPath $BuildDirectory -Recurse -Force -Confirm:$false
}

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

Invoke-Checked -FilePath 'cmake' -ArgumentList @('-S', $ProjectRoot, '-B', $BuildDirectory, '-G', 'Visual Studio 18 2026', '-A', 'x64', "-DLOGGEN_BUILD_GUI=$BuildGui")
Invoke-Checked -FilePath 'cmake' -ArgumentList @('--build', $BuildDirectory, '--config', $Configuration, '--parallel')
Invoke-Checked -FilePath 'ctest' -ArgumentList @('--test-dir', $BuildDirectory, '-C', $Configuration, '--output-on-failure')
