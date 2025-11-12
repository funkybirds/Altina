param(
    [string]$MessageFile
)

if (-not $MessageFile) {
    Write-Error "commit-msg hook: no message file provided"
    exit 2
}

$firstLine = Get-Content -Path $MessageFile -TotalCount 1 -ErrorAction Stop | ForEach-Object { $_.TrimEnd("`r") }

# Allowed types
$types = 'feat|fix|docs|style|refactor|perf|test|chore|build|ci'
# Regex: type(scope)?: subject (max 72 chars for subject line)
$regex = "^($types)(\([a-zA-Z0-9._ \-]+\))?: .{1,72}$"

if (-not ($firstLine -match $regex)) {
    Write-Error "ERROR: Commit message does not follow the project's commit format.`nExpected: <type>(optional-scope): <subject> (max 72 chars)`nAllowed types: feat, fix, docs, style, refactor, perf, test, chore, build, ci`nExample: feat(renderer): add Vulkan renderer"
    exit 1
}

exit 0
