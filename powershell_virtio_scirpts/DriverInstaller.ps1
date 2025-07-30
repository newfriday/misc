param(
    [string]$DevconPath = "C:\Program Files\Windows Kits\10\Tools\10.0.22621.0\x64\devcon.exe",
    [ValidateSet("vioscsi", "viostor", "NetKVM")]
    [string]$DriverType,
    [string]$InfFilePath
)

# Verify administrator privileges
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "This script requires administrator privileges. Please run PowerShell as administrator."
    exit 1
}

# Validate parameters
if (-not (Test-Path $DevconPath -PathType Leaf)) {
    Write-Error "Devcon tool not found at: $DevconPath"
    exit 1
}

if (-not (Test-Path $InfFilePath -PathType Leaf)) {
    Write-Error "INF file not found at: $InfFilePath"
    exit 1
}

# Hardware ID mapping
$hardwareIds = @{
    "vioscsi" = "PCI\VEN_1AF4&DEV_1004"
    "viostor" = "PCI\VEN_1AF4&DEV_1001"
    "NetKVM"  = "PCI\VEN_1AF4&DEV_1000"
}

# Get hardware ID
$hardwareId = $hardwareIds[$DriverType]
if (-not $hardwareId) {
    Write-Error "Invalid driver type: $DriverType. Valid options are vioscsi, viostor, NetKVM."
    exit 1
}

Write-Host "`n==================== Driver Installation ===================="
Write-Host "Driver Type: $DriverType"
Write-Host "INF File: $InfFilePath"
Write-Host "Hardware ID: $hardwareId"
Write-Host "Devcon Path: $DevconPath"

try {
    # Step 1: Add driver to driver store using pnputil
    Write-Host "`n[1/2] Adding driver to driver store..."
    pnputil /add-driver $InfFilePath /install

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to add driver to driver store. Error code: $LASTEXITCODE"
        exit 1
    }

    Write-Host "Driver successfully added to driver store"

    # Step 2: Install driver using devcon
    Write-Host "`n[2/2] Installing driver for hardware ID..."
    & $DevconPath install $InfFilePath $hardwareId

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Driver installation failed. Error code: $LASTEXITCODE"
        exit 1
    }

    Write-Host "Driver installed successfully"
}
catch {
    Write-Error "An error occurred during installation: $_"
    exit 1
}

Write-Host "`n==================== Summary ===================="
Write-Host "Driver installation completed successfully"
Write-Host "Hardware ID: $hardwareId"
Write-Host "Driver file: $(Split-Path $InfFilePath -Leaf)"
