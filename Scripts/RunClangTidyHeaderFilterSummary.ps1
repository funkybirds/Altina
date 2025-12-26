param(
    [string]$BuildPath = "out/build/windows-msvc-relwithdebinfo",
    [string]$LogPath = "build/clang-tidy-header-filter.log"
)

$ctCmd = Get-Command clang-tidy -ErrorAction SilentlyContinue
if (-not $ctCmd) { Write-Error "clang-tidy not found in PATH"; exit 1 }
$ct = $ctCmd.Path

if (Test-Path $LogPath) { Remove-Item $LogPath }

$files = @()
try {
    $out = git ls-files -- "*.c" "*.cc" "*.cpp" "*.cxx" 2>$null
    if ($out) { $files = $out -split "\r?\n" | Where-Object { $_ -ne "" } }
} catch { }

if ($files.Count -eq 0) {
    $files = Get-ChildItem -Path Source -Recurse -Include *.c,*.cc,*.cpp,*.cxx -File | ForEach-Object { $_.FullName }
}

if ($files.Count -eq 0) { Write-Host "No C/C++ source files found to analyze."; exit 0 }

# Exclude test code from analysis by default
$excludeRegex = '(^|/|\\)Source(/|\\)Tests(/|\\|$)'
$beforeCount = $files.Count
$files = $files | Where-Object { -not ($_ -match $excludeRegex) }
$afterCount = ($files | Measure-Object).Count
Write-Host "Filtered out $($beforeCount - $afterCount) test files; analyzing $afterCount files."

Write-Host "Running clang-tidy with --header-filter='.*' and -p $BuildPath on $($files.Count) files..."
foreach ($f in $files) {
    Write-Host "-- $f"
    & $ct --header-filter='.*' -p $BuildPath $f 2>&1 | Tee-Object -FilePath $LogPath -Append
}

Write-Host "Parsing log: $LogPath"
$pattern = '^(.*):\d+:\d+: \w+: .* \[(.*)\]'
$matches = Select-String -Path $LogPath -Pattern $pattern -AllMatches | ForEach-Object { $_.Matches } | ForEach-Object { $_ }

$checkCounts = @{}
$fileCounts = @{}
foreach ($m in $matches) {
    $file = $m.Groups[1].Value.Replace('\\','/')
    $check = $m.Groups[2].Value
    if (-not $checkCounts.ContainsKey($check)) { $checkCounts[$check] = 0 }
    $checkCounts[$check] += 1
    if (-not $fileCounts.ContainsKey($file)) { $fileCounts[$file] = 0 }
    $fileCounts[$file] += 1
}

Write-Host "\nTop checks:" 
$checkCounts.GetEnumerator() | Sort-Object -Property Value -Descending | Select-Object -First 15 | ForEach-Object { Write-Host ("{0,5}  {1}" -f $_.Value, $_.Name) }

Write-Host "\nTop files:" 
$fileCounts.GetEnumerator() | Sort-Object -Property Value -Descending | Select-Object -First 15 | ForEach-Object { Write-Host ("{0,5}  {1}" -f $_.Value, $_.Name) }

Write-Host "\nFull log saved to: $LogPath"

Write-Host "Summary complete."