param([string]$Exe = 'bin/rechan.exe', [string]$Python = 'python', [switch]$EngineIntegration)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Push-Location $Root
try {
    & $Python scripts/validate-localization.py
    if ($LASTEXITCODE -ne 0) { throw 'Catalog validation failed.' }
    & $Python -m unittest discover -s tests -p test_localization.py
    if ($LASTEXITCODE -ne 0) { throw 'Catalog ordering regression tests failed.' }
    & $Python -m unittest discover -s tests -p test_localization_hashes.py
    if ($LASTEXITCODE -ne 0) { throw 'Localization hash tests failed.' }
    & build/tests/support-tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Subtitle/prompt tests failed. Generate with premake5 --tests vs2022 and build Release first.' }
    & build/tests/gamepad-tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Virtual-controller tests failed. Disconnect physical controllers for this isolated test.' }
    $Output = Join-Path $Root 'build/tests/native'
    New-Item -ItemType Directory -Force $Output | Out-Null
    $Executable = (Resolve-Path $Exe).Path
    $Test = Start-Process -FilePath $Executable -ArgumentList "--test-localization `"$Output`"" -WorkingDirectory (Join-Path $Root 'res') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $Output 'stdout.txt') -RedirectStandardError (Join-Path $Output 'stderr.txt')
    if (-not $Test.WaitForExit(15000)) { $Test.Kill(); throw 'Native settings test timed out.' }
    Get-Content (Join-Path $Output 'stdout.txt'), (Join-Path $Output 'stderr.txt')
    if ($Test.ExitCode -ne 0) { throw "Native settings test failed: $($Test.ExitCode)" }
    if ($EngineIntegration) {
        $EngineOutput = Join-Path $Output ('engine-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Force $EngineOutput | Out-Null
        $EngineTest = Start-Process -FilePath $Executable -ArgumentList "--test-engine-integration `"$EngineOutput`"" -WorkingDirectory (Join-Path $Root 'res') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $EngineOutput 'stdout.txt') -RedirectStandardError (Join-Path $EngineOutput 'stderr.txt')
        if (-not $EngineTest.WaitForExit(15000)) { $EngineTest.Kill(); throw 'Engine integration test timed out; generate with --tests.' }
        Get-Content (Join-Path $EngineOutput 'stdout.txt'), (Join-Path $EngineOutput 'stderr.txt')
        if ($EngineTest.ExitCode -ne 0) { throw "Engine integration test failed: $($EngineTest.ExitCode)" }
    }
}
finally { Pop-Location }
