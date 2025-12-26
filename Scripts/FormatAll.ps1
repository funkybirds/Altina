<#
Run clang-format over all tracked source files (or all project files if git isn't available).

Usage:
  .\FormatAll.ps1            # uses clang-format from PATH
  .\FormatAll.ps1 -ClangFormatPath C:\path\to\clang-format.exe

This script prefers `git ls-files` to find project sources. If `git` is not available
it falls back to a recursive file search and excludes common build output directories.
#>

param(
    [string]$ClangFormatPath
)

function Find-ClangFormat {
    if ($ClangFormatPath) { return $ClangFormatPath }
    $cmd = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Path }
    $cmd = Get-Command clang-format.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Path }
    return $null
}

$clang = Find-ClangFormat
if (-not $clang) {
    Write-Error "clang-format not found in PATH. Install clang-format or pass -ClangFormatPath."
    exit 1
}

# Prefer git to enumerate tracked files so we only format source files under version control
$files = @()
if (Get-Command git -ErrorAction SilentlyContinue) {
    try {
        $out = git ls-files -- "*.h" "*.hpp" "*.hh" "*.c" "*.cc" "*.cpp" "*.cxx" 2>$null
        if ($out) { $files = $out -split "\r?\n" | Where-Object { $_ -ne "" } }
    } catch { $files = @() }
}

if ($files.Count -eq 0) {
    Write-Host "git not available or no tracked files found — falling back to file system scan"
    $excludes = @('build','out','Debug','Release','RelWithDebInfo','Binaries')
    $files = Get-ChildItem -Path . -Recurse -Include *.h,*.hpp,*.hh,*.c,*.cc,*.cpp,*.cxx -File |
             Where-Object {
                 $full = $_.FullName.ToLower()
                 -not ($excludes | ForEach-Object { $full -like "*\\$_*" } | Where-Object { $_ })
             } |
             ForEach-Object { $_.FullName }
}

if ($files.Count -eq 0) {
    Write-Host "No source files found to format."
    exit 0
}

Write-Host "Formatting $($files.Count) files using: $clang"

foreach ($f in $files) {
    & $clang -i -style=file "$f" 2>$null
}

Write-Host "Formatting complete."
