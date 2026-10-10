# Smalltalk-80 interpreter in portable C89
CC ?= cc
CFLAGS ?= -O2
STD = -std=c89 -pedantic -Wall -Wextra
POSIX = -DST_POSIX

st: st_all.c
	$(CC) $(STD) $(CFLAGS) $(POSIX) -o st st_all.c -lm

st_all.c: src/st.c src/s0_base.c src/s1_obj.c src/s2_lex.c src/s3_parse.c src/s4_res.c src/s5_eval.c src/s6_main.c src/s7_num.c src/s8_str.c src/s9_sys.c src/s_gen.h
	sh tools/amalg.sh > st_all.c

test: st
	sh tests/run_tests.sh

clean:
	rm -f st
