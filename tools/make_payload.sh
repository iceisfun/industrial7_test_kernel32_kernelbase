#!/bin/bash
# make_payload.sh BUILD_DIR OUT_DIR [mingw-prefix]
# Lays out a Windows-guest payload from the built area exes:
#   OUT/run.cmd run.ps1 README.md manifest.tsv testlists/<area>.txt areas/<area>.exe
# manifest.tsv columns: area exe sha256 tests run timeout_s   (tests = registered TEST() count read
# from the exe's symbol table; run=0 skips the area; timeout_s=0 means the runner default).
# Optional: SKIP="area1 area2" excludes areas (run=0), TIMEOUTS="area=sec ..." overrides timeouts.
set -eu
B=${1:?build dir}; O=${2:?out dir}; PFX=${3:-x86_64-w64-mingw32-}
H=$(cd "$(dirname "$0")/.." && pwd)
rm -rf "$O"; mkdir -p "$O/areas" "$O/testlists"
cp "$H/payload/run.cmd" "$H/payload/run.ps1" "$H/payload/README.md" "$O/"
printf 'area\texe\tsha256\ttests\trun\ttimeout_s\n' > "$O/manifest.tsv"
for e in "$B"/*.exe; do
  a=$(basename "$e" .exe)
  cp "$e" "$O/areas/$a.exe"
  ${PFX}nm "$e" | sed -n 's/.* [tT] test_//p' | sort > "$O/testlists/$a.txt"
  n=$(wc -l < "$O/testlists/$a.txt")
  run=1; case " ${SKIP:-} " in *" $a "*) run=0;; esac
  t=0; for kv in ${TIMEOUTS:-}; do [ "${kv%%=*}" = "$a" ] && t=${kv#*=}; done
  printf '%s\t%s.exe\t%s\t%s\t%s\t%s\n' "$a" "$a" "$(sha256sum "$e" | cut -d' ' -f1)" "$n" "$run" "$t" >> "$O/manifest.tsv"
done
(cd "$O" && find . -type f ! -name payload.sha256 | sort | xargs sha256sum > payload.sha256)
echo "payload: $O ($(ls "$O/areas" | wc -l) areas, $(awk -F'\t' 'NR>1{s+=$4}END{print s}' "$O/manifest.tsv") tests)"
