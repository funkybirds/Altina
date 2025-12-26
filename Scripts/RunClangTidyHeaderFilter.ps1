param(
    [string]$BuildPath = "out/build/windows-msvc-relwithdebinfo"
)

$ctCmd = Get-Command clang-tidy -ErrorAction SilentlyContinue
if (-not $ctCmd) { Write-Error "clang-tidy not found in PATH"; exit 1 }
$ct = $ctCmd.Path

$files = @()
try {
    $out = git ls-files -- "*.c" "*.cc" "*.cpp" "*.cxx" 2>$null
    if ($out) { $files = $out -split "\r?\n" | Where-Object { $_ -ne "" } }
} catch { }

if ($files.Count -eq 0) {
    $files = Get-ChildItem -Path Source -Recurse -Include *.c,*.cc,*.cpp,*.cxx -File | ForEach-Object { $_.FullName }
}

if ($files.Count -eq 0) { Write-Host "No C/C++ source files found to analyze."; exit 0 }

Write-Host "Running clang-tidy with --header-filter='.*' and -p $BuildPath on $($files.Count) files..."
foreach ($f in $files) {
    Write-Host "-- $f"
    & $ct --header-filter='.*' -p $BuildPath $f
}

Write-Host "clang-tidy header-filter run complete."