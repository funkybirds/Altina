param(
    [string]$VSDevBat = "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
)

$ErrorActionPreference = "Stop"

# 仓库根目录
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

if (-not (Test-Path $VSDevBat)) {
    Write-Error "VS developer environment script not found: $VSDevBat"
    exit 1
}

# 在同一个 cmd 进程里先执行 vcvars64.bat，再启动 VS Code，
# 这样 VS Code 会继承 MSVC 的环境变量（包含标准库头文件路径等）。
$cmdLine = '"{0}" && code .'-f $VSDevBat

& cmd.exe /c $cmdLine
