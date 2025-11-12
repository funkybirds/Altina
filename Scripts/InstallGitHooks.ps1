param(
    [string]$HooksPath = ".githooks"
)

$ErrorActionPreference = 'Stop'

Write-Host "Installing git hooks by setting core.hooksPath to '$HooksPath'"
git config core.hooksPath $HooksPath

Write-Host "Done. To revert: git config --unset core.hooksPath"
