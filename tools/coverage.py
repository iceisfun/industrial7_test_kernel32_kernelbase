#!/usr/bin/env python3
"""Coverage measurer: which exports are called (not merely resolved) by tests/.
Usage: tools/coverage.py [--json] [--thin N]
A "use" is U/UK/UB/UNT/UN/U_/NTP(Name) or t_proc*("Name") outside REQUIRE(...)
and outside #define lines. A/W variants count separately."""
import re, sys, os, collections, json
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rd = lambda f: [l.strip() for l in open(os.path.join(root, 'tools', f)) if l.strip()]
pub = set(rd('public_surface.txt')); k32 = set(rd('kernel32_exports.txt')); kb = set(rd('kernelbase_exports.txt'))
pat = re.compile(r'\b(?:UK|UB|UNT|UN|U_|U|NTP)\(\s*([A-Za-z_]\w*)\s*\)|\bt_proc\w*\(\s*"([A-Za-z_]\w*)"\s*\)')
req = re.compile(r'REQUIRE\s*\((?:[^()]|\((?:[^()]|\([^()]*\))*\))*\)')
uses = collections.Counter(); files = collections.defaultdict(set)
for d, _, fs in os.walk(os.path.join(root, 'tests')):
    for f in fs:
        if not f.endswith(('.c', '.h')): continue
        for ln in open(os.path.join(d, f), errors='replace'):
            if ln.lstrip().startswith('#define'): continue
            s = req.sub('', ln)
            for m in pat.finditer(s):
                n = m.group(1) or m.group(2); uses[n] += 1; files[n].add(f)
allx = pub | k32 | kb
pu = sorted(n for n in pub if not uses[n])
ou = sorted(n for n in (k32 | kb) - pub if not uses[n])
thin = sorted((n for n in pub if uses[n] == 1), key=lambda n: n)
if '--json' in sys.argv:
    print(json.dumps(dict(publicUncovered=pu, otherUncovered=ou, thin=thin)))
else:
    print(f"public {len(pub)} uncovered {len(pu)}; other {len(allx-pub)} uncovered {len(ou)}; public single-use {len(thin)}")
    print("PUBLIC UNCOVERED:\n" + "\n".join(pu)); print("OTHER UNCOVERED:\n" + "\n".join(ou)); print("SINGLE-USE PUBLIC:\n" + "\n".join(thin))
