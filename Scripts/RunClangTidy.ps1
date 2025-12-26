<#
Run clang-tidy over project source files using the build compile_commands.json if available.

Usage:
  .\RunClangTidy.ps1                    # auto-discover clang-tidy, use out/build as -p, no fix
  .\RunClangTidy.ps1 -Fix -Jobs 8       # run with auto-fixes and 8 parallel jobs
  .\RunClangTidy.ps1 -ClangTidyPath "C:\LLVM\bin\clang-tidy.exe" -BuildPath out/build
#>

param(
    [string]$ClangTidyPath,
    [string]$BuildPath = "out/build",
    [int]$Jobs = 4,
    [switch]$Fix,
    [string]$Checks = ""
)

function Find-ClangTidy {
    if ($ClangTidyPath) { return $ClangTidyPath }
    $cmd = Get-Command clang-tidy -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Path }
    $cmd = Get-Command clang-tidy.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Path }
    return $null
}

$ct = Find-ClangTidy
if (-not $ct) {
    Write-Error "clang-tidy not found in PATH. Install clang-tidy or pass -ClangTidyPath."
    exit 1
}

Write-Host "Using clang-tidy: $ct"

# Gather source files (prefer git tracked files)
$files = @()
if (Get-Command git -ErrorAction SilentlyContinue) {
    try {
        $out = git ls-files -- "*.c" "*.cc" "*.cpp" "*.cxx" 2>$null
        if ($out) { $files = $out -split "\r?\n" | Where-Object { $_ -ne "" } }
    } catch { $files = @() }
}

if ($files.Count -eq 0) {
    Write-Host "git not available or no tracked sources found — scanning Source/ recursively"
    $files = Get-ChildItem -Path Source -Recurse -Include *.c,*.cc,*.cpp,*.cxx -File | ForEach-Object { $_.FullName }
}

if ($files.Count -eq 0) {
    Write-Host "No C/C++ source files found to analyze."
    exit 0
}

# Build -p path if the user provided a build path and compile_commands.json exists
$pArgs = @()
if ($BuildPath) {
    $cc = Join-Path $BuildPath "compile_commands.json"
    if (Test-Path $cc) { $pArgs += '-p'; $pArgs += $BuildPath }
    else { Write-Host "No compile_commands.json found at $cc — clang-tidy will run without a compile DB (may produce spurious results)" }
}

# Build the clang-tidy invocation
$baseArgs = @()
if ($Checks -ne "") { $baseArgs += ("--checks=$Checks") }
if ($Fix.IsPresent) { $baseArgs += "--fix" }
if ($pArg -ne "") { $baseArgs += $pArg }

Write-Host "Running clang-tidy on $($files.Count) files..."

# Prefer run-clang-tidy.py for parallel execution if available
$runScript = Get-Command run-clang-tidy.py -ErrorAction SilentlyContinue
if ($runScript) {
    Write-Host "Found run-clang-tidy.py; using it for parallel execution (jobs=$Jobs)."
    $py = $runScript.Path
    $runArgs = @()
    $runArgs += ("-j" + $Jobs.ToString())
    if ($BuildPath) { $runArgs += ('-p'); $runArgs += $BuildPath }
    if ($Checks -ne "") { $runArgs += ("--clang-tidy-args=--checks=$Checks") }
    if ($Fix.IsPresent) { $runArgs += "--fix" }
    & $py @runArgs
} else {
    # Fallback: invoke clang-tidy serially per-file (clang-tidy doesn't support -j)
    foreach ($f in $files) {
        $args = @($f) + $baseArgs + $pArgs
        & $ct @args
    }
}

Write-Host "clang-tidy run complete. Review fixes if -Fix was used."
