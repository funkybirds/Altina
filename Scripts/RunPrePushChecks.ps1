param(
    [int]$Count = 20
)

Write-Host "Running local pre-push checks (commit-msg)..."
& "$PSScriptRoot\CheckCommitMessages.ps1" $Count
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Running quick build+test checks (AltinaEngineTests)..."
& "$PSScriptRoot\BuildEngine.ps1" -Target AltinaEngineTests
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$testExe = Join-Path $PSScriptRoot "..\out\build\windows-msvc-relwithdebinfo\Test\AltinaEngineTests.exe"
if (Test-Path $testExe) {
    Write-Host "Running tests..."
    & $testExe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

exit 0
