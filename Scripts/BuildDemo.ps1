param(
    [string]$Preset = "windows-msvc-relwithdebinfo",
    [ValidateSet("Minimal", "SpaceshipGame", "All")]
    [string]$Demo = "Minimal",
    [switch]$ForceConfigure,
    [ValidateSet("Keep", "On", "Off")]
    [string]$Shipping = "Keep"
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$initialLocation = Get-Location

function Get-VsDevCmdPath {
    $vsWherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    if (-not (Test-Path $vsWherePath)) {
        throw "vswhere.exe not found. Install Visual Studio Build Tools or launch from a Developer Command Prompt."
    }

    $vsInstallPath = & $vsWherePath -latest -prerelease -products * -requires Microsoft.Component.MSBuild -property installationPath
    if (-not $vsInstallPath) {
        throw "Visual Studio installation with C++ components not detected."
    }

    $vsDevCmdPath = Join-Path $vsInstallPath "Common7/Tools/VsDevCmd.bat"
    if (-not (Test-Path $vsDevCmdPath)) {
        throw "VsDevCmd.bat not found under '$vsInstallPath'."
    }

    return $vsDevCmdPath
}

function Invoke-WithVisualStudioEnv {
    param(
        [Parameter(Mandatory)] [string[]] $Command
    )

    $exe = $Command[0]
    $args = @()
    if ($Command.Length -gt 1) {
        $args = $Command[1..($Command.Length - 1)]
    }

    if ($env:VSCMD_VER) {
        & $exe @args
    }
    else {
        $vsDevCmd = Get-VsDevCmdPath
        $quotedArgs = $Command | ForEach-Object {
            if ($_ -match '\s') { '"' + $_ + '"' } else { $_ }
        }
        $commandLine = $quotedArgs -join ' '
        $chainedCommand = 'call "{0}" -no_logo -arch=amd64 -host_arch=amd64 && {1}' -f $vsDevCmd, $commandLine
        & cmd.exe /c $chainedCommand
    }

    if ($LASTEXITCODE -ne 0) {
        throw "Command failed: $($Command -join ' ')"
    }
}

function Invoke-Configure {
    param(
        [Parameter(Mandatory)] [string] $PresetName,
        [string[]] $ExtraArgs = @()
    )
    Write-Host "[CMake] Configuring preset '$PresetName'"
    Invoke-WithVisualStudioEnv (@("cmake", "--preset", $PresetName) + $ExtraArgs)
}

try {
    Set-Location $repoRoot

    $buildDir = Join-Path $repoRoot "out\build\$Preset"

    $configureArgs = @()
    if ($Shipping -eq "On") {
        $configureArgs += "-DAE_DEMO_ENABLE_SHIPPING=ON"
    }
    elseif ($Shipping -eq "Off") {
        $configureArgs += "-DAE_DEMO_ENABLE_SHIPPING=OFF"
    }

    # Changing cache variables requires a configure pass even if the build dir already exists.
    if ($ForceConfigure.IsPresent -or -not (Test-Path $buildDir) -or $configureArgs.Count -gt 0) {
        Invoke-Configure -PresetName $Preset -ExtraArgs $configureArgs
    }

    if ($Demo -eq "Minimal" -or $Demo -eq "All") {
        Write-Host "[CMake] Cleaning Demo/Minimal output folders (Binaries/Shipping)"
        Invoke-WithVisualStudioEnv @("cmake", "--build", "--preset", $Preset, "--target", "AltinaEngineDemoMinimalPreClean")

        Write-Host "[CMake] Building AltinaEngineDemoMinimal via preset '$Preset'"
        Invoke-WithVisualStudioEnv @("cmake", "--build", "--preset", $Preset, "--target", "AltinaEngineDemoMinimal")
    }

    if ($Demo -eq "SpaceshipGame" -or $Demo -eq "All") {
        Write-Host "[CMake] Cleaning Demo/SpaceshipGame output folders (Binaries/Shipping)"
        Invoke-WithVisualStudioEnv @("cmake", "--build", "--preset", $Preset, "--target", "SpaceshipGamePreClean")

        Write-Host "[CMake] Building SpaceshipGame via preset '$Preset'"
        Invoke-WithVisualStudioEnv @("cmake", "--build", "--preset", $Preset, "--target", "SpaceshipGame")
    }
}
finally {
    Set-Location $initialLocation
}
