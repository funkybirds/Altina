param(
    [string]$Message = ''
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Message)) {
    $Message = @'
chore(repo): add gitattributes and pre-push build/test checks

- Add .gitattributes to normalize line endings
- Add pre-push hooks (bash + PowerShell)
- Add RunPrePushChecks.ps1 convenience runner
'@
}


# Use a temp file outside the repository root to avoid accidentally committing it
$tempPath = [System.IO.Path]::GetTempPath()
$msgPath = Join-Path $tempPath ("altina_commit_msg_{0}.txt" -f ([System.Guid]::NewGuid().ToString('N')))
Set-Content -Path $msgPath -Value $Message -Encoding UTF8

try {
    Write-Host 'Validating commit message with local hook...'
    $hook = Join-Path $repoRoot '.githooks\commit-msg.ps1'
    if (Test-Path $hook) {
        & $hook $msgPath
        if ($LASTEXITCODE -ne 0) { throw "Commit message hook failed ($LASTEXITCODE)" }
    } else {
        Write-Host 'Warning: commit hook not found, skipping validation.' -ForegroundColor Yellow
    }

    Write-Host 'Validation passed — creating commit...'
    git add -A
    git commit -F $msgPath
    if ($LASTEXITCODE -ne 0) { throw "git commit failed ($LASTEXITCODE)" }

    $log = git log -1 --pretty=format:'%h %s%n%n%b%nAuthor: %an <%ae>%nDate: %ad'
    Write-Host $log
}
catch {
    Write-Error "Commit failed: $_"
    exit 1
}
finally {
    Remove-Item -Path $msgPath -Force -ErrorAction SilentlyContinue
}

exit 0
