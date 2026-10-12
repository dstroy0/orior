# Load the read-only BAR0 driver's service and point it at the card's register aperture. Run elevated. It finds BAR0
# (the 16 MiB memory range Windows assigned the GPU) unless a base and size are given, writes them to the service's
# Parameters, and starts the service. The driver maps that range read-only; nothing here or in it writes the part.
#
#   install_bar0.ps1 [-Sys <path to bar0_driver.sys>] [-Base <hex>] [-Size <hex>] [-Sign] [-Remove]
#
# -Sign self-signs the .sys with a test certificate and trusts it, for a machine in test-signing mode
# (bcdedit /set testsigning on, then reboot). -Remove stops and deletes the service.
param(
    [string]$Sys,
    [string]$Base,
    [string]$Size,
    [switch]$Sign,
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$service = 'GpuBar0'

$elevated = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $elevated) {
    Write-Output "run this elevated: it creates and starts a kernel service"
    exit 1
}

if ($Remove) {
    & sc.exe stop $service | Out-Null
    & sc.exe delete $service | Out-Null
    Write-Output "service $service stopped and deleted"
    exit 0
}

$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..\..\..\..\..')).Path
if (-not $Sys) {
    $Sys = Join-Path $repo 'build\harness\mmio\bin\bar0_driver.sys'
}
if (-not (Test-Path $Sys)) {
    Write-Output "no driver at $Sys; build it first with build_bar0.ps1"
    exit 1
}
$Sys = (Resolve-Path $Sys).Path

# BAR0: the 16 MiB (0x1000000) memory range the card was assigned, unless a base and size are given
if ($Base -and $Size) {
    $base = [uint64]$Base
    $size = [uint64]$Size
} else {
    $dev = Get-CimInstance Win32_PnPEntity -Filter "DeviceID LIKE '%VEN_10DE&DEV_2484%'"
    $ranges = Get-CimAssociatedInstance -InputObject $dev -Association Win32_PnPAllocatedResource |
        Where-Object { $_.CimClass.CimClassName -eq 'Win32_DeviceMemoryAddress' }
    $bar0 = $ranges | Where-Object { ([uint64]$_.EndingAddress - [uint64]$_.StartingAddress + 1) -eq 0x1000000 } |
        Select-Object -First 1
    if (-not $bar0) {
        Write-Output "no 16 MiB BAR0 range found; pass -Base and -Size in hex"
        exit 1
    }
    $base = [uint64]$bar0.StartingAddress
    $size = [uint64]0x1000000
}
Write-Output ("BAR0 base 0x{0}, size 0x{1}" -f ([uint64]$base).ToString('X'), ([uint64]$size).ToString('X'))

$testsigning = (& bcdedit /enum '{current}' | Select-String 'testsigning\s+Yes')
if (-not $testsigning) {
    Write-Output "note: test signing is OFF. A self-signed driver loads only after: bcdedit /set testsigning on, then reboot"
}

if ($Sign) {
    $subject = 'CN=Bar0TestCert'
    # one test certificate, reused across runs: the machine's own, where signtool /sm reads it below
    $cert = Get-ChildItem Cert:\LocalMachine\My | Where-Object { $_.Subject -eq $subject } | Select-Object -First 1
    if (-not $cert) {
        $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $subject `
            -CertStoreLocation Cert:\LocalMachine\My -KeyUsage DigitalSignature -KeySpec Signature
    }
    # trusted as a root and as a publisher, a driver it signs loading in test-signing mode; imported once
    foreach ($store in 'Root', 'TrustedPublisher') {
        $path = "Cert:\LocalMachine\$store"
        $have = Get-ChildItem $path | Where-Object { $_.Thumbprint -eq $cert.Thumbprint }
        if (-not $have) {
            $tmp = Join-Path $env:TEMP 'bar0testcert.cer'
            Export-Certificate -Cert $cert -FilePath $tmp | Out-Null
            Import-Certificate -FilePath $tmp -CertStoreLocation $path | Out-Null
            Remove-Item $tmp -Force
        }
    }
    $signtool = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe' -ErrorAction SilentlyContinue |
        Sort-Object FullName | Select-Object -Last 1
    if (-not $signtool) {
        Write-Output "signtool not found; install the Windows SDK/WDK"
        exit 1
    }
    # /sm reads the machine store, where the certificate above lives
    & $signtool.FullName sign /v /sm /fd sha256 /s My /n 'Bar0TestCert' $Sys
    if ($LASTEXITCODE -ne 0) {
        Write-Output "signing failed"
        exit $LASTEXITCODE
    }
}

& sc.exe query $service | Out-Null
if ($LASTEXITCODE -eq 0) {
    & sc.exe stop $service | Out-Null
    & sc.exe delete $service | Out-Null
}
& sc.exe create $service type= kernel start= demand binPath= $Sys
if ($LASTEXITCODE -ne 0) {
    Write-Output "sc create failed"
    exit $LASTEXITCODE
}

$params = "HKLM:\SYSTEM\CurrentControlSet\Services\$service\Parameters"
New-Item -Path $params -Force | Out-Null
New-ItemProperty -Path $params -Name 'Bar0Base' -PropertyType QWord -Value $base -Force | Out-Null
New-ItemProperty -Path $params -Name 'Bar0Size' -PropertyType QWord -Value $size -Force | Out-Null

& sc.exe start $service
if ($LASTEXITCODE -ne 0) {
    Write-Output "sc start failed (code $LASTEXITCODE). If 577, the driver is not trusted: -Sign it and turn on test signing"
    exit $LASTEXITCODE
}
Write-Output "service $service started; read it with build\harness\mmio\mmio_read_check.exe"
