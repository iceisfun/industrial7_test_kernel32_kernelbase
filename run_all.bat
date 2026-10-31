@echo off
rem Runs every build\*.exe with a per-exe timeout, writing results\<area>.txt, then summarises.
rem Run from a normal (non-elevated unless noted in README) cmd.exe console window, not from a service or CI session.
setlocal enabledelayedexpansion
cd /d "%~dp0"
if not exist results mkdir results
set TMO=600
for %%F in (build\*.exe) do (
  echo === %%~nF
  rem start /b + waitfor-less timeout: PowerShell enforces the limit and kills the exe and its children on expiry
  powershell -NoProfile -Command "$p = Start-Process -FilePath '%%F' -NoNewWindow -PassThru -RedirectStandardOutput 'results\%%~nF.txt' -RedirectStandardError 'results\%%~nF.err'; if (-not $p.WaitForExit(%TMO%*1000)) { taskkill.exe /F /T /PID $p.Id | Out-Null; Add-Content 'results\%%~nF.txt' 'TIMEOUT'; exit 124 } else { exit $p.ExitCode }"
  echo     exit code !errorlevel!
)
where python >nul 2>nul && python tools\summarize.py results || echo python not found: run tools\summarize.py results later
endlocal
