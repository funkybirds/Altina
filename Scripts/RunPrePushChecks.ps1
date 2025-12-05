param(
    [int]$Count = 20
)

Write-Host "Running local pre-push checks (commit-msg)..."
& "$PSScriptRoot\CheckCommitMessages.ps1" $Count
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Running quick build+test checks (AltinaEngineTests)..."
& "$PSScriptRoot\BuildEngine.ps1" -Target AltinaEngineTests
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Running tests via ctest (RelWithDebInfo)..."
try {
    & "ctest" -C RelWithDebInfo --output-on-failure
    $rc = $LASTEXITCODE
} catch {
    $rc = 1
}
if ($rc -ne 0) {
    Write-Warning "ctest reported failures (exit $rc). Falling back to test exe..."
    $testExe = Join-Path $PSScriptRoot "..\out\build\windows-msvc-relwithdebinfo\Source\Tests\AltinaEngineTests.exe"
    if (Test-Path $testExe) {
        Write-Host "Running tests (exe fallback)..."
        & $testExe
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } else {
        exit $rc
    }
}

exit 0
