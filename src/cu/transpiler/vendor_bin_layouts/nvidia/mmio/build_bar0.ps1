# Build the read-only BAR0 driver (bar0_driver.c) to a .sys under the repo's build directory, compiling and linking
# directly with the MSVC toolchain against the Windows Driver Kit's kernel headers and libraries. This needs the WDK
# installed (the kernel libraries and headers under Windows Kits\10), and MSVC from Visual Studio or the Build Tools.
# It does not need the WDK's Visual Studio extension. Build products only; nothing is loaded or signed here.
$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..\..\..\..\..')).Path
$out = Join-Path $repo 'build\harness\mmio\bin'
$obj = Join-Path $repo 'build\harness\mmio\obj'
New-Item -ItemType Directory -Force -Path $out, $obj | Out-Null

# the MSVC host-x64 compiler and linker, the newest found under a Visual Studio or Build Tools install
$cl = $null
foreach ($root in 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC',
                  'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC',
                  'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC',
                  'C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC') {
    if (Test-Path $root) {
        foreach ($v in (Get-ChildItem $root -Directory | Sort-Object Name -Descending)) {
            $candidate = Join-Path $v.FullName 'bin\Hostx64\x64\cl.exe'
            if (Test-Path $candidate) { $cl = $candidate; break }
        }
    }
    if ($cl) { break }
}
if (-not $cl) {
    Write-Output "MSVC (cl.exe, Hostx64/x64) not found; install Visual Studio 2022 or the Build Tools with C++"
    exit 1
}
$clbin = Split-Path $cl
$link = Join-Path $clbin 'link.exe'
$msvcInc = Join-Path (Split-Path (Split-Path (Split-Path $clbin))) 'include'

# the newest WDK kit that holds the kernel libraries
$kits = 'C:\Program Files (x86)\Windows Kits\10'
$ver = Get-ChildItem (Join-Path $kits 'Lib') -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName 'km\x64\ntoskrnl.lib') } |
    Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
if (-not $ver) {
    Write-Output "the Windows Driver Kit is not installed (no km\x64\ntoskrnl.lib under Windows Kits\10\Lib)."
    Write-Output "install it: winget install Microsoft.WindowsWDK.10.0.26100"
    exit 1
}
$inc = Join-Path $kits "Include\$ver"
$km = Join-Path $inc 'km'
$shared = Join-Path $inc 'shared'
$kmcrt = Join-Path $km 'crt'
$lib = Join-Path $kits "Lib\$ver\km\x64"

$objFile = Join-Path $obj 'bar0_driver.obj'
$sys = Join-Path $out 'bar0_driver.sys'
$source = Join-Path $here 'bar0_driver.c'

Write-Output "cl:  $cl"
Write-Output "wdk: $ver"
Write-Output ""

$clArgs = @(
    '/nologo', '/c', '/GS', '/Oi', '/O2', '/Oy-', '/Gy', '/Zc:wchar_t-', '/Zc:inline', '/W3',
    '/D_WIN64', '/D_AMD64_', '/DAMD64', '/D_KERNEL_MODE',
    '/DNTDDI_VERSION=0x0A000010', '/D_WIN32_WINNT=0x0A00', '/DWINVER=0x0A00',
    "/I$kmcrt", "/I$km", "/I$shared", "/I$msvcInc",
    "/Fo$objFile", $source
)
& $cl @clArgs
if ($LASTEXITCODE -ne 0) {
    Write-Output "the driver did not compile"
    exit $LASTEXITCODE
}

$linkArgs = @(
    '/nologo', '/DRIVER', '/SUBSYSTEM:NATIVE,10.00', '/ENTRY:DriverEntry', '/NODEFAULTLIB',
    '/MACHINE:X64', '/RELEASE', "/OUT:$sys", "/LIBPATH:$lib",
    $objFile, 'ntoskrnl.lib', 'hal.lib', 'BufferOverflowFastFailK.lib'
)
& $link @linkArgs
if ($LASTEXITCODE -ne 0) {
    Write-Output "the driver did not link"
    exit $LASTEXITCODE
}

if (Test-Path $sys) {
    Write-Output ""
    Write-Output "built: $sys"
    Write-Output "next: turn on test signing (bcdedit /set testsigning on, then reboot), then install_bar0.ps1 -Sign (elevated)"
} else {
    Write-Output "link reported success but no bar0_driver.sys is at $out"
    exit 1
}
