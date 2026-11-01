#!/usr/bin/env python3
"""Convert a guest serial.log produced by payload/run.ps1 into per-area raw files and TSVs.
Usage: serial_to_results.py SERIAL_LOG OUT_DIR [PAYLOAD_DIR]
Writes OUT_DIR/raw/<area>.txt, OUT_DIR/areas.tsv (area rc status tests failed skipped checks_pass checks_fail),
OUT_DIR/tests.tsv (area test status), OUT_DIR/summary.json.
Per-test status: OK, FAIL, SKIP, HANG (RUN seen, no result line: the test crashed the area or hung until
the area timeout), NOTRUN (listed in PAYLOAD_DIR/testlists/<area>.txt, never started).
Area status: PASS, FAIL, CRASH (no SUMMARY, exited), TIMEOUT (rc -999), NOSTART (-998), SKIPPED (-997), MISSING."""
import json, os, re, sys

def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    log, out = sys.argv[1], sys.argv[2]
    pay = sys.argv[3] if len(sys.argv) > 3 else None
    s = open(log, 'rb').read().decode('latin-1').replace('\r\n', '\n').replace('\r', '\n')
    a = s.find('I7TC results begin'); b = s.rfind('I7TC results end')
    body = s[a:b if b > a else len(s)] if a >= 0 else ''
    os.makedirs(os.path.join(out, 'raw'), exist_ok=True)
    parts = re.split(r'^### (\S+) rc=(-?\d+)\n', body, flags=re.M)
    areas, tests, tot = [], [], {}
    seen = set()
    for i in range(1, len(parts), 3):
        area, rc, txt = parts[i], int(parts[i + 1]), parts[i + 2]
        txt = re.sub(r'^I7TC (done|count|progress)[^\n]*\n?', '', txt, flags=re.M)
        open(os.path.join(out, 'raw', area + '.txt'), 'w').write(txt + 'EXITCODE=%d\n' % rc)
        seen.add(area)
        m = re.search(r'^SUMMARY tests=(\d+) failed=(\d+) skipped=(\d+) checks_pass=(\d+) checks_fail=(\d+)', txt, re.M)
        res = {}; order = []
        for ln in txt.split('\n'):
            mm = re.match(r'^(RUN|OK  |FAIL|SKIP) (\S+)$', ln)
            if not mm: continue
            k, n = mm.group(1).strip(), mm.group(2)
            if k == 'RUN': res.setdefault(n, 'HANG'); order.append(n)
            else: res[n] = k
        if pay:
            lst = os.path.join(pay, 'testlists', area + '.txt')
            if os.path.exists(lst):
                for n in open(lst).read().split():
                    res.setdefault(n, 'NOTRUN')
        if rc == -997: st = 'SKIPPED'
        elif rc == -999: st = 'TIMEOUT'
        elif rc == -998: st = 'NOSTART'
        elif not m: st = 'CRASH'
        else: st = 'PASS' if rc == 0 and m.group(2) == '0' else 'FAIL'
        v = m.groups() if m else ('', '', '', '', '')
        areas.append((area, rc, st) + tuple(v))
        for n, k in res.items(): tests.append((area, n, k))
    if pay:
        mf = os.path.join(pay, 'manifest.tsv')
        if os.path.exists(mf):
            for ln in open(mf).read().split('\n')[1:]:
                f = ln.split('\t')
                if len(f) > 4 and f[0] not in seen:
                    areas.append((f[0], '', 'MISSING', '', '', '', '', ''))
    with open(os.path.join(out, 'areas.tsv'), 'w') as f:
        f.write('area\trc\tstatus\ttests\tfailed\tskipped\tchecks_pass\tchecks_fail\n')
        for r in areas: f.write('\t'.join(map(str, r)) + '\n')
    with open(os.path.join(out, 'tests.tsv'), 'w') as f:
        f.write('area\ttest\tstatus\n')
        for r in tests: f.write('\t'.join(r) + '\n')
    sm = {}
    for r in areas: sm[r[2]] = sm.get(r[2], 0) + 1
    ts = {}
    for r in tests: ts[r[2]] = ts.get(r[2], 0) + 1
    json.dump({'areas': sm, 'tests': ts, 'complete': b > a >= 0}, open(os.path.join(out, 'summary.json'), 'w'))
    print('areas', sm, 'tests', ts, 'complete' if b > a >= 0 else 'INCOMPLETE (no results end marker)')

main()
