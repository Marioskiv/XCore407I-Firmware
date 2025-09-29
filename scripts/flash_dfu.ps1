#requires -version 5.1
[CmdletBinding()]
param(
    [string]$Bin,
    [string]$Addr = "0x08000000",
    [switch]$List,
    [switch]$UseCube,             # Force STM32CubeProgrammer CLI
    [string]$CubeCli,             # Explicit path to STM32_Programmer_CLI.exe
    [string]$Port                 # For Cube CLI: e.g. USB1
)

function Fail($msg) { Write-Error $msg; exit 1 }
function Info($msg) { Write-Host $msg -ForegroundColor Cyan }
function Tip($msg) { Write-Host $msg -ForegroundColor Yellow }

function Find-CubeCli {
    param([string]$Hint)
    if ($Hint -and (Test-Path $Hint)) { return (Resolve-Path $Hint).Path }
    $candidates = @(
        "C:\\Program Files\\STMicroelectronics\\STM32Cube\\STM32CubeProgrammer\\bin\\STM32_Programmer_CLI.exe",
        "C:\\Program Files (x86)\\STMicroelectronics\\STM32Cube\\STM32CubeProgrammer\\bin\\STM32_Programmer_CLI.exe"
    )
    foreach ($c in $candidates) { if (Test-Path $c) { return $c } }
    $cli = Get-Command STM32_Programmer_CLI.exe -ErrorAction SilentlyContinue
    if ($cli) { return $cli.Source }
    return $null
}

# Tool detection
$dfu = Get-Command dfu-util -ErrorAction SilentlyContinue
$cube = $null
if ($UseCube -or -not $dfu) { $cube = Find-CubeCli -Hint $CubeCli }

if ($List) {
    if ($dfu) {
        & dfu-util -l | Out-Host
    } elseif ($cube) {
        & $cube -l usb | Out-Host
    } else {
        Write-Warning "Neither dfu-util nor STM32_Programmer_CLI.exe found."
        Tip "Install dfu-util (choco install dfu-util) or STM32CubeProgrammer."
        exit 2
    }
    exit 0
}

# Resolve default binary if not provided
if (-not $Bin) {
    $repoRoot = Split-Path -Parent $PSScriptRoot
    $candidates = @(
        Join-Path $repoRoot "build-ninja/targets/TARGET_XCORE407I/XCORE407I_FIRMWARE_flash.bin" ,
        Join-Path $repoRoot "targets/TARGET_XCORE407I/build/XCORE407I_FIRMWARE_flash.bin",
        Join-Path $repoRoot "build/targets/TARGET_XCORE407I/XCORE407I_FIRMWARE_flash.bin"
    )
    $Bin = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if (-not $Bin -or -not (Test-Path $Bin)) { Fail "DFU binary not found. Build first, or pass -Bin <path>." }

Info "Using binary: $Bin"
Info "Target address: $Addr"

# Execution paths
if ($dfu -and -not $UseCube) {
    Write-Host "Listing DFU devices..." -ForegroundColor Gray
    & dfu-util -l | Out-Host
    Tip "If you don't see 'STMicroelectronics STM Device in DFU Mode (0483:df11)', set BOOT0=1 and reset, then reconnect USB FS (PA11/PA12)."

    # Flash with dfu-util: alt=0 internal flash, address + leave to exit DFU
    $cmd = @(
        "-d", "0483:df11",
        "-a", "0",
        "-s", "$Addr:leave",
        "-D", "$Bin"
    )
    Write-Host "Flashing via dfu-util..." -ForegroundColor Green
    & dfu-util @cmd
    $code = $LASTEXITCODE
    if ($code -ne 0) { Fail "dfu-util failed with exit code $code" }
    Write-Host "Flash OK (dfu-util). The device may reboot now." -ForegroundColor Green
    Tip "Set BOOT0=0 and reset to boot from flash if needed."
    exit 0
}
elseif ($cube) {
    Info "Using STM32CubeProgrammer CLI: $cube"
    Write-Host "Enumerating USB DFU ports..." -ForegroundColor Gray
    & $cube -l usb | Out-Host
    if (-not $Port) { $Port = "USB1" }
    Tip "If the device isn't listed, set BOOT0=1 and reset, then reconnect USB FS."

    $args = @(
        "-c", "port=$Port",
        "-w", "$Bin", "$Addr",
        "-v",
        "-s"
    )
    Write-Host "Flashing via STM32CubeProgrammer CLI on $Port..." -ForegroundColor Green
    & $cube @args
    $code = $LASTEXITCODE
    if ($code -ne 0) { Fail "STM32CubeProgrammer CLI failed with exit code $code" }
    Write-Host "Flash OK (Cube CLI). The device may reboot now." -ForegroundColor Green
    Tip "Set BOOT0=0 and reset to boot from flash if needed."
    exit 0
}
else {
    Write-Warning "Neither dfu-util nor STM32_Programmer_CLI.exe found."
    Tip "Install dfu-util (e.g., choco install dfu-util) or STM32CubeProgrammer."
    exit 2
}
