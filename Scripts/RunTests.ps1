param(
    [string]$Preset = "windows-msvc-relwithdebinfo",
    [switch]$BuildBeforeRun,
    [switch]$ForceConfigure
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

function Get-CtestConfigFromPreset($presetName) {
    if (-not $presetName) { return 'RelWithDebInfo' }
    $tokens = $presetName -split '[-_.]'
    if ($tokens.Length -eq 0) { return 'RelWithDebInfo' }
    $candidate = $tokens[-1].ToLowerInvariant()
    switch ($candidate) {
        'debug' { return 'Debug' }
        'release' { return 'Release' }
        'relwithdebinfo' { return 'RelWithDebInfo' }
        'minsizerel' { return 'MinSizeRel' }
        default { return ($candidate.Substring(0,1).ToUpper() + $candidate.Substring(1)) }
    }
}


$buildDir = Join-Path $repoRoot "out\build\$Preset"
$ctestConfig = Get-CtestConfigFromPreset $Preset
Write-Host "Running tests for preset '$Preset' (ctest config: $ctestConfig)"

if ($BuildBeforeRun.IsPresent) {
    Write-Host "Building test target via BuildEngine.ps1..."
    & "$PSScriptRoot\BuildEngine.ps1" -Preset $Preset -Target AltinaEngineTests -ForceConfigure:$ForceConfigure
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Invoking: ctest -C $ctestConfig --output-on-failure (from build dir: $buildDir)"
$testBuildDir = Join-Path $buildDir "Source\Tests"
if (Test-Path $testBuildDir) {
    Push-Location $testBuildDir
    try {
        & ctest -C $ctestConfig --output-on-failure
        $rc = $LASTEXITCODE
    } catch {
        $rc = 1
    }
    Pop-Location
} elseif (Test-Path $buildDir) {
    Push-Location $buildDir
    try {
        & ctest -C $ctestConfig --output-on-failure
        $rc = $LASTEXITCODE
    } catch {
        $rc = 1
    }
    Pop-Location
} else {
    Write-Warning "Build directory not found: $buildDir. Attempting to run ctest from repo root."
    try {
        & ctest -C $ctestConfig --output-on-failure
        $rc = $LASTEXITCODE
    } catch {
        $rc = 1
    }
}

if ($rc -ne 0) {
    Write-Warning "ctest failed or returned non-zero. Falling back to test exe."
    $testExe = Join-Path $buildDir "Source\Tests\AltinaEngineTests.exe"
    if (Test-Path $testExe) {
        Write-Host "Running test exe: $testExe"
        & $testExe
        $rc = $LASTEXITCODE
    } else {
        Write-Error "Test executable not found at: $testExe"
        exit 2
    }
}

exit $rc
