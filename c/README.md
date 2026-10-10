# Smalltalk in portable C89

A C89 version of the interpreter, written to run later on VM/370 (CMS).
It loads the same class library (`../lib/*.st`) and passes the same regression
tests (`../tests/*.st`) as the CREXX version.  Layout, rules and checks follow
`mikebeer/apl`.

    make                  # builds ./st from the single file st_all.c
    make test             # runs ../tests against ./st (16_net and 23_unicode are skipped)
    ./st -L ../lib prog.st
    ./st -L ../lib -i     # interactive prompt

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

## Checked

gcc and clang with `-std=c89 -pedantic`; AddressSanitizer and UBSan clean on
all tests; the `-DST_SI30` build gives the same test output (except two
class names printed for large numbers).

## Speed

See the table in the main README.
