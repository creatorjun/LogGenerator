# tests/store_sample_exclusion_tests.ps1
param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Preflight = Join-Path $ProjectRoot 'scripts\test-store-package.ps1'
$SourcePackage = (Resolve-Path -LiteralPath $PackagePath).Path
$FixtureDirectory = Join-Path ([IO.Path]::GetTempPath()) ('LogGeneratorStoreSamples_' + [guid]::NewGuid().ToString('N'))
$FixtureFiles = [Collections.Generic.List[string]]::new()
$EmptyCatalog = '{"schema_version":1,"logs":[]}'
$Cases = @(
    @{ Name = 'empty'; Catalog = $EmptyCatalog; ExtraFile = ''; Reject = $false },
    @{ Name = 'populated'; Catalog = '{"schema_version":1,"logs":[{"sample":"private fixture"}]}'; ExtraFile = ''; Reject = $true },
    @{ Name = 'extra-field'; Catalog = '{"schema_version":1,"logs":[],"private":"fixture"}'; ExtraFile = ''; Reject = $true },
    @{ Name = 'duplicate-logs'; Catalog = '{"schema_version":1,"logs":[{"sample":"private fixture"}],"logs":[]}'; ExtraFile = ''; Reject = $true },
    @{ Name = 'invalid-schema'; Catalog = '{"schema_version":2,"logs":[]}'; ExtraFile = ''; Reject = $true },
    @{ Name = 'object-logs'; Catalog = '{"schema_version":1,"logs":{}}'; ExtraFile = ''; Reject = $true },
    @{ Name = 'extra-csv'; Catalog = $EmptyCatalog; ExtraFile = 'private.csv'; Reject = $true },
    @{ Name = 'metadata-csv'; Catalog = $EmptyCatalog; ExtraFile = 'AppxMetadata/private.csv'; Reject = $true }
)
New-Item -ItemType Directory -Path $FixtureDirectory | Out-Null
try {
    foreach ($Case in $Cases) {
        $FixturePath = Join-Path $FixtureDirectory ($Case.Name + '.msix')
        $FixtureFiles.Add($FixturePath)
        Copy-Item -LiteralPath $SourcePackage -Destination $FixturePath
        $Archive = [IO.Compression.ZipFile]::Open($FixturePath, [IO.Compression.ZipArchiveMode]::Update)
        try {
            $CatalogEntries = @($Archive.Entries | Where-Object { [Uri]::UnescapeDataString($_.FullName).Replace('\', '/') -ceq 'Sample Logs/sample_logs.json' })
            if ($CatalogEntries.Count -ne 1) { throw 'The baseline package must contain one sample catalog entry.' }
            $CatalogEntryName = $CatalogEntries[0].FullName
            $CatalogEntries[0].Delete()
            $CatalogEntry = $Archive.CreateEntry($CatalogEntryName)
            $Writer = [IO.StreamWriter]::new($CatalogEntry.Open(), [Text.UTF8Encoding]::new($false))
            try { $Writer.Write($Case.Catalog) } finally { $Writer.Dispose() }
            if ($Case.ExtraFile) {
                $ExtraEntry = $Archive.CreateEntry($Case.ExtraFile)
                $Writer = [IO.StreamWriter]::new($ExtraEntry.Open(), [Text.UTF8Encoding]::new($false))
                try { $Writer.Write('private fixture') } finally { $Writer.Dispose() }
            }
        } finally { $Archive.Dispose() }
        $Rejected = $false
        try {
            & $Preflight -PackagePath $FixturePath | Out-Null
        } catch {
            $Rejected = $true
            if (-not $Case.Reject) { throw }
            if ($_.Exception.Message -notmatch 'canonical empty catalog|Unexpected payload file') { throw }
        }
        if ($Rejected -ne $Case.Reject) { throw "Unexpected preflight result: $($Case.Name)" }
    }
    Write-Host "Store sample exclusion regressions passed: $($Cases.Count) fixtures."
} finally {
    foreach ($FixturePath in $FixtureFiles) {
        if (Test-Path -LiteralPath $FixturePath) { Remove-Item -LiteralPath $FixturePath -Force }
    }
    Remove-Item -LiteralPath $FixtureDirectory -Force
}
