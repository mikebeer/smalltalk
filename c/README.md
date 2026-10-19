# Smalltalk in portable C89

A C89 version of the interpreter, written to run later on VM/370 (CMS).
It loads the same class library (`../lib/*.st`) and passes the same regression
tests (`../tests/*.st`) as the CREXX version.  Layout, rules and checks follow
`mikebeer/apl`.

    make                  # builds ./st from the single file st_all.c
    make test             # runs ../tests against ./st (16_net and 23_unicode are skipped),
                          # then tests/c_only (garbage collector, with a 200 MB RSS check)
    ./st -L ../lib prog.st
    ./st -L ../lib -i     # interactive prompt
    ./st -nogc ...        # disable the garbage collector
    ./st -gcstress=N ...  # collect at every N-th safe point (testing); -gcverbose logs

## Layout

    src/s0_base.c    types, globals, prototypes shared by all parts
    src/s1_obj.c     memory, hash maps, objects, classes, methods, boot
    src/s2_lex.c     lexer
    src/s3_parse.c   parser, class bodies, top-level items, file-in
    src/s4_res.c     resolver (variables, inlined control flow), doits
    src/s5_eval.c    frames, message sends, blocks, evaluator
    src/s7_num.c     numbers: SmallInteger, big integers, floats, printing
    src/s8_str.c     strings, arrays, dictionaries, exception signalling
    src/s9_sys.c     blocks, exceptions, classes, files, system services
    src/s_gc.c       garbage collector (mark and sweep)
    src/s6_main.c    initialisation, prompt, main
    src/s_gen.h      constants and primitive numbers (tools/gen_consts.py)
    src/st.c         includes the parts; tools/amalg.sh inlines them into
    st_all.c         ONE self-contained source file (generated; checked in)

## Portability rules

* Strict C89: no `//`, no declarations after statements, no `long long`, no
  `stdint.h`.  `gcc -std=c89 -pedantic -Wall -Wextra` is clean.
* All functions are `static`; the only external symbol is `main`.
* Character classes use literal sets, never code point ranges.
* A SmallInteger is a tagged `long`: +-10^18 where `long` has 64 bits, +-2^30
  where it has 32 bits (VM/370); beyond that numbers are big integers.
  `-DST_SI30` selects the 32-bit range on a 64-bit host for testing.
* Operating-system services (directories, `popen`) are only used with
  `-DST_POSIX`; sockets and Unicode are not part of the C version.
* Object, slot and frame tables are plain arrays that grow with `realloc`:
  never keep a pointer into them across a call that can allocate.

## Garbage collector

A precise mark-and-sweep collector (`src/s_gc.c`).  Objects are entries of the object
table (an oop is the index), so they never move and the table is swept in place:
dead objects go to a free list and their string memory is freed.  Roots: globals, class
table, symbols, characters, the literals in the syntax tree, the argument stack `ast[]`,
the handler stack, `unw_v`, the frames being executed (`act_f[]`) and the frames reachable
from live blocks (their slots, outer and home frames).  Frames carry their size
(`vsz[]`); unreachable ones return to the frame free lists, so methods that create blocks
no longer leak their frames.  Dictionary entries of dead dictionaries and removed entries
are recycled, and the slot array `sl[]` is compacted when more than half of it is dead.

* A collection runs only at a *safe point*: entry of a non-primitive method or block, and
  each iteration of `whileTrue:`, `to:do:`, `timesRepeat:`, `repeat`.  Primitives therefore
  never see a collection while they run, and C locals need no registration.  The few C
  callers that hold an oop across a callback keep it on `ast[]`: `do_send` keeps the
  receiver at `ast[base]` while arguments are evaluated and the send runs, cascades, brace
  arrays, the general `to:do:` loop, `ensure:` / `ifCurtailed:` and the top-level value.
* The trigger is the volume allocated since the last collection (object, slot and string
  bytes) compared with `max(4 MB, live volume)`, so the heap stays at about twice the
  live data.  `-nogc` disables it; `-gcstress=N` collects at every N-th safe point and
  always compacts, which exposes unregistered references (all tests are run with
  N = 1, 2, 3, 7, 13, 97, 1000).
* Effect: 200,000 `s := s , x` appends (the benchmark string task, which was killed by the
  kernel at 110,000) run in 3 s with a peak of 72 MB; a 1.5 million iteration loop that
  allocates strings, arrays and blocks stays below 75 MB.  `Behavior allInstances` now
  reports only live objects.
* `tests/c_only/gc1.st` (large) and `gc2.st` (small, also run under `-gcstress`) are
  C-only tests: the CREXX version has no collector and cannot run them.

## Checked

gcc and clang with `-std=c89 -pedantic`; AddressSanitizer and UBSan clean on
all tests (also with `-gcstress`); `-m32` build and `-DST_SI30` give the same output; the `-DST_SI30` build gives the same test output (except two
class names printed for large numbers).

## Speed

See the table in the main README.
