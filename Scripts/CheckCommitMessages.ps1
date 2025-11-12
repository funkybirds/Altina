param(
    [string]$Since = 'HEAD~20'
)

$ErrorActionPreference = 'Stop'

Write-Host "Checking commit messages since: $Since"
# If repository has no commits (no HEAD), exit early
try {
    git rev-parse --verify HEAD > $null 2>&1
} catch {
    Write-Host "Repository has no commits (no HEAD). Nothing to check." -ForegroundColor Yellow
    exit 0
}

#
# Get commit hashes since ref (default: last 20 commits)
#
# Get commit hashes since ref (default: last 20 commits)
try {
    $commits = git rev-list --max-count=100 $Since 2>$null
} catch {
    $commits = $null
}

if (-not $commits) {
    # Handle common shorthand like HEAD~10 as "last 10 commits"
    if ($Since -match '^HEAD~(\d+)$') {
        $n = [int]$Matches[1]
        try { $commits = git rev-list --max-count=$n HEAD } catch { $commits = $null }
    }

    # Try parsing as count
    if (-not $commits -and ($Since -as [int])) {
        $num = [int]$Since
        try { $commits = git rev-list --max-count=$num HEAD } catch { $commits = $null }
    }
}

if (-not $commits) {
    Write-Host "No commits found for: $Since"
    exit 0
}

$bad = @()
foreach ($c in $commits) {
    $msg = git log -n 1 --pretty=format:%s $c
    # write to temp file and invoke hook script to reuse validation
    $tmp = New-TemporaryFile
    Set-Content -Path $tmp -Value $msg
    try {
        & "$PSScriptRoot\..\.githooks\commit-msg.ps1" $tmp > $null
    }
    catch {
        $bad += @{Commit=$c; Message=$msg}
    }
    Remove-Item $tmp -ErrorAction SilentlyContinue
}

if ($bad.Count -eq 0) {
    Write-Host "All commit messages OK"
    exit 0
}

Write-Host "Found $($bad.Count) commits with invalid messages:" -ForegroundColor Yellow
foreach ($b in $bad) {
    Write-Host "$($b.Commit): $($b.Message)"
}

exit 1
