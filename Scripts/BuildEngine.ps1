param(
    [string]$Preset = "windows-msvc-relwithdebinfo",
    [string]$Target = "AltinaEngineCore",
    [switch]$ForceConfigure,
    [switch]$RunTests
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

function Invoke-Configure($presetName) {
    Write-Host "[CMake] Configuring preset '$presetName'"
    Invoke-WithVisualStudioEnv @("cmake", "--preset", $presetName)
}

function Get-CtestConfigFromPreset($presetName) {
    # Infer common CTest configuration names from the preset token.
    # Examples:
    #  - windows-msvc-relwithdebinfo -> RelWithDebInfo
    #  - windows-msvc-debug -> Debug
    #  - linux-clang-release -> Release
    if (-not $presetName) { return 'RelWithDebInfo' }
    $tokens = $presetName -split '[-_.]'
    if ($tokens.Length -eq 0) { return 'RelWithDebInfo' }
    $candidate = $tokens[-1].ToLowerInvariant()
    switch ($candidate) {
        'debug' { return 'Debug' }
        'release' { return 'Release' }
        'relwithdebinfo' { return 'RelWithDebInfo' }
        'minsizerel' { return 'MinSizeRel' }
        default {
            # Fallback: capitalize first letter (least surprising)
            return ($candidate.Substring(0,1).ToUpper() + $candidate.Substring(1))
        }
    }
}

try {
    Set-Location $repoRoot

    $buildDir = Join-Path $repoRoot "out\build\$Preset"
    if ($ForceConfigure.IsPresent -or -not (Test-Path $buildDir)) {
        Invoke-Configure $Preset
    }

    Write-Host "[CMake] Building $Target via preset '$Preset'"
    Invoke-WithVisualStudioEnv @("cmake", "--build", "--preset", $Preset, "--target", $Target)

    # Decide whether to run tests after a successful build.
    $shouldRunTests = $RunTests.IsPresent -or ($Target -match 'Test') -or ($Target -match 'Tests')
    if ($shouldRunTests) {
        $ctestConfig = Get-CtestConfigFromPreset $Preset
        Write-Host "[CTest] Running tests for preset '$Preset' (config: $ctestConfig)..."
        $buildDir = Join-Path $repoRoot "out\build\$Preset"
        $testBuildDir = Join-Path $buildDir "Source\Tests"
        if (Test-Path $testBuildDir) {
            Push-Location $testBuildDir
            try {
                Invoke-WithVisualStudioEnv @("ctest", "-C", $ctestConfig, "--output-on-failure")
            } catch {
                Pop-Location
                throw "ctest reported failures or failed to run."
            }
            Pop-Location
        } elseif (Test-Path $buildDir) {
            Push-Location $buildDir
            try {
                Invoke-WithVisualStudioEnv @("ctest", "-C", $ctestConfig, "--output-on-failure")
            } catch {
                Pop-Location
                throw "ctest reported failures or failed to run."
            }
            Pop-Location
        } else {
            Write-Warning "Build directory not found: $buildDir. Running ctest from repo root."
            try {
                Invoke-WithVisualStudioEnv @("ctest", "-C", $ctestConfig, "--output-on-failure")
            } catch {
                throw "ctest reported failures or failed to run."
            }
        }
    }
}
finally {
    Set-Location $initialLocation
}
