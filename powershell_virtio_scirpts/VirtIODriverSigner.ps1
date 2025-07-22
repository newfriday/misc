param(
    [string]$SignToolPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe",
    [string]$DriverPath = $PWD.Path
)

# Check for administrator privileges
$currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "This script requires administrator privileges. Please run PowerShell as administrator and try again."
    exit 1
}

# Certificate parameters
$certSubject = "CN=VirtIO Test Certificate"
$pfxPassword = ConvertTo-SecureString -String "VirtIOTest123!" -Force -AsPlainText
$exportPfxPath = Join-Path $PWD.Path "VirtIO_Test_Certificate.pfx"
$exportCerPath = Join-Path $PWD.Path "VirtIO_Test_Certificate.cer"
$certStoreMy = "Cert:\CurrentUser\My"
$certStoreRoot = "Cert:\LocalMachine\Root"

# Function: Enable test signing mode
function Enable-TestSigning {
    $signingStatus = bcdedit /enum | Select-String -Pattern "testsigning"
    if ($signingStatus -match "Yes") {
        Write-Host "Test signing mode is already enabled"
    } else {
        Write-Host "Enabling test signing mode..."
        bcdedit /set testsigning on | Out-Null
        Write-Warning "Test signing mode has been enabled. System reboot is required!"
    }
}

# Function: Validate certificate
function Test-CertificateValid {
    param($cert)
    if (-not $cert.HasPrivateKey) {
        Write-Error "Certificate does not contain a private key!"
        return $false
    }

    $ekuValid = $cert.EnhancedKeyUsageList.ObjectId -contains "1.3.6.1.5.5.7.3.3"
    if (-not $ekuValid) {
        Write-Error "Certificate does not contain Code Signing EKU (1.3.6.1.5.5.7.3.3)"
        return $false
    }

    return $true
}

# Function: Export certificate files
function Export-CertificateFiles {
    param($cert)
    try {
        Export-PfxCertificate -Cert $cert -FilePath $exportPfxPath -Password $pfxPassword -Force | Out-Null
        Export-Certificate -Cert $cert -FilePath $exportCerPath -Force | Out-Null
        Write-Host "Certificate exported successfully:"
        Write-Host "  - PFX (with private key): $exportPfxPath"
        Write-Host "  - CER (public key): $exportCerPath"
    } catch {
        Write-Error "Failed to export certificate: $_"
        exit 1
    }
}

# Function: Sign a file
function Sign-File {
    param($filePath)
    Write-Host "Signing file: $(Split-Path $filePath -Leaf)"
    & $SignToolPath sign /v /fd SHA256 /a /f $exportPfxPath /p "VirtIOTest123!" $filePath

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Signing failed: $filePath"
        return $false
    }

    Write-Host "Verifying signature..."
    & $SignToolPath verify /pa /v $filePath

    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Verification failed: $filePath (signature may not be trusted by the system)"
        return $false
    }

    Write-Host "Signature verified successfully"
    return $true
}

# =============================================
# Main script logic begins
# =============================================

Write-Host "`n==================== Certificate Management ===================="

# Check for existing certificates
$existingCerts = @(Get-ChildItem $certStoreMy | Where-Object { $_.Subject -eq $certSubject })
$cert = $null

if ($existingCerts.Count -gt 0) {
    $cert = $existingCerts[0]
    Write-Host "Found existing certificate: $($cert.Subject)"
    Write-Host "  Thumbprint: $($cert.Thumbprint)"
    Write-Host "  Valid from: $($cert.NotBefore) to $($cert.NotAfter)"

    # Validate certificate
    if (-not (Test-CertificateValid -cert $cert)) {
        Write-Error "Existing certificate is invalid, please remove or regenerate"
        exit 1
    }
    Write-Host "Certificate validation passed"
} else {
    Write-Host "No valid certificate found, creating new one..."

    # Create new certificate
    $cert = New-SelfSignedCertificate `
        -Subject $certSubject `
        -CertStoreLocation $certStoreMy `
        -Type CodeSigningCert `
        -KeyUsage DigitalSignature `
        -KeyExportPolicy Exportable `
        -KeySpec Signature `
        -HashAlgorithm SHA256 `
        -NotAfter (Get-Date).AddYears(5)

    Write-Host "Certificate created successfully"
    Write-Host "  Thumbprint: $($cert.Thumbprint)"

    # Validate new certificate
    if (-not (Test-CertificateValid -cert $cert)) {
        Write-Error "Certificate generation failed"
        exit 1
    }

    # Import to root store
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store("Root", "LocalMachine")
    $store.Open("ReadWrite")
    $store.Add($cert)
    $store.Close()
    Write-Host "Certificate imported to Local Machine Root store"

    # Enable test signing
    Enable-TestSigning
}

# Export certificates
Export-CertificateFiles -cert $cert

# =============================================
# Driver signing section
# =============================================

Write-Host "`n==================== Driver Signing ===================="

# Check sign tool
if (-not (Test-Path $SignToolPath)) {
    Write-Error "Sign tool not found: $SignToolPath"
    exit 1
}
Write-Host "Sign tool path: $SignToolPath"

# Check driver directory
if (-not (Test-Path $DriverPath -PathType Container)) {
    Write-Error "Driver directory does not exist: $DriverPath"
    exit 1
}
Write-Host "Driver directory: $DriverPath"

# Find driver files
$filesToSign = Get-ChildItem -Path $DriverPath -Include *.sys, *.cat -Recurse
if (-not $filesToSign) {
    Write-Error "No .sys or .cat files found"
    exit 1
}

Write-Host "Found $($filesToSign.Count) files to sign:"
$filesToSign | ForEach-Object { Write-Host "  - $($_.Name)" }

# Sign all files
$successCount = 0
foreach ($file in $filesToSign) {
    if (Sign-File -filePath $file.FullName) {
        $successCount++
    }
}

# Results summary
Write-Host "`n==================== Summary ===================="
Write-Host "Successfully signed files: $successCount/$($filesToSign.Count)"
Write-Host "Certificate location: $exportPfxPath"
Write-Host "Driver directory: $DriverPath"

if ($successCount -eq $filesToSign.Count) {
    Write-Host "`nAll files signed successfully!" -ForegroundColor Green
} else {
    Write-Host "`nSome files failed to sign, please check error messages" -ForegroundColor Yellow
}
