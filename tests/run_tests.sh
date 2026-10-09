#!/bin/sh
# Regression tests: for every tests/NAME.st run the interpreter and compare
# stdout+stderr with tests/NAME.out.  Optional file NAME.in is fed to stdin.
# "run_tests.sh --update" rewrites the expected output files.
DIR=$(cd "$(dirname "$0")" && pwd)
ST="$DIR/../smalltalk"
pass=0; fail=0
for src in "$DIR"/*.st; do
  name=$(basename "$src" .st)
  input=/dev/null; [ -f "$DIR/$name.in" ] && input="$DIR/$name.in"
  actual=$("$ST" "$src" < "$input" 2>&1)
  if [ "$1" = "--update" ]; then
    printf '%s\n' "$actual" > "$DIR/$name.out"; echo "updated $name"; continue
  fi
  if [ "$actual" = "$(cat "$DIR/$name.out")" ]; then
    pass=$((pass+1)); echo "ok   $name"
  else
    fail=$((fail+1)); echo "FAIL $name"
    printf '%s\n' "$actual" | diff - "$DIR/$name.out" | head -10
  fi
done
echo "passed $pass, failed $fail"
[ "$fail" -eq 0 ]
