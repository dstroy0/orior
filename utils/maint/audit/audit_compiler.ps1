# Audits every bench for compiler-dependent results.
#
# The concern is exact. A compiler can fold, reassociate, contract and hoist arithmetic. Where it
# folds a measurement into an immediate, the bench prints a compile-time answer. The bench then
# seems to compute it. bench_engines shows a whole engine that is a constant function. No structure
# stops the same defect in a statistic.
#
# The test is differential, and not inspective. The script builds each bench three ways and
# runs it three times:
#
#   O0    no optimization at all. The compiler does not fold, hoist or contract
#   O2    the flags of the normal build
#   NOFMA O2 with fused multiply-add contraction disabled. GCC turns contraction on by default
#
# A bench with three byte-identical outputs computes its numbers, and does not inherit them.
# Where the outputs differ, the compiler takes part in a result. Derive that result again before
# you quote it. This is a whole-tree check. You do not need to guess which statistic is fragile.
#
# Benches seeded from a fixed constant are deterministic by construction. Any difference between
# the three is the compiler.

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)))
$src = Join-Path $root "examples" "miner"
$engine = Join-Path $src "core"
# Not $bench: the loop below binds that name to each source in turn. A directory with the same
# name would change silently part way through.
$benchDirectory = Join-Path $root "utils" "bench"
$format = Join-Path $src "format"
$testDirectory = Join-Path $src "test"
$work = Join-Path $root "build" "audit" "compiler"
$compiler = "g++"

New-Item -ItemType Directory -Force -Path $work | Out-Null

# Each arm is a name and the flags that define it. The script builds the core again per arm. Thus
# the compression function gets the same treatment as the statistics that read it.
#
# The O0 arm carries no -mavx2, because of a compiler defect, and not a preference.
# MinGW-w64 GCC mishandles over-aligned locals at -O0 on this ABI. It aligns a pointer by hand for
# one __m256i local. It then puts another at a fixed offset from a frame pointer without realignment.
# It emits vmovdqa - the *aligned* 256-bit move - against an address that is 16 mod 32, and the
# process takes a general protection fault. The faulting instruction is
#
#   vmovdqa %ymm0,-0x20(%rbp)     with rbp = 0x5fe0b0. The target is 0x5fe090, 16 mod 32
#
# and -mstackrealign changes the prologue not at all. Nothing in this tree causes it and no source
# change here avoids it.
#
# sha256_core.c gates its vector arm on __AVX2__ and its else arm defers to the scalar reference.
# Without -mavx2 the O0 build is the reference arm compiled unoptimized. That is the arm a fold
# audit wants anyway: the statistics read its arithmetic.
#
# The cost of this: for the two benches that exercise the vector arm, the O0 column tests
# different code from the other two columns. An agreement there is evidence about the reference
# arm, and not about the vector arm. kat_validation covers the vector arm. It asserts that the
# vector arm equals the reference on the FIPS vectors, the genesis block, block 125552 and 1000
# consecutive real headers.
$arms = @(
    @{ Name = "O0";    Flags = @("-O0") },
    @{ Name = "O2";    Flags = @("-O2", "-mavx2") },
    @{ Name = "NOFMA"; Flags = @("-O2", "-mavx2", "-ffp-contract=off") }
)

# The list includes kat_validation deliberately. Every other result rests on that correctness gate.
# A compiler-dependent answer there would be the max expensive one to miss.
#
# The device benches need nvcc and a card. The list excludes them, and their own scripts build them.
$benches = @(Get-ChildItem -Path $benchDirectory -Filter "bench_*.cpp" |
    Where-Object { $_.Name -notin @("bench_cuda.cpp", "bench_cuda_v2.cpp", "bench_keyhole.cpp") }) +
    @(Get-ChildItem -Path $testDirectory -Filter "kat_validation.cpp")
$benches = $benches | Sort-Object Name

Write-Host "[*] auditing $($benches.Count) benches over $($arms.Count) arms" -ForegroundColor Cyan

foreach ($arm in $arms)
{
    $armDirectory = Join-Path $work $arm.Name
    New-Item -ItemType Directory -Force -Path $armDirectory | Out-Null

    $coreObject = Join-Path $armDirectory "sha256_core.o"
    Write-Host "[*] arm $($arm.Name): building core" -ForegroundColor DarkCyan
    & $compiler -c (Join-Path $engine "sha256_core.c") -o $coreObject @($arm.Flags) -std=c11
    if ($LASTEXITCODE -ne 0) { Write-Error "core failed on arm $($arm.Name)"; exit 1 }

    foreach ($bench in $benches)
    {
        $exe = Join-Path $armDirectory ($bench.BaseName + ".exe")
        & $compiler $bench.FullName $coreObject -o $exe @($arm.Flags) -std=c++17 `
            -I $engine -I $benchDirectory -I $format 2>&1 |
            Out-Null
        if ($LASTEXITCODE -ne 0)
        {
            Write-Host "    [skip] $($bench.BaseName) did not build on $($arm.Name)" -ForegroundColor DarkYellow
        }
    }
    Write-Host "[*] arm $($arm.Name): built" -ForegroundColor DarkCyan
}

# The run is the slow part, and the benches are independent. Run them concurrently. Keep one core
# for the machine. That is the located rule for this tree.
$parallel = [Math]::Max(1, [int]$env:NUMBER_OF_PROCESSORS - 1)
Write-Host "[*] running, $parallel at a time" -ForegroundColor Cyan

$jobs = @()
foreach ($arm in $arms)
{
    foreach ($bench in $benches)
    {
        $exe = Join-Path (Join-Path $work $arm.Name) ($bench.BaseName + ".exe")
        if (-not (Test-Path $exe)) { continue }
        $jobs += [pscustomobject]@{ Arm = $arm.Name; Bench = $bench.BaseName; Exe = $exe }
    }
}

$jobs | ForEach-Object -ThrottleLimit $parallel -Parallel {
    $out = Join-Path (Split-Path -Parent $_.Exe) ($_.Bench + ".out")
    # Benches that read a data file resolve it relative to the tree root. Run from there instead
    # of from wherever the arm's binaries happen to live.
    Set-Location $using:root
    try
    {
        & $_.Exe *> $out
    }
    catch
    {
        Set-Content -Path $out -Value "RUN FAILED: $_"
    }
}

Write-Host "[*] comparing arms" -ForegroundColor Cyan

# A line that reports wall-clock time changes on every run, and an arm does not cause it. The hash
# skips these lines. Every other line must be identical across the arms.
$clockLine = '^\s*(elapsed|rate)\s*:'
$hasher = [System.Security.Cryptography.SHA256]::Create()

$report = @()
foreach ($bench in $benches)
{
    $hashes = @{}
    foreach ($arm in $arms)
    {
        $out = Join-Path (Join-Path $work $arm.Name) ($bench.BaseName + ".out")
        if (Test-Path $out)
        {
            $kept = @(Get-Content -Path $out | Where-Object { $_ -notmatch $clockLine }) -join "`n"
            $digest = $hasher.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($kept))
            $hashes[$arm.Name] = [System.Convert]::ToHexString($digest)
        }
        else
        {
            $hashes[$arm.Name] = "ABSENT"
        }
    }

    $distinct = ($hashes.Values | Sort-Object -Unique)
    $verdict = if ($distinct.Count -eq 1) { "identical" } else { "DIFFERS" }

    $report += [pscustomobject]@{
        Bench   = $bench.BaseName
        Verdict = $verdict
        O0      = $hashes["O0"].Substring(0, 8)
        O2      = $hashes["O2"].Substring(0, 8)
        NOFMA   = $hashes["NOFMA"].Substring(0, 8)
    }
}

$report | Format-Table -AutoSize

$differing = @($report | Where-Object { $_.Verdict -ne "identical" })
Write-Host ""
Write-Host "[*] $($differing.Count) of $($report.Count) benches are compiler-dependent" -ForegroundColor `
    $(if ($differing.Count -eq 0) { "Green" } else { "Yellow" })
foreach ($row in $differing)
{
    Write-Host "    $($row.Bench)" -ForegroundColor Yellow
}

$report | Export-Csv -Path (Join-Path $work "audit.csv")
Write-Host "[*] wrote $(Join-Path $work 'audit.csv')" -ForegroundColor Cyan
