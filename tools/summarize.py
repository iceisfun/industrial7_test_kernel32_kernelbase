#!/usr/bin/env python3
"""Aggregate results\\<area>.txt (stdout of build\\<area>.exe) into one table.
Usage: python tools/summarize.py [results_dir] [--failures]"""
import sys, os, re, glob
args = [a for a in sys.argv[1:] if not a.startswith('--')]
d = args[0] if args else 'results'
show = '--failures' in sys.argv
sm = re.compile(r'SUMMARY tests=(\d+) failed=(\d+) skipped=(\d+) checks_pass=(\d+) checks_fail=(\d+)')
tot = [0] * 5; rows = []; bad = []
for f in sorted(glob.glob(os.path.join(d, '*.txt'))):
    area = os.path.splitext(os.path.basename(f))[0]
    txt = open(f, errors='replace').read()
    m = sm.search(txt)
    if not m:
        note = 'TIMEOUT' if 'TIMEOUT' in txt else 'NO SUMMARY (crash/hang)'
        rows.append((area, '-', '-', '-', '-', '-', note)); bad.append(area); continue
    v = list(map(int, m.groups())); tot = [a + b for a, b in zip(tot, v)]
    rows.append((area, *v, 'FAIL' if v[1] else 'ok'))
    if v[1]: bad.append(area)
    if show:
        for l in txt.splitlines():
            if l.startswith('FAIL ') or l.startswith('  FAIL'): print(f'[{area}] {l.strip()}')
print(f'{"area":28}{"tests":>7}{"failed":>8}{"skip":>6}{"pass":>8}{"cfail":>7}  status')
for r in rows: print(f'{r[0]:28}{r[1]!s:>7}{r[2]!s:>8}{r[3]!s:>6}{r[4]!s:>8}{r[5]!s:>7}  {r[6]}')
print(f'{"TOTAL":28}{tot[0]:>7}{tot[1]:>8}{tot[2]:>6}{tot[3]:>8}{tot[4]:>7}')
print('areas with failures/problems:', ', '.join(bad) or 'none')
sys.exit(1 if bad else 0)
