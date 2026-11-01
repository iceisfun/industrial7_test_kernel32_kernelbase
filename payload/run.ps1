# Runs every area exe listed in manifest.tsv (columns: area, exe, sha256, tests, run, timeout_s)
# one at a time, headless, with a per-area hard timeout. Each area gets a fresh scratch tree
# (TEMP/TMP/cwd all point into it). Raw stdout of each area is streamed to COM1 as soon as the
# area ends, as "### <area> rc=<code>" blocks. rc = exit code, -999 = killed on timeout,
# -998 = could not start, -997 = skipped by manifest (run=0).
param([string]$Root = 'C:\k32c', [string]$Out = 'C:\k32c\res.txt', [int]$DefaultTimeout = 600,
      [string]$Only = '', [int]$MaxBytes = 262144)
$ErrorActionPreference = 'Continue'
$areas = Join-Path $Root 'areas'
$work  = Join-Path $Root 'work'
New-Item -ItemType Directory -Force -Path $work | Out-Null
$rows = @(Get-Content (Join-Path $Root 'manifest.tsv') | Where-Object { $_ -and -not $_.StartsWith('#') -and -not $_.StartsWith("area`t") } |
          ForEach-Object { $f = $_ -split "`t"; [pscustomobject]@{ Area = $f[0]; Exe = $f[1]; Run = [int]$f[4]; Tmo = [int]$f[5] } })
if ($Only) { $sel = $Only -split ','; $rows = @($rows | Where-Object { $sel -contains $_.Area }) }

function Emit([string]$s) {
  # Results go to $Out only; run.cmd replays the file on COM1 when the runner returns (writing to the serial device from here is not reliable).
  Add-Content -Encoding ascii -Path $Out -Value $s
}
# Printable ASCII only on the wire: other bytes become \xNN (keeps serial.log parseable).
function Clean([byte[]]$b) {
  $sb = New-Object System.Text.StringBuilder
  foreach ($x in $b) {
    if (($x -ge 32 -and $x -lt 127) -or $x -eq 10 -or $x -eq 9) { [void]$sb.Append([char]$x) }
    elseif ($x -ne 13) { [void]$sb.AppendFormat('\x{0:x2}', $x) }
  }
  $sb.ToString()
}
function Cut([string]$t) {
  if ($t.Length -le $MaxBytes) { return $t }
  $h = [int]($MaxBytes * 3 / 4); $tl = $MaxBytes - $h
  $t.Substring(0, $h) + "`n...[cut " + ($t.Length - $MaxBytes) + " bytes]...`n" + $t.Substring($t.Length - $tl)
}

Set-Content -Encoding ascii -Path $Out -Value ''
function State([string]$tag) {
  # Snapshot of the few persistent things the audit (docs/SAFETY.md) says could change if a negative test misbehaves.
  $cn = ''; try { $k = 'HKLM:\SYSTEM\CurrentControlSet\Control\ComputerName'; $cn = (Get-ItemProperty "$k\ComputerName").ComputerName + '/' + (Get-ItemProperty "$k\ActiveComputerName").ComputerName } catch {}
  $tz = ''; try { $tz = (Get-TimeZone).Id } catch {}
  $pr = 0; try { $v = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager').PendingFileRenameOperations; if ($v) { $pr = @($v).Count } } catch {}
  $dd = ''; try { $dd = ((& cmd.exe /c 'mountvol' 2>$null | Measure-Object -Line).Lines) } catch {}
  $free = ''; try { $free = [int]((Get-PSDrive C).Free / 1MB) } catch {}
  $ul = ''; try { $ul = (Get-ItemProperty 'HKCU:\Control Panel\International').LocaleName + '/' + (Get-ItemProperty 'HKCU:\Control Panel\International\Geo').Name } catch {}
  Emit ("I7TC state $tag computername=$cn tz=$tz pendingrename=$pr mountvol_lines=$dd c_free_mb=$free userlocale=$ul")
}
Emit ('I7TC count=' + $rows.Count)
State 'pre'
$i = 0
foreach ($r in $rows) {
  $i++
  $exe = Join-Path $areas $r.Exe
  $tmo = if ($r.Tmo -gt 0) { $r.Tmo } else { $DefaultTimeout }
  if ($r.Run -eq 0) { Emit ("### $($r.Area) rc=-997`nSKIPPED by manifest`n"); continue }
  $sc = Join-Path $work $r.Area
  if (Test-Path $sc) { Remove-Item -Recurse -Force $sc -ErrorAction SilentlyContinue }
  New-Item -ItemType Directory -Force -Path (Join-Path $sc 'tmp'), (Join-Path $sc 'cwd') | Out-Null
  $so = Join-Path $sc 'stdout.bin'; $se = Join-Path $sc 'stderr.bin'
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.WorkingDirectory = (Join-Path $sc 'cwd'); $psi.UseShellExecute = $false
  $psi.CreateNoWindow = $true
  $psi.EnvironmentVariables['TEMP'] = (Join-Path $sc 'tmp'); $psi.EnvironmentVariables['TMP'] = (Join-Path $sc 'tmp')
  # cmd.exe does the file redirection so the output survives a kill (no pipe to drain).
  $psi.FileName = $env:ComSpec
  $psi.Arguments = '/d /c ""' + $exe + '" < NUL > "' + $so + '" 2> "' + $se + '""'
  $rc = -998
  try {
    $p = [System.Diagnostics.Process]::Start($psi)
    if (-not $p.WaitForExit($tmo * 1000)) { & taskkill.exe /F /T /PID $p.Id | Out-Null; $p.WaitForExit(15000) | Out-Null; $rc = -999 }
    else { $rc = $p.ExitCode }
  } catch { $rc = -998 }
  # Sweep stragglers: any process still running from the payload copy (orphaned children).
  $left = 0
  Get-Process | Where-Object { try { $_.Path -and $_.Path.StartsWith($areas, [StringComparison]::OrdinalIgnoreCase) } catch { $false } } |
    ForEach-Object { $left++; try { $_.Kill() } catch {} }
  Start-Sleep -Milliseconds 300
  $t = ''; $x = ''
  if (Test-Path $so) { $t = Clean ([IO.File]::ReadAllBytes($so)) }
  if (Test-Path $se) { $x = Clean ([IO.File]::ReadAllBytes($se)) }
  $blk = "### $($r.Area) rc=$rc`n" + (Cut $t)
  if ($x) { $blk += "`n[stderr] " + $x.Substring(0, [Math]::Min(512, $x.Length)) }
  # Leak report: scratch dirs the harness did not remove, HKCU keys left behind, stray processes.
  $leak = @(Get-ChildItem (Join-Path $sc 'tmp') -Force -ErrorAction SilentlyContinue | ForEach-Object { $_.Name })
  $hk = @(Get-ChildItem 'HKCU:\Software' -ErrorAction SilentlyContinue | Where-Object { $_.PSChildName -like 'k32t_*' } | ForEach-Object { $_.PSChildName })
  $hk | ForEach-Object { Remove-Item -Recurse -Force ('HKCU:\Software\' + $_) -ErrorAction SilentlyContinue }
  $blk += "`nI7TC leak area=$($r.Area) tmp=" + ($leak -join ',') + " hkcu=" + ($hk -join ',') + " stragglers=$left"
  Emit ($blk + "`n")
  Remove-Item -Recurse -Force $sc -ErrorAction SilentlyContinue
  if (($i % 10) -eq 0) { Emit "I7TC progress $i/$($rows.Count)" }
}
State 'post'
Emit ('I7TC done ' + $rows.Count)
exit 0
