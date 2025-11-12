param(
    [string[]]$Args
)

Write-Host "Running pre-push checks..."

Write-Host "Checking recent commit messages (last 20)..."
try {
    & "${PSScriptRoot}\..\Scripts\CheckCommitMessages.ps1" 20
    $rc = $LASTEXITCODE
} catch {
    $rc = 1
}

if ($rc -ne 0) {
    Write-Error "Commit message checks failed. Aborting push."
    exit $rc
}

Write-Host "Building quick checks (core + tests)..."
try {
    & "$PSScriptRoot\..\Scripts\BuildEngine.ps1" -Target AltinaEngineTests
    $rc = $LASTEXITCODE
} catch {
    $rc = 1
}

if ($rc -ne 0) {
    Write-Error "Build failed. Aborting push."
    exit $rc
}

$testExe = Join-Path $PSScriptRoot "..\out\build\windows-msvc-relwithdebinfo\Test\AltinaEngineTests.exe"
if (Test-Path $testExe) {
    Write-Host "Running tests (fast)..."
    & $testExe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Tests failed (exit $LASTEXITCODE). Aborting push."
        exit $LASTEXITCODE
    }
}

Write-Host "All pre-push checks passed."
exit 0
