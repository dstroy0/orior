param([int]$Minutes = 2, [int]$Step = 5, [int]$Spike = 150, [string]$Log = "build\harness\window_cpu.txt", [string]$Symbols = "build\cli\x86_64-pc-windows-msvc", [int]$ProcessId = 0)
# Watch the window's processor time. Each step writes the time the window used and its busiest threads to the log.
# On a step past the spike, the script also writes a stack of the busiest thread, and the thread runs on. The window
# is the process named, or the first tab process if none is named.
$cdb = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"
if ($ProcessId -ne 0) { $p = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue }
else { $p = Get-Process tab -ErrorAction SilentlyContinue | Select-Object -First 1 }
if ($p -eq $null) { "no window running" | Set-Content $Log; "no window running"; exit 1 }
"watch began {0} pid {1}" -f (Get-Date -Format o), $p.Id | Set-Content $Log
$until = (Get-Date).AddMinutes($Minutes)
$before = @{}
foreach ($t in $p.Threads) { $before[$t.Id] = $t.TotalProcessorTime.TotalMilliseconds }
$whole = $p.TotalProcessorTime.TotalMilliseconds
while ((Get-Date) -lt $until)
{
    Start-Sleep -Seconds $Step
    $p = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
    if ($p -eq $null) { "window ended" | Add-Content $Log; break }
    $used = [int]($p.TotalProcessorTime.TotalMilliseconds - $whole)
    $whole = $p.TotalProcessorTime.TotalMilliseconds
    $hot = @()
    foreach ($t in $p.Threads)
    {
        $was = if ($before.ContainsKey($t.Id)) { $before[$t.Id] } else { 0 }
        $delta = [int]($t.TotalProcessorTime.TotalMilliseconds - $was)
        $before[$t.Id] = $t.TotalProcessorTime.TotalMilliseconds
        if ($delta -gt 0) { $hot += [pscustomobject]@{ id = $t.Id; ms = $delta } }
    }
    $hot = $hot | Sort-Object -Property ms -Descending
    $line = "{0} used_ms={1} " -f (Get-Date -Format HH:mm:ss), $used
    foreach ($h in $hot | Select-Object -First 4) { $line += ("t{0}={1}ms " -f $h.id, $h.ms) }
    $line | Add-Content $Log
    $line
    if ($used -ge $Spike -and $hot.Count -gt 0 -and (Test-Path $cdb))
    {
        $tid = "0x{0:x}" -f $hot[0].id
        $stack = & $cdb -pv -p $p.Id -y "$Symbols;srv*" -c "~~[$tid]k 40; q" 2>&1 | Select-String -Pattern "tab!|KERNELBASE!|ntdll!|win32u!" | ForEach-Object { $_.Line.Trim() } | Select-Object -First 40
        "  stack of $tid :" | Add-Content $Log
        $stack | ForEach-Object { "    $_" } | Add-Content $Log
    }
}
"watch ended {0}" -f (Get-Date -Format o) | Add-Content $Log
