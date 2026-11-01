@echo off
REM Payload entry point for a throwaway Windows guest. Contract (as the CRT corpus payload):
REM   - the payload directory is mounted read-only; this script copies what it needs to C:\k32c
REM   - progress and results go to COM1 (serial), markers are "I7TC ..." lines, one block per area:
REM       I7TC begin / I7TC results begin / I7TC count=N / ### <area> rc=<code> + raw stdout / I7TC done N / I7TC results end
REM   - exit code 0 when the runner finished (area failures are in the results, not in the exit code)
REM Headless: the runner opens no window and injects no input; console tests create their own hidden consoles in children. WER UI is disabled for this guest.
setlocal
mode COM1:115200,n,8,1 >nul
echo I7TC begin >COM1
if exist C:\k32c rmdir /s /q C:\k32c 2>nul
mkdir C:\k32c 2>nul
robocopy "%~dp0areas" C:\k32c\areas /E /NFL /NDL /NJH /NJS /NP >nul
copy /y "%~dp0manifest.tsv" C:\k32c\manifest.tsv >nul
rem No crash dialogs / dumps in the throwaway guest (the harness also sets SEM_NOGPFAULTERRORBOX).
reg add "HKCU\Software\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f >nul 2>nul
reg add "HKCU\Software\Microsoft\Windows\Windows Error Reporting" /v Disabled /t REG_DWORD /d 1 /f >nul 2>nul
reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v Disabled /t REG_DWORD /d 1 /f >nul 2>nul
reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f >nul 2>nul
reg delete "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps" /f >nul 2>nul
echo I7TC runner start >COM1
powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0run.ps1" -Root C:\k32c -Out C:\k32c\res.txt >C:\k32c\ps.out 2>C:\k32c\ps.err
set RC=%ERRORLEVEL%
echo I7TC runner returned %RC% >COM1
rem Results go to a file and are replayed on COM1 with `type` (what the CRT payload does): writing to \\.\COM1 from PowerShell is not reliable.
echo I7TC results begin >COM1
if exist C:\k32c\res.txt type C:\k32c\res.txt >COM1
echo I7TC results end >COM1
if exist C:\k32c\ps.err for %%F in (C:\k32c\ps.err) do if %%~zF GTR 0 (echo I7TC ps.err begin >COM1 & type C:\k32c\ps.err >COM1 & echo I7TC ps.err end >COM1)
if exist C:\k32c\ps.out for %%F in (C:\k32c\ps.out) do if %%~zF GTR 0 (echo I7TC ps.out begin >COM1 & type C:\k32c\ps.out >COM1 & echo I7TC ps.out end >COM1)
echo I7TC runner exit %RC% >COM1
exit /b %RC%
