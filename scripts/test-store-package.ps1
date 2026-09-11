# scripts/test-store-package.ps1
param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath,
    [string]$ExpectedIdentityName,
    [string]$ExpectedPublisher
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Drawing

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Read-EntryBytes {
    param([System.IO.Compression.ZipArchiveEntry]$Entry)
    Assert-Condition ($Entry.Length -le 100MB) "Unexpectedly large package entry: $($Entry.FullName)"
    $Stream = $Entry.Open()
    $Memory = [IO.MemoryStream]::new()
    try {
        $Stream.CopyTo($Memory)
        return ,$Memory.ToArray()
    } finally {
        $Memory.Dispose()
        $Stream.Dispose()
    }
}

$ResolvedPackage = (Resolve-Path -LiteralPath $PackagePath).Path
$Archive = [IO.Compression.ZipFile]::OpenRead($ResolvedPackage)
try {
    $Entries = @{}
    foreach ($Entry in $Archive.Entries) {
        $Name = [Uri]::UnescapeDataString($Entry.FullName).Replace('\', '/')
        Assert-Condition (-not $Entries.ContainsKey($Name)) "Duplicate package entry: $Name"
        Assert-Condition ($Name -cmatch '^[\x20-\x7e]+$' -and $Name -notmatch '(^|/)\.\.(/|$)' -and -not $Name.StartsWith('/')) "Invalid package entry: $Name"
        $Entries[$Name] = $Entry
    }
    $Required = @('AppxManifest.xml', 'AppxBlockMap.xml', '[Content_Types].xml', 'resources.pri', 'LogGenerator.exe', 'LogGeneratorCli.exe', 'Sample Logs/sample_logs.json', 'Licenses/DearImGui.txt', 'Licenses/nlohmann-json.txt', 'Assets/StoreLogo.png', 'Assets/Square44x44Logo.png', 'Assets/Square150x150Logo.png')
    foreach ($Name in $Required) {
        Assert-Condition ($Entries.ContainsKey($Name)) "Required package entry is missing: $Name"
        Assert-Condition ($Entries[$Name].Length -gt 0) "Required package entry is empty: $Name"
    }
    foreach ($Name in $Entries.Keys) {
        Assert-Condition ($Name -cin $Required -or $Name -ceq 'AppxSignature.p7x' -or $Name -ceq 'AppxMetadata/CodeIntegrity.cat') "Unexpected payload file: $Name"
    }
    [xml]$Manifest = [Text.Encoding]::UTF8.GetString((Read-EntryBytes $Entries['AppxManifest.xml']))
    $Ns = [Xml.XmlNamespaceManager]::new($Manifest.NameTable)
    $Ns.AddNamespace('p', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $Ns.AddNamespace('r', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities')
    $Ns.AddNamespace('u3', 'http://schemas.microsoft.com/appx/manifest/uap/windows10/3')
    $Identity = $Manifest.SelectSingleNode('/p:Package/p:Identity', $Ns)
    Assert-Condition ($null -ne $Identity -and $Identity.ProcessorArchitecture -ceq 'x64') 'Package identity must target x64.'
    Assert-Condition ($Identity.Version -match '^\d+\.\d+\.\d+\.0$') 'Store revision must be 0.'
    if ($ExpectedIdentityName) { Assert-Condition ($Identity.Name -ceq $ExpectedIdentityName) 'Package identity does not match the requested name.' }
    if ($ExpectedPublisher) { Assert-Condition ($Identity.Publisher -ceq $ExpectedPublisher) 'Package publisher does not match the requested publisher.' }
    $Family = $Manifest.SelectSingleNode('/p:Package/p:Dependencies/p:TargetDeviceFamily', $Ns)
    Assert-Condition ($Family.Name -ceq 'Windows.Desktop' -and $Family.MinVersion -ceq '10.0.19041.0') 'Unexpected Windows desktop minimum version.'
    Assert-Condition ([version]$Family.MaxVersionTested -ge [version]$Family.MinVersion) 'MaxVersionTested is below MinVersion.'
    $Applications = $Manifest.SelectNodes('/p:Package/p:Applications/p:Application', $Ns)
    Assert-Condition ($Applications.Count -eq 1 -and $Applications[0].Executable -ceq 'LogGenerator.exe' -and $Applications[0].EntryPoint -ceq 'Windows.FullTrustApplication') 'The desktop entry point is invalid.'
    $Capabilities = $Manifest.SelectNodes('/p:Package/p:Capabilities/*', $Ns)
    Assert-Condition ($Capabilities.Count -eq 1 -and $Capabilities[0].LocalName -ceq 'Capability' -and $Capabilities[0].NamespaceURI -ceq $Ns.LookupNamespace('r') -and $Capabilities[0].GetAttribute('Name') -ceq 'runFullTrust') 'Only runFullTrust is required by this desktop package.'
    $Alias = $Manifest.SelectSingleNode('//u3:Extension[@Category="windows.appExecutionAlias"]', $Ns)
    Assert-Condition ($null -ne $Alias -and $Alias.Executable -ceq 'LogGeneratorCli.exe') 'The CLI execution alias is missing.'
    [xml]$BlockMap = [Text.Encoding]::UTF8.GetString((Read-EntryBytes $Entries['AppxBlockMap.xml']))
    Assert-Condition ($BlockMap.DocumentElement.GetAttribute('HashMethod') -ceq 'http://www.w3.org/2001/04/xmlenc#sha256') 'Package block map must use SHA256.'

    foreach ($Executable in @('LogGenerator.exe', 'LogGeneratorCli.exe')) {
        $Bytes = Read-EntryBytes $Entries[$Executable]
        Assert-Condition ($Bytes.Length -ge 256 -and [BitConverter]::ToUInt16($Bytes, 0) -eq 0x5a4d) "Invalid executable: $Executable"
        $PeOffset = [BitConverter]::ToInt32($Bytes, 0x3c)
        Assert-Condition ($PeOffset -ge 64 -and $PeOffset + 96 -lt $Bytes.Length) "Invalid PE header: $Executable"
        Assert-Condition ([BitConverter]::ToUInt32($Bytes, $PeOffset) -eq 0x00004550 -and [BitConverter]::ToUInt16($Bytes, $PeOffset + 4) -eq 0x8664) "Executable must be Windows x64: $Executable"
        Assert-Condition ([BitConverter]::ToUInt16($Bytes, $PeOffset + 24) -eq 0x20b) "Executable must be PE32+: $Executable"
        $DllFlags = [BitConverter]::ToUInt16($Bytes, $PeOffset + 24 + 70)
        Assert-Condition (($DllFlags -band 0x4160) -eq 0x4160) "ASLR, high-entropy ASLR, DEP or CFG is missing: $Executable"
    }
    foreach ($Asset in @(@{Name='StoreLogo'; Size=50}, @{Name='Square44x44Logo'; Size=44}, @{Name='Square150x150Logo'; Size=150})) {
        $Stream = $Entries["Assets/$($Asset.Name).png"].Open()
        try {
            $Image = [Drawing.Image]::FromStream($Stream)
            try {
                Assert-Condition ($Image.Width -eq $Asset.Size -and $Image.Height -eq $Asset.Size) "Invalid logo dimensions: $($Asset.Name)"
            } finally { $Image.Dispose() }
        } finally { $Stream.Dispose() }
    }
    $CatalogText = [Text.UTF8Encoding]::new($false, $true).GetString((Read-EntryBytes $Entries['Sample Logs/sample_logs.json']))
    Assert-Condition ($CatalogText -ceq '{"schema_version":1,"logs":[]}') 'The Store catalog must contain only the canonical empty catalog, without sample logs, extra fields or duplicate keys.'
    $Catalog = $CatalogText | ConvertFrom-Json
    $CatalogFields = @($Catalog.PSObject.Properties.Name)
    Assert-Condition ($CatalogFields.Count -eq 2 -and $CatalogFields -ccontains 'schema_version' -and $CatalogFields -ccontains 'logs') 'The Store catalog must contain only schema_version and logs.'
    Assert-Condition (($Catalog.schema_version -is [int] -or $Catalog.schema_version -is [long]) -and $Catalog.schema_version -eq 1) 'The Store catalog schema_version must be integer 1.'
    Assert-Condition ($Catalog.logs -is [array] -and $Catalog.logs.Count -eq 0) 'The Store package must contain zero sample logs.'
    Write-Host 'MSIX preflight passed: zero sample logs, exact payload allowlist, identity, manifest, SHA256 block map, x64 executable protections and PNG dimensions.'
    Write-Host 'This check does not replace installed application testing or Windows App Certification Kit certification.'
} finally {
    $Archive.Dispose()
}
