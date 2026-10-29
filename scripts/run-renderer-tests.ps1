param([string]$Exe = 'bin/rechan.exe')
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Executable = (Resolve-Path (Join-Path $Root $Exe)).Path
$Output = Join-Path $Root 'build/tests/opengl'
New-Item -ItemType Directory -Force $Output | Out-Null
$Test = Start-Process -FilePath $Executable -ArgumentList '--test-opengl' -WorkingDirectory (Join-Path $Root 'res') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $Output 'stdout.txt') -RedirectStandardError (Join-Path $Output 'stderr.txt')
if (-not $Test.WaitForExit(20000)) { $Test.Kill(); throw 'OpenGL tests timed out; generate with premake5 --tests vs2022.' }
Get-Content (Join-Path $Output 'stdout.txt'), (Join-Path $Output 'stderr.txt')
if ($Test.ExitCode -ne 0) { throw "OpenGL tests failed: $($Test.ExitCode)" }
