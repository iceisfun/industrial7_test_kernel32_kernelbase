# kernel32 / kernelbase conformance corpus

Black-box tests for Windows 10 x64 `kernel32.dll` and `kernelbase.dll`. The functions under test are
resolved with `GetProcAddress` (`U(Name)`: kernel32 -> kernelbase -> ntdll; `UK`/`UB`/`UNT` pin a module,
which matters for forwarders), typed from the SDK headers.

    make                        # build/<area>.exe (mingw-w64)
    build\<area>.exe [substr]   # on Windows; optional test-name filter

Tests re-launch the exe as a child (`--child <name>`) when they need a separate process, and use a
private scratch directory `%TEMP%\k32t_<pid>\` that is removed on exit.
Output: `RUN/OK/FAIL/SKIP` per test, `  FAIL file:line expr (detail)` per failed check, `SUMMARY` last; exit 1 on failure.
`tools/kernel32_exports.txt` / `tools/kernelbase_exports.txt` list named exports.
