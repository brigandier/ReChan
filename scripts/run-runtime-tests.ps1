param([string]$Exe = 'bin/rechan.exe')
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Push-Location $Root
try {
    $Executable = (Resolve-Path $Exe).Path
    $Output = Join-Path $Root 'build/tests/runtime'
    New-Item -ItemType Directory -Force $Output | Out-Null
    $Test = Start-Process -FilePath $Executable -ArgumentList "--test-runtime `"$Output`"" -WorkingDirectory (Join-Path $Root 'res') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $Output 'stdout.txt') -RedirectStandardError (Join-Path $Output 'stderr.txt')
    if (-not $Test.WaitForExit(20000)) { $Test.Kill(); throw 'Runtime tests timed out. Build with premake5 --tests vs2022 first.' }
    Get-Content (Join-Path $Output 'stdout.txt'), (Join-Path $Output 'stderr.txt')
    if ($Test.ExitCode -ne 0) { throw "Runtime tests failed: $($Test.ExitCode)" }
}
finally { Pop-Location }
