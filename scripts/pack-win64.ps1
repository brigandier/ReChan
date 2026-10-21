param([string]$Root, [switch]$Experimental)

$ErrorActionPreference = 'Stop'

if (-not $Root) { $Root = "$PSScriptRoot\.." }
$Root     = (Resolve-Path $Root).Path
$Exe      = Join-Path $Root "bin\rechan.exe"
$ResDir   = Join-Path $Root "res"
$OutDir   = Join-Path $Root "shipping"
$OutZip   = Join-Path $OutDir "rechan-win64.zip"
if ($Experimental) { $OutZip = Join-Path $OutDir "rechan-win64-experimental.zip" }
$BuildRoot = [IO.Path]::GetFullPath((Join-Path $Root 'build'))
$TmpDir   = [IO.Path]::GetFullPath((Join-Path $BuildRoot ("package-win64-" + [guid]::NewGuid().ToString('N'))))
if (-not $TmpDir.StartsWith($BuildRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Package staging directory must be inside the build directory.'
}
$Stage    = Join-Path $TmpDir "rechan-win64"

New-Item -ItemType Directory -Force $Stage | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null

$hash = (Get-FileHash $Exe -Algorithm SHA256).Hash.ToLower()
"$hash  rechan.exe" | Set-Content -Encoding UTF8 (Join-Path $Stage "SHA256SUM.txt")

Copy-Item $Exe (Join-Path $Stage "rechan.exe")
Copy-Item (Join-Path $ResDir "pc") (Join-Path $Stage "pc") -Recurse
Copy-Item (Join-Path $Root 'LICENSE') (Join-Path $Stage 'LICENSE')
Copy-Item (Join-Path $Root 'README.md') (Join-Path $Stage 'README.md')
$Notices = Join-Path $Stage 'licenses'
New-Item -ItemType Directory -Force $Notices | Out-Null
Copy-Item (Join-Path $Root 'vendor/libp3d/vendor/sdl2/LICENSE.txt') (Join-Path $Notices 'SDL2.txt')
Copy-Item (Join-Path $Root 'vendor/libp3d/vendor/glfw/LICENSE.md') (Join-Path $Notices 'GLFW.md')

if (Test-Path -LiteralPath $OutZip) { Remove-Item -LiteralPath $OutZip }
Compress-Archive -Path "$Stage\*" -DestinationPath $OutZip

Remove-Item -LiteralPath $TmpDir -Recurse -Force
Write-Host "Packed: $OutZip"
