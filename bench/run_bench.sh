#!/bin/sh
# usage: bench/run_bench.sh  - runs every benchmark, prints milliseconds
DIR=$(cd "$(dirname "$0")" && pwd)
for f in loop fib sends colls strs blocks; do
  ms=$(timeout ${BENCH_TIMEOUT:-300} "$DIR/../smalltalk" "$DIR/$f.st" 2>&1 | tail -1)
  printf '%-8s %8s ms\n' "$f" "$ms"
done
