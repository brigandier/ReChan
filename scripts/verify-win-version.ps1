param([string]$Exe = 'bin/rechan.exe')
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Header = Get-Content -LiteralPath (Join-Path $Root 'src/extra/versionnumber.h') -Raw
$Parts = foreach ($Part in @('MAJOR', 'MINOR', 'PATCH')) {
    $Match = [regex]::Match($Header, "#define RECHAN_VERSION_$Part\s+(\d+)")
    if (-not $Match.Success) { throw "Missing numeric version component: $Part" }
    $Match.Groups[1].Value
}
$Version = $Parts -join '.'
$Info = (Get-Item -LiteralPath $Exe).VersionInfo
$Expected = @{
    CompanyName = 'SilverwireGames and contributors'
    FileDescription = 'ReChan - Jackie Chan: Stuntmaster (PC)'
    FileVersion = "$Version.0"
    InternalName = 'rechan'
    OriginalFilename = 'rechan.exe'
    ProductName = 'ReChan'
    ProductVersion = "$Version (experimental)"
    LegalCopyright = 'Copyright (c) 2026 _AG'
    PrivateBuild = 'Experimental integration build'
}
foreach ($Key in $Expected.Keys) {
    if ($Info.$Key -ne $Expected[$Key]) { throw "Invalid metadata [$Key]: '$($Info.$Key)'" }
}
if (-not $Info.IsPreRelease -or -not $Info.IsPrivateBuild) { throw 'Experimental version flags are missing.' }
if ($Info.Language -notmatch 'Portug') { throw "Unexpected resource language: $($Info.Language)" }
if ($Info.FileMajorPart -ne $Parts[0] -or $Info.FileMinorPart -ne $Parts[1] -or $Info.FileBuildPart -ne $Parts[2]) {
    throw 'Numeric and string file versions differ.'
}
Write-Host "PASS Windows metadata: $($Info.FileVersion), $($Info.ProductVersion), $($Info.Language), prerelease/private build"
