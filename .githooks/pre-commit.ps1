param()

$ErrorActionPreference = 'Stop'

function Get-ClangFormatCommand {
    $cmd = Get-Command clang-format -ErrorAction SilentlyContinue
    if (-not $cmd) { $cmd = Get-Command clang-format.exe -ErrorAction SilentlyContinue }
    return $cmd
}

$clangFormat = Get-ClangFormatCommand
if (-not $clangFormat) {
    Write-Error "clang-format not found in PATH. Install it or add to PATH."
    exit 1
}

$files = git diff --cached --name-only --diff-filter=ACM | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' }
if (-not $files) { exit 0 }

$exts = @('.c','.cc','.cpp','.cxx','.h','.hh','.hpp','.hxx','.ipp','.inl')
$toFormat = @()
foreach ($f in $files) {
    $ext = [System.IO.Path]::GetExtension($f)
    if ($exts -contains $ext -and (Test-Path $f)) {
        $toFormat += $f
    }
}

if ($toFormat.Count -eq 0) { exit 0 }

Write-Host "clang-format: formatting staged files..."
& $clangFormat.Source -i $toFormat
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

git add -- $toFormat | Out-Null
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

exit 0
