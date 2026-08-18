#!/usr/bin/env bash
#
# Runs every topology in tests/input/ through the simulator twice -- once with
# poisoned reverse enabled, once with it disabled -- and diffs the result
# against the reference outputs committed under tests/expected/.
#
#   ./scripts/run_tests.sh            compare against tests/expected/
#   ./scripts/run_tests.sh --regen    overwrite tests/expected/ with fresh runs
#
set -u

cd "$(dirname "$0")/.."

BIN=./rip
[ -x "$BIN" ] || BIN=./rip.exe
if [ ! -x "$BIN" ]; then
  echo "error: build the simulator first (make)" >&2
  exit 1
fi

REGEN=0
[ "${1:-}" = "--regen" ] && REGEN=1

OUT=tests/actual
[ "$REGEN" -eq 1 ] && OUT=tests/expected
mkdir -p "$OUT"

fail=0
for input in tests/input/*.txt; do
  name=$(basename "$input" .txt)
  for mode in poisoned no-poison; do
    # main.cpp reads the topology, then prompts for the routing mode on stdin.
    [ "$mode" = poisoned ] && choice=1 || choice=0
    result="$OUT/$name.$mode.txt"
    { cat "$input"; echo "$choice"; } | "$BIN" > "$result" 2>&1

    if [ "$REGEN" -eq 1 ]; then
      echo "regen  $name [$mode]"
      continue
    fi

    if diff -q "tests/expected/$name.$mode.txt" "$result" >/dev/null 2>&1; then
      echo "ok     $name [$mode]"
    else
      echo "FAIL   $name [$mode]"
      diff "tests/expected/$name.$mode.txt" "$result" | head -20
      fail=1
    fi
  done
done

if [ "$REGEN" -eq 1 ]; then
  echo "Reference outputs regenerated under tests/expected/."
  exit 0
fi

[ "$fail" -eq 0 ] && echo "All topologies match the reference output." \
                  || echo "Some topologies drifted from the reference output."
exit "$fail"
