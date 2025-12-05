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
        $rc = $LASTEXITCODE
    }
}

if ($rc -ne 0) {
    Write-Error "Tests failed (exit $rc). Aborting push."
    exit $rc
}

Write-Host "All pre-push checks passed."
exit 0
