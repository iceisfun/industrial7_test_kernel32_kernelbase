# Windows guest payload

This directory holds the runner that executes every built area exe in a throwaway Windows guest
and reports over the serial port. `tools/make_payload.sh` assembles the full payload (runner +
exes + manifest) from a `make` build.

    make -j4
    tools/make_payload.sh build /path/to/payload-k32
    # optional: SKIP="area1 area2" TIMEOUTS="file_io=900" tools/make_payload.sh ...

Payload layout:

    run.cmd  run.ps1  README.md
    manifest.tsv        area  exe  sha256  tests  run  timeout_s   (run=0 skips; timeout_s=0 -> 600)
    areas/<area>.exe    statically linked, no dependencies beyond system DLLs
    testlists/<area>.txt  registered test names per area (from the exe symbol table)
    payload.sha256

## Run contract

`run.cmd` is started at logon by the guest runner (the payload directory is a read-only disc).
It copies the exes to `C:\k32c`, runs each area one at a time under `cmd /c` with stdout and stderr
redirected to files (so a hang or kill never loses the output), a hard per-area timeout
(default 600 s, process tree killed with `taskkill /F /T`), a fresh per-area scratch tree
(`TEMP`, `TMP` and the working directory all point into it), and streams the raw stdout of each
area to COM1 as soon as the area ends:

    I7TC begin
    I7TC results begin
    I7TC count=<areas>
    ### <area> rc=<exit code>        rc: -999 killed on timeout, -998 not started, -997 skipped
    <raw stdout: RUN/OK/FAIL/SKIP per test, "  FAIL file:line expr (detail)", SUMMARY ...>
    [stderr] <first 512 bytes>
    I7TC leak area=<area> tmp=<leftover scratch entries> hkcu=<leftover HKCU\Software\k32t_* keys> stragglers=<killed leftover processes>
    ...
    I7TC done <areas>
    I7TC results end

Non-printable bytes are written as `\xNN`; per-area output is capped at 256 KB (head + tail kept).
After each area, leftover processes started from `C:\k32c\areas`, leftover `k32t_*` registry
keys and leftover scratch files are reported on the `I7TC leak` line and then removed.
The runner exit code is 0 when it ran to the end (area failures are in the results).
The harness is unchanged: `common/t.c` already prints one flushed line per test; a test that
hangs or crashes the whole exe shows as `RUN <name>` without a result line (HANG).

## Converting the serial log

    tools/serial_to_results.py serial.log OUT /path/to/payload-k32

writes `OUT/raw/<area>.txt`, `OUT/areas.tsv` (PASS/FAIL/CRASH/TIMEOUT per area), `OUT/tests.tsv`
(OK/FAIL/SKIP/HANG/NOTRUN per test) and `OUT/summary.json`. The `### <area> rc=` framing and the
`I7TC` markers are the same as the CRT corpus payload, so its converter also splits the raw files.

## Safety

See `docs/SAFETY.md` for the per-area audit and the guards this runner implements. Run only in a
throwaway guest (snapshot or overlay discarded afterwards), never on a workstation.
