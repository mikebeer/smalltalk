#!/bin/sh
# Run the shared regression tests (../tests/NAME.st, expected ../tests/NAME.out)
# with the C interpreter.  ST_FLAGS adds interpreter options (e.g. -gcstress=7).  Tests listed in tests/skip.txt are not run.
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$DIR/../.."
ST="$DIR/../st"
pass=0; fail=0; skip=0
for src in "$ROOT"/tests/*.st; do
  name=$(basename "$src" .st)
  if grep -qx "$name" "$DIR/skip.txt" 2>/dev/null; then
    skip=$((skip+1)); echo "skip $name"; continue
  fi
  input=/dev/null; [ -f "$ROOT/tests/$name.in" ] && input="$ROOT/tests/$name.in"
  flags=""; [ -f "$ROOT/tests/$name.flags" ] && flags=$(cat "$ROOT/tests/$name.flags")
  actual=$(cd "$ROOT" && timeout 120 "$ST" -L lib $ST_FLAGS $flags "$src" < "$input" 2>&1)
  if [ "$actual" = "$(cat "$ROOT/tests/$name.out")" ]; then
    pass=$((pass+1)); echo "ok   $name"
  else
    fail=$((fail+1)); echo "FAIL $name"
    if [ -n "$VERBOSE" ]; then
      printf '%s\n' "$actual" > /tmp/st_actual.txt
      diff /tmp/st_actual.txt "$ROOT/tests/$name.out" | head -${VERBOSE}
    fi
  fi
done

# C-only tests (tests/c_only/NAME.st, expected NAME.out): the garbage collector.
# gc1 must also stay under 200 MB resident (measured with /usr/bin/time -v or python3).
for src in "$DIR"/c_only/*.st; do
  [ -f "$src" ] || continue
  name=c_only/$(basename "$src" .st)
  case "$ST_FLAGS" in *gcstress*) [ "$name" = c_only/gc1 ] && continue;; esac   # too slow when collecting at every safe point
  actual=$(cd "$ROOT" && timeout 300 "$ST" -L lib $ST_FLAGS "$src" 2>&1)
  if [ "$actual" = "$(cat "${src%.st}.out")" ]; then
    pass=$((pass+1)); echo "ok   $name"
  else
    fail=$((fail+1)); echo "FAIL $name"
  fi
done
if [ -z "$ST_FLAGS" ] && [ -f "$DIR/c_only/gc1.st" ]; then
  if [ -x /usr/bin/time ]; then
    kb=$(cd "$ROOT" && /usr/bin/time -v "$ST" -L lib "$DIR/c_only/gc1.st" 2>&1 >/dev/null | sed -n 's/.*Maximum resident set size (kbytes): //p')
  else
    kb=$(cd "$ROOT" && python3 -c 'import resource,subprocess,sys
subprocess.run(sys.argv[1:],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
print(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)' "$ST" -L lib "$DIR/c_only/gc1.st" 2>/dev/null)
  fi
  if [ -n "$kb" ] && [ "$kb" -lt 200000 ]; then
    pass=$((pass+1)); echo "ok   c_only/gc1 max RSS ${kb} KB (< 200000)"
  elif [ -n "$kb" ]; then
    fail=$((fail+1)); echo "FAIL c_only/gc1 max RSS ${kb} KB"
  else
    echo "skip c_only/gc1 RSS check (no /usr/bin/time or python3)"
  fi
fi
echo "passed $pass, failed $fail, skipped $skip"
