#!/bin/sh
# Inline the parts of src/st.c into one self-contained C file (stdout).
# Usage: sh tools/amalg.sh > st_all.c
cd "$(dirname "$0")/../src" || exit 1
awk '
/^#include "s[0-9_a-z]+\.[ch]"$/ {
  f = $2; gsub(/"/, "", f);
  print "/* ===== " f " ===== */";
  while ((getline line < f) > 0) print line;
  close(f);
  next
}
{ print }' st.c
