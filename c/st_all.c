/* Smalltalk-80 interpreter in portable C89 - the parts are inlined by tools/amalg.sh */
/* ===== s0_base.c ===== */
/* =========================================================================
   Smalltalk-80 interpreter in portable C89 (port of the CREXX version).

   s0_base.c   types, globals, memory growth, output, string helpers,
               hash maps, prototypes of everything shared between parts.

   Portability rules: strict C89 (no //, no declarations after statements,
   no long long, no stdint.h), all functions static, character classes by
   literal sets.  Integers are 32-bit safe: a SmallInteger is a tagged
   long holding -2^30 .. 2^30-1; larger integers are big integers.
   ========================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include <time.h>
#include <limits.h>


#define ST_VERSION "1.0"
#if LONG_MAX > 2147483647L && !defined(ST_SI30)
#define SI_MAX 999999999999999999L
#define SI_MIN (-999999999999999999L)
#define SI_MULLIM 1000000000L
#define SI_DIGITS 18
#else
#define SI_MAX 1073741823L
#define SI_MIN (-1073741823L - 1L)
#define SI_MULLIM 32768L
#define SI_DIGITS 9
#endif

/* ---- oops: SmallInteger = 2n+1, heap object = 2*index ---------------- */
typedef long oop;

#define IS_INT(x) (((x) & 1L) != 0)
#define INTVAL(x) (((x) - 1L) / 2L)
#define MKINT(n) ((n) * 2L + 1L)
#define HI(x) ((x) / 2L)

#define NIL_OOP 2L
#define TRUE_OOP 4L
#define FALSE_OOP 6L

/* ---- object table ----------------------------------------------------- */
typedef struct {
    int cls;       /* class index */
    int kind;      /* KD_* */
    long n;        /* number of slots */
    long slot;     /* first slot in sl[] */
    long x;        /* extra: class index, dictionary id, node, char code */
    char *s;       /* string/symbol/bigint text (NUL-terminated) */
    long slen;     /* its length */
    double f;      /* float value */
} Obj;

static Obj *objs;
static long no, objcap;
static oop *sl;
static long nsl, slcap;

#define OC(q) (objs[HI(q)].cls)
#define OK(q) (objs[HI(q)].kind)
#define ON(q) (objs[HI(q)].n)
#define OSLOT(q) (objs[HI(q)].slot)
#define OX(q) (objs[HI(q)].x)
#define OS(q) (objs[HI(q)].s)
#define OSLEN(q) (objs[HI(q)].slen)
#define OF(q) (objs[HI(q)].f)
#define SLOT(q, i) (sl[objs[HI(q)].slot + (i)])

/* ---- classes ---------------------------------------------------------- */
typedef struct {
    char *name;
    char *ivn;       /* instance variable names, blank separated */
    int super_;
    int meta;
    int ismeta;
    int this_;
    int kind;
    int ninst;
    int lib;
    oop oop_;
} Cls;
static Cls *cls;
static long nc, clscap;

/* ---- methods ---------------------------------------------------------- */
typedef struct {
    int sel;
    int cls;
    int nargs;
    long body;
    int ntemps;
    int hasblk;
    int prim;
    long src;
} Meth;
static Meth *meths;
static long nmeth, methcap;
static long gen;

/* ---- syntax nodes ----------------------------------------------------- */
typedef struct {
    int t;
    long a, b, c, d, i;
    char *s;
} Node;
static Node *nodes;
static long nnode, nodecap;
static long *al;
static long nal, alcap;

#define NT(n) (nodes[n].t)
#define NA(n) (nodes[n].a)
#define NB(n) (nodes[n].b)
#define NC(n) (nodes[n].c)
#define ND(n) (nodes[n].d)
#define NI(n) (nodes[n].i)
#define NS(n) (nodes[n].s)

/* ---- selectors, globals ----------------------------------------------- */
static char **sel_name;
static int *sel_n;
static long nsel, selcap;
static long *g_val;
static char **g_name;
static long ng, gcap;

/* ---- frames and stacks ------------------------------------------------ */
static oop *vp;
static long nvp, vpcap;
static long *fl_head;
static long flcap;
static oop *ast;
static long asp, astcap;
#define CDEPTH_MAX 5000
static long *cs_m;
static oop *cs_r;
static long cdepth;
static int in_dnu;

/* ---- unwinding and handlers ------------------------------------------- */
static int unw;
static long unw_t;
static oop unw_v;
static long htop, hid, sigcount;
static oop *h_cls, *h_blk;
static long *h_id;
static long hcap;

/* ---- dictionaries ----------------------------------------------------- */
static long nd0, ne;
static long *dd_head, *dd_tail, *dd_n;
static long ddcap;
static oop *de_key, *de_val;
static long *de_next;
static int *de_live;
static long decap;

/* ---- well known classes ----------------------------------------------- */
static int cls_Object, cls_Class, cls_Metaclass, cls_String, cls_Symbol;
static int cls_Float, cls_Character, cls_Array, cls_SmallInteger;
static int cls_LargePositiveInteger, cls_LargeNegativeInteger;
static int cls_BlockClosure, cls_UndefinedObject, cls_Message;
static oop chr_oop[256];

/* ---- state ------------------------------------------------------------ */
static int booted, halted, rc_exit, repl_mode, repl_echo, in_eval;
static int stdin_eof;
static long rnd_state;
static int nsargs;
static char **sargs;
static char *libdir;
static int sys_rc;
static char *sys_err;

typedef struct SMap SMap;
struct SMap {
    char **keys;
    long *vals;
    unsigned char *used;   /* 0 empty, 1 live, 2 deleted */
    long cap, cnt, fill;
};
static SMap symtab, cmap, cvmap, gmap, selmap, dmap;

/* ---- lexer / parser state (s2/s3) -------------------------------------- */
typedef struct {
    int t;
    char *s;
    long slen;
    long i;
    double f;
    long p;
} Tok;

#define GROW(T, p, cap, need) do { if ((long)(need) >= (cap)) p = (T *)xgrow(p, &(cap), (long)(need), sizeof(T)); } while (0)

/* ---- prototypes: s0 ---------------------------------------------------- */
static void fatal(const char *msg);
static void *xmalloc(size_t n);
static void *xrealloc(void *p, size_t n);
static void *xgrow(void *p, long *cap, long need, size_t esz);
static char *dupn(const char *s, long n);
static char *dups(const char *s);
static void outs(const char *s);
static void outn(const char *s, long n);
static void flush_out(void);

typedef struct {
    char *p;
    long n, cap;
} SB;
static void sb_init(SB *b);
static void sb_addn(SB *b, const char *s, long n);
static void sb_adds(SB *b, const char *s);
static void sb_addc(SB *b, int c);
static void sb_addl(SB *b, long v);
static char *sb_str(SB *b);   /* NUL-terminated, valid until next add */
static void sb_free(SB *b);

static void smap_init(SMap *m);
static int smap_get(SMap *m, const char *key, long *val);
static void smap_put(SMap *m, const char *key, long val);
static void smap_del(SMap *m, const char *key);

/* character classes (literal sets, EBCDIC safe) */
static int is_letter(int c);
static int is_digit(int c);
static int is_upper(int c);
static int is_binch(int c);
static int is_sep(int c);
static int to_upper(int c);
static int to_lower(int c);

/* ---- prototypes: s1 objects, classes, methods ------------------------- */
static oop mkint(long n);
static long intval(oop x);
static oop eval_string(const char *text);
static oop newobj(int cls_, int kind, long n);
static oop mkstr(const char *s);
static oop mkstrn(const char *s, long n);
static oop mksym(const char *s);
static oop mkfloat(double f);
static oop mkchar(long code);
static oop mkarray(long n);
static oop mkbool(int b);
static int cof(oop x);
static int kindof(oop x);
static const char *strof(oop x);   /* "" for non-strings; String, Symbol, BigInt */
static long strlen_of(oop x);
static int isstr(oop x);
static void set_str(oop o, const char *s, long n);
static int gcell(const char *name);
static void gset(const char *name, oop v);
static int selid(const char *name);
static int find_class(const char *name);
static int define_class(const char *name, int sup);
static void add_ivar(int c, const char *name);
static int install_method(int c, int sel, int nargs, long body, int ntemps, int hasblk, int prim);
static int lookup(int c, int sel);
static int inherits(int c, int d);
static int find_cvar(int c0, const char *name);
static void define_classvar(int c0, const char *name, long e);
static void declare_classvar(int c, const char *name);
static void boot(void);
static oop new_instance(int c, long n);
static oop shallow_copy(oop x);
static int wordpos_str(const char *w, const char *list);   /* 1-based, 0 if absent */
static int count_words(const char *list);

/* ---- prototypes: lexer/parser/resolver/top level (s2..s5) ------------- */
static void file_in(const char *text, long len, const char *name);
static void compile_method(int c, const char *sel, long pfirst, int np, long tfirst, int ntemps, long body, int prim, long startp);
static void run_doit(long st);
static long eval_in_class(int c, long e);
static void run_scheduler(void);
static char *read_file(const char *fn, long *len);
static void repl_loop(void);

/* ---- prototypes: evaluator (s6) --------------------------------------- */
static oop ev(long n, long fb);
static oop send_sel(oop rcv, int sel, long base, int na);
static oop invoke(int m, oop rcv, long base, int na);
static oop call_block(oop blk, long base, int na);
static void st_error(oop rcv, const char *msg);
static void print_trace(void);
static void init_temps(long base, long n);
static long alloc_frame(long size);
static char *printstr0(oop x);   /* malloc'ed */
static oop make_context(void);
static void ast_push(oop v);

/* ---- prototypes: primitives (s7..s9) ----------------------------------
   A primitive receives its arguments at ast[base+1 .. base+na] and
   answers 0 on failure (the Smalltalk fallback code then runs). */
static oop prim(int p, oop rcv, long base, int na);
static oop prim_a(int p, oop rcv, long base, int na);   /* numbers, objects, reflection */
static oop prim_b(int p, oop rcv, long base, int na);   /* strings, arrays, dictionaries */
static oop prim_c(int p, oop rcv, long base, int na);   /* blocks, exceptions, classes, system */
static oop prim_net(int p, oop rcv, long base, int na); /* sockets (stubs) */

/* numbers (s7) */
static int numkind(oop x);                  /* 0 none, 1 SmallInteger, 2 big, 3 float */
static double tofloat(oop x);
static oop p_arith(int op, oop x, oop y);   /* 1 add 2 sub 3 mul 4 div 5 idiv 6 mod 7 quo 8 rem; 0 on failure */
static int p_cmp(oop x, oop y);             /* -1/0/1, 2 when not comparable */
static oop mkbig(const char *decimal);      /* decimal text with optional '-'; normalised */
static oop big_norm(const char *decimal);   /* SmallInteger if it fits, else big */
static char *float_str(double f);           /* malloc'ed */
static char *num_str(oop x);                /* decimal text of an integer; malloc'ed */
static oop parse_number(const char *s);

/* dictionaries and exceptions (s8) */
static oop dict_new(int c);
static long dict_find(oop o, oop key);
static void dict_put(oop o, oop key, oop val);
static long dict_remove(oop o, oop key);
static oop do_signal(oop ex, long start, int fresh);
static int exc_matches(oop h, oop ex);
/* ===== s_gen.h ===== */
/* generated by tools/gen_consts.py - do not edit */
#define TK_EOF 0
#define TK_IDENT 1
#define TK_KW 2
#define TK_BIN 3
#define TK_INT 4
#define TK_FLOAT 5
#define TK_STR 6
#define TK_SYM 7
#define TK_CHAR 8
#define TK_ASSIGN 9
#define TK_CARET 10
#define TK_LP 11
#define TK_RP 12
#define TK_LB 13
#define TK_RB 14
#define TK_LC 15
#define TK_RC 16
#define TK_DOT 17
#define TK_SEMI 18
#define TK_COLON 19
#define TK_BANG 20
#define TK_HLP 21
#define TK_HLB 22
#define TK_BIGINT 23
#define ND_LIT 1
#define ND_SELF 2
#define ND_LOCAL 3
#define ND_IVAR 4
#define ND_GVAR 5
#define ND_ASGL 6
#define ND_ASGI 7
#define ND_ASGG 8
#define ND_SEND 9
#define ND_CASC 10
#define ND_MSG 11
#define ND_BLOCK 12
#define ND_RET 13
#define ND_SEQ 14
#define ND_BRACE 15
#define ND_THISCTX 16
#define ND_IF 20
#define ND_WHILE 21
#define ND_TODO 22
#define ND_TIMES 23
#define ND_AND 24
#define ND_OR 25
#define ND_IFNIL 26
#define ND_REPEAT 27
#define ND_RVAR 40
#define ND_RASG 41
#define ND_RSEND 42
#define ND_RCASC 43
#define ND_RBLOCK 44
#define ND_RRET 45
#define ND_RMSG 46
#define KD_PLAIN 0
#define KD_STR 1
#define KD_SYM 2
#define KD_FLT 3
#define KD_ARR 4
#define KD_CHR 5
#define KD_BLK 6
#define KD_BIG 7
#define KD_DICT 8
#define KD_CLS 9
#define KD_META 10
#define UW_NONE 0
#define UW_RET 1
#define UW_EXC 2
#define UW_RETRY 3
#define UW_RESUME 4
#define UW_ABORT 5
#define UW_EXIT 6
#define SELMAX 2048
#define TK_SCALED 24
#define S_ADD 1
#define S_SUB 2
#define S_MUL 3
#define S_DIV 4
#define S_IDIV 5
#define S_MOD 6
#define S_LT 7
#define S_GT 8
#define S_LE 9
#define S_GE 10
#define S_EQ 11
#define S_NE 12
#define S_IDENT 13
#define S_NIDENT 14
#define S_COMMA 15
#define S_VALUE 16
#define S_VALUE1 17
#define S_VALUE2 18
#define S_VALUE3 19
#define S_VALUE4 20
#define S_DNU 21
#define S_PRINTSTRING 22
#define S_DISPLAYSTRING 23
#define S_PRINTON 24
#define S_HASH 25
#define S_SIGNAL 26
#define S_DEFAULTACTION 27
#define S_NEW 28
#define S_INIT 29
#define S_SIZE 30
#define S_AT 31
#define S_ATPUT 32
#define S_DO 33
#define S_ISKINDOF 34
#define S_MESSAGETEXT 35
#define S_DESCRIPTION 36
#define S_NEXT 37
#define S_ATEND 38
#define S_CLASS 39
#define S_ISNIL 40
#define S_NOTNIL 41
#define S_LESSEQ 42
#define S_ASSOC 43
#define S_KEY 44
#define S_PRINTSTRING2 45
#define S_DISPLAYNL 46
#define S_PRINTNL 47
#define S_EXECSTMT 48
#define S_MUSTBEBOOLEAN 49
#define S_ERROR 50
#define S_RUNPENDING 51
#define NSPECIAL 51
static const char *const special_sels[] = {
    "+",
    "-",
    "*",
    "/",
    "//",
    "\\\\",
    "<",
    ">",
    "<=",
    ">=",
    "=",
    "~=",
    "==",
    "~~",
    ",",
    "value",
    "value:",
    "value:value:",
    "value:value:value:",
    "value:value:value:value:",
    "doesNotUnderstand:",
    "printString",
    "displayString",
    "printOn:",
    "hash",
    "signal",
    "defaultAction",
    "new",
    "initialize",
    "size",
    "at:",
    "at:put:",
    "do:",
    "isKindOf:",
    "messageText",
    "description",
    "next",
    "atEnd",
    "class",
    "isNil",
    "notNil",
    "lessOrEqual:",
    "->",
    "key",
    "printString:",
    "displayNl",
    "printNl",
    "executeStatements",
    "mustBeBoolean",
    "error:",
    "runPending",
    0
};
#define PR_ADD 1
#define PR_SUB 2
#define PR_MUL 3
#define PR_DIV 4
#define PR_IDIV 5
#define PR_MOD 6
#define PR_QUO 7
#define PR_REM 8
#define PR_LT 9
#define PR_GT 10
#define PR_LE 11
#define PR_GE 12
#define PR_EQ 13
#define PR_NE 14
#define PR_IDENT 15
#define PR_NIDENT 16
#define PR_CLASS 17
#define PR_BASICSIZE 18
#define PR_BASICAT 19
#define PR_BASICATPUT 20
#define PR_INSTVARAT 21
#define PR_INSTVARATPUT 22
#define PR_IDHASH 23
#define PR_PERFORM 24
#define PR_PERFORMARGS 25
#define PR_ISKINDOF 26
#define PR_ISMEMBEROF 27
#define PR_RESPONDSTO 28
#define PR_SHALLOWCOPY 29
#define PR_BASICNEW 30
#define PR_BASICNEWSIZE 31
#define PR_FLOATOP 32
#define PR_TRUNCATED 33
#define PR_FLOOR 34
#define PR_CEILING 35
#define PR_ROUNDED 36
#define PR_ASFLOAT 37
#define PR_BITAND 38
#define PR_BITOR 39
#define PR_BITXOR 40
#define PR_BITSHIFT 41
#define PR_PRINTSTRING 42
#define PR_PRINTRADIX 43
#define PR_ISINT 44
#define PR_ISFLOAT 45
#define PR_ISSTRING 46
#define PR_ISSYMBOL 47
#define PR_ISCHAR 48
#define PR_ISNUMBER 49
#define PR_INTSQRT 50
#define PR_STRSIZE 51
#define PR_STRAT 52
#define PR_STRATPUT 53
#define PR_STRCAT 54
#define PR_STRCOPY 55
#define PR_STREQ 56
#define PR_STRLT 57
#define PR_STRHASH 58
#define PR_ASSYMBOL 59
#define PR_ASSTRING 60
#define PR_UPPER 61
#define PR_LOWER 62
#define PR_INDEXOF 63
#define PR_REPLACEALL 64
#define PR_ASNUMBER 65
#define PR_STRTRIM 66
#define PR_STRSPLIT 67
#define PR_STRREV 68
#define PR_STRNEW 69
#define PR_CHARVALUE 70
#define PR_CHARFROM 71
#define PR_ARRNEW 72
#define PR_ARRAT 73
#define PR_ARRATPUT 74
#define PR_ARRSIZE 75
#define PR_ARRCOPY 76
#define PR_ARRFILL 77
#define PR_ARRREPLACE 78
#define PR_DICTNEW 79
#define PR_DICTAT 80
#define PR_DICTATPUT 81
#define PR_DICTHAS 82
#define PR_DICTREMOVE 83
#define PR_DICTKEYS 84
#define PR_DICTVALUES 85
#define PR_DICTSIZE 86
#define PR_DICTCOPY 87
#define PR_BLOCKVALUE 88
#define PR_BLOCKVALUEARGS 89
#define PR_BLOCKNUMARGS 90
#define PR_WHILETRUE 91
#define PR_WHILEFALSE 92
#define PR_ONDO 93
#define PR_ENSURE 94
#define PR_IFCURTAILED 95
#define PR_SIGNAL 96
#define PR_RESUME 97
#define PR_RETURN 98
#define PR_RETRY 99
#define PR_PASS 100
#define PR_OUTER 101
#define PR_RETRYUSING 102
#define PR_CLSNAME 103
#define PR_CLSSUPER 104
#define PR_CLSSUBCLASS 105
#define PR_CLSADDIVAR 106
#define PR_CLSIVARS 107
#define PR_CLSSELECTORS 108
#define PR_CLSINCLUDES 109
#define PR_CLSNEW 110
#define PR_CLSCOMPILE 111
#define PR_CLSADDCVAR 112
#define PR_CLSLOOKUP 113
#define PR_CLSSUBCLASSES 114
#define PR_CLSCATEGORY 115
#define PR_GLOBALAT 116
#define PR_GLOBALATPUT 117
#define PR_GLOBALHAS 118
#define PR_TRANSCRIPT 119
#define PR_STDINLINE 120
#define PR_STDINATEND 121
#define PR_MSECS 122
#define PR_HALT 123
#define PR_EXIT 124
#define PR_DNUARGS 125
#define PR_ISNIL 126
#define PR_EVALSTRING 127
#define PR_DISPLAYSTRING 128
#define PR_SYMBOLS 129
#define PR_EXCINFO 130
#define PR_BIGOP 131
#define PR_FLOATPRINT 132
#define PR_RANDOM 133
#define PR_STRFIND 134
#define PR_STRCOMPARE 135
#define PR_STRAS 136
#define PR_SECS 137
#define PR_FORMAT 138
#define PR_FILEREAD 139
#define PR_FILEWRITE 140
#define PR_DATESTR 141
#define PR_TIMESTR 142
#define PR_RANDSEED 143
#define PR_GETENV 144
#define PR_ARGCOUNT 145
#define PR_ARGAT 146
#define PR_FDELETE 147
#define PR_FRENAME 148
#define PR_FKIND 149
#define PR_FMKDIR 150
#define PR_FRMDIR 151
#define PR_FLIST 152
#define PR_FCWD 153
#define PR_FCHDIR 154
#define PR_FCOPY 155
#define PR_FSIZE 156
#define PR_SYSCMD 157
#define PR_SYSRC 158
#define PR_SYSERR 159
#define PR_FILEINSTR 160
#define PR_ALLINST 161
#define PR_BECOME 162
#define PR_UNICODE 163
#define PR_SOCKNEW 164
#define PR_SOCKCLOSE 165
#define PR_SOCKCONNECT 166
#define PR_SOCKTLS 167
#define PR_SOCKBIND 168
#define PR_SOCKLISTEN 169
#define PR_SOCKACCEPT 170
#define PR_SOCKSEND 171
#define PR_SOCKRECV 172
#define PR_SOCKPENDING 173
#define PR_SOCKTIMEOUT 174
#define PR_SOCKSTATUS 175
#define PR_SOCKERROR 176
#define PR_SOCKPEER 177
#define PR_SOCKLOCAL 178
#define PR_SOCKNODELAY 179
#define PR_SOCKSHUT 180
static const char *const prim_names[] = {
    "",
    "add",
    "sub",
    "mul",
    "div",
    "idiv",
    "mod",
    "quo",
    "rem",
    "lt",
    "gt",
    "le",
    "ge",
    "eq",
    "ne",
    "ident",
    "nident",
    "class",
    "basicSize",
    "basicAt",
    "basicAtPut",
    "instVarAt",
    "instVarAtPut",
    "identityHash",
    "perform",
    "performWithArgs",
    "isKindOf",
    "isMemberOf",
    "respondsTo",
    "shallowCopy",
    "basicNew",
    "basicNewSize",
    "floatOp",
    "truncated",
    "floor",
    "ceiling",
    "rounded",
    "asFloat",
    "bitAnd",
    "bitOr",
    "bitXor",
    "bitShift",
    "printString",
    "printRadix",
    "isInteger",
    "isFloat",
    "isString",
    "isSymbol",
    "isCharacter",
    "isNumber",
    "sqrtFloor",
    "strSize",
    "strAt",
    "strAtPut",
    "strCat",
    "strCopy",
    "strEq",
    "strLt",
    "strHash",
    "asSymbol",
    "asString",
    "upper",
    "lower",
    "strIndexOf",
    "strReplaceAll",
    "asNumber",
    "strTrim",
    "strSplit",
    "strReverse",
    "strNew",
    "charValue",
    "charFrom",
    "arrNew",
    "arrAt",
    "arrAtPut",
    "arrSize",
    "arrCopy",
    "arrFill",
    "arrReplace",
    "dictNew",
    "dictAt",
    "dictAtPut",
    "dictHas",
    "dictRemove",
    "dictKeys",
    "dictValues",
    "dictSize",
    "dictCopy",
    "blockValue",
    "blockValueArgs",
    "blockNumArgs",
    "whileTrue",
    "whileFalse",
    "onDo",
    "ensure",
    "ifCurtailed",
    "signal",
    "resume",
    "return",
    "retry",
    "pass",
    "outer",
    "retryUsing",
    "className",
    "superclass",
    "subclassNamed",
    "addInstVar",
    "instanceVariableNames",
    "selectors",
    "includesSelector",
    "basicNewClass",
    "compile",
    "addClassVar",
    "lookupSelector",
    "subclasses",
    "category",
    "globalAt",
    "globalAtPut",
    "globalHas",
    "transcript",
    "stdinLine",
    "stdinAtEnd",
    "msecs",
    "halt",
    "exit",
    "dnuArgs",
    "isNil",
    "evaluate",
    "displayString",
    "allSymbols",
    "excInfo",
    "bigOp",
    "floatPrint",
    "random",
    "strFind",
    "strCompare",
    "asStringOf",
    "secs",
    "format",
    "fileRead",
    "fileWrite",
    "dateStr",
    "timeStr",
    "randSeed",
    "getenv",
    "argCount",
    "argAt",
    "fileDelete",
    "fileRename",
    "fileKind",
    "dirCreate",
    "dirRemove",
    "dirList",
    "cwd",
    "chdir",
    "fileCopy",
    "fileSize",
    "sysCmd",
    "sysRc",
    "sysErr",
    "fileInString",
    "allInstances",
    "become",
    "unicode",
    "sockNew",
    "sockClose",
    "sockConnect",
    "sockConnectTls",
    "sockBind",
    "sockListen",
    "sockAccept",
    "sockSend",
    "sockRecv",
    "sockPending",
    "sockTimeout",
    "sockStatus",
    "sockError",
    "sockPeer",
    "sockLocal",
    "sockNoDelay",
    "sockShutdown",
    0
};
/* ===== s1_obj.c ===== */
/* =========================================================================
   s1_obj.c   utilities, hash maps, objects, globals, selectors, classes,
              methods, boot
   ========================================================================= */

/* ---- memory ------------------------------------------------------------ */
static void fatal(const char *msg)
{
    flush_out();
    fprintf(stderr, "smalltalk: %s\n", msg);
    exit(2);
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (p == NULL) fatal("out of memory");
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (q == NULL) fatal("out of memory");
    return q;
}

static void *xgrow(void *p, long *cap, long need, size_t esz)
{
    long nc_ = *cap ? *cap : 64;
    while (nc_ <= need) nc_ *= 2;
    p = xrealloc(p, (size_t)nc_ * esz);
    *cap = nc_;
    return p;
}

static char *dupn(const char *s, long n)
{
    char *p = (char *)xmalloc((size_t)n + 1);
    if (n > 0) memcpy(p, s, (size_t)n);
    p[n] = '\0';
    return p;
}

static char *dups(const char *s)
{
    return dupn(s, (long)strlen(s));
}

/* ---- output ------------------------------------------------------------- */
static char obuf[4096];
static int olen;

static void flush_out(void)
{
    if (olen > 0) {
        fwrite(obuf, 1, (size_t)olen, stdout);
        olen = 0;
    }
    fflush(stdout);
}

static void outn(const char *s, long n)
{
    long i;
    for (i = 0; i < n; i++) {
        if (olen >= (int)sizeof(obuf)) {
            fwrite(obuf, 1, (size_t)olen, stdout);
            olen = 0;
        }
        obuf[olen++] = s[i];
    }
}

static void outs(const char *s)
{
    outn(s, (long)strlen(s));
}

/* ---- string builder ------------------------------------------------------ */
static void sb_init(SB *b)
{
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
}

static void sb_addn(SB *b, const char *s, long n)
{
    if (b->n + n + 1 > b->cap) {
        long need = b->n + n + 1;
        b->p = (char *)xgrow(b->p, &b->cap, need, 1);
    }
    if (n > 0) memcpy(b->p + b->n, s, (size_t)n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_adds(SB *b, const char *s)
{
    sb_addn(b, s, (long)strlen(s));
}

static void sb_addc(SB *b, int c)
{
    char ch = (char)c;
    sb_addn(b, &ch, 1);
}

static void sb_addl(SB *b, long v)
{
    char tmp[40];
    sprintf(tmp, "%ld", v);
    sb_adds(b, tmp);
}

static char *sb_str(SB *b)
{
    if (b->p == NULL) sb_addn(b, "", 0);
    return b->p;
}

static void sb_free(SB *b)
{
    if (b->p != NULL) free(b->p);
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
}

/* ---- character classes ----------------------------------------------------- */
#define LOWERS "abcdefghijklmnopqrstuvwxyz"
#define UPPERS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

static int is_upper(int c)
{
    return c > 0 && strchr(UPPERS, c) != NULL;
}


static int is_letter(int c)
{
    return c > 0 && (strchr(UPPERS, c) != NULL || strchr(LOWERS, c) != NULL || c == '_');
}

static int is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static int is_binch(int c)
{
    return c > 0 && strchr("+-*/\\<>=~@%|&?,", c) != NULL;
}

static int is_sep(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static int to_upper(int c)
{
    const char *p;
    if (c <= 0) return c;
    p = strchr(LOWERS, c);
    if (p == NULL) return c;
    return UPPERS[p - LOWERS];
}

static int to_lower(int c)
{
    const char *p;
    if (c <= 0) return c;
    p = strchr(UPPERS, c);
    if (p == NULL) return c;
    return LOWERS[p - UPPERS];
}

/* ---- hash maps with string keys ---------------------------------------------- */
static unsigned long str_hash_fn(const char *s)
{
    unsigned long h = 5381UL;
    while (*s) {
        h = ((h << 5) + h) ^ (unsigned long)(unsigned char)*s;
        s++;
    }
    return h;
}

static void smap_init(SMap *m)
{
    m->cap = 64;
    m->cnt = 0;
    m->fill = 0;
    m->keys = (char **)xmalloc(sizeof(char *) * (size_t)m->cap);
    m->vals = (long *)xmalloc(sizeof(long) * (size_t)m->cap);
    m->used = (unsigned char *)xmalloc((size_t)m->cap);
    memset(m->used, 0, (size_t)m->cap);
}

static long smap_slot(SMap *m, const char *key)
{
    unsigned long h = str_hash_fn(key);
    long i = (long)(h & (unsigned long)(m->cap - 1));
    long firstdel = -1;
    for (;;) {
        if (m->used[i] == 0) return firstdel >= 0 ? firstdel : i;
        if (m->used[i] == 2) {
            if (firstdel < 0) firstdel = i;
        } else if (strcmp(m->keys[i], key) == 0) {
            return i;
        }
        i = (i + 1) & (m->cap - 1);
    }
}

static int smap_get(SMap *m, const char *key, long *val)
{
    long i = smap_slot(m, key);
    if (m->used[i] == 1) {
        if (val != NULL) *val = m->vals[i];
        return 1;
    }
    return 0;
}

static void smap_grow(SMap *m)
{
    char **ok = m->keys;
    long *ov = m->vals;
    unsigned char *ou = m->used;
    long oc = m->cap;
    long i, j;
    m->cap = oc * 2;
    m->keys = (char **)xmalloc(sizeof(char *) * (size_t)m->cap);
    m->vals = (long *)xmalloc(sizeof(long) * (size_t)m->cap);
    m->used = (unsigned char *)xmalloc((size_t)m->cap);
    memset(m->used, 0, (size_t)m->cap);
    m->cnt = 0;
    m->fill = 0;
    for (i = 0; i < oc; i++) {
        if (ou[i] == 1) {
            j = smap_slot(m, ok[i]);
            m->keys[j] = ok[i];
            m->vals[j] = ov[i];
            m->used[j] = 1;
            m->cnt++;
            m->fill++;
        } else if (ou[i] == 2) {
            free(ok[i]);
        }
    }
    free(ok);
    free(ov);
    free(ou);
}

static void smap_put(SMap *m, const char *key, long val)
{
    long i = smap_slot(m, key);
    if (m->used[i] == 1) {
        m->vals[i] = val;
        return;
    }
    if (m->used[i] == 0) {
        if ((m->fill + 1) * 4 > m->cap * 3) {
            smap_grow(m);
            i = smap_slot(m, key);
        }
        m->fill++;
    } else {
        free(m->keys[i]);
    }
    m->keys[i] = dups(key);
    m->vals[i] = val;
    m->used[i] = 1;
    m->cnt++;
}

static void smap_del(SMap *m, const char *key)
{
    long i = smap_slot(m, key);
    if (m->used[i] == 1) {
        m->used[i] = 2;
        m->cnt--;
    }
}

/* ---- word lists (instance variable names) ------------------------------------- */
static int count_words(const char *list)
{
    int n = 0, in = 0;
    while (*list) {
        if (*list == ' ') in = 0;
        else if (!in) { in = 1; n++; }
        list++;
    }
    return n;
}

static int wordpos_str(const char *w, const char *list)
{
    int n = 0;
    long wl = (long)strlen(w);
    const char *p = list;
    while (*p) {
        const char *q;
        while (*p == ' ') p++;
        if (!*p) break;
        q = p;
        while (*q && *q != ' ') q++;
        n++;
        if (q - p == wl && strncmp(p, w, (size_t)wl) == 0) return n;
        p = q;
    }
    return 0;
}

/* the words after the first n words, joined with blanks */
static char *subword_str(const char *list, int n)
{
    SB b;
    const char *p = list;
    int k = 0;
    sb_init(&b);
    while (*p) {
        const char *q;
        while (*p == ' ') p++;
        if (!*p) break;
        q = p;
        while (*q && *q != ' ') q++;
        k++;
        if (k > n) {
            if (b.n > 0) sb_addc(&b, ' ');
            sb_addn(&b, p, (long)(q - p));
        }
        p = q;
    }
    return sb_str(&b);
}

/* ---- objects ------------------------------------------------------------------- */
static oop mkint(long n)
{
    return MKINT(n);
}

static long intval(oop x)
{
    return INTVAL(x);
}

static oop newobj(int cls_, int kind, long n)
{
    long i;
    no++;
    GROW(Obj, objs, objcap, no);
    objs[no].cls = cls_;
    objs[no].kind = kind;
    objs[no].n = n;
    objs[no].x = 0;
    objs[no].s = NULL;
    objs[no].slen = 0;
    objs[no].f = 0.0;
    objs[no].slot = nsl + 1;
    if (n > 0) {
        GROW(oop, sl, slcap, nsl + n);
        for (i = 1; i <= n; i++) {
            nsl++;
            sl[nsl] = NIL_OOP;
        }
    }
    return no * 2L;
}

static void set_str(oop o, const char *s, long n)
{
    char *p = dupn(s, n);
    if (objs[HI(o)].s != NULL) free(objs[HI(o)].s);
    objs[HI(o)].s = p;
    objs[HI(o)].slen = n;
}

static oop mkstrn(const char *s, long n)
{
    oop o = newobj(cls_String, KD_STR, 0);
    set_str(o, s, n);
    return o;
}

static oop mkstr(const char *s)
{
    return mkstrn(s, (long)strlen(s));
}

static oop mksym(const char *s)
{
    long v;
    oop o;
    if (smap_get(&symtab, s, &v)) return (oop)v;
    o = newobj(cls_Symbol, KD_SYM, 0);
    set_str(o, s, (long)strlen(s));
    smap_put(&symtab, s, (long)o);
    return o;
}

static oop mkfloat(double f)
{
    oop o = newobj(cls_Float, KD_FLT, 0);
    objs[HI(o)].f = f;
    return o;
}

static oop mkchar(long code)
{
    oop o;
    if (code < 0 || code > 255) code = 63;
    if (chr_oop[code] == 0) {
        o = newobj(cls_Character, KD_CHR, 0);
        objs[HI(o)].x = code;
        chr_oop[code] = o;
    }
    return chr_oop[code];
}

static oop mkarray(long n)
{
    return newobj(cls_Array, KD_ARR, n);
}

static oop mkbool(int b)
{
    return b ? TRUE_OOP : FALSE_OOP;
}

static int cof(oop x)
{
    if (IS_INT(x)) return cls_SmallInteger;
    return objs[HI(x)].cls;
}

static int kindof(oop x)
{
    if (IS_INT(x)) return -1;
    return objs[HI(x)].kind;
}

static const char *strof(oop x)
{
    int k;
    if (IS_INT(x)) return "";
    k = objs[HI(x)].kind;
    if ((k == KD_STR || k == KD_SYM || k == KD_BIG) && objs[HI(x)].s != NULL) return objs[HI(x)].s;
    return "";
}

static long strlen_of(oop x)
{
    int k;
    if (IS_INT(x)) return 0;
    k = objs[HI(x)].kind;
    if (k == KD_STR || k == KD_SYM || k == KD_BIG) return objs[HI(x)].slen;
    return 0;
}

static int isstr(oop x)
{
    if (IS_INT(x)) return 0;
    return objs[HI(x)].kind == KD_STR || objs[HI(x)].kind == KD_SYM;
}

static void ast_push(oop v)
{
    asp++;
    GROW(oop, ast, astcap, asp);
    ast[asp] = v;
}

/* ---- globals, selectors ----------------------------------------------------------- */
static void g_ensure(long need)
{
    if (need >= gcap) {
        g_val = (long *)xgrow(g_val, &gcap, need, sizeof(long));
        g_name = (char **)xrealloc(g_name, sizeof(char *) * (size_t)gcap);
    }
}

static int gcell(const char *name)
{
    long v;
    if (smap_get(&gmap, name, &v)) return (int)v;
    ng++;
    g_ensure(ng);
    g_val[ng] = NIL_OOP;
    g_name[ng] = dups(name);
    smap_put(&gmap, name, ng);
    return (int)ng;
}

static void gset(const char *name, oop v)
{
    int cell = gcell(name);
    g_val[cell] = v;
}

static int selid(const char *name)
{
    long v;
    int n;
    if (smap_get(&selmap, name, &v)) return (int)v;
    nsel++;
    if (nsel >= selcap) {
        sel_name = (char **)xgrow(sel_name, &selcap, nsel, sizeof(char *));
        sel_n = (int *)xrealloc(sel_n, sizeof(int) * (size_t)selcap);
    }
    sel_name[nsel] = dups(name);
    if (is_letter((unsigned char)name[0])) {
        const char *p;
        n = 0;
        for (p = name; *p; p++) if (*p == ':') n++;
    } else {
        n = 1;
    }
    sel_n[nsel] = n;
    smap_put(&selmap, name, nsel);
    return (int)nsel;
}

/* ---- classes ------------------------------------------------------------------------ */
static int find_class(const char *name)
{
    long v;
    if (smap_get(&cmap, name, &v)) return (int)v;
    return 0;
}

static int mkclassrec(const char *name, int sup)
{
    int c;
    nc++;
    GROW(Cls, cls, clscap, nc);
    c = (int)nc;
    cls[c].name = dups(name);
    cls[c].super_ = sup;
    cls[c].meta = 0;
    cls[c].ismeta = 0;
    cls[c].this_ = 0;
    cls[c].kind = KD_PLAIN;
    cls[c].ninst = 0;
    cls[c].ivn = dups("");
    cls[c].oop_ = 0;
    cls[c].lib = booted ? 0 : 1;
    if (sup > 0) {
        cls[c].kind = cls[sup].kind;
        cls[c].ninst = cls[sup].ninst;
        free(cls[c].ivn);
        cls[c].ivn = dups(cls[sup].ivn);
    }
    return c;
}

/* method dictionary: open addressing on (class, selector) */
static long *mt_key, *mt_val;
static long mt_cap, mt_cnt;
#define MT_KEY(c, s) ((long)(c) * 65536L + (long)(s))

static void mt_init(void)
{
    mt_cap = 4096;
    mt_cnt = 0;
    mt_key = (long *)xmalloc(sizeof(long) * (size_t)mt_cap);
    mt_val = (long *)xmalloc(sizeof(long) * (size_t)mt_cap);
    memset(mt_key, 0, sizeof(long) * (size_t)mt_cap);
    memset(mt_val, 0, sizeof(long) * (size_t)mt_cap);
}

static long mt_find(long key)
{
    long i = (long)(((unsigned long)key * 2654435761UL) & (unsigned long)(mt_cap - 1));
    while (mt_key[i] != 0 && mt_key[i] != key) i = (i + 1) & (mt_cap - 1);
    return i;
}

static int mt_get(int c, int sel)
{
    long i = mt_find(MT_KEY(c, sel));
    if (mt_key[i] == 0) return 0;
    return (int)mt_val[i];
}

static void mt_set(int c, int sel, int m)
{
    long key = MT_KEY(c, sel);
    long i = mt_find(key);
    if (mt_key[i] == 0) {
        if ((mt_cnt + 1) * 2 > mt_cap) {
            long *ok = mt_key, *ov = mt_val;
            long oc = mt_cap, j;
            mt_cap *= 2;
            mt_key = (long *)xmalloc(sizeof(long) * (size_t)mt_cap);
            mt_val = (long *)xmalloc(sizeof(long) * (size_t)mt_cap);
            memset(mt_key, 0, sizeof(long) * (size_t)mt_cap);
            memset(mt_val, 0, sizeof(long) * (size_t)mt_cap);
            for (j = 0; j < oc; j++) {
                if (ok[j] != 0) {
                    long k = mt_find(ok[j]);
                    mt_key[k] = ok[j];
                    mt_val[k] = ov[j];
                }
            }
            free(ok);
            free(ov);
            i = mt_find(key);
        }
        mt_cnt++;
        mt_key[i] = key;
    }
    mt_val[i] = m;
}

static void mt_clear_class(int c)
{
    long i;
    for (i = 0; i < mt_cap; i++) {
        if (mt_key[i] != 0 && mt_key[i] / 65536L == c) mt_val[i] = 0;
    }
}

/* lookup cache */
#define LC_SIZE 8192
static int lc_c[LC_SIZE], lc_s[LC_SIZE], lc_m[LC_SIZE];
static long lc_g[LC_SIZE];

static int lookup(int c, int sel)
{
    long i = ((long)c * 31L + (long)sel * 7L) & (LC_SIZE - 1);
    int m, k;
    if (lc_g[i] == gen && lc_c[i] == c && lc_s[i] == sel) return lc_m[i];
    m = 0;
    k = c;
    while (k > 0) {
        m = mt_get(k, sel);
        if (m > 0) break;
        k = cls[k].super_;
    }
    lc_c[i] = c;
    lc_s[i] = sel;
    lc_m[i] = m;
    lc_g[i] = gen;
    return m;
}

static int inherits(int c, int d)
{
    int k = c;
    while (k > 0) {
        if (k == d) return 1;
        k = cls[k].super_;
    }
    return 0;
}

/* create (or reopen) class name with superclass sup (0 = none) */
static int define_class(const char *name, int sup)
{
    int c, m, msup;
    oop o;
    c = find_class(name);
    if (c > 0) {
        if (booted && cls[c].lib) {
            cls[c].lib = 0;
            m = cls[c].meta;
            mt_clear_class(c);
            mt_clear_class(m);
            free(cls[c].ivn);
            if (sup > 0) {
                cls[c].ivn = dups(cls[sup].ivn);
                cls[c].ninst = cls[sup].ninst;
            } else {
                cls[c].ivn = dups("");
                cls[c].ninst = 0;
            }
            gen++;
        }
        if (cls[c].super_ != sup) {
            cls[c].super_ = sup;
            if (sup > 0) {
                free(cls[c].ivn);
                cls[c].ivn = dups(cls[sup].ivn);
                cls[c].ninst = cls[sup].ninst;
                cls[c].kind = cls[sup].kind;
                cls[cls[c].meta].super_ = cls[sup].meta;
            }
            gen++;
        }
        return c;
    }
    {
        char *mname;
        c = mkclassrec(name, sup);
        msup = sup > 0 ? cls[sup].meta : cls_Class;
        mname = (char *)xmalloc(strlen(name) + 8);
        sprintf(mname, "%s class", name);
        m = mkclassrec(mname, msup);
        free(mname);
    }
    cls[m].ismeta = 1;
    cls[m].this_ = c;
    cls[c].meta = m;
    if (sup > 0) {
        cls[m].ninst = cls[msup].ninst;
        free(cls[m].ivn);
        cls[m].ivn = dups(cls[msup].ivn);
    } else {
        cls[m].ninst = 0;
        free(cls[m].ivn);
        cls[m].ivn = dups("");
    }
    cls[m].kind = KD_PLAIN;
    o = newobj(m, KD_CLS, cls[m].ninst);
    objs[HI(o)].x = c;
    cls[c].oop_ = o;
    o = newobj(cls_Metaclass, KD_META, 0);
    objs[HI(o)].x = m;
    cls[m].oop_ = o;
    smap_put(&cmap, name, c);
    gset(name, cls[c].oop_);
    if (strcmp(name, "Class") == 0) cls_Class = c;
    return c;
}

static void ivar_inherit(int k, int oldn, int parent)
{
    char *own;
    SB b;
    int kold, j;
    kold = count_words(cls[k].ivn);
    own = (kold > oldn) ? subword_str(cls[k].ivn, oldn) : dups("");
    sb_init(&b);
    sb_adds(&b, cls[parent].ivn);
    if (own[0]) {
        if (b.n > 0) sb_addc(&b, ' ');
        sb_adds(&b, own);
    }
    free(cls[k].ivn);
    cls[k].ivn = dups(sb_str(&b));
    sb_free(&b);
    cls[k].ninst = count_words(cls[k].ivn);
    for (j = 1; j <= nc; j++) {
        if (cls[j].super_ == k) ivar_inherit(j, kold, k);
    }
}

static void grow_class_slots(int c)
{
    long o = HI(cls[c].oop_);
    long need = cls[cls[c].meta].ninst;
    long have = objs[o].n;
    long old, base, i;
    if (need <= have) return;
    old = objs[o].slot;
    base = nsl + 1;
    GROW(oop, sl, slcap, nsl + need);
    for (i = 0; i < have; i++) {
        nsl++;
        sl[nsl] = sl[old + i];
    }
    for (i = have + 1; i <= need; i++) {
        nsl++;
        sl[nsl] = NIL_OOP;
    }
    objs[o].slot = base;
    objs[o].n = need;
}

static void add_ivar(int c, const char *name)
{
    int k, oldn;
    SB b;
    if (wordpos_str(name, cls[c].ivn) > 0) return;
    oldn = count_words(cls[c].ivn);
    sb_init(&b);
    sb_adds(&b, cls[c].ivn);
    if (b.n > 0) sb_addc(&b, ' ');
    sb_adds(&b, name);
    free(cls[c].ivn);
    cls[c].ivn = dups(sb_str(&b));
    sb_free(&b);
    cls[c].ninst = oldn + 1;
    for (k = 1; k <= nc; k++) {
        if (cls[k].super_ == c) ivar_inherit(k, oldn, c);
    }
    for (k = 1; k <= nc; k++) {
        if (cls[k].ismeta && cls[cls[k].this_].oop_ > 0) {
            if (inherits(k, c)) grow_class_slots(cls[k].this_);
        }
    }
}

static int install_method(int c, int sel, int nargs, long body, int ntemps, int hasblk, int prim_)
{
    int m;
    nmeth++;
    GROW(Meth, meths, methcap, nmeth);
    m = (int)nmeth;
    meths[m].sel = sel;
    meths[m].cls = c;
    meths[m].nargs = nargs;
    meths[m].body = body;
    meths[m].ntemps = ntemps;
    meths[m].hasblk = hasblk;
    meths[m].prim = prim_;
    meths[m].src = 0;
    mt_set(c, sel, m);
    gen++;
    return m;
}

/* class variables: "<class>.<name>" -> global cell */
static char *cvkey(int c, const char *name)
{
    char *k = (char *)xmalloc(strlen(name) + 24);
    sprintf(k, "%d.%s", c, name);
    return k;
}

static int find_cvar(int c0, const char *name)
{
    int c = c0;
    long v;
    char *k;
    if (cls[c].ismeta) c = cls[c].this_;
    while (c > 0) {
        k = cvkey(c, name);
        if (smap_get(&cvmap, k, &v)) {
            free(k);
            return (int)v;
        }
        free(k);
        c = cls[c].super_;
    }
    return 0;
}

/* declare a class variable (cell initialised to nil) without an initialiser */
static void declare_classvar(int c, const char *name)
{
    char *k;
    long tmp;
    if (cls[c].ismeta) c = cls[c].this_;
    k = cvkey(c, name);
    if (!smap_get(&cvmap, k, &tmp)) {
        ng++;
        g_ensure(ng);
        g_val[ng] = NIL_OOP;
        g_name[ng] = dups(name);
        smap_put(&cvmap, k, ng);
    }
    free(k);
}

static void define_classvar(int c0, const char *name, long e)
{
    int c = c0;
    int cell;
    oop v;
    char *k;
    long tmp;
    if (cls[c].ismeta) c = cls[c].this_;
    cell = find_cvar(c, name);
    k = cvkey(c, name);
    if (cell == 0 || !smap_get(&cvmap, k, &tmp)) {
        ng++;
        g_ensure(ng);
        g_val[ng] = NIL_OOP;
        g_name[ng] = dups(name);
        smap_put(&cvmap, k, ng);
        cell = (int)ng;
    }
    free(k);
    v = (oop)eval_in_class(c, e);
    g_val[cell] = v;
}

/* ---- instances ---------------------------------------------------------------------------- */
static oop new_instance(int c, long n)
{
    int k = cls[c].kind;
    oop o;
    switch (k) {
    case KD_PLAIN:
        if (n > 0) return newobj(c, KD_ARR, cls[c].ninst + n);
        return newobj(c, KD_PLAIN, cls[c].ninst);
    case KD_ARR:
        return newobj(c, KD_ARR, cls[c].ninst + n);
    case KD_STR:
        o = newobj(c, KD_STR, 0);
        {
            char *sp = (char *)xmalloc((size_t)n + 1);
            memset(sp, ' ', (size_t)n);
            sp[n] = '\0';
            if (objs[HI(o)].s != NULL) free(objs[HI(o)].s);
            objs[HI(o)].s = sp;
            objs[HI(o)].slen = n;
        }
        return o;
    case KD_DICT:
        return dict_new(c);
    case KD_FLT:
        return mkfloat(0.0);
    default:
        return 0;
    }
}

static oop shallow_copy(oop x)
{
    int k;
    oop o;
    long i, e;
    if (IS_INT(x)) return x;
    k = objs[HI(x)].kind;
    switch (k) {
    case KD_STR:
        o = newobj(objs[HI(x)].cls, KD_STR, 0);
        set_str(o, objs[HI(x)].s ? objs[HI(x)].s : "", objs[HI(x)].slen);
        return o;
    case KD_SYM: case KD_CHR: case KD_CLS: case KD_META: case KD_BLK: case KD_BIG:
        return x;
    case KD_FLT:
        return mkfloat(objs[HI(x)].f);
    case KD_DICT:
        o = dict_new(objs[HI(x)].cls);
        e = dd_head[objs[HI(x)].x];
        while (e > 0) {
            if (de_live[e] == 1) dict_put(o, de_key[e], de_val[e]);
            e = de_next[e];
        }
        for (i = 0; i < objs[HI(x)].n; i++) sl[objs[HI(o)].slot + i] = sl[objs[HI(x)].slot + i];
        return o;
    default:
        break;
    }
    o = newobj(objs[HI(x)].cls, k, objs[HI(x)].n);
    for (i = 0; i < objs[HI(x)].n; i++) sl[objs[HI(o)].slot + i] = sl[objs[HI(x)].slot + i];
    objs[HI(o)].x = objs[HI(x)].x;
    return o;
}

/* ---- boot ----------------------------------------------------------------------------------------------- */
static const char *const boot_tbl[] = {
    "Object/-/0", "Behavior/Object/0", "ClassDescription/Behavior/0", "Class/ClassDescription/0",
    "Metaclass/ClassDescription/0", "Boolean/Object/0", "True/Boolean/0", "False/Boolean/0",
    "UndefinedObject/Object/0", "Magnitude/Object/0", "Character/Magnitude/5", "Number/Magnitude/0",
    "Integer/Number/0", "SmallInteger/Integer/0", "LargeInteger/Integer/7",
    "LargePositiveInteger/LargeInteger/7", "LargeNegativeInteger/LargeInteger/7",
    "Float/Number/3", "FloatD/Float/3", "Fraction/Number/0", "LookupKey/Magnitude/0",
    "Association/LookupKey/0",
    "Collection/Object/0", "SequenceableCollection/Collection/0",
    "ArrayedCollection/SequenceableCollection/0",
    "Array/ArrayedCollection/4", "CharacterArray/ArrayedCollection/0", "String/CharacterArray/1",
    "Symbol/String/2",
    "OrderedCollection/SequenceableCollection/0", "SortedCollection/OrderedCollection/0",
    "Interval/ArrayedCollection/0", "HashedCollection/Collection/0",
    "Dictionary/HashedCollection/8", "IdentityDictionary/Dictionary/8", "Set/HashedCollection/8",
    "IdentitySet/Set/8", "Bag/HashedCollection/0", "BlockClosure/Object/6", "Message/Object/0",
    "Stream/Object/0", "PositionableStream/Stream/0", "ReadStream/PositionableStream/0",
    "WriteStream/PositionableStream/0", "ReadWriteStream/WriteStream/0", "TextCollector/Stream/0",
    "SystemDictionary/Object/0", "Point/Object/0", "Random/Object/0", "Date/Magnitude/0",
    "Time/Magnitude/0",
    "Signal/Object/0", "Exception/Signal/0", "Error/Exception/0", "Warning/Exception/0",
    "ArithmeticError/Error/0", "ZeroDivide/ArithmeticError/0", "MessageNotUnderstood/Error/0",
    "InvalidValue/Error/0", "InvalidArgument/InvalidValue/0", "IndexOutOfRange/InvalidArgument/0",
    "NotFound/InvalidArgument/0", "EmptyCollection/Error/0", "WrongClass/InvalidValue/0",
    "InvalidReturn/Error/0", "UserInterrupt/Exception/0", "ExceptionSet/Object/0",
    0
};

static void boot(void)
{
    int i, c, sup, kd;
    oop o;
    char nm[64], sp[64];
    const char *e;
    const char *p1;
    const char *p2;
    for (i = 0; special_sels[i] != 0; i++) selid(special_sels[i]);
    o = newobj(0, KD_PLAIN, 0);
    o = newobj(0, KD_PLAIN, 0);
    o = newobj(0, KD_PLAIN, 0);
    for (i = 0; boot_tbl[i] != 0; i++) {
        e = boot_tbl[i];
        p1 = strchr(e, '/');
        p2 = strchr(p1 + 1, '/');
        memcpy(nm, e, (size_t)(p1 - e));
        nm[p1 - e] = '\0';
        memcpy(sp, p1 + 1, (size_t)(p2 - p1 - 1));
        sp[p2 - p1 - 1] = '\0';
        kd = atoi(p2 + 1);
        if (strcmp(sp, "-") == 0) sup = 0;
        else sup = find_class(sp);
        c = define_class(nm, sup);
        cls[c].kind = kd;
        if (strcmp(nm, "Class") == 0) {
            cls_Class = c;
            cls[cls[find_class("Object")].meta].super_ = c;
        }
        if (strcmp(nm, "Metaclass") == 0) cls_Metaclass = c;
    }
    for (i = 1; i <= nc; i++) {
        if (cls[i].ismeta == 1) objs[HI(cls[i].oop_)].cls = cls_Metaclass;
    }
    cls_Object = find_class("Object");
    cls_String = find_class("String");
    cls_Symbol = find_class("Symbol");
    cls_Float = find_class("FloatD");
    cls_Character = find_class("Character");
    cls_Array = find_class("Array");
    cls_SmallInteger = find_class("SmallInteger");
    cls_LargePositiveInteger = find_class("LargePositiveInteger");
    cls_LargeNegativeInteger = find_class("LargeNegativeInteger");
    cls_BlockClosure = find_class("BlockClosure");
    cls_UndefinedObject = find_class("UndefinedObject");
    cls_Message = find_class("Message");
    objs[1].cls = cls_UndefinedObject;
    c = find_class("True");
    objs[2].cls = c;
    c = find_class("False");
    objs[3].cls = c;
    gset("nil", NIL_OOP);
    gset("true", TRUE_OOP);
    gset("false", FALSE_OOP);
    o = newobj(find_class("SystemDictionary"), KD_PLAIN, 0);
    gset("Smalltalk", o);
    o = newobj(find_class("TextCollector"), KD_PLAIN, 0);
    gset("Transcript", o);
}
/* ===== s2_lex.c ===== */
/* =========================================================================
   s2_lex.c   lexer (text in txt[0..tlen-1], position tp is 0-based)
   ========================================================================= */
static const char *txt;
static long tlen, tp;
static const char *src_name;
static int pe;
static Tok cur, pk, lx;
static int pk_have;

#define ctk (cur.t)
#define cts (cur.s)
#define cti (cur.i)
#define ctf (cur.f)
#define ctpos (cur.p)

typedef struct {
    long tp;
    Tok cur, pk;
    int have;
} Mark;
static Mark *marks;
static long nmk, markcap;

static void tok_set(Tok *d, int t, const char *s, long slen, long i, double f, long p)
{
    if (d->s != NULL) free(d->s);
    d->s = (s != NULL) ? dupn(s, slen) : dupn("", 0);
    d->slen = slen;
    d->t = t;
    d->i = i;
    d->f = f;
    d->p = p;
}

static void tok_assign(Tok *d, const Tok *s)
{
    tok_set(d, s->t, s->s, s->slen, s->i, s->f, s->p);
}

static int tchar(long p)
{
    if (p < 0 || p >= tlen) return 0;
    return (unsigned char)txt[p];
}

static long line_of(long p)
{
    long n = 1, i;
    for (i = 0; i < p && i < tlen; i++) {
        if (txt[i] == '\n') n++;
    }
    return n;
}

static void perr(const char *msg)
{
    char buf[40];
    if (pe == 0) {
        pe = 1;
        outs("Parse error (");
        outs(src_name);
        outs(" line ");
        sprintf(buf, "%ld", line_of(tp));
        outs(buf);
        outs("): ");
        outs(msg);
        outs("\n");
    }
}

static void lex_set(int t, const char *s, long slen, long i, double f)
{
    tok_set(&lx, t, s, slen, i, f, lx.p);
}

static int is_alnum_ch(int c)
{
    return is_digit(c) || is_letter(c);
}

static void lex_number(void)
{
    long q = tp, p, v;
    SB dg, fr, ex, num;
    int isf = 0, neg = 0, c;
    long startp = tp;
    sb_init(&dg); sb_init(&fr); sb_init(&ex); sb_init(&num);
    while (q < tlen && is_digit(tchar(q))) q++;
    sb_addn(&dg, txt + tp, q - tp);
    c = tchar(q);
    if (c == 'r' && (is_digit(tchar(q + 1)) || is_upper(tchar(q + 1)))) {
        long radix = atol(sb_str(&dg));
        double acc = 0.0;
        q++;
        while (q < tlen) {
            c = tchar(q);
            if (is_digit(c)) p = c - '0';
            else if (is_upper(c)) p = (long)(strchr(UPPERS, c) - UPPERS) + 10;
            else break;
            if (p >= radix) break;
            acc = acc * (double)radix + (double)p;
            q++;
        }
        tp = q;
        if (acc <= (double)SI_MAX) {
            lex_set(TK_INT, "", 0, (long)acc, 0.0);
        } else {
            char tmp[64];
            sprintf(tmp, "%.0f", acc);
            lex_set(TK_BIGINT, tmp, (long)strlen(tmp), 0, 0.0);
        }
        goto done;
    }
    if (tchar(q) == '.' && is_digit(tchar(q + 1))) {
        isf = 1;
        p = q + 1;
        while (p < tlen && is_digit(tchar(p))) p++;
        sb_addn(&fr, txt + q + 1, p - q - 1);
        q = p;
    }
    c = tchar(q);
    if (c == 'e' || c == 'd' || c == 'q') {
        p = q + 1;
        neg = 0;
        if (tchar(p) == '-') {
            neg = 1;
            p++;
        }
        if (is_digit(tchar(p))) {
            v = p;
            while (v < tlen && is_digit(tchar(v))) v++;
            if (neg) {
                sb_addc(&ex, '-');
                isf = 1;
            }
            sb_addn(&ex, txt + p, v - p);
            q = v;
        }
    }
    /* scaled decimal 1.5s2, 3s2, 1.5s */
    if (tchar(q) == 's' && ex.n == 0) {
        p = q + 1;
        if (is_digit(tchar(p))) {
            v = p;
            while (v < tlen && is_digit(tchar(v))) v++;
            {
                char *t = dupn(txt + p, v - p);
                long sc = atol(t);
                free(t);
                if (fr.n == 0) {
                    lex_set(TK_SCALED, dg.p, dg.n, sc, 0.0);
                } else {
                    sb_addn(&num, dg.p, dg.n);
                    sb_addc(&num, '.');
                    sb_addn(&num, fr.p, fr.n);
                    lex_set(TK_SCALED, num.p, num.n, sc, 0.0);
                }
            }
            tp = v;
            goto done;
        } else if (isf == 1 && !is_alnum_ch(tchar(p))) {
            sb_addn(&num, dg.p, dg.n);
            sb_addc(&num, '.');
            sb_addn(&num, fr.p, fr.n);
            lex_set(TK_SCALED, num.p, num.n, fr.n, 0.0);
            tp = p;
            goto done;
        }
    }
    tp = q;
    if (isf == 1) {
        double d;
        sb_addn(&num, dg.p, dg.n);
        sb_addc(&num, '.');
        if (fr.n == 0) sb_addc(&num, '0');
        else sb_addn(&num, fr.p, fr.n);
        if (ex.n > 0) {
            sb_addc(&num, 'E');
            sb_addn(&num, ex.p, ex.n);
        }
        d = strtod(sb_str(&num), NULL);
        lex_set(TK_FLOAT, "", 0, 0, d);
        goto done;
    }
    if (ex.n > 0 && strcmp(sb_str(&ex), "0") != 0) {
        long z = atol(ex.p);
        long k;
        for (k = 0; k < z && k < 10000; k++) sb_addc(&dg, '0');
    }
    {
        long k = 0;
        while (k < dg.n - 1 && dg.p[k] == '0') k++;
        if (dg.n - k > 9) {
            lex_set(TK_BIGINT, dg.p + k, dg.n - k, 0, 0.0);
        } else {
            lex_set(TK_INT, "", 0, atol(dg.p), 0.0);
        }
    }
done:
    (void)startp;
    sb_free(&dg); sb_free(&fr); sb_free(&ex); sb_free(&num);
}

/* scan the next token into lx */
static void lex_token(void)
{
    int c, d;
    long q, p;
    SB s;
    for (;;) {
        if (tp >= tlen) break;
        c = tchar(tp);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') tp++;
        else if (c == '"') {
            q = tp + 1;
            while (q < tlen && txt[q] != '"') q++;
            if (q >= tlen) tp = tlen;
            else tp = q + 1;
        } else break;
    }
    lx.p = tp;
    if (tp >= tlen) {
        lex_set(TK_EOF, "", 0, 0, 0.0);
        return;
    }
    c = tchar(tp);
    if (is_letter(c)) {
        q = tp;
        while (q < tlen && is_alnum_ch(tchar(q))) q++;
        /* namespace dots:  Foo.Bar */
        while (q + 1 < tlen && tchar(q) == '.' && is_letter(tchar(q + 1)) && is_upper(tchar(tp)) && is_upper(tchar(q + 1))) {
            q++;
            while (q < tlen && is_alnum_ch(tchar(q))) q++;
        }
        if (tchar(q) == ':' && tchar(q + 1) != '=') {
            lex_set(TK_KW, txt + tp, q + 1 - tp, 0, 0.0);
            tp = q + 1;
            return;
        }
        p = q;
        while (p > tp && txt[p - 1] != '.') p--;
        lex_set(TK_IDENT, txt + p, q - p, 0, 0.0);
        tp = q;
        return;
    }
    if (is_digit(c)) {
        lex_number();
        return;
    }
    if (c == '\'') {
        sb_init(&s);
        q = tp + 1;
        for (;;) {
            p = q;
            while (p < tlen && txt[p] != '\'') p++;
            if (p >= tlen) {
                sb_addn(&s, txt + q, tlen - q);
                tp = tlen;
                break;
            }
            sb_addn(&s, txt + q, p - q);
            if (tchar(p + 1) == '\'') {
                sb_addc(&s, '\'');
                q = p + 2;
            } else {
                tp = p + 1;
                break;
            }
        }
        lex_set(TK_STR, sb_str(&s), s.n, 0, 0.0);
        sb_free(&s);
        return;
    }
    if (c == '$') {
        lex_set(TK_CHAR, "", 0, tchar(tp + 1), 0.0);
        tp += 2;
        return;
    }
    if (c == '#') {
        d = tchar(tp + 1);
        if (d == '(') {
            lex_set(TK_HLP, "", 0, 0, 0.0);
            tp += 2;
            return;
        }
        if (d == '[') {
            lex_set(TK_HLB, "", 0, 0, 0.0);
            tp += 2;
            return;
        }
        if (d == '\'') {
            tp++;
            lex_token();
            lx.t = TK_SYM;
            return;
        }
        if (d == '#') {
            tp++;
            lex_token();
            return;
        }
        if (is_letter(d)) {
            q = tp + 1;
            while (q < tlen && (is_alnum_ch(tchar(q)) || tchar(q) == ':' || (tchar(q) == '.' && is_letter(tchar(q + 1))))) q++;
            lex_set(TK_SYM, txt + tp + 1, q - tp - 1, 0, 0.0);
            tp = q;
            return;
        }
        if (is_binch(d)) {
            q = tp + 1;
            while (q < tlen && is_binch(tchar(q))) q++;
            lex_set(TK_SYM, txt + tp + 1, q - tp - 1, 0, 0.0);
            tp = q;
            return;
        }
        lex_set(TK_BIN, "#", 1, 0, 0.0);
        tp++;
        return;
    }
    tp++;
    switch (c) {
    case ':':
        if (tchar(tp) == '=') {
            tp++;
            lex_set(TK_ASSIGN, "", 0, 0, 0.0);
        } else lex_set(TK_COLON, "", 0, 0, 0.0);
        return;
    case '^': lex_set(TK_CARET, "", 0, 0, 0.0); return;
    case '(': lex_set(TK_LP, "", 0, 0, 0.0); return;
    case ')': lex_set(TK_RP, "", 0, 0, 0.0); return;
    case '[': lex_set(TK_LB, "", 0, 0, 0.0); return;
    case ']': lex_set(TK_RB, "", 0, 0, 0.0); return;
    case '{': lex_set(TK_LC, "", 0, 0, 0.0); return;
    case '}': lex_set(TK_RC, "", 0, 0, 0.0); return;
    case '.': lex_set(TK_DOT, "", 0, 0, 0.0); return;
    case ';': lex_set(TK_SEMI, "", 0, 0, 0.0); return;
    case '!': lex_set(TK_BANG, "", 0, 0, 0.0); return;
    default:
        sb_init(&s);
        sb_addc(&s, c);
        while (tp < tlen && is_binch(tchar(tp))) {
            sb_addc(&s, tchar(tp));
            tp++;
        }
        if (s.n > 1 && s.p[s.n - 1] == '-' && is_digit(tchar(tp))) {
            s.n--;
            s.p[s.n] = '\0';
            tp--;
        }
        lex_set(TK_BIN, sb_str(&s), s.n, 0, 0.0);
        sb_free(&s);
        return;
    }
}

/* token stream with one token of lookahead */
static void advance(void)
{
    if (pk_have) {
        tok_assign(&cur, &pk);
        pk_have = 0;
    } else {
        lex_token();
        tok_assign(&cur, &lx);
    }
}

static void peek_tok(void)
{
    if (!pk_have) {
        lex_token();
        tok_assign(&pk, &lx);
        pk_have = 1;
    }
}

static long lex_mark(void)
{
    Mark *m;
    nmk++;
    if (nmk >= markcap) {
        long oc = markcap;
        marks = (Mark *)xgrow(marks, &markcap, nmk, sizeof(Mark));
        memset(marks + oc, 0, sizeof(Mark) * (size_t)(markcap - oc));
    }
    m = &marks[nmk];
    m->tp = tp;
    tok_assign(&m->cur, &cur);
    tok_assign(&m->pk, &pk);
    m->have = pk_have;
    return nmk;
}

static void lex_reset(long m)
{
    tp = marks[m].tp;
    tok_assign(&cur, &marks[m].cur);
    tok_assign(&pk, &marks[m].pk);
    pk_have = marks[m].have;
    nmk = m - 1;
}

static void lex_drop(long m)
{
    nmk = m - 1;
}

static int isbin(const char *s)
{
    return ctk == TK_BIN && strcmp(cts, s) == 0;
}
/* ===== s3_parse.c ===== */
/* =========================================================================
   s3_parse.c   raw syntax tree, methods, classes, top level items, file-in
   ========================================================================= */
typedef struct {
    long *p;
    long n, cap;
} LV;

static void lv_init(LV *v)
{
    v->p = NULL;
    v->n = 0;
    v->cap = 0;
}

static void lv_push(LV *v, long x)
{
    if (v->n + 1 >= v->cap) v->p = (long *)xgrow(v->p, &v->cap, v->n + 1, sizeof(long));
    v->p[v->n++] = x;
}

static void lv_free(LV *v)
{
    if (v->p != NULL) free(v->p);
    v->p = NULL;
    v->n = v->cap = 0;
}

/* name list for parameters and temporaries */
static char **nl_name;
static long nnl, nlcap;
static int pm_prim;
static long pm_tfirst;
static int pm_ntemps;

static void nl_add(const char *s)
{
    nnl++;
    if (nnl >= nlcap) nl_name = (char **)xgrow(nl_name, &nlcap, nnl, sizeof(char *));
    nl_name[nnl] = dups(s);
}

static long nd(int t, long a, long b, long c, long d, long i, const char *s)
{
    nnode++;
    if (nnode >= nodecap) nodes = (Node *)xgrow(nodes, &nodecap, nnode, sizeof(Node));
    nodes[nnode].t = t;
    nodes[nnode].a = a;
    nodes[nnode].b = b;
    nodes[nnode].c = c;
    nodes[nnode].d = d;
    nodes[nnode].i = i;
    nodes[nnode].s = (s != NULL && s[0] != '\0') ? dups(s) : NULL;
    return nnode;
}

static long al_put(long v)
{
    nal++;
    if (nal >= alcap) al = (long *)xgrow(al, &alcap, nal, sizeof(long));
    al[nal] = v;
    return nal;
}

/* copy a temp list into al[]; answers the index of the first element */
static long al_list(LV *v)
{
    long first = nal + 1, k;
    for (k = 0; k < v->n; k++) al_put(v->p[k]);
    return first;
}

static const char *nsx(long n)
{
    return nodes[n].s != NULL ? nodes[n].s : "";
}

static long parse_expr(void);
static long parse_keyword_expr(void);
static long parse_statement(void);
static long parse_primary(void);
static long parse_binary_expr(void);
static long parse_unary_expr(void);
static long parse_block(void);
static int parse_temps(void);

static long mk_send(long rcv, const char *sel, long first, long n)
{
    int sup = 0;
    if (NT(rcv) == ND_RVAR && strcmp(nsx(rcv), "super") == 0) sup = 1;
    return nd(ND_RSEND, rcv, first, 0, sup, n, sel);
}

/* statements until the closing token; returns an ND_SEQ node */
static long parse_stmts(int endt)
{
    LV tmp;
    long st, first;
    lv_init(&tmp);
    for (;;) {
        while (ctk == TK_DOT) advance();
        if (ctk == endt || ctk == TK_EOF || pe == 1) break;
        if (endt == TK_RB && ctk == TK_BANG) break;
        st = parse_statement();
        lv_push(&tmp, st);
        if (ctk == TK_DOT) advance();
        else break;
    }
    first = al_list(&tmp);
    st = nd(ND_SEQ, first, tmp.n, 0, 0, 0, "");
    lv_free(&tmp);
    return st;
}

static long parse_statement(void)
{
    long e;
    if (ctk == TK_CARET) {
        advance();
        e = parse_expr();
        return nd(ND_RRET, e, 0, 0, 0, 0, "");
    }
    return parse_expr();
}

static long parse_msg_part(void);

static long parse_cascade(long e0)
{
    long rcv, first, m;
    LV tmp;
    if (NT(e0) != ND_RSEND) {
        perr("cascade needs a message send");
        return e0;
    }
    rcv = NA(e0);
    lv_init(&tmp);
    lv_push(&tmp, nd(ND_RMSG, 0, NB(e0), 0, ND(e0), NI(e0), nsx(e0)));
    while (ctk == TK_SEMI && pe == 0) {
        advance();
        lv_push(&tmp, parse_msg_part());
    }
    first = al_list(&tmp);
    m = nd(ND_RCASC, rcv, first, tmp.n, 0, 0, "");
    lv_free(&tmp);
    return m;
}

static long parse_expr(void)
{
    long e;
    char *nme;
    if (ctk == TK_IDENT) {
        peek_tok();
        if (pk.t == TK_ASSIGN) {
            nme = dups(cts);
            advance();
            advance();
            e = parse_expr();
            e = nd(ND_RASG, e, 0, 0, 0, 0, nme);
            free(nme);
            return e;
        }
    }
    e = parse_keyword_expr();
    if (ctk == TK_SEMI) e = parse_cascade(e);
    return e;
}

/* one message (unary, binary or keyword) without a receiver */
static long parse_msg_part(void)
{
    long a, first, m;
    char *sel;
    LV tmp;
    SB sb;
    if (ctk == TK_IDENT) {
        sel = dups(cts);
        advance();
        m = nd(ND_RMSG, 0, 0, 0, 0, 0, sel);
        free(sel);
        return m;
    }
    if (ctk == TK_BIN) {
        sel = dups(cts);
        advance();
        a = parse_unary_expr();
        first = al_put(a);
        m = nd(ND_RMSG, 0, first, 0, 0, 1, sel);
        free(sel);
        return m;
    }
    if (ctk == TK_KW) {
        sb_init(&sb);
        lv_init(&tmp);
        while (ctk == TK_KW && pe == 0) {
            sb_adds(&sb, cts);
            advance();
            lv_push(&tmp, parse_binary_expr());
        }
        first = al_list(&tmp);
        m = nd(ND_RMSG, 0, first, 0, 0, tmp.n, sb_str(&sb));
        lv_free(&tmp);
        sb_free(&sb);
        return m;
    }
    perr("bad cascade message");
    return nd(ND_RMSG, 0, 0, 0, 0, 0, "yourself");
}

static long parse_keyword_expr(void)
{
    long rcv, first, r;
    LV tmp;
    SB sb;
    rcv = parse_binary_expr();
    if (ctk != TK_KW) return rcv;
    sb_init(&sb);
    lv_init(&tmp);
    while (ctk == TK_KW && pe == 0) {
        sb_adds(&sb, cts);
        advance();
        lv_push(&tmp, parse_binary_expr());
    }
    first = al_list(&tmp);
    r = mk_send(rcv, sb_str(&sb), first, tmp.n);
    lv_free(&tmp);
    sb_free(&sb);
    return r;
}

static long parse_binary_expr(void)
{
    long rcv, a, first;
    char *sel;
    rcv = parse_unary_expr();
    while (ctk == TK_BIN && pe == 0) {
        sel = dups(cts);
        advance();
        a = parse_unary_expr();
        first = al_put(a);
        rcv = mk_send(rcv, sel, first, 1);
        free(sel);
    }
    return rcv;
}

static long parse_unary_expr(void)
{
    long rcv;
    rcv = parse_primary();
    while (ctk == TK_IDENT && pe == 0) {
        peek_tok();
        if (pk.t == TK_ASSIGN) break;
        rcv = mk_send(rcv, cts, 0, 0);
        advance();
    }
    return rcv;
}

static long parse_brace(void)
{
    LV tmp;
    long first, r;
    advance();
    lv_init(&tmp);
    for (;;) {
        while (ctk == TK_DOT) advance();
        if (ctk == TK_RC || ctk == TK_EOF || pe == 1) break;
        lv_push(&tmp, parse_expr());
        if (ctk == TK_DOT) advance();
        else break;
    }
    if (ctk == TK_RC) advance();
    else perr("missing }");
    first = al_list(&tmp);
    r = nd(ND_BRACE, first, tmp.n, 0, 0, 0, "");
    lv_free(&tmp);
    return r;
}

static oop parse_litelem(void);
static oop parse_bytearray(void);

static oop litarray_from(LV *tmp)
{
    oop arr = mkarray(tmp->n);
    long m;
    for (m = 0; m < tmp->n; m++) SLOT(arr, m) = tmp->p[m];
    return arr;
}

/* literal array contents after #( or ( */
static oop parse_litarray_o(void)
{
    LV tmp;
    oop arr;
    lv_init(&tmp);
    for (;;) {
        if (ctk == TK_RP || ctk == TK_EOF || pe == 1) break;
        lv_push(&tmp, parse_litelem());
    }
    if (ctk == TK_RP) advance();
    else perr("missing ) in literal array");
    arr = litarray_from(&tmp);
    lv_free(&tmp);
    return arr;
}

static oop parse_litarray2(void)
{
    LV tmp;
    oop arr;
    lv_init(&tmp);
    while (ctk != TK_RC && ctk != TK_EOF && pe == 0) lv_push(&tmp, parse_litelem());
    if (ctk == TK_RC) advance();
    arr = litarray_from(&tmp);
    lv_free(&tmp);
    return arr;
}

static oop parse_bytearray(void)
{
    LV tmp;
    oop arr;
    int m;
    lv_init(&tmp);
    while (ctk == TK_INT) {
        lv_push(&tmp, mkint(cti));
        advance();
    }
    if (ctk == TK_RB) advance();
    else perr("missing ] in byte array");
    arr = litarray_from(&tmp);
    lv_free(&tmp);
    m = find_class("ByteArray");
    if (m > 0) objs[HI(arr)].cls = m;
    return arr;
}

static oop parse_litelem(void)
{
    oop o;
    char *s;
    if (ctk == TK_INT) { o = mkint(cti); advance(); return o; }
    if (ctk == TK_BIGINT) { o = mkbig(cts); advance(); return o; }
    if (ctk == TK_FLOAT) { o = mkfloat(ctf); advance(); return o; }
    if (ctk == TK_STR) { o = mkstrn(cts, cur.slen); advance(); return o; }
    if (ctk == TK_SYM) { o = mksym(cts); advance(); return o; }
    if (ctk == TK_CHAR) { o = mkchar(cti); advance(); return o; }
    if (ctk == TK_HLP || ctk == TK_LP) { advance(); return parse_litarray_o(); }
    if (ctk == TK_HLB || ctk == TK_LB) { advance(); return parse_bytearray(); }
    if (ctk == TK_LC) { advance(); return parse_litarray2(); }
    if (ctk == TK_IDENT) {
        s = dups(cts);
        advance();
        if (strcmp(s, "nil") == 0) o = NIL_OOP;
        else if (strcmp(s, "true") == 0) o = TRUE_OOP;
        else if (strcmp(s, "false") == 0) o = FALSE_OOP;
        else o = mksym(s);
        free(s);
        return o;
    }
    if (ctk == TK_KW) {
        SB sb;
        sb_init(&sb);
        sb_adds(&sb, cts);
        advance();
        while (ctk == TK_KW) {
            sb_adds(&sb, cts);
            advance();
        }
        o = mksym(sb_str(&sb));
        sb_free(&sb);
        return o;
    }
    if (ctk == TK_BIN) {
        s = dups(cts);
        advance();
        if (strcmp(s, "-") == 0 && (ctk == TK_INT || ctk == TK_FLOAT)) {
            if (ctk == TK_INT) o = mkint(0 - cti);
            else o = mkfloat(0.0 - ctf);
            advance();
            free(s);
            return o;
        }
        o = mksym(s);
        free(s);
        return o;
    }
    perr("bad literal array element");
    advance();
    return NIL_OOP;
}

static long parse_primary(void)
{
    char *s;
    long e, t, o;
    oop v;
    if (ctk == TK_IDENT) {
        s = dups(cts);
        advance();
        if (strcmp(s, "nil") == 0) o = nd(ND_LIT, NIL_OOP, 0, 0, 0, 0, "");
        else if (strcmp(s, "true") == 0) o = nd(ND_LIT, TRUE_OOP, 0, 0, 0, 0, "");
        else if (strcmp(s, "false") == 0) o = nd(ND_LIT, FALSE_OOP, 0, 0, 0, 0, "");
        else if (strcmp(s, "thisContext") == 0) o = nd(ND_THISCTX, 0, 0, 0, 0, 0, "");
        else o = nd(ND_RVAR, 0, 0, 0, 0, 0, s);
        free(s);
        return o;
    }
    if (ctk == TK_INT) { v = mkint(cti); advance(); return nd(ND_LIT, v, 0, 0, 0, 0, ""); }
    if (ctk == TK_BIGINT) { v = mkbig(cts); advance(); return nd(ND_LIT, v, 0, 0, 0, 0, ""); }
    if (ctk == TK_FLOAT) { v = mkfloat(ctf); advance(); return nd(ND_LIT, v, 0, 0, 0, 0, ""); }
    if (ctk == TK_SCALED) {
        e = nd(ND_LIT, mkstrn(cts, cur.slen), 0, 0, 0, 0, "");
        t = nd(ND_LIT, mkint(cti), 0, 0, 0, 0, "");
        o = al_put(e);
        al_put(t);
        advance();
        return mk_send(nd(ND_RVAR, 0, 0, 0, 0, 0, "ScaledDecimal"), "fromString:scale:", o, 2);
    }
    if (ctk == TK_STR) { v = mkstrn(cts, cur.slen); advance(); return nd(ND_LIT, v, 0, 0, 0, 0, ""); }
    if (ctk == TK_SYM) { v = mksym(cts); advance(); return nd(ND_LIT, v, 0, 0, 0, 0, ""); }
    if (ctk == TK_CHAR) { v = mkchar(cti); advance(); return nd(ND_LIT, v, 0, 0, 0, 0, ""); }
    if (ctk == TK_HLP) { advance(); v = parse_litarray_o(); return nd(ND_LIT, v, 0, 0, 0, 0, ""); }
    if (ctk == TK_HLB) { advance(); v = parse_bytearray(); return nd(ND_LIT, v, 0, 0, 0, 0, ""); }
    if (ctk == TK_LP) {
        advance();
        e = parse_expr();
        if (ctk == TK_RP) advance();
        else perr("missing )");
        return e;
    }
    if (ctk == TK_LB) return parse_block();
    if (ctk == TK_LC) return parse_brace();
    if (ctk == TK_BIN && strcmp(cts, "-") == 0) {
        peek_tok();
        if (pk.t == TK_INT || pk.t == TK_FLOAT || pk.t == TK_BIGINT) {
            if (pk.p == ctpos + 1) {
                advance();
                if (ctk == TK_INT) v = mkint(0 - cti);
                else if (ctk == TK_FLOAT) v = mkfloat(0.0 - ctf);
                else {
                    SB sb;
                    sb_init(&sb);
                    sb_addc(&sb, '-');
                    sb_adds(&sb, cts);
                    v = mkbig(sb_str(&sb));
                    sb_free(&sb);
                }
                advance();
                return nd(ND_LIT, v, 0, 0, 0, 0, "");
            }
        }
    }
    {
        SB sb;
        sb_init(&sb);
        sb_adds(&sb, "expression expected, found '");
        sb_adds(&sb, cts);
        sb_adds(&sb, "'");
        perr(sb_str(&sb));
        sb_free(&sb);
    }
    advance();
    return nd(ND_LIT, NIL_OOP, 0, 0, 0, 0, "");
}

/* | a b c |  (returns the count; names are appended to nl_name) */
static int parse_temps(void)
{
    int n = 0;
    while (isbin("|") || isbin("||")) {
        if (isbin("||")) advance();
        else {
            advance();
            while (ctk == TK_IDENT) {
                nl_add(cts);
                n++;
                advance();
            }
            if (isbin("|")) advance();
            else perr("| expected after temporaries");
        }
    }
    return n;
}

/* [ :a :b | | t | stmts ] */
static long parse_block(void)
{
    int np = 0, nt2;
    long first_p, first_t, body;
    advance();
    first_p = nnl + 1;
    while (ctk == TK_COLON) {
        advance();
        if (ctk == TK_IDENT) {
            nl_add(cts);
            np++;
            advance();
        } else perr("block parameter name expected");
    }
    if (np > 0) {
        if (isbin("|")) advance();
        else if (isbin("||")) {
            /* [:a || t | ...]  -- the second bar opens a temporaries list */
            tok_set(&cur, TK_BIN, "|", 1, 0, 0.0, cur.p);
        } else if (ctk != TK_RB) perr("| expected after block parameters");
    }
    first_t = nnl + 1;
    nt2 = parse_temps();
    body = parse_stmts(TK_RB);
    if (ctk == TK_RB) advance();
    else perr("missing ]");
    return nd(ND_RBLOCK, body, first_p, np, first_t, nt2, "");
}

/* ---- methods and classes ------------------------------------------------ */
static void skip_pragma(void)
{
    int k;
    advance();
    if (ctk == TK_KW && strcmp(cts, "primitive:") == 0) {
        advance();
        if (ctk == TK_IDENT) {
            pm_prim = 0;
            for (k = 1; prim_names[k] != 0; k++) {
                if (strcmp(prim_names[k], cts) == 0) {
                    pm_prim = k;
                    break;
                }
            }
            if (pm_prim == 0) {
                SB sb;
                sb_init(&sb);
                sb_adds(&sb, "unknown primitive ");
                sb_adds(&sb, cts);
                perr(sb_str(&sb));
                sb_free(&sb);
            }
        } else if (ctk == TK_INT) pm_prim = (int)cti;
        advance();
    }
    while (ctk != TK_EOF && !isbin(">")) advance();
    if (ctk != TK_EOF) advance();
}

/* method body after the "[": temps, pragmas, statements, up to "]" */
static long parse_method_body(void)
{
    long body;
    pm_prim = 0;
    pm_tfirst = nnl + 1;
    pm_ntemps = 0;
    for (;;) {
        if (isbin("|") || isbin("||")) pm_ntemps += parse_temps();
        else if (isbin("<")) skip_pragma();
        else break;
    }
    body = parse_stmts(TK_RB);
    if (ctk == TK_RB) advance();
    else perr("missing ] at end of method");
    return body;
}

/* parse "pattern [ body ]" and install it in class c */
static void parse_method_def(int c)
{
    SB sel;
    int np = 0;
    long pfirst, body, tfirst, startp;
    int ntemps, prim_;
    startp = ctpos;
    sb_init(&sel);
    pfirst = nnl + 1;
    if (ctk == TK_IDENT) {
        sb_adds(&sel, cts);
        advance();
    } else if (ctk == TK_BIN) {
        sb_adds(&sel, cts);
        advance();
        if (ctk == TK_IDENT) {
            nl_add(cts);
            np = 1;
            advance();
        } else perr("binary method parameter expected");
    } else if (ctk == TK_KW) {
        while (ctk == TK_KW) {
            sb_adds(&sel, cts);
            advance();
            if (ctk == TK_IDENT) {
                nl_add(cts);
                np++;
                advance();
            } else perr("keyword method parameter expected");
        }
    } else {
        perr("method pattern expected");
        advance();
        sb_free(&sel);
        return;
    }
    if (ctk != TK_LB) {
        SB m;
        sb_init(&m);
        sb_adds(&m, "[ expected after method pattern ");
        sb_adds(&m, sb_str(&sel));
        perr(sb_str(&m));
        sb_free(&m);
        sb_free(&sel);
        return;
    }
    advance();
    body = parse_method_body();
    if (pe == 1) {
        sb_free(&sel);
        return;
    }
    tfirst = pm_tfirst;
    ntemps = pm_ntemps;
    prim_ = pm_prim;
    compile_method(c, sb_str(&sel), pfirst, np, tfirst, ntemps, body, prim_, startp);
    sb_free(&sel);
}

/* is the token sequence at the mark "pattern [" ? */
static int pattern_follows(void)
{
    long m;
    int ok = 0;
    m = lex_mark();
    if (ctk == TK_IDENT) {
        advance();
        if (ctk == TK_LB) ok = 1;
    } else if (ctk == TK_BIN) {
        advance();
        if (ctk == TK_IDENT) {
            advance();
            if (ctk == TK_LB) ok = 1;
        }
    } else if (ctk == TK_KW) {
        while (ctk == TK_KW) {
            advance();
            if (ctk == TK_IDENT) advance();
            else break;
        }
        if (ctk == TK_LB) ok = 1;
    }
    lex_reset(m);
    return ok;
}

static void unknown_class(const char *nme, const char *what)
{
    SB sb;
    sb_init(&sb);
    sb_adds(&sb, what);
    sb_adds(&sb, nme);
    perr(sb_str(&sb));
    sb_free(&sb);
}

/* items inside  Foo subclass: Bar [ ... ]  (cnum is the class) */
static void parse_class_body(int cnum)
{
    char *nme;
    int c2;
    long m, e;
    for (;;) {
        if (ctk == TK_RB || ctk == TK_EOF || pe == 1) break;
        if (ctk == TK_DOT) {
            advance();
            continue;
        }
        if ((isbin("|") || isbin("||")) && !pattern_follows()) {
            while (isbin("|") || isbin("||")) {
                if (isbin("||")) advance();
                else {
                    advance();
                    while (ctk == TK_IDENT) {
                        add_ivar(cnum, cts);
                        advance();
                    }
                    if (isbin("|")) advance();
                    else perr("| expected");
                }
            }
            continue;
        }
        if (isbin("<") && !pattern_follows()) {
            pm_prim = 0;
            skip_pragma();
            continue;
        }
        if (ctk == TK_IDENT) {
            peek_tok();
            if (pk.t == TK_ASSIGN) {
                nme = dups(cts);
                advance();
                advance();
                e = parse_expr();
                if (ctk == TK_DOT) advance();
                if (pe == 0) define_classvar(cnum, nme, e);
                free(nme);
                continue;
            }
            if (pk.t == TK_IDENT && strcmp(pk.s, "class") == 0) {
                /* Foo class >> sel [ ]   or   Foo class [ | a | ] */
                nme = dups(cts);
                m = lex_mark();
                advance();
                advance();
                if (isbin(">>")) {
                    lex_drop(m);
                    advance();
                    c2 = find_class(nme);
                    if (c2 == 0) {
                        unknown_class(nme, "unknown class ");
                        free(nme);
                        break;
                    }
                    free(nme);
                    parse_method_def(cls[c2].meta);
                    continue;
                }
                if (ctk == TK_LB) {
                    lex_drop(m);
                    advance();
                    c2 = find_class(nme);
                    if (c2 == 0) {
                        unknown_class(nme, "unknown class ");
                        free(nme);
                        break;
                    }
                    free(nme);
                    parse_class_body(cls[c2].meta);
                    continue;
                }
                lex_reset(m);
                free(nme);
            }
            if (pk.t == TK_BIN && strcmp(pk.s, ">>") == 0) {
                nme = dups(cts);
                advance();
                advance();
                c2 = find_class(nme);
                if (c2 == 0) {
                    unknown_class(nme, "unknown class ");
                    free(nme);
                    break;
                }
                free(nme);
                parse_method_def(c2);
                continue;
            }
        }
        parse_method_def(cnum);
    }
    if (ctk == TK_RB) advance();
}

/* process one top-level construct */
static void top_item(void)
{
    long m, st;
    int n, cn, sc;
    char *nme, *sup;
    if (ctk == TK_IDENT && strcmp(cts, "Eval") == 0) {
        peek_tok();
        if (pk.t == TK_LB) {
            advance();
            advance();
            in_eval = 1;
            for (;;) {
                while (ctk == TK_DOT) advance();
                if (ctk == TK_RB || ctk == TK_EOF || pe == 1) break;
                if (isbin("|") || isbin("||")) {
                    n = parse_temps();
                    (void)n;
                    continue;
                }
                st = parse_statement();
                if (pe == 0) run_doit(st);
                if (ctk == TK_DOT) advance();
            }
            if (ctk == TK_RB) advance();
            in_eval = 0;
            run_scheduler();
            return;
        }
    }
    if (ctk == TK_IDENT) {
        m = lex_mark();
        nme = dups(cts);
        advance();
        if (ctk == TK_KW && strcmp(cts, "subclass:") == 0) {
            advance();
            if (ctk == TK_IDENT) {
                sup = nme;
                nme = dups(cts);
                advance();
                if (ctk == TK_LB) {
                    lex_drop(m);
                    advance();
                    sc = find_class(sup);
                    if (sc == 0 && strcmp(sup, "nil") != 0) {
                        unknown_class(sup, "unknown superclass ");
                        free(sup);
                        free(nme);
                        return;
                    }
                    cn = define_class(nme, sc);
                    free(sup);
                    free(nme);
                    parse_class_body(cn);
                    return;
                }
                free(sup);
            }
        } else if (ctk == TK_IDENT && strcmp(cts, "extend") == 0) {
            advance();
            if (ctk == TK_LB) {
                lex_drop(m);
                advance();
                cn = find_class(nme);
                if (cn == 0) {
                    unknown_class(nme, "unknown class ");
                    free(nme);
                    return;
                }
                free(nme);
                parse_class_body(cn);
                return;
            }
        } else if (ctk == TK_IDENT && strcmp(cts, "class") == 0) {
            advance();
            if (ctk == TK_IDENT && strcmp(cts, "extend") == 0) {
                advance();
                if (ctk == TK_LB) {
                    lex_drop(m);
                    advance();
                    cn = find_class(nme);
                    if (cn == 0) {
                        unknown_class(nme, "unknown class ");
                        free(nme);
                        return;
                    }
                    free(nme);
                    parse_class_body(cls[cn].meta);
                    return;
                }
            } else if (isbin(">>")) {
                advance();
                if (pattern_follows()) {
                    lex_drop(m);
                    cn = find_class(nme);
                    if (cn == 0) {
                        unknown_class(nme, "unknown class ");
                        free(nme);
                        return;
                    }
                    free(nme);
                    parse_method_def(cls[cn].meta);
                    return;
                }
            } else if (ctk == TK_LB) {
                lex_drop(m);
                advance();
                cn = find_class(nme);
                if (cn == 0) {
                    unknown_class(nme, "unknown class ");
                    free(nme);
                    return;
                }
                free(nme);
                parse_class_body(cls[cn].meta);
                return;
            }
        } else if (isbin(">>")) {
            advance();
            if (pattern_follows()) {
                lex_drop(m);
                cn = find_class(nme);
                if (cn == 0) {
                    unknown_class(nme, "unknown class ");
                    free(nme);
                    return;
                }
                free(nme);
                parse_method_def(cn);
                return;
            }
        }
        lex_reset(m);
        free(nme);
    }
    if (isbin("|") || isbin("||")) {
        n = parse_temps();
        (void)n;
        return;
    }
    st = parse_statement();
    if (pe == 0) run_doit(st);
}

/* ---- file-in: parse and execute a whole source text ----------------------------- */
static void file_in(const char *text, long len, const char *name)
{
    const char *sv_txt = txt;
    long sv_tp = tp, sv_len = tlen;
    const char *sv_name = src_name;
    int sv_pe = pe, sv_have = pk_have;
    Tok sv_cur, sv_pk;
    memset(&sv_cur, 0, sizeof(sv_cur));
    memset(&sv_pk, 0, sizeof(sv_pk));
    tok_assign(&sv_cur, &cur);
    tok_assign(&sv_pk, &pk);
    txt = text;
    tp = 0;
    tlen = len;
    src_name = name;
    pk_have = 0;
    pe = 0;
    advance();
    while (ctk != TK_EOF && halted == 0) {
        pe = 0;
        if (ctk == TK_DOT || ctk == TK_BANG) {
            advance();
            continue;
        }
        top_item();
        if (pe == 1) {
            while (ctk != TK_EOF && ctk != TK_DOT && ctk != TK_BANG) advance();
        } else if (ctk == TK_DOT || ctk == TK_BANG) advance();
    }
    txt = sv_txt;
    tp = sv_tp;
    tlen = sv_len;
    src_name = sv_name;
    tok_assign(&cur, &sv_cur);
    tok_assign(&pk, &sv_pk);
    pk_have = sv_have;
    pe = sv_pe;
    free(sv_cur.s);
    free(sv_pk.s);
}
/* ===== s4_res.c ===== */
/* =========================================================================
   s4_res.c   resolver: raw syntax tree -> executable nodes; entry points
   ========================================================================= */
static char **env_name;
static long *env_slot, *env_lvl;
static long envn, envcap;
static long lvl;
static long *sc_next, *sc_max, *sc_blk;
static long sccap;
static int r_cls;
static int ib_first, ib_slot;
static int rv_kind;
static long rv_a, rv_b;

static void sc_need(long l)
{
    if (l >= sccap) {
        sc_next = (long *)xgrow(sc_next, &sccap, l, sizeof(long));
        sc_max = (long *)xrealloc(sc_max, sizeof(long) * (size_t)sccap);
        sc_blk = (long *)xrealloc(sc_blk, sizeof(long) * (size_t)sccap);
    }
}

static int declare_var(const char *nme)
{
    long s = sc_next[lvl];
    sc_next[lvl] = s + 1;
    if (s + 1 > sc_max[lvl]) sc_max[lvl] = s + 1;
    envn++;
    if (envn >= envcap) {
        long oc = envcap;
        env_name = (char **)xgrow(env_name, &envcap, envn, sizeof(char *));
        env_slot = (long *)xrealloc(env_slot, sizeof(long) * (size_t)envcap);
        env_lvl = (long *)xrealloc(env_lvl, sizeof(long) * (size_t)envcap);
        (void)oc;
    }
    env_name[envn] = (char *)nme;
    env_slot[envn] = s;
    env_lvl[envn] = lvl;
    return (int)s;
}

/* sets rv_kind (1 local, 2 ivar, 3 global cell), rv_a, rv_b */
static void lookup_var(const char *nme)
{
    long k;
    int w;
    for (k = envn; k >= 1; k--) {
        if (strcmp(env_name[k], nme) == 0) {
            rv_kind = 1;
            rv_a = lvl - env_lvl[k];
            rv_b = env_slot[k];
            return;
        }
    }
    w = wordpos_str(nme, cls[r_cls].ivn);
    if (w > 0) {
        rv_kind = 2;
        rv_a = w - 1;
        return;
    }
    k = find_cvar(r_cls, nme);
    if (k > 0) {
        rv_kind = 3;
        rv_a = k;
        return;
    }
    rv_kind = 3;
    rv_a = gcell(nme);
}

static long rs(long n);

static void rs_list(long first, long n)
{
    long i;
    for (i = first; i < first + n; i++) al[i] = rs(al[i]);
}

static void rs_seq(long n)
{
    rs_list(NA(n), NB(n));
}

/* an inlined block: declare its parameters and temporaries, resolve the body */
static long inline_block(long b)
{
    long first = -1, i, body, sv = envn;
    for (i = 0; i < NC(b); i++) {
        ib_slot = declare_var(nl_name[NB(b) + i]);
        if (first < 0) first = ib_slot;
    }
    for (i = 0; i < NI(b); i++) ib_slot = declare_var(nl_name[ND(b) + i]);
    body = NA(b);
    rs_seq(body);
    envn = sv;
    ib_first = (int)first;
    return body;
}

static int is_blk(long n, long np)
{
    return NT(n) == ND_RBLOCK && NC(n) == np;
}

static int is_blk01(long n)
{
    return NT(n) == ND_RBLOCK && NC(n) <= 1;
}

#define SEL_IS(x) (strcmp(s, x) == 0)

/* try to compile a control-flow message inline; returns 1 if done */
static int inline_send(long n, const char *s)
{
    long rcv, f, na, a1 = 0, a2 = 0, a3 = 0, c, st;
    rcv = NA(n);
    f = NB(n);
    na = NI(n);
    if (na >= 1) a1 = al[f];
    if (na >= 2) a2 = al[f + 1];
    if (na >= 3) a3 = al[f + 2];
    if (SEL_IS("ifTrue:") && is_blk(a1, 0)) {
        NA(n) = rs(rcv); NB(n) = inline_block(a1); NC(n) = 0; NT(n) = ND_IF;
        return 1;
    }
    if (SEL_IS("ifFalse:") && is_blk(a1, 0)) {
        NA(n) = rs(rcv); NC(n) = inline_block(a1); NB(n) = 0; NT(n) = ND_IF;
        return 1;
    }
    if (SEL_IS("ifTrue:ifFalse:") && is_blk(a1, 0) && is_blk(a2, 0)) {
        NA(n) = rs(rcv); NB(n) = inline_block(a1); NC(n) = inline_block(a2); NT(n) = ND_IF;
        return 1;
    }
    if (SEL_IS("ifFalse:ifTrue:") && is_blk(a1, 0) && is_blk(a2, 0)) {
        NA(n) = rs(rcv); NC(n) = inline_block(a1); NB(n) = inline_block(a2); NT(n) = ND_IF;
        return 1;
    }
    if ((SEL_IS("and:") || SEL_IS("or:")) && is_blk(a1, 0)) {
        int isand = SEL_IS("and:");
        NA(n) = rs(rcv); NB(n) = inline_block(a1);
        NT(n) = isand ? ND_AND : ND_OR;
        return 1;
    }
    if ((SEL_IS("whileTrue:") || SEL_IS("whileFalse:")) && is_blk(rcv, 0) && is_blk(a1, 0)) {
        int wt = SEL_IS("whileTrue:");
        NA(n) = inline_block(rcv); NB(n) = inline_block(a1);
        NI(n) = wt ? 1 : 0;
        NT(n) = ND_WHILE;
        return 1;
    }
    if ((SEL_IS("whileTrue") || SEL_IS("whileFalse")) && is_blk(rcv, 0)) {
        int wt = SEL_IS("whileTrue");
        NA(n) = inline_block(rcv); NB(n) = 0;
        NI(n) = wt ? 1 : 0;
        NT(n) = ND_WHILE;
        return 1;
    }
    if (SEL_IS("repeat") && is_blk(rcv, 0)) {
        NA(n) = inline_block(rcv); NT(n) = ND_REPEAT;
        return 1;
    }
    if (SEL_IS("timesRepeat:") && is_blk(a1, 0)) {
        NA(n) = rs(rcv); NB(n) = inline_block(a1); NT(n) = ND_TIMES;
        return 1;
    }
    if (SEL_IS("to:do:") && is_blk(a2, 1)) {
        NA(n) = rs(rcv); NB(n) = rs(a1); NC(n) = inline_block(a2);
        NI(n) = ib_first; ND(n) = 0; NT(n) = ND_TODO;
        return 1;
    }
    if (SEL_IS("to:by:do:") && is_blk(a3, 1)) {
        NA(n) = rs(rcv); NB(n) = rs(a1);
        st = rs(a2);
        NC(n) = inline_block(a3);
        NI(n) = ib_first; ND(n) = st; NT(n) = ND_TODO;
        return 1;
    }
    if (SEL_IS("ifNil:") && is_blk(a1, 0)) {
        NA(n) = rs(rcv); NB(n) = inline_block(a1); NC(n) = 0; ND(n) = 1; NI(n) = -1;
        NT(n) = ND_IFNIL;
        return 1;
    }
    if (SEL_IS("ifNotNil:") && is_blk01(a1)) {
        NA(n) = rs(rcv); NB(n) = 0; NC(n) = inline_block(a1);
        NI(n) = -1;
        if (NC(a1) == 1) NI(n) = ib_first;
        ND(n) = 0; NT(n) = ND_IFNIL;
        return 1;
    }
    if (SEL_IS("ifNil:ifNotNil:") && is_blk(a1, 0) && is_blk01(a2)) {
        NA(n) = rs(rcv); NB(n) = inline_block(a1);
        c = inline_block(a2);
        NC(n) = c;
        NI(n) = -1;
        if (NC(a2) == 1) NI(n) = ib_first;
        ND(n) = 0; NT(n) = ND_IFNIL;
        return 1;
    }
    if (SEL_IS("ifNotNil:ifNil:") && is_blk01(a1) && is_blk(a2, 0)) {
        NA(n) = rs(rcv);
        c = inline_block(a1);
        NC(n) = c;
        NI(n) = -1;
        if (NC(a1) == 1) NI(n) = ib_first;
        NB(n) = inline_block(a2);
        ND(n) = 0; NT(n) = ND_IFNIL;
        return 1;
    }
    return 0;
}

static long rs(long n)
{
    int t = NT(n);
    long i, k, sv, f;
    const char *nme;
    switch (t) {
    case ND_LIT: case ND_SELF: case ND_THISCTX:
        break;
    case ND_RVAR:
        nme = nsx(n);
        if (strcmp(nme, "self") == 0 || strcmp(nme, "super") == 0) NT(n) = ND_SELF;
        else {
            lookup_var(nme);
            if (rv_kind == 1) {
                NT(n) = ND_LOCAL; NA(n) = rv_a; NB(n) = rv_b;
            } else if (rv_kind == 2) {
                NT(n) = ND_IVAR; NA(n) = rv_a;
            } else {
                NT(n) = ND_GVAR; NA(n) = rv_a;
            }
        }
        break;
    case ND_RASG:
        NC(n) = rs(NA(n));
        lookup_var(nsx(n));
        if (rv_kind == 1) {
            NT(n) = ND_ASGL; NA(n) = rv_a; NB(n) = rv_b;
        } else if (rv_kind == 2) {
            NT(n) = ND_ASGI; NA(n) = rv_a;
        } else {
            NT(n) = ND_ASGG; NA(n) = rv_a;
        }
        break;
    case ND_RRET:
        NA(n) = rs(NA(n));
        NT(n) = ND_RET;
        NI(n) = lvl > 1 ? 1 : 0;
        break;
    case ND_SEQ:
        rs_seq(n);
        break;
    case ND_BRACE:
        rs_list(NA(n), NB(n));
        break;
    case ND_RBLOCK:
        sv = envn;
        sc_blk[lvl] = 1;
        lvl++;
        sc_need(lvl);
        sc_next[lvl] = 0;
        sc_max[lvl] = 0;
        sc_blk[lvl] = 0;
        for (i = 0; i < NC(n); i++) declare_var(nl_name[NB(n) + i]);
        for (i = 0; i < NI(n); i++) declare_var(nl_name[ND(n) + i]);
        rs_seq(NA(n));
        NT(n) = ND_BLOCK;
        NB(n) = NC(n);
        NC(n) = sc_max[lvl];
        ND(n) = sc_blk[lvl];
        lvl--;
        envn = sv;
        break;
    case ND_RCASC:
        NA(n) = rs(NA(n));
        f = NB(n);
        for (i = f; i < f + NC(n); i++) {
            k = al[i];
            rs_list(NB(k), NI(k));
            NC(k) = selid(nsx(k));
            NT(k) = ND_MSG;
        }
        NT(n) = ND_CASC;
        break;
    case ND_RSEND:
        nme = nsx(n);
        if (!inline_send(n, nme)) {
            NA(n) = rs(NA(n));
            rs_list(NB(n), NI(n));
            NC(n) = selid(nme);
            NT(n) = ND_SEND;
        }
        break;
    default:
        break;
    }
    return n;
}

/* ---- entry points ------------------------------------------------------------ */
static void start_scope(int c)
{
    r_cls = c;
    envn = 0;
    lvl = 1;
    sc_need(1);
    sc_next[1] = 0;
    sc_max[1] = 0;
    sc_blk[1] = 0;
}

static void compile_method(int c, const char *sel, long pfirst, int np, long tfirst, int ntemps, long body, int prim_, long startp)
{
    int i, m;
    start_scope(c);
    for (i = 0; i < np; i++) declare_var(nl_name[pfirst + i]);
    for (i = 0; i < ntemps; i++) declare_var(nl_name[tfirst + i]);
    rs_seq(body);
    m = install_method(c, selid(sel), np, body, (int)sc_max[1], (int)sc_blk[1], prim_);
    meths[m].src = startp;
}

static oop run_seq_in(int c, long st, long *keepfb)
{
    long seq, fb;
    oop v;
    start_scope(c);
    seq = nd(ND_SEQ, al_put(st), 1, 0, 0, 0, "");
    rs_seq(seq);
    fb = alloc_frame(4 + sc_max[1]);
    vp[fb + 0] = 0;
    vp[fb + 1] = fb;
    vp[fb + 2] = NIL_OOP;
    vp[fb + 3] = 0;
    init_temps(fb + 4, sc_max[1]);
    if (keepfb != NULL) *keepfb = fb;
    v = ev(seq, fb);
    return v;
}

/* run a top-level statement (a raw node) */
static void run_doit(long st)
{
    oop v, er;
    long eb;
    cdepth = 0;
    unw = UW_NONE;
    htop = 0;
    v = run_seq_in(cls_UndefinedObject, st, NULL);
    if (unw == 0 && in_eval == 0) run_scheduler();
    if (repl_echo == 1 && unw == 0 && v != NIL_OOP) {
        if (cof(v) != find_class("TextCollector")) {
            eb = asp;
            er = send_sel(v, S_PRINTSTRING, eb, 0);
            asp = eb;
            if (unw == 0 && isstr(er)) {
                outn(strof(er), strlen_of(er));
                outs("\n");
            } else {
                char *ps = printstr0(v);
                outs(ps);
                outs("\n");
                free(ps);
            }
            unw = UW_NONE;
        }
    }
    if (unw > 0) {
        if (unw == UW_EXIT) halted = 1;
        unw = UW_NONE;
    }
    htop = 0;
}

/* evaluate an expression node in the context of a class (class variable initialisers) */
static long eval_in_class(int c, long e)
{
    oop v = run_seq_in(c, e, NULL);
    if (unw > 0) unw = UW_NONE;
    return v;
}

/* evaluate source text (Behavior>>evaluate:); answers the value of the last statement */
static oop eval_string(const char *text)
{
    const char *sv_txt = txt;
    long sv_tp = tp, sv_len = tlen, sv_lvl = lvl, sv_envn = envn;
    int sv_pe = pe, sv_have = pk_have, sv_cls = r_cls;
    Tok sv_cur, sv_pk;
    long seq, fb;
    oop v = NIL_OOP;
    memset(&sv_cur, 0, sizeof(sv_cur));
    memset(&sv_pk, 0, sizeof(sv_pk));
    tok_assign(&sv_cur, &cur);
    tok_assign(&sv_pk, &pk);
    txt = text;
    tp = 0;
    tlen = (long)strlen(text);
    pk_have = 0;
    pe = 0;
    advance();
    pm_ntemps = 0;
    if (isbin("|")) pm_ntemps = parse_temps();
    seq = parse_stmts(TK_EOF);
    if (pe == 0) {
        start_scope(cls_UndefinedObject);
        rs_seq(seq);
        fb = alloc_frame(4 + sc_max[1]);
        vp[fb] = 0;
        vp[fb + 1] = fb;
        vp[fb + 2] = NIL_OOP;
        vp[fb + 3] = 0;
        init_temps(fb + 4, sc_max[1]);
        v = ev(seq, fb);
        if (unw == UW_RET) {
            unw = UW_NONE;
            v = unw_v;
        }
        if (unw > 0) v = NIL_OOP;
    }
    txt = sv_txt;
    tp = sv_tp;
    tlen = sv_len;
    tok_assign(&cur, &sv_cur);
    tok_assign(&pk, &sv_pk);
    pk_have = sv_have;
    pe = sv_pe;
    lvl = sv_lvl;
    envn = sv_envn;
    r_cls = sv_cls;
    free(sv_cur.s);
    free(sv_pk.s);
    return v;
}

/* give forked Smalltalk processes a chance to run at the end of a statement */
static void run_scheduler(void)
{
    long g;
    oop pr;
    long eb;
    if (!smap_get(&gmap, "Processor", &g)) return;
    pr = g_val[g];
    if (pr == NIL_OOP) return;
    eb = asp;
    send_sel(pr, S_RUNPENDING, eb, 0);
    asp = eb;
    if (unw > 0) {
        if (unw == UW_EXIT) halted = 1;
        unw = UW_NONE;
    }
}
/* ===== s5_eval.c ===== */
/* =========================================================================
   s5_eval.c   frames, sending messages, blocks, the evaluator
   ========================================================================= */
static void fl_need(long size)
{
    if (size >= flcap) {
        long oc = flcap, i;
        fl_head = (long *)xgrow(fl_head, &flcap, size, sizeof(long));
        for (i = oc; i < flcap; i++) fl_head[i] = 0;
    }
}

static long alloc_frame(long size)
{
    long fb;
    fl_need(size);
    fb = fl_head[size];
    if (fb > 0) {
        fl_head[size] = vp[fb];
        return fb;
    }
    fb = nvp + 1;
    nvp += size;
    GROW(oop, vp, vpcap, nvp + 1);
    return fb;
}

static void init_temps(long base, long n)
{
    long i;
    for (i = 0; i < n; i++) vp[base + i] = NIL_OOP;
}

/* ---- error reporting ---------------------------------------------------------- */
static char *trace_line(long m, oop r)
{
    int rc = cof(r);
    SB b;
    sb_init(&b);
    sb_adds(&b, cls[rc].name);
    if (rc != meths[m].cls) {
        sb_addc(&b, '(');
        sb_adds(&b, cls[meths[m].cls].name);
        sb_addc(&b, ')');
    }
    sb_adds(&b, ">>");
    sb_adds(&b, sel_name[meths[m].sel]);
    return sb_str(&b);
}

static void print_trace(void)
{
    long i;
    int sg0, mc;
    char *tl;
    sg0 = find_class("Signal");
    for (i = cdepth; i >= 1; i--) {
        mc = meths[cs_m[i]].cls;
        if (cls[mc].ismeta) mc = cls[mc].this_;
        if (sg0 == 0 || !inherits(mc, sg0)) {
            tl = trace_line(cs_m[i], cs_r[i]);
            outs(tl);
            free(tl);
            outs("\n");
        }
    }
    outs("UndefinedObject>>executeStatements\n");
}

/* report an error as a Smalltalk Error (signal) with the given text */
static void st_error(oop rcv, const char *msg)
{
    long base = asp;
    ast_push(mkstr(msg));
    send_sel(rcv, S_ERROR, base, 1);
    asp = base;
}

static char *printstr0(oop x)
{
    int k;
    SB b;
    sb_init(&b);
    if (IS_INT(x)) {
        sb_addl(&b, intval(x));
        return sb_str(&b);
    }
    k = OK(x);
    if (k == KD_STR) {
        sb_addc(&b, '\'');
        sb_addn(&b, OS(x), OSLEN(x));
        sb_addc(&b, '\'');
    } else if (k == KD_SYM) {
        sb_addc(&b, '#');
        sb_addn(&b, OS(x), OSLEN(x));
    } else if (x == NIL_OOP) sb_adds(&b, "nil");
    else if (x == TRUE_OOP) sb_adds(&b, "true");
    else if (x == FALSE_OOP) sb_adds(&b, "false");
    else if (k == KD_CLS || k == KD_META) sb_adds(&b, cls[OX(x)].name);
    else {
        sb_adds(&b, "a ");
        sb_adds(&b, cls[OC(x)].name);
    }
    return sb_str(&b);
}

static oop send_dnu(oop rcv, int sel, long base, int na)
{
    oop msg, arr, r, v0;
    long i, b2;
    int m;
    if (in_dnu > 50) {
        outs("doesNotUnderstand: recursion for #");
        outs(sel_name[sel]);
        outs("\n");
        unw = UW_ABORT;
        return 0;
    }
    arr = mkarray(na);
    for (i = 1; i <= na; i++) SLOT(arr, i - 1) = ast[base + i];
    msg = newobj(cls_Message, KD_PLAIN, 2);
    v0 = mksym(sel_name[sel]);
    SLOT(msg, 0) = v0;
    SLOT(msg, 1) = arr;
    m = lookup(cof(rcv), S_DNU);
    if (m == 0) {
        char *ps = printstr0(rcv);
        outs("Object: ");
        outs(ps);
        outs(" error: did not understand #");
        outs(sel_name[sel]);
        outs("\n");
        free(ps);
        unw = UW_ABORT;
        return 0;
    }
    b2 = asp;
    ast_push(msg);
    in_dnu++;
    r = invoke(m, rcv, b2, 1);
    in_dnu--;
    asp = b2;
    return r;
}

/* perform a send whose arguments are already at ast[base+1 .. base+na] */
static oop send_sel(oop rcv, int sel, long base, int na)
{
    int m = lookup(cof(rcv), sel);
    if (m == 0) return send_dnu(rcv, sel, base, na);
    return invoke(m, rcv, base, na);
}

/* frame for a method or block of the given size */
static long frame_get(long size)
{
    long nf;
    fl_need(size);
    nf = fl_head[size];
    if (nf > 0) {
        fl_head[size] = vp[nf];
    } else {
        nf = nvp + 1;
        nvp += size;
        GROW(oop, vp, vpcap, nvp + 1);
    }
    return nf;
}

/* run method m for receiver rcv with args at ast[base+1..base+na] */
static oop invoke(int m, oop rcv, long base, int na)
{
    int p;
    oop r;
    long nf, size, i, nt0;
    p = meths[m].prim;
    if (p > 0) {
        r = prim(p, rcv, base, na);
        if (unw > 0) return 0;
        if (r != 0) return r;
        if (NB(meths[m].body) == 0) {
            SB b;
            sb_init(&b);
            sb_adds(&b, "primitive failed: ");
            sb_adds(&b, sel_name[meths[m].sel]);
            st_error(rcv, sb_str(&b));
            sb_free(&b);
            return 0;
        }
    }
    nt0 = meths[m].ntemps;
    size = 4 + nt0;
    nf = frame_get(size);
    vp[nf] = 0;
    vp[nf + 1] = nf;
    vp[nf + 2] = rcv;
    vp[nf + 3] = m;
    for (i = 1; i <= meths[m].nargs; i++) vp[nf + 3 + i] = ast[base + i];
    for (i = meths[m].nargs + 1; i <= nt0; i++) vp[nf + 3 + i] = NIL_OOP;
    cdepth++;
    if (cdepth > CDEPTH_MAX) {
        outs("call stack too deep\n");
        cdepth--;
        unw = UW_ABORT;
        return 0;
    }
    cs_m[cdepth] = m;
    cs_r[cdepth] = rcv;
    r = ev(meths[m].body, nf);
    cdepth--;
    if (unw == UW_RET && unw_t == nf) {
        unw = UW_NONE;
        r = unw_v;
    } else if (unw == UW_NONE) r = rcv;
    else r = 0;
    if (meths[m].hasblk == 0) {
        vp[nf] = fl_head[size];
        fl_head[size] = nf;
    } else vp[nf + 1] = 0;
    return r;
}

/* ---- blocks ----------------------------------------------------------------------- */
static oop call_block(oop blk, long base, int na)
{
    long node, outer, size, bf, i;
    oop r;
    node = OX(blk);
    if (NB(node) != na) {
        st_error(blk, "wrong argument count");
        return 0;
    }
    outer = OSLOT(blk);
    size = 4 + NC(node);
    bf = frame_get(size);
    vp[bf] = outer;
    vp[bf + 1] = vp[outer + 1];
    vp[bf + 2] = vp[outer + 2];
    vp[bf + 3] = vp[outer + 3];
    for (i = 1; i <= na; i++) vp[bf + 3 + i] = ast[base + i];
    for (i = na + 1; i <= NC(node); i++) vp[bf + 3 + i] = NIL_OOP;
    r = ev(NA(node), bf);
    if (ND(node) == 0) {
        vp[bf] = fl_head[size];
        fl_head[size] = bf;
    }
    if (unw > 0) return 0;
    return r;
}


/* ---- the evaluator ---------------------------------------------------------------------- */
static oop ev_slow(long n, long fb);
static oop do_send(long n, long fb);

#define EVAL(dst, nn, fb_) do { \
    long n__ = (nn); \
    switch (nodes[n__].t) { \
    case ND_LIT: dst = nodes[n__].a; break; \
    case ND_SELF: dst = vp[(fb_) + 2]; break; \
    case ND_LOCAL: \
        if (nodes[n__].a == 0) dst = vp[(fb_) + 4 + nodes[n__].b]; \
        else dst = ev_slow(n__, (fb_)); \
        break; \
    case ND_SEND: dst = do_send(n__, (fb_)); break; \
    default: dst = ev(n__, (fb_)); break; \
    } } while (0)

static oop ev(long n, long fb)
{
    switch (nodes[n].t) {
    case ND_SEND: return do_send(n, fb);
    case ND_LIT: return nodes[n].a;
    case ND_SELF: return vp[fb + 2];
    case ND_LOCAL:
        if (nodes[n].a == 0) return vp[fb + 4 + nodes[n].b];
        break;
    case ND_IVAR: return sl[objs[HI(vp[fb + 2])].slot + nodes[n].a];
    case ND_GVAR: return g_val[nodes[n].a];
    default: break;
    }
    return ev_slow(n, fb);
}

static oop not_boolean(oop v, const char *sel)
{
    SB b;
    sb_init(&b);
    sb_adds(&b, "doesNotUnderstand: #");
    sb_adds(&b, sel);
    sb_adds(&b, " (receiver is not a Boolean)");
    st_error(v, sb_str(&b));
    sb_free(&b);
    return 0;
}

static oop not_number(oop v, const char *sel)
{
    SB b;
    sb_init(&b);
    sb_adds(&b, "doesNotUnderstand: #");
    sb_adds(&b, sel);
    st_error(v, sb_str(&b));
    sb_free(&b);
    return 0;
}

static int is_neg(oop x)
{
    if (IS_INT(x)) return intval(x) < 0;
    if (OK(x) == KD_FLT && OF(x) < 0.0) return 1;
    return 0;
}

static oop do_todo(long n, long fb)
{
    oop a, b, st, v, r;
    long x, y, s, slot, base;
    EVAL(a, NA(n), fb);
    if (unw > 0) return 0;
    EVAL(b, NB(n), fb);
    if (unw > 0) return 0;
    if (ND(n) > 0) {
        EVAL(st, ND(n), fb);
        if (unw > 0) return 0;
    } else st = mkint(1);
    slot = fb + 4 + NI(n);
    if (IS_INT(a) && IS_INT(b) && IS_INT(st)) {
        x = intval(a);
        y = intval(b);
        s = intval(st);
        if (s == 0) {
            st_error(a, "step must be non-zero");
            return 0;
        }
        if (s > 0) {
            while (x <= y) {
                vp[slot] = mkint(x);
                v = ev(NC(n), fb);
                if (unw > 0) return 0;
                x += s;
            }
        } else {
            while (x >= y) {
                vp[slot] = mkint(x);
                v = ev(NC(n), fb);
                if (unw > 0) return 0;
                x += s;
            }
        }
        return a;
    }
    /* general numbers: use sends */
    base = asp;
    v = a;
    for (;;) {
        asp = base;
        ast_push(b);
        if (!is_neg(st)) r = send_sel(v, S_LE, base, 1);
        else r = send_sel(v, S_GE, base, 1);
        asp = base;
        if (unw > 0) return 0;
        if (r != TRUE_OOP) break;
        vp[slot] = v;
        r = ev(NC(n), fb);
        if (unw > 0) return 0;
        asp = base;
        ast_push(st);
        v = send_sel(v, S_ADD, base, 1);
        asp = base;
        if (unw > 0) return 0;
    }
    return a;
}

static oop do_cascade(long n, long fb)
{
    oop rcv, v, r;
    long i, k, j, base;
    EVAL(rcv, NA(n), fb);
    if (unw > 0) return 0;
    r = NIL_OOP;
    for (i = NB(n); i < NB(n) + NC(n); i++) {
        k = al[i];
        base = asp;
        for (j = 0; j < NI(k); j++) {
            EVAL(v, al[NB(k) + j], fb);
            if (unw > 0) {
                asp = base;
                return 0;
            }
            ast_push(v);
        }
        r = send_sel(rcv, (int)NC(k), base, (int)NI(k));
        asp = base;
        if (unw > 0) return 0;
    }
    return r;
}

static oop ev_slow(long n, long fb)
{
    int t = nodes[n].t;
    long d, f, i, c, lim;
    oop v;
    switch (t) {
    case ND_LOCAL:
        d = NA(n);
        f = fb;
        while (d > 0) {
            f = vp[f];
            d--;
        }
        return vp[f + 4 + NB(n)];
    case ND_SEND: return do_send(n, fb);
    case ND_LIT: return NA(n);
    case ND_SELF: return vp[fb + 2];
    case ND_IVAR: return sl[objs[HI(vp[fb + 2])].slot + NA(n)];
    case ND_GVAR: return g_val[NA(n)];
    case ND_ASGL:
        v = ev(NC(n), fb);
        if (unw > 0) return 0;
        d = NA(n);
        f = fb;
        while (d > 0) {
            f = vp[f];
            d--;
        }
        vp[f + 4 + NB(n)] = v;
        return v;
    case ND_ASGI:
        v = ev(NC(n), fb);
        if (unw > 0) return 0;
        sl[objs[HI(vp[fb + 2])].slot + NA(n)] = v;
        return v;
    case ND_ASGG:
        v = ev(NC(n), fb);
        if (unw > 0) return 0;
        g_val[NA(n)] = v;
        return v;
    case ND_SEQ:
        v = NIL_OOP;
        f = NA(n);
        for (i = f; i < f + NB(n); i++) {
            EVAL(v, al[i], fb);
            if (unw > 0) return 0;
        }
        return v;
    case ND_RET:
        v = ev(NA(n), fb);
        if (unw > 0) return 0;
        f = vp[fb + 1];
        if (NI(n) == 1 && (f == 0 || vp[f + 1] != f)) {
            outs("BlockContext>>cannotReturn: non-local return from a dead method context\n");
            unw = UW_ABORT;
            return 0;
        }
        unw = UW_RET;
        unw_t = f;
        unw_v = v;
        return 0;
    case ND_IF:
        EVAL(v, NA(n), fb);
        if (unw > 0) return 0;
        if (v == TRUE_OOP) {
            if (NB(n) > 0) return ev(NB(n), fb);
            return NIL_OOP;
        }
        if (v == FALSE_OOP) {
            if (NC(n) > 0) return ev(NC(n), fb);
            return NIL_OOP;
        }
        return not_boolean(v, "ifTrue:");
    case ND_WHILE:
        for (;;) {
            v = ev(NA(n), fb);
            if (unw > 0) return 0;
            if (v == TRUE_OOP) c = 1;
            else if (v == FALSE_OOP) c = 0;
            else return not_boolean(v, "whileTrue:");
            if (c != NI(n)) break;
            if (NB(n) > 0) {
                v = ev(NB(n), fb);
                if (unw > 0) return 0;
            }
        }
        return NIL_OOP;
    case ND_AND: case ND_OR:
        v = ev(NA(n), fb);
        if (unw > 0) return 0;
        if (v == TRUE_OOP) {
            if (t == ND_AND) return ev(NB(n), fb);
            return TRUE_OOP;
        }
        if (v == FALSE_OOP) {
            if (t == ND_OR) return ev(NB(n), fb);
            return FALSE_OOP;
        }
        return not_boolean(v, "and:");
    case ND_TODO: return do_todo(n, fb);
    case ND_TIMES:
        v = ev(NA(n), fb);
        if (unw > 0) return 0;
        if (!IS_INT(v)) return not_number(v, "timesRepeat:");
        lim = intval(v);
        for (i = 1; i <= lim; i++) {
            ev(NB(n), fb);
            if (unw > 0) return 0;
        }
        return v;
    case ND_IFNIL:
        v = ev(NA(n), fb);
        if (unw > 0) return 0;
        if (v == NIL_OOP) {
            if (NB(n) > 0) return ev(NB(n), fb);
            return NIL_OOP;
        }
        if (NC(n) > 0) {
            if (NI(n) >= 0) vp[fb + 4 + NI(n)] = v;
            return ev(NC(n), fb);
        }
        return v;
    case ND_REPEAT:
        for (;;) {
            ev(NA(n), fb);
            if (unw > 0) return 0;
        }
    case ND_CASC: return do_cascade(n, fb);
    case ND_BLOCK:
        v = newobj(cls_BlockClosure, KD_BLK, 0);
        objs[HI(v)].x = n;
        objs[HI(v)].slot = fb;
        return v;
    case ND_BRACE:
        c = NB(n);
        v = mkarray(c);
        for (i = 0; i < c; i++) {
            oop e;
            e = ev(al[NA(n) + i], fb);
            if (unw > 0) return 0;
            sl[objs[HI(v)].slot + i] = e;
        }
        return v;
    case ND_THISCTX: return make_context();
    default: break;
    }
    return NIL_OOP;
}

/* floor division and modulo for tagged-free C longs (C89 safe) */
static void floor_divmod(long x, long y, long *q, long *r)
{
    long ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    long qa = ax / ay, ra = ax % ay;
    long qq = ((x < 0) != (y < 0)) ? -qa : qa;
    long rr = x < 0 ? -ra : ra;
    if (rr != 0 && ((rr < 0) != (y < 0))) {
        qq -= 1;
        rr += y;
    }
    *q = qq;
    *r = rr;
}

static oop do_send(long n, long fb)
{
    long rn, na, i, base, f, x, y, r;
    int sel, c, m;
    oop rcv, v;
    rn = nodes[n].a;
    sel = (int)nodes[n].c;
    na = nodes[n].i;
    if (nodes[n].d == 1) {
        rcv = vp[fb + 2];
        c = cls[meths[vp[fb + 3]].cls].super_;
        if (c == 0) c = cls_Object;
    } else {
        EVAL(rcv, rn, fb);
        if (unw > 0) return 0;
        c = 0;
    }
    base = asp;
    f = nodes[n].b;
    for (i = 0; i < na; i++) {
        EVAL(v, al[f + i], fb);
        if (unw > 0) {
            asp = base;
            return 0;
        }
        asp++;
        if (asp >= astcap) ast = (oop *)xgrow(ast, &astcap, asp, sizeof(oop));
        ast[asp] = v;
    }
    /* fast paths for SmallInteger arithmetic and comparison */
    if (na == 1 && sel <= 14) {
        v = ast[asp];
        if (IS_INT(rcv) && IS_INT(v)) {
            x = INTVAL(rcv);
            y = INTVAL(v);
            switch (sel) {
            case S_ADD:
                r = x + y;
                if (r >= SI_MIN && r <= SI_MAX) { asp = base; return MKINT(r); }
                break;
            case S_SUB:
                r = x - y;
                if (r >= SI_MIN && r <= SI_MAX) { asp = base; return MKINT(r); }
                break;
            case S_MUL:
                if (x > -SI_MULLIM && x < SI_MULLIM && y > -SI_MULLIM && y < SI_MULLIM) {
                    asp = base;
                    return MKINT(x * y);
                }
                break;
            case S_LT: asp = base; return x < y ? TRUE_OOP : FALSE_OOP;
            case S_GT: asp = base; return x > y ? TRUE_OOP : FALSE_OOP;
            case S_LE: asp = base; return x <= y ? TRUE_OOP : FALSE_OOP;
            case S_GE: asp = base; return x >= y ? TRUE_OOP : FALSE_OOP;
            case S_EQ: asp = base; return x == y ? TRUE_OOP : FALSE_OOP;
            case S_NE: asp = base; return x != y ? TRUE_OOP : FALSE_OOP;
            case S_IDIV:
                if (y != 0) {
                    long q, rm;
                    floor_divmod(x, y, &q, &rm);
                    asp = base;
                    return MKINT(q);
                }
                break;
            case S_MOD:
                if (y != 0) {
                    long q, rm;
                    floor_divmod(x, y, &q, &rm);
                    asp = base;
                    return MKINT(rm);
                }
                break;
            default:
                break;
            }
        }
        if (sel == S_IDENT) {
            asp = base;
            return rcv == v ? TRUE_OOP : FALSE_OOP;
        }
        if (sel == S_NIDENT) {
            asp = base;
            return rcv == v ? FALSE_OOP : TRUE_OOP;
        }
    }
    if (c == 0) c = cof(rcv);
    m = lookup(c, sel);
    if (m == 0) v = send_dnu(rcv, sel, base, (int)na);
    else v = invoke(m, rcv, base, (int)na);
    asp = base;
    return v;
}

/* thisContext: a chain of Context objects built from the call stack */
static oop make_context(void)
{
    int cc;
    long i, s0;
    oop o, prev, sy;
    cc = find_class("Context");
    if (cc == 0) return NIL_OOP;
    prev = NIL_OOP;
    for (i = 1; i <= cdepth; i++) {
        o = new_instance(cc, 0);
        s0 = OSLOT(o);
        sl[s0] = cs_r[i];
        sy = mksym(sel_name[meths[cs_m[i]].sel]);
        s0 = OSLOT(o);
        sl[s0 + 1] = sy;
        sl[s0 + 2] = cls[meths[cs_m[i]].cls].oop_;
        sl[s0 + 3] = prev;
        prev = o;
    }
    if (prev == NIL_OOP) {
        o = new_instance(cc, 0);
        s0 = OSLOT(o);
        sl[s0] = NIL_OOP;
        sy = mksym("executeStatements");
        s0 = OSLOT(o);
        sl[s0 + 1] = sy;
        sl[s0 + 2] = NIL_OOP;
        sl[s0 + 3] = NIL_OOP;
        prev = o;
    }
    return prev;
}
/* ===== s7_num.c ===== */
/* =========================================================================
   s7_num.c   numbers: SmallInteger / LargeInteger (decimal text, computed
              on base-10000 limb arrays) / Float, and the primitives
              PR_ADD .. PR_INTSQRT (prim_a).
   Included after s0_base.c .. s6; strict C89.
   ========================================================================= */

/* ---- private prototypes ------------------------------------------------ */
static double num_inf(void);
static double num_nan(void);
static oop float_int(double f);
static char *float_to_istr(double f);
static oop bitop(int p, long x, long y);
static char *radix_str(long n, int rdx);

#define BIG_B 10000L
#define BIG_W 4

/* ---- limb arrays (little endian, base 10000, no leading zero limbs,
        zero is one limb holding 0) ---------------------------------------- */

static void big_trim(long *d, long *n)
{
    while (*n > 1 && d[*n - 1] == 0) (*n)--;
}

/* parse magnitude digits (no sign) */
static long *big_parse(const char *s, long len, long *nout)
{
    long n, i, k, v, w, pos;
    long *d;

    while (len > 1 && *s == '0') {
        s++;
        len--;
    }
    if (len < 1) {
        d = (long *) xmalloc(sizeof(long));
        d[0] = 0;
        *nout = 1;
        return d;
    }
    n = (len + BIG_W - 1) / BIG_W;
    d = (long *) xmalloc((size_t) n * sizeof(long));
    pos = len;
    for (i = 0; i < n; i++) {
        v = 0;
        w = pos - BIG_W;
        if (w < 0) w = 0;
        for (k = w; k < pos; k++) v = v * 10 + (s[k] - '0');
        d[i] = v;
        pos = w;
    }
    *nout = n;
    big_trim(d, nout);
    return d;
}

/* decimal text of magnitude, optional '-' */
static char *big_text(const long *d, long n, int neg)
{
    char *r, *p;
    long i, top, len;
    char t[8];

    while (n > 1 && d[n - 1] == 0) n--;
    if (n == 1 && d[0] == 0) neg = 0;
    sprintf(t, "%ld", d[n - 1]);
    top = (long) strlen(t);
    len = top + (n - 1) * BIG_W + (neg ? 1 : 0);
    r = (char *) xmalloc((size_t) len + 1);
    p = r;
    if (neg) *p++ = '-';
    memcpy(p, t, (size_t) top);
    p += top;
    for (i = n - 2; i >= 0; i--) {
        long v = d[i];
        p[3] = (char) ('0' + v % 10);
        v /= 10;
        p[2] = (char) ('0' + v % 10);
        v /= 10;
        p[1] = (char) ('0' + v % 10);
        v /= 10;
        p[0] = (char) ('0' + v % 10);
        p += 4;
    }
    *p = '\0';
    return r;
}

static int big_mcmp(const long *a, long na, const long *b, long nb)
{
    long i;

    if (na != nb) return na < nb ? -1 : 1;
    for (i = na - 1; i >= 0; i--) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

static long *big_madd(const long *a, long na, const long *b, long nb, long *nr)
{
    long n, i, c, s;
    long *r;

    n = (na > nb ? na : nb) + 1;
    r = (long *) xmalloc((size_t) n * sizeof(long));
    c = 0;
    for (i = 0; i < n; i++) {
        s = c;
        if (i < na) s += a[i];
        if (i < nb) s += b[i];
        if (s >= BIG_B) {
            s -= BIG_B;
            c = 1;
        } else {
            c = 0;
        }
        r[i] = s;
    }
    *nr = n;
    big_trim(r, nr);
    return r;
}

/* a >= b */
static long *big_msub(const long *a, long na, const long *b, long nb, long *nr)
{
    long i, c, s;
    long *r;

    r = (long *) xmalloc((size_t) na * sizeof(long));
    c = 0;
    for (i = 0; i < na; i++) {
        s = a[i] - c;
        if (i < nb) s -= b[i];
        if (s < 0) {
            s += BIG_B;
            c = 1;
        } else {
            c = 0;
        }
        r[i] = s;
    }
    *nr = na;
    big_trim(r, nr);
    return r;
}

static long *big_mmul(const long *a, long na, const long *b, long nb, long *nr)
{
    long n, i, j, c, t, x;
    long *r;

    n = na + nb;
    r = (long *) xmalloc((size_t) n * sizeof(long));
    for (i = 0; i < n; i++) r[i] = 0;
    for (i = 0; i < na; i++) {
        c = 0;
        x = a[i];
        if (x == 0) continue;
        for (j = 0; j < nb; j++) {
            t = x * b[j] + r[i + j] + c;
            c = t / BIG_B;
            r[i + j] = t % BIG_B;
        }
        j = i + nb;
        while (c > 0) {
            t = r[j] + c;
            c = t / BIG_B;
            r[j] = t % BIG_B;
            j++;
        }
    }
    *nr = n;
    big_trim(r, nr);
    return r;
}

/* truncated division of magnitudes, a >= 0, b > 0 */
static void big_mdivmod(const long *a, long na, const long *b, long nb,
                        long **q, long *nq, long **r, long *nr)
{
    long i, j, m, d, rem, cur;
    long *qq, *rr, *u, *v;

    if (big_mcmp(a, na, b, nb) < 0) {
        qq = (long *) xmalloc(sizeof(long));
        qq[0] = 0;
        *q = qq;
        *nq = 1;
        rr = (long *) xmalloc((size_t) na * sizeof(long));
        memcpy(rr, a, (size_t) na * sizeof(long));
        *r = rr;
        *nr = na;
        return;
    }
    if (nb == 1) {
        d = b[0];
        qq = (long *) xmalloc((size_t) na * sizeof(long));
        rem = 0;
        for (i = na - 1; i >= 0; i--) {
            cur = rem * BIG_B + a[i];
            qq[i] = cur / d;
            rem = cur % d;
        }
        *q = qq;
        *nq = na;
        big_trim(qq, nq);
        rr = (long *) xmalloc(sizeof(long));
        rr[0] = rem;
        *r = rr;
        *nr = 1;
        return;
    }
    /* Knuth algorithm D */
    d = BIG_B / (b[nb - 1] + 1);
    u = (long *) xmalloc((size_t) (na + 1) * sizeof(long));
    v = (long *) xmalloc((size_t) nb * sizeof(long));
    {
        long c = 0, t;

        for (i = 0; i < na; i++) {
            t = a[i] * d + c;
            u[i] = t % BIG_B;
            c = t / BIG_B;
        }
        u[na] = c;
        c = 0;
        for (i = 0; i < nb; i++) {
            t = b[i] * d + c;
            v[i] = t % BIG_B;
            c = t / BIG_B;
        }
    }
    m = na - nb;
    qq = (long *) xmalloc((size_t) (m + 1) * sizeof(long));
    for (j = m; j >= 0; j--) {
        long num, qhat, rhat, borrow, carry, p, sub;

        num = u[j + nb] * BIG_B + u[j + nb - 1];
        qhat = num / v[nb - 1];
        rhat = num % v[nb - 1];
        while (qhat >= BIG_B || qhat * v[nb - 2] > rhat * BIG_B + u[j + nb - 2]) {
            qhat--;
            rhat += v[nb - 1];
            if (rhat >= BIG_B) break;
        }
        borrow = 0;
        carry = 0;
        for (i = 0; i < nb; i++) {
            p = qhat * v[i] + carry;
            carry = p / BIG_B;
            sub = u[i + j] - (p % BIG_B) - borrow;
            if (sub < 0) {
                sub += BIG_B;
                borrow = 1;
            } else {
                borrow = 0;
            }
            u[i + j] = sub;
        }
        sub = u[j + nb] - carry - borrow;
        if (sub < 0) {
            sub += BIG_B;
            borrow = 1;
        } else {
            borrow = 0;
        }
        u[j + nb] = sub;
        if (borrow) {
            qhat--;
            carry = 0;
            for (i = 0; i < nb; i++) {
                p = u[i + j] + v[i] + carry;
                if (p >= BIG_B) {
                    p -= BIG_B;
                    carry = 1;
                } else {
                    carry = 0;
                }
                u[i + j] = p;
            }
            u[j + nb] = (u[j + nb] + carry) % BIG_B;
        }
        qq[j] = qhat;
    }
    *q = qq;
    *nq = m + 1;
    big_trim(qq, nq);
    /* remainder = u[0..nb-1] / d */
    rr = (long *) xmalloc((size_t) nb * sizeof(long));
    rem = 0;
    for (i = nb - 1; i >= 0; i--) {
        cur = rem * BIG_B + u[i];
        rr[i] = cur / d;
        rem = cur % d;
    }
    *r = rr;
    *nr = nb;
    big_trim(rr, nr);
    free(u);
    free(v);
}

/* split signed decimal text into sign and limbs */
static long *big_split(const char *s, int *neg, long *n)
{
    long len;

    *neg = 0;
    if (*s == '-') {
        *neg = 1;
        s++;
    }
    len = (long) strlen(s);
    return big_parse(s, len, n);
}

/* signed arithmetic on decimal texts. op: 1 add 2 sub 3 mul
   5 floor-div 6 floor-mod 7 trunc-quo 8 trunc-rem 4 exact div.
   Answers a malloc'ed text, or NULL when the operation fails
   (division by zero, inexact exact division). */
static char *big_arith(int op, const char *sx, const char *sy)
{
    long *a, *b, *r, *q, *rm;
    long na, nb, nr, nq, nrm;
    int ng_a, ng_b, c;
    char *res;

    a = big_split(sx, &ng_a, &na);
    b = big_split(sy, &ng_b, &nb);
    if (na == 1 && a[0] == 0) ng_a = 0;
    if (nb == 1 && b[0] == 0) ng_b = 0;
    res = NULL;
    r = NULL;
    if (op == 1 || op == 2) {
        if (op == 2) ng_b = !ng_b;
        if (nb == 1 && b[0] == 0) ng_b = 0;
        if (ng_a == ng_b) {
            r = big_madd(a, na, b, nb, &nr);
            res = big_text(r, nr, ng_a);
        } else {
            c = big_mcmp(a, na, b, nb);
            if (c == 0) {
                res = dups("0");
            } else if (c > 0) {
                r = big_msub(a, na, b, nb, &nr);
                res = big_text(r, nr, ng_a);
            } else {
                r = big_msub(b, nb, a, na, &nr);
                res = big_text(r, nr, ng_b);
            }
        }
        if (r != NULL) free(r);
    } else if (op == 3) {
        r = big_mmul(a, na, b, nb, &nr);
        res = big_text(r, nr, ng_a != ng_b);
        free(r);
    } else {
        if (nb == 1 && b[0] == 0) {
            free(a);
            free(b);
            return NULL;
        }
        big_mdivmod(a, na, b, nb, &q, &nq, &rm, &nrm);
        {
            int rzero = (nrm == 1 && rm[0] == 0);
            int sdiff = (ng_a != ng_b);

            if (op == 4) {
                if (rzero) res = big_text(q, nq, sdiff);
            } else if (op == 7) {
                res = big_text(q, nq, sdiff);
            } else if (op == 8) {
                res = big_text(rm, nrm, ng_a);
            } else if (op == 5) {
                if (!rzero && sdiff) {
                    long one = 1, nn;
                    long *q2 = big_madd(q, nq, &one, 1, &nn);

                    res = big_text(q2, nn, 1);
                    free(q2);
                } else {
                    res = big_text(q, nq, sdiff);
                }
            } else {
                /* op == 6: floor modulo, sign of the divisor */
                if (!rzero && sdiff) {
                    long nn;
                    long *m2 = big_msub(b, nb, rm, nrm, &nn);

                    res = big_text(m2, nn, ng_b);
                    free(m2);
                } else {
                    res = big_text(rm, nrm, ng_b && !rzero);
                }
            }
        }
        free(q);
        free(rm);
    }
    free(a);
    free(b);
    return res;
}

/* signed comparison of decimal texts (normalised: no leading zeros) */
static int big_scmp(const char *a, const char *b)
{
    int na = (*a == '-'), nb = (*b == '-');
    long la, lb;
    int c;

    if (na != nb) return na ? -1 : 1;
    if (na) {
        a++;
        b++;
    }
    la = (long) strlen(a);
    lb = (long) strlen(b);
    if (la != lb) c = la < lb ? -1 : 1;
    else {
        c = strcmp(a, b);
        c = c < 0 ? -1 : (c > 0 ? 1 : 0);
    }
    return na ? -c : c;
}

/* ---- object construction ------------------------------------------------ */

/* oop for an integer given as signed decimal text of any shape */
static oop num_norm(const char *s0)
{
    const char *s = s0;
    int neg = 0;
    long len;
    oop o;

    if (*s == '-') {
        neg = 1;
        s++;
    }
    while (*s == '0' && s[1] != '\0') s++;
    len = (long) strlen(s);
    if (len == 1 && *s == '0') return mkint(0L);
    if (len <= SI_DIGITS) {
        long v = 0, i;

        for (i = 0; i < len; i++) v = v * 10 + (s[i] - '0');
        return mkint(neg ? -v : v);
    }
#if SI_DIGITS == 9
    if (len == 10) {
        const char *lim = neg ? "1073741824" : "1073741823";

        if (strcmp(s, lim) <= 0) {
            long v = 0, i;

            for (i = 0; i < len; i++) v = v * 10 + (s[i] - '0');
            return mkint(neg ? -v : v);
        }
    }
#endif
    o = newobj(neg ? cls_LargeNegativeInteger : cls_LargePositiveInteger,
               KD_BIG, 0);
    if (neg) {
        /* keep the sign in front of the cleaned digits */
        char *t = (char *) xmalloc((size_t) len + 2);

        t[0] = '-';
        memcpy(t + 1, s, (size_t) len + 1);
        set_str(o, t, len + 1);
        free(t);
    } else {
        set_str(o, s, len);
    }
    return o;
}

static oop mkbig(const char *decimal)
{
    return num_norm(decimal);
}

static oop big_norm(const char *decimal)
{
    return num_norm(decimal);
}

/* same, taking ownership of a malloc'ed text; 0 stays 0 (failure) */
static oop big_result(char *t)
{
    oop o;

    if (t == NULL) return 0;
    o = num_norm(t);
    free(t);
    return o;
}

static oop num_from_long(long v)
{
    char buf[32];

    if (v >= SI_MIN && v <= SI_MAX) return mkint(v);
    sprintf(buf, "%ld", v);
    return num_norm(buf);
}

/* ---- helpers ------------------------------------------------------------ */

static double num_inf(void)
{
    volatile double z = 1.0e308;

    z = z * 10.0;
    return z;
}

static double num_nan(void)
{
    volatile double z = num_inf();

    return z - z;
}

static int numkind(oop x)
{
    int k;

    if (IS_INT(x)) return 1;
    k = OK(x);
    if (k == KD_BIG) return 2;
    if (k == KD_FLT) return 3;
    return 0;
}

static double tofloat(oop x)
{
    if (IS_INT(x)) return (double) INTVAL(x);
    if (OK(x) == KD_BIG) return strtod(OS(x), NULL);
    return OF(x);
}

static char *num_str(oop x)
{
    char buf[32];

    if (IS_INT(x)) {
        sprintf(buf, "%ld", INTVAL(x));
        return dups(buf);
    }
    return dups(OS(x));
}

/* a float of large magnitude -> exact integer text */
static char *float_to_istr(double f)
{
    char *buf;
    int need;

    buf = (char *) xmalloc(400);
    need = sprintf(buf, "%.0f", f);
    if (need < 1) buf[0] = '\0';
    return buf;
}

static oop float_int(double f)
{
    char *t;
    oop o;

    if (f != f) return 0;
    if (f > 1.0e308 || f < -1.0e308) return 0;
    if (f > -1.0e9 && f < 1.0e9) return mkint((long) f);
    t = float_to_istr(f);
    o = num_norm(t);
    free(t);
    return o;
}

/* 15 significant digits, fixed notation for moderate exponents */
static char *float_str(double f)
{
    char buf[64], out[400];
    char dig[32];
    int nd, e10, i, k;
    long p;

    if (f != f) return dups("nan");
    if (f > 1.0e308) return dups("inf");
    if (f < -1.0e308) return dups("-inf");
    p = 0;
    if (f < 0.0) {
        out[p++] = '-';
        f = -f;
    }
    if (f == 0.0) {
        strcpy(out + p, "0.0");
        return dups(out);
    }
    sprintf(buf, "%.14e", f);
    /* d.dddddddddddddde[+-]XX */
    nd = 0;
    for (i = 0; buf[i] != 'e' && buf[i] != '\0'; i++) {
        if (is_digit((unsigned char) buf[i])) dig[nd++] = buf[i];
    }
    e10 = atoi(buf + i + 1);
    while (nd > 1 && dig[nd - 1] == '0') nd--;
    dig[nd] = '\0';
    if (e10 >= -7 && e10 < 21) {
        if (e10 >= 0) {
            for (k = 0; k <= e10; k++) out[p++] = (k < nd) ? dig[k] : '0';
            out[p++] = '.';
            if (nd > e10 + 1) {
                for (k = e10 + 1; k < nd; k++) out[p++] = dig[k];
            } else {
                out[p++] = '0';
            }
        } else {
            out[p++] = '0';
            out[p++] = '.';
            for (k = 0; k < -e10 - 1; k++) out[p++] = '0';
            for (k = 0; k < nd; k++) out[p++] = dig[k];
        }
        out[p] = '\0';
    } else {
        out[p++] = dig[0];
        out[p++] = '.';
        if (nd > 1) {
            for (k = 1; k < nd; k++) out[p++] = dig[k];
        } else {
            out[p++] = '0';
        }
        out[p] = '\0';
        if (e10 < 0) sprintf(out + p, "e-%02d", -e10);
        else sprintf(out + p, "e%02d", e10);
    }
    return dups(out);
}

/* ---- arithmetic --------------------------------------------------------- */

static oop p_arith(int op, oop x, oop y)
{
    int kx, ky;
    double fx, fy;
    long a, b, r;
    char *sx, *sy, *t;
    oop res;

    kx = numkind(x);
    ky = numkind(y);
    if (kx == 0 || ky == 0) return 0;
    if (kx == 3 || ky == 3) {
        fx = tofloat(x);
        fy = tofloat(y);
        switch (op) {
        case 1: return mkfloat(fx + fy);
        case 2: return mkfloat(fx - fy);
        case 3: return mkfloat(fx * fy);
        case 4:
            if (fy == 0.0) return 0;
            return mkfloat(fx / fy);
        default:
            if (fy == 0.0) return 0;
            if (op == 5) return float_int(floor(fx / fy));
            if (op == 6) return mkfloat(fx - fy * floor(fx / fy));
            {
                double q = fx / fy;

                q = (q < 0.0) ? ceil(q) : floor(q);
                if (op == 7) return float_int(q);
                return mkfloat(fx - fy * q);
            }
        }
    }
    if (kx == 1 && ky == 1) {
        a = INTVAL(x);
        b = INTVAL(y);
        switch (op) {
        case 1: return num_from_long(a + b);
        case 2: return num_from_long(a - b);
        case 3:
            fx = (double) a * (double) b;
            if (fx < 1.0e9 && fx > -1.0e9) return num_from_long(a * b);
            break;
        default:
            if (b == 0) return 0;
            if (op == 4) {
                if (a % b == 0) return num_from_long(a / b);
                return 0;
            }
            if (op == 5 || op == 6) {
                r = a % b;
                if (op == 5) {
                    long q = a / b;

                    if (r != 0 && ((a < 0) != (b < 0))) q--;
                    return num_from_long(q);
                }
                if (r != 0 && ((r < 0) != (b < 0))) r += b;
                return mkint(r);
            }
            if (op == 7) return num_from_long(a / b);
            return mkint(a % b);
        }
    }
    sx = num_str(x);
    sy = num_str(y);
    t = big_arith(op, sx, sy);
    free(sx);
    free(sy);
    res = big_result(t);
    return res;
}

static int p_cmp(oop x, oop y)
{
    int kx, ky, c;
    double fx, fy;
    char *sx, *sy;

    kx = numkind(x);
    ky = numkind(y);
    if (kx == 0 || ky == 0) return 2;
    if (kx == 3 || ky == 3) {
        fx = tofloat(x);
        fy = tofloat(y);
        if (fx < fy) return -1;
        if (fx > fy) return 1;
        if (fx == fy) return 0;
        return 2;
    }
    if (kx == 1 && ky == 1) {
        if (INTVAL(x) < INTVAL(y)) return -1;
        if (INTVAL(x) > INTVAL(y)) return 1;
        return 0;
    }
    sx = num_str(x);
    sy = num_str(y);
    c = big_scmp(sx, sy);
    free(sx);
    free(sy);
    return c;
}

/* ---- bits, radix -------------------------------------------------------- */

static oop bitop(int p, long x, long y)
{
    if (x < 0 || y < 0) return 0;
    if (p == PR_BITAND) return mkint(x & y);
    if (p == PR_BITOR) return mkint(x | y);
    return mkint(x ^ y);
}

static char *radix_str(long n, int rdx)
{
    static const char digs[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buf[64];
    int i, neg;
    long d;

    if (rdx < 2 || rdx > 36) rdx = 10;
    if (n == 0) return dups("0");
    neg = 0;
    if (n < 0) {
        neg = 1;
        n = -n;
    }
    i = 63;
    buf[i] = '\0';
    while (n > 0) {
        d = n % rdx;
        buf[--i] = digs[d];
        n = n / rdx;
    }
    if (neg) buf[--i] = '-';
    return dups(buf + i);
}

/* ---- parsing ------------------------------------------------------------ */

static oop parse_number(const char *s0)
{
    const char *s = s0;
    long len, q, p, i;
    int neg, isf;
    char *t;
    double v;
    oop o;

    while (*s != '\0' && is_sep((unsigned char) *s)) s++;
    len = (long) strlen(s);
    while (len > 0 && is_sep((unsigned char) s[len - 1])) len--;
    if (len == 0) return NIL_OOP;
    neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
        len--;
    }
    q = 0;
    while (q < len && is_digit((unsigned char) s[q])) q++;
    if (q == len) {
        if (len == 0) return NIL_OOP;
        t = (char *) xmalloc((size_t) len + 2);
        i = 0;
        if (neg) t[i++] = '-';
        memcpy(t + i, s, (size_t) len);
        t[i + len] = '\0';
        o = num_norm(t);
        free(t);
        return o;
    }
    if (q == 0) return NIL_OOP;
    isf = 0;
    if (s[q] == '.' && q + 1 < len && is_digit((unsigned char) s[q + 1])) {
        p = q + 1;
        while (p < len && is_digit((unsigned char) s[p])) p++;
        q = p;
        isf = 1;
    }
    if (q < len && (s[q] == 'e' || s[q] == 'E')) {
        p = q + 1;
        if (p < len && (s[p] == '-' || s[p] == '+')) p++;
        i = p;
        while (i < len && is_digit((unsigned char) s[i])) i++;
        if (i > p) {
            q = i;
            isf = 1;
        }
    }
    t = dupn(s, q);
    if (isf) {
        v = strtod(t, NULL);
        free(t);
        return mkfloat(neg ? -v : v);
    }
    {
        char *u = (char *) xmalloc((size_t) q + 2);

        i = 0;
        if (neg) u[i++] = '-';
        memcpy(u + i, t, (size_t) q);
        u[i + q] = '\0';
        o = num_norm(u);
        free(u);
    }
    free(t);
    return o;
}

/* ---- primitives PR_ADD .. PR_INTSQRT ------------------------------------ */

static oop num_strobj(char *t)
{
    oop o = mkstr(t);

    free(t);
    return o;
}

static oop prim_a(int p, oop rcv, long base, int na)
{
    oop a1, a2, r, o;
    long i, k, c;
    int kd;
    double x;
    const char *s;

    a1 = (na >= 1) ? ast[base + 1] : 0L;
    a2 = (na >= 2) ? ast[base + 2] : 0L;
    switch (p) {
    case PR_ADD: return p_arith(1, rcv, a1);
    case PR_SUB: return p_arith(2, rcv, a1);
    case PR_MUL: return p_arith(3, rcv, a1);
    case PR_DIV: return p_arith(4, rcv, a1);
    case PR_IDIV: return p_arith(5, rcv, a1);
    case PR_MOD: return p_arith(6, rcv, a1);
    case PR_QUO: return p_arith(7, rcv, a1);
    case PR_REM: return p_arith(8, rcv, a1);
    case PR_LT:
    case PR_GT:
    case PR_LE:
    case PR_GE:
        c = p_cmp(rcv, a1);
        if (c == 2) return 0;
        if (p == PR_LT) return mkbool(c < 0);
        if (p == PR_GT) return mkbool(c > 0);
        if (p == PR_LE) return mkbool(c <= 0);
        return mkbool(c >= 0);
    case PR_EQ:
    case PR_NE:
        c = p_cmp(rcv, a1);
        if (p == PR_EQ) return mkbool(c == 0);
        return mkbool(c != 0);
    case PR_IDENT: return mkbool(rcv == a1);
    case PR_NIDENT: return mkbool(rcv != a1);
    case PR_CLASS: return cls[cof(rcv)].oop_;
    case PR_BASICSIZE:
        if (IS_INT(rcv)) return mkint(0L);
        kd = OK(rcv);
        if (kd == KD_STR || kd == KD_SYM) return mkint(OSLEN(rcv));
        if (kd == KD_ARR || kd == KD_PLAIN)
            return mkint(ON(rcv) - cls[OC(rcv)].ninst);
        return mkint(0L);
    case PR_BASICAT:
        if (IS_INT(rcv) || !IS_INT(a1)) return 0;
        i = INTVAL(a1);
        kd = OK(rcv);
        if (kd == KD_STR || kd == KD_SYM) {
            if (i < 1 || i > OSLEN(rcv)) return 0;
            return mkchar((long) (unsigned char) OS(rcv)[i - 1]);
        }
        c = cls[OC(rcv)].ninst;
        if (i < 1 || i > ON(rcv) - c) return 0;
        return SLOT(rcv, c + i - 1);
    case PR_BASICATPUT:
        if (IS_INT(rcv) || !IS_INT(a1)) return 0;
        i = INTVAL(a1);
        kd = OK(rcv);
        if (kd == KD_STR) {
            if (i < 1 || i > OSLEN(rcv) || IS_INT(a2)) return 0;
            if (OK(a2) != KD_CHR) return 0;
            OS(rcv)[i - 1] = (char) objs[HI(a2)].x;
            return a2;
        }
        if (kd == KD_SYM) return 0;
        c = cls[OC(rcv)].ninst;
        if (i < 1 || i > ON(rcv) - c) return 0;
        SLOT(rcv, c + i - 1) = a2;
        return a2;
    case PR_INSTVARAT:
        if (IS_INT(rcv) || !IS_INT(a1)) return 0;
        i = INTVAL(a1);
        if (i < 1 || i > ON(rcv)) return 0;
        return SLOT(rcv, i - 1);
    case PR_INSTVARATPUT:
        if (IS_INT(rcv) || !IS_INT(a1)) return 0;
        i = INTVAL(a1);
        if (i < 1 || i > ON(rcv)) return 0;
        SLOT(rcv, i - 1) = a2;
        return a2;
    case PR_IDHASH:
        if (IS_INT(rcv)) return rcv;
        return mkint(HI(rcv));
    case PR_PERFORM:
    case PR_PERFORMARGS:
        s = strof(a1);
        if (s[0] == '\0' || !isstr(a1)) return 0;
        k = selid(s);
        if (p == PR_PERFORM) {
            for (i = 2; i <= na; i++) ast[base + i - 1] = ast[base + i];
            asp = base + na - 1;
            r = send_sel(rcv, (int) k, base, na - 1);
            asp = base;
            return r;
        }
        if (IS_INT(a2)) return 0;
        c = ON(a2);
        asp = base;
        for (i = 0; i < c; i++) ast_push(SLOT(a2, i));
        r = send_sel(rcv, (int) k, base, (int) c);
        asp = base;
        return r;
    case PR_ISKINDOF:
        if (IS_INT(a1)) return 0;
        if (OK(a1) != KD_CLS && OK(a1) != KD_META) return 0;
        return mkbool(inherits(cof(rcv), (int) objs[HI(a1)].x));
    case PR_ISMEMBEROF:
        if (IS_INT(a1)) return 0;
        return mkbool(cof(rcv) == (int) objs[HI(a1)].x);
    case PR_RESPONDSTO:
        if (!isstr(a1)) return 0;
        s = strof(a1);
        return mkbool(lookup(cof(rcv), selid(s)) > 0);
    case PR_SHALLOWCOPY: return shallow_copy(rcv);
    case PR_BASICNEW:
        if (IS_INT(rcv)) return 0;
        return new_instance((int) objs[HI(rcv)].x, 0L);
    case PR_BASICNEWSIZE:
        if (IS_INT(rcv) || !IS_INT(a1)) return 0;
        if (INTVAL(a1) < 0) return 0;
        return new_instance((int) objs[HI(rcv)].x, INTVAL(a1));
    case PR_FLOATOP:
        if (!IS_INT(a1)) return 0;
        k = INTVAL(a1);
        if (numkind(rcv) == 0) return 0;
        x = tofloat(rcv);
        switch (k) {
        case 1:
            if (x < 0.0) return mkfloat(num_nan());
            return mkfloat(sqrt(x));
        case 2: return mkfloat(sin(x));
        case 3: return mkfloat(cos(x));
        case 4: return mkfloat(tan(x));
        case 5: return mkfloat(atan(x));
        case 6:
            if (x < 0.0) return mkfloat(num_nan());
            if (x == 0.0) return mkfloat(0.0 - num_inf());
            return mkfloat(log(x));
        case 7: return mkfloat(exp(x));
        case 8:
            if (x < 0.0) return mkfloat(num_nan());
            if (x == 0.0) return mkfloat(0.0 - num_inf());
            return mkfloat(log10(x));
        case 9:
            if (x > 1.0 || x < -1.0) return mkfloat(num_nan());
            return mkfloat(asin(x));
        case 10:
            if (x > 1.0 || x < -1.0) return mkfloat(num_nan());
            return mkfloat(acos(x));
        case 11:
            if (numkind(a2) == 0) return 0;
            return mkfloat(pow(x, tofloat(a2)));
        default: return 0;
        }
    case PR_TRUNCATED:
    case PR_FLOOR:
    case PR_CEILING:
    case PR_ROUNDED:
        kd = numkind(rcv);
        if (kd == 1 || kd == 2) return rcv;
        if (kd != 3) return 0;
        x = OF(rcv);
        if (p == PR_TRUNCATED) return float_int(x < 0.0 ? ceil(x) : floor(x));
        if (p == PR_FLOOR) return float_int(floor(x));
        if (p == PR_CEILING) return float_int(ceil(x));
        if (x >= 0.0) return float_int(floor(x + 0.5));
        return float_int(ceil(x - 0.5));
    case PR_ASFLOAT:
        if (numkind(rcv) == 0) return 0;
        return mkfloat(tofloat(rcv));
    case PR_BITAND:
    case PR_BITOR:
    case PR_BITXOR:
        if (!IS_INT(rcv) || !IS_INT(a1)) return 0;
        return bitop(p, INTVAL(rcv), INTVAL(a1));
    case PR_BITSHIFT:
        if (!IS_INT(rcv) || !IS_INT(a1)) return 0;
        k = INTVAL(a1);
        o = rcv;
        if (k >= 0) {
            if (k > 1000000L) return 0;
            while (k > 0) {
                c = (k > 29) ? 29 : k;
                o = p_arith(3, o, mkint(1L << c));
                if (o == 0) return 0;
                k -= c;
            }
            return o;
        }
        k = -k;
        while (k > 0) {
            c = (k > 29) ? 29 : k;
            o = p_arith(5, o, mkint(1L << c));
            if (o == 0) return 0;
            k -= c;
        }
        return o;
    case PR_PRINTSTRING:
        kd = numkind(rcv);
        if (kd == 1 || kd == 2) return num_strobj(num_str(rcv));
        if (kd == 3) return num_strobj(float_str(OF(rcv)));
        return 0;
    case PR_PRINTRADIX:
        if (numkind(rcv) != 1 || !IS_INT(a1)) return 0;
        return num_strobj(radix_str(INTVAL(rcv), (int) INTVAL(a1)));
    case PR_ISINT:
        kd = numkind(rcv);
        return mkbool(kd == 1 || kd == 2);
    case PR_ISFLOAT: return mkbool(numkind(rcv) == 3);
    case PR_ISSTRING:
        kd = kindof(rcv);
        return mkbool(kd == KD_STR || kd == KD_SYM);
    case PR_ISSYMBOL: return mkbool(kindof(rcv) == KD_SYM);
    case PR_ISCHAR: return mkbool(kindof(rcv) == KD_CHR);
    case PR_ISNUMBER: return mkbool(numkind(rcv) > 0);
    case PR_INTSQRT:
        if (numkind(rcv) != 1) return 0;
        if (INTVAL(rcv) < 0) return 0;
        k = (long) floor(sqrt((double) INTVAL(rcv)));
        while (k * k > INTVAL(rcv)) k--;
        while ((k + 1) * (k + 1) <= INTVAL(rcv)) k++;
        return mkint(k);
    default:
        return 0;
    }
}
/* ===== s8_str.c ===== */
/* =========================================================================
   s8_str.c   dictionaries, exceptions (signal), primitive dispatch and
              string / symbol / character / array / dictionary primitives.
   ========================================================================= */

#define S8X(q) (objs[HI(q)].x)

/* ---- dictionaries -------------------------------------------------------
   Insertion-ordered linked list of entries per dictionary id (dd_head,
   dd_tail, dd_n; de_key, de_val, de_next, de_live), indexed 1-based.
   Lookup goes through dmap with the composite key "<dictid>|<dkey>".      */

/* Build the composite key for (dictionary id d, key x); malloc'ed. */
static char *dict_ckey(long d, oop x)
{
    char *r;
    char *t;
    long len;
    int len2;
    int k;
    double f;

    if (IS_INT(x)) {
        r = (char *)xmalloc(64);
        sprintf(r, "%ld|i%ld", d, INTVAL(x));
        return r;
    }
    k = OK(x);
    if (k == KD_STR || k == KD_SYM || k == KD_BIG) {
        t = OS(x);
        len = OSLEN(x);
        r = (char *)xmalloc((size_t)len + 40);
        if (k == KD_STR) len2 = sprintf(r, "%ld|s", d);
        else if (k == KD_SYM) len2 = sprintf(r, "%ld|y", d);
        else len2 = sprintf(r, "%ld|i", d);
        memcpy(r + len2, t, (size_t)len);
        r[len2 + len] = '\0';
        return r;
    }
    if (k == KD_FLT) {
        f = OF(x);
        r = (char *)xmalloc(400);
        if (f == floor(f) && fabs(f) < 900000000000000000.0) {
            if (f == 0.0) sprintf(r, "%ld|i0", d);
            else sprintf(r, "%ld|i%.0f", d, f);
        } else {
            sprintf(r, "%ld|f%.17g", d, f);
        }
        return r;
    }
    r = (char *)xmalloc(64);
    if (k == KD_CHR) sprintf(r, "%ld|c%ld", d, S8X(x));
    else sprintf(r, "%ld|o%ld", d, x);
    return r;
}

static oop dict_new(int c)
{
    oop o;
    long d;
    long cp;

    nd0 = nd0 + 1;
    d = nd0;
    cp = ddcap;
    dd_head = (long *)xgrow(dd_head, &cp, d + 1, sizeof(long));
    cp = ddcap;
    dd_tail = (long *)xgrow(dd_tail, &cp, d + 1, sizeof(long));
    cp = ddcap;
    dd_n = (long *)xgrow(dd_n, &cp, d + 1, sizeof(long));
    ddcap = cp;
    dd_head[d] = 0;
    dd_tail[d] = 0;
    dd_n[d] = 0;
    o = newobj(c, KD_DICT, cls[c].ninst);
    S8X(o) = d;
    return o;
}

static long dict_find(oop o, oop key)
{
    char *k;
    long v;
    int found;

    k = dict_ckey(S8X(o), key);
    found = smap_get(&dmap, k, &v);
    free(k);
    if (!found) return 0;
    return v;
}

static void dict_put(oop o, oop key, oop val)
{
    long e;
    long d;
    long cp;
    char *k;

    e = dict_find(o, key);
    if (e > 0) {
        de_val[e] = val;
        return;
    }
    d = S8X(o);
    ne = ne + 1;
    cp = decap;
    de_key = (oop *)xgrow(de_key, &cp, ne + 1, sizeof(oop));
    cp = decap;
    de_val = (oop *)xgrow(de_val, &cp, ne + 1, sizeof(oop));
    cp = decap;
    de_next = (long *)xgrow(de_next, &cp, ne + 1, sizeof(long));
    cp = decap;
    de_live = (int *)xgrow(de_live, &cp, ne + 1, sizeof(int));
    decap = cp;
    de_key[ne] = key;
    de_val[ne] = val;
    de_next[ne] = 0;
    de_live[ne] = 1;
    if (dd_tail[d] == 0) dd_head[d] = ne;
    else de_next[dd_tail[d]] = ne;
    dd_tail[d] = ne;
    dd_n[d] = dd_n[d] + 1;
    k = dict_ckey(d, key);
    smap_put(&dmap, k, ne);
    free(k);
}

static long dict_remove(oop o, oop key)
{
    long e;
    long d;
    char *k;

    e = dict_find(o, key);
    if (e == 0) return 0;
    d = S8X(o);
    de_live[e] = 0;
    dd_n[d] = dd_n[d] - 1;
    k = dict_ckey(d, key);
    smap_del(&dmap, k);
    free(k);
    return e;
}

/* ---- exceptions ---------------------------------------------------------- */

/* does handler class spec h (class or ExceptionSet array) match exception ex ? */
static int exc_matches(oop h, oop ex)
{
    long i;
    long n;

    if (IS_INT(h)) return 0;
    if (OK(h) == KD_CLS) return inherits(cof(ex), (int)S8X(h));
    if (OK(h) == KD_ARR) {
        n = ON(h);
        for (i = 0; i < n; i++) {
            if (exc_matches(SLOT(h, i), ex)) return 1;
        }
    }
    return 0;
}

/* signal ex, looking for handlers from index start downwards */
static oop do_signal(oop ex, long start, int fresh)
{
    long k, v, sig, oldtop, base;
    oop hb;
    oop r;

    if (fresh == 1) {
        sigcount = sigcount + 1;
        sig = sigcount;
        SLOT(ex, 2) = mkint(sig);
    } else {
        sig = intval(SLOT(ex, 2));
    }
    k = 0;
    for (v = start; v >= 1; v--) {
        if (exc_matches(h_cls[v], ex)) {
            k = v;
            break;
        }
    }
    if (k == 0) {
        /* unhandled */
        base = asp;
        r = send_sel(ex, S_DEFAULTACTION, base, 0);
        asp = base;
        if (unw > 0) return 0;
        return r;
    }
    oldtop = htop;
    htop = k - 1;
    SLOT(ex, 1) = mkint(k);
    hb = h_blk[k];
    base = asp;
    if (NB(S8X(hb)) == 0) {
        r = call_block(hb, base, 0);
    } else {
        ast_push(ex);
        r = call_block(hb, base, 1);
        asp = base;
    }
    htop = oldtop;
    if (unw == UW_NONE) {
        /* the handler block completed: return its value from on:do: */
        unw = UW_EXC;
        unw_t = h_id[k];
        unw_v = r;
        return 0;
    }
    if (unw == UW_RESUME && unw_t == sig) {
        unw = UW_NONE;
        return unw_v;
    }
    return 0;
}

/* ---- primitive dispatch --------------------------------------------------- */
static oop prim(int p, oop rcv, long base, int na)
{
    if (p >= PR_SOCKNEW) return prim_net(p, rcv, base, na);
    if (p < PR_STRSIZE) return prim_a(p, rcv, base, na);
    if (p < PR_BLOCKVALUE) return prim_b(p, rcv, base, na);
    return prim_c(p, rcv, base, na);
}

/* ---- string helpers -------------------------------------------------------- */
#define STR_WS " \t\n\r"

static int str_in(const char *set, long n, int c)
{
    long i;
    for (i = 0; i < n; i++) {
        if ((unsigned char)set[i] == (unsigned)c) return 1;
    }
    return 0;
}

/* 1-based position of t (length tl) in s (length sl_) from 1-based start; 0 if none */
static long str_pos(const char *t, long tl, const char *s, long sl_, long start)
{
    long i;
    if (tl == 0) return 0;
    for (i = start - 1; i + tl <= sl_; i++) {
        if (s[i] == t[0] && memcmp(s + i, t, (size_t)tl) == 0) return i + 1;
    }
    return 0;
}

static oop str_split(const char *s, long n, const char *seps, long nseps)
{
    long i, j, cnt, k;
    oop r;
    oop e;

    cnt = 0;
    i = 0;
    while (i < n) {
        while (i < n && str_in(seps, nseps, (unsigned char)s[i])) i++;
        if (i >= n) break;
        while (i < n && !str_in(seps, nseps, (unsigned char)s[i])) i++;
        cnt++;
    }
    r = mkarray(cnt);
    k = 0;
    i = 0;
    while (i < n) {
        while (i < n && str_in(seps, nseps, (unsigned char)s[i])) i++;
        if (i >= n) break;
        j = i;
        while (j < n && !str_in(seps, nseps, (unsigned char)s[j])) j++;
        e = mkstrn(s + i, j - i);
        SLOT(r, k) = e;
        k++;
        i = j;
    }
    return r;
}

/* ---- strings, symbols, characters, arrays, dictionaries ------------------- */
static oop prim_b(int p, oop rcv, long base, int na)
{
    oop a1, a2, a3, e4, r, v;
    long i, j, k, n, c, len, tl, cnt;
    double h;
    char *buf;
    const char *s;
    const char *t;
    char tb[2];

    a1 = ast[base + 1];
    a2 = ast[base + 2];
    a3 = ast[base + 3];

    switch (p) {
    case PR_STRSIZE:
        if (!isstr(rcv)) return 0;
        return mkint(OSLEN(rcv));

    case PR_STRAT:
        if (!isstr(rcv) || !IS_INT(a1)) return 0;
        i = intval(a1);
        if (i < 1 || i > OSLEN(rcv)) return 0;
        return mkchar((long)(unsigned char)OS(rcv)[i - 1]);

    case PR_STRATPUT:
        if (kindof(rcv) != KD_STR || !IS_INT(a1)) return 0;
        i = intval(a1);
        if (i < 1 || i > OSLEN(rcv) || kindof(a2) != KD_CHR) return 0;
        if (S8X(a2) < 0 || S8X(a2) > 255) return 0;
        OS(rcv)[i - 1] = (char)(unsigned char)S8X(a2);
        return a2;

    case PR_STRCAT:
        if (!isstr(rcv) || !isstr(a1)) return 0;
        n = OSLEN(rcv);
        len = OSLEN(a1);
        buf = (char *)xmalloc((size_t)(n + len) + 1);
        memcpy(buf, OS(rcv), (size_t)n);
        memcpy(buf + n, OS(a1), (size_t)len);
        buf[n + len] = '\0';
        r = mkstrn(buf, n + len);
        free(buf);
        return r;

    case PR_STRCOPY:
        if (!isstr(rcv) || !IS_INT(a1) || !IS_INT(a2)) return 0;
        i = intval(a1);
        j = intval(a2);
        if (j < i) {
            if (i >= 1 && i <= OSLEN(rcv) + 1 && j == i - 1) return mkstr("");
            return 0;
        }
        if (i < 1 || j > OSLEN(rcv)) return 0;
        return mkstrn(OS(rcv) + (i - 1), j - i + 1);

    case PR_STREQ:
        if (!isstr(rcv) || !isstr(a1)) return mkbool(0);
        if (OK(rcv) != OK(a1)) return mkbool(0);
        if (OSLEN(rcv) != OSLEN(a1)) return mkbool(0);
        return mkbool(memcmp(OS(rcv), OS(a1), (size_t)OSLEN(rcv)) == 0);

    case PR_STRLT:
    case PR_STRCOMPARE:
        if (!isstr(rcv) || !isstr(a1)) return 0;
        n = strcmp(OS(rcv), OS(a1));
        if (p == PR_STRCOMPARE) {
            if (n == 0) return mkint(0);
            if (n < 0) return mkint(-1);
            return mkint(1);
        }
        return mkbool(n < 0);

    case PR_STRHASH:
        if (!isstr(rcv)) return 0;
        s = OS(rcv);
        n = OSLEN(rcv);
        h = (double)n;
        len = n < 24 ? n : 24;
        for (i = 0; i < len; i++) {
            h = fmod(h * 31.0 + (double)(unsigned char)s[i], 1000000007.0);
        }
        return mkint((long)h);

    case PR_ASSYMBOL:
        if (!isstr(rcv)) return 0;
        return mksym(OS(rcv));

    case PR_ASSTRING:
        if (!isstr(rcv)) return 0;
        if (OK(rcv) == KD_STR) return rcv;
        return mkstrn(OS(rcv), OSLEN(rcv));

    case PR_UPPER:
    case PR_LOWER:
        if (!isstr(rcv)) return 0;
        n = OSLEN(rcv);
        buf = (char *)xmalloc((size_t)n + 1);
        for (i = 0; i < n; i++) {
            c = (unsigned char)OS(rcv)[i];
            if (p == PR_UPPER) c = to_upper((int)c);
            else c = to_lower((int)c);
            buf[i] = (char)(unsigned char)c;
        }
        buf[n] = '\0';
        if (kindof(rcv) == KD_SYM) r = mksym(buf);
        else r = mkstrn(buf, n);
        free(buf);
        return r;

    case PR_INDEXOF:
        if (!isstr(rcv)) return 0;
        if (kindof(a1) == KD_CHR) {
            c = S8X(a1);
            if (c < 0 || c > 255) return mkint(0);
            s = OS(rcv);
            n = OSLEN(rcv);
            for (i = 0; i < n; i++) {
                if ((unsigned char)s[i] == (unsigned)c) return mkint(i + 1);
            }
            return mkint(0);
        }
        if (isstr(a1)) {
            if (na >= 2 && IS_INT(a2)) i = intval(a2);
            else i = 1;
            if (i < 1) i = 1;
            if (OSLEN(a1) == 0) return mkint(0);
            return mkint(str_pos(OS(a1), OSLEN(a1), OS(rcv), OSLEN(rcv), i));
        }
        return 0;

    case PR_REPLACEALL: {
        SB sb;
        if (!isstr(rcv) || !isstr(a1) || !isstr(a2)) return 0;
        s = OS(rcv);
        n = OSLEN(rcv);
        t = OS(a1);
        tl = OSLEN(a1);
        if (tl == 0) return mkstrn(s, n);
        sb_init(&sb);
        i = 1;
        for (;;) {
            j = str_pos(t, tl, s, n, i);
            if (j == 0) break;
            sb_addn(&sb, s + (i - 1), j - i);
            sb_addn(&sb, OS(a2), OSLEN(a2));
            i = j + tl;
        }
        if (i <= n) sb_addn(&sb, s + (i - 1), n - i + 1);
        r = mkstrn(sb_str(&sb), sb.n);
        sb_free(&sb);
        return r;
    }

    case PR_ASNUMBER:
        if (!isstr(rcv)) return 0;
        return parse_number(OS(rcv));

    case PR_STRTRIM:
        if (!isstr(rcv)) return 0;
        s = OS(rcv);
        n = OSLEN(rcv);
        i = 0;
        while (i < n && str_in(STR_WS, 4, (unsigned char)s[i])) i++;
        if (i >= n) return mkstr("");
        j = n - 1;
        while (j > i && str_in(STR_WS, 4, (unsigned char)s[j])) j--;
        return mkstrn(s + i, j - i + 1);

    case PR_STRSPLIT:
        if (!isstr(rcv)) return 0;
        if (na >= 1 && isstr(a1)) {
            return str_split(OS(rcv), OSLEN(rcv), OS(a1), OSLEN(a1));
        }
        if (na >= 1 && kindof(a1) == KD_CHR) {
            if (S8X(a1) < 0 || S8X(a1) > 255) tl = 0;
            else {
                tb[0] = (char)(unsigned char)S8X(a1);
                tb[1] = '\0';
                tl = 1;
            }
            return str_split(OS(rcv), OSLEN(rcv), tb, tl);
        }
        return str_split(OS(rcv), OSLEN(rcv), STR_WS, 4);

    case PR_STRREV:
        if (!isstr(rcv)) return 0;
        n = OSLEN(rcv);
        buf = (char *)xmalloc((size_t)n + 1);
        for (i = 0; i < n; i++) buf[i] = OS(rcv)[n - 1 - i];
        buf[n] = '\0';
        r = mkstrn(buf, n);
        free(buf);
        return r;

    case PR_STRNEW:
        if (!IS_INT(a1)) return 0;
        n = intval(a1);
        if (n < 0) n = 0;
        buf = (char *)xmalloc((size_t)n + 1);
        memset(buf, ' ', (size_t)n);
        buf[n] = '\0';
        r = mkstrn(buf, n);
        free(buf);
        return r;

    case PR_CHARVALUE:
        if (kindof(rcv) != KD_CHR) return 0;
        return mkint(S8X(rcv));

    case PR_CHARFROM:
        if (!IS_INT(a1)) return 0;
        i = intval(a1);
        if (i < 0 || i > 255) return 0;
        return mkchar(i);

    case PR_ARRNEW:
        if (!IS_INT(a1)) return 0;
        n = intval(a1);
        if (n < 0) n = 0;
        return mkarray(n);

    case PR_ARRAT:
        if (IS_INT(rcv) || !IS_INT(a1)) return 0;
        if (OK(rcv) != KD_ARR) return 0;
        i = intval(a1);
        c = cls[OC(rcv)].ninst;
        if (i < 1 || i > ON(rcv) - c) return 0;
        return SLOT(rcv, c + i - 1);

    case PR_ARRATPUT:
        if (IS_INT(rcv) || !IS_INT(a1)) return 0;
        if (OK(rcv) != KD_ARR) return 0;
        i = intval(a1);
        c = cls[OC(rcv)].ninst;
        if (i < 1 || i > ON(rcv) - c) return 0;
        SLOT(rcv, c + i - 1) = a2;
        return a2;

    case PR_ARRSIZE:
        if (IS_INT(rcv)) return 0;
        if (OK(rcv) != KD_ARR) return 0;
        return mkint(ON(rcv) - cls[OC(rcv)].ninst);

    case PR_ARRCOPY:
        if (IS_INT(rcv) || !IS_INT(a1) || !IS_INT(a2)) return 0;
        if (OK(rcv) != KD_ARR) return 0;
        i = intval(a1);
        j = intval(a2);
        c = cls[OC(rcv)].ninst;
        if (j < i) {
            if (j == i - 1 && i >= 1) return mkarray(0);
            return 0;
        }
        if (i < 1 || j > ON(rcv) - c) return 0;
        r = mkarray(j - i + 1);
        for (k = 0; k <= j - i; k++) {
            SLOT(r, k) = SLOT(rcv, c + i - 1 + k);
        }
        return r;

    case PR_ARRFILL:
        if (IS_INT(rcv)) return 0;
        if (OK(rcv) != KD_ARR) return 0;
        c = cls[OC(rcv)].ninst;
        for (k = c; k < ON(rcv); k++) SLOT(rcv, k) = a1;
        return rcv;

    case PR_ARRREPLACE:
        /* replaceFrom: start to: stop with: coll startingAt: repStart */
        if (IS_INT(rcv) || !IS_INT(a1) || !IS_INT(a2) || IS_INT(a3)) return 0;
        i = intval(a1);
        j = intval(a2);
        e4 = ast[base + 4];
        if (!IS_INT(e4)) return 0;
        k = intval(e4);
        n = j - i + 1;
        if (n <= 0) return rcv;
        if (OK(rcv) == KD_STR && isstr(a3)) {
            if (i < 1 || j > OSLEN(rcv) || k < 1 || k + n - 1 > OSLEN(a3)) return 0;
            memmove(OS(rcv) + (i - 1), OS(a3) + (k - 1), (size_t)n);
            return rcv;
        }
        if (OK(rcv) == KD_ARR && kindof(a3) == KD_ARR) {
            c = cls[OC(rcv)].ninst;
            cnt = cls[OC(a3)].ninst;
            if (i < 1 || j > ON(rcv) - c || k < 1 || k + n - 1 > ON(a3) - cnt) return 0;
            if (rcv == a3 && k < i) {
                for (len = n - 1; len >= 0; len--) {
                    SLOT(rcv, c + i - 1 + len) = SLOT(a3, cnt + k - 1 + len);
                }
            } else {
                for (len = 0; len < n; len++) {
                    SLOT(rcv, c + i - 1 + len) = SLOT(a3, cnt + k - 1 + len);
                }
            }
            return rcv;
        }
        return 0;

    case PR_DICTNEW:
        if (IS_INT(rcv)) return 0;
        return dict_new((int)S8X(rcv));

    case PR_DICTAT:
        if (kindof(rcv) != KD_DICT) return 0;
        k = dict_find(rcv, a1);
        if (k == 0) return 0;
        return de_val[k];

    case PR_DICTATPUT:
        if (kindof(rcv) != KD_DICT) return 0;
        dict_put(rcv, a1, a2);
        return a2;

    case PR_DICTHAS:
        if (kindof(rcv) != KD_DICT) return 0;
        return mkbool(dict_find(rcv, a1) > 0);

    case PR_DICTREMOVE:
        if (kindof(rcv) != KD_DICT) return 0;
        k = dict_remove(rcv, a1);
        if (k == 0) return 0;
        return de_val[k];

    case PR_DICTKEYS:
    case PR_DICTVALUES:
        if (kindof(rcv) != KD_DICT) return 0;
        n = dd_n[S8X(rcv)];
        r = mkarray(n);
        k = dd_head[S8X(rcv)];
        j = 0;
        while (k > 0) {
            if (de_live[k] == 1) {
                v = (p == PR_DICTKEYS) ? de_key[k] : de_val[k];
                SLOT(r, j) = v;
                j++;
            }
            k = de_next[k];
        }
        return r;

    case PR_DICTCOPY:
        if (kindof(rcv) != KD_DICT) return 0;
        return shallow_copy(rcv);

    case PR_DICTSIZE:
        if (kindof(rcv) != KD_DICT) return 0;
        return mkint(dd_n[S8X(rcv)]);

    default:
        break;
    }
    return 0;
}
/* ===== s9_sys.c ===== */
/* =========================================================================
   s9_sys.c   primitives PR_BLOCKVALUE .. PR_UNICODE (prim_c) and the
              socket primitives (prim_net, stubs).

              Blocks, exceptions, classes, globals, Transcript, stdin,
              time, random numbers, files, operating system, evaluation.
   ========================================================================= */

/* OX() in s0 is broken (parameter x clashes with member x) */
#define SYS_OX(o) (objs[HI(o)].x)

#ifdef ST_POSIX
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
/* not declared by <stdio.h> in strict C89 mode */
extern FILE *popen(const char *command, const char *type);
extern int pclose(FILE *stream);
#endif

/* provided by the core (parser/evaluator part): parse and run a string of
   statements, answer the value of the last one (nil on error) */
static oop eval_string(const char *text);

/* ---- handler stack capacity (arrays are 1-based, index 1..htop) ------- */
static void sys_hneed(long need)
{
    long ncap;
    if (need < hcap) return;
    ncap = hcap * 2;
    if (ncap < need + 16) ncap = need + 16;
    if (ncap < 64) ncap = 64;
    if (h_cls == NULL) h_cls = (oop *)xmalloc((size_t)ncap * sizeof(oop));
    else h_cls = (oop *)xrealloc(h_cls, (size_t)ncap * sizeof(oop));
    if (h_blk == NULL) h_blk = (oop *)xmalloc((size_t)ncap * sizeof(oop));
    else h_blk = (oop *)xrealloc(h_blk, (size_t)ncap * sizeof(oop));
    if (h_id == NULL) h_id = (long *)xmalloc((size_t)ncap * sizeof(long));
    else h_id = (long *)xrealloc(h_id, (size_t)ncap * sizeof(long));
    hcap = ncap;
}

/* ---- time ------------------------------------------------------------- */
static double sys_ms_last = 0.0;

/* milliseconds since 1970 (monotonic non-decreasing) */
static double sys_now_ms(void)
{
    static double base_ms = 0.0;
    static double base_clk = 0.0;
    static int started = 0;
    double wall, cpu, v;
    wall = (double)time(NULL) * 1000.0;
    if (!started) {
        started = 1;
        base_ms = wall;
        base_clk = (double)clock();
    }
    cpu = base_ms + ((double)clock() - base_clk) * 1000.0 / (double)CLOCKS_PER_SEC;
    v = cpu;
    if (wall > v) v = wall;
    if (v < sys_ms_last) v = sys_ms_last;
    sys_ms_last = v;
    return v;
}

/* integer from a double (may exceed SmallInteger range) */
static oop sys_mkint_d(double d)
{
    char buf[64];
    sprintf(buf, "%.0f", d);
    return big_norm(buf);
}

/* ---- random numbers (LCG 1103515245 * s + 12345 mod 2^31) ------------- */
static long sys_rand_next(void)
{
    unsigned long s;
    s = (unsigned long)rnd_state;
    s = (s * 1103515245UL + 12345UL) & 0x7FFFFFFFUL;
    rnd_state = (long)s;
    return rnd_state;
}

/* ---- stdin ------------------------------------------------------------ */
static oop sys_stdin_line(void)
{
    SB b;
    int c;
    long got;
    oop r;
    if (stdin_eof) return 0;
    sb_init(&b);
    got = 0;
    for (;;) {
        c = getc(stdin);
        if (c == EOF) break;
        got = 1;
        if (c == '\n') break;
        sb_addc(&b, c);
    }
    if (c == EOF && !got) {
        stdin_eof = 1;
        sb_free(&b);
        return 0;
    }
    if (b.n > 0 && b.p[b.n - 1] == '\r') b.n--;
    r = mkstrn(sb_str(&b), b.n);
    sb_free(&b);
    return r;
}

/* ---- files ------------------------------------------------------------ */

/* whole file; NULL when it cannot be read */
static char *sys_slurp(const char *fn, long *len)
{
    FILE *f;
    SB b;
    char buf[4096];
    size_t k;
    char *res;
    f = fopen(fn, "rb");
    if (f == NULL) return NULL;
    sb_init(&b);
    while ((k = fread(buf, 1, sizeof(buf), f)) > 0) sb_addn(&b, buf, (long)k);
    if (ferror(f)) {
        fclose(f);
        sb_free(&b);
        return NULL;
    }
    fclose(f);
    *len = b.n;
    res = dupn(sb_str(&b), b.n);
    sb_free(&b);
    return res;
}

static int sys_write(const char *fn, const char *s, long n, int append)
{
    FILE *f;
    size_t w;
    f = fopen(fn, append ? "ab" : "wb");
    if (f == NULL) return 0;
    w = (n > 0) ? fwrite(s, 1, (size_t)n, f) : 0;
    if (fclose(f) != 0) return 0;
    return (long)w == n;
}

static int sys_copy(const char *from, const char *to)
{
    FILE *f, *g;
    char buf[4096];
    size_t k;
    int ok;
    f = fopen(from, "rb");
    if (f == NULL) return 0;
    g = fopen(to, "wb");
    if (g == NULL) {
        fclose(f);
        return 0;
    }
    ok = 1;
    while ((k = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (fwrite(buf, 1, k, g) != k) {
            ok = 0;
            break;
        }
    }
    if (ferror(f)) ok = 0;
    fclose(f);
    if (fclose(g) != 0) ok = 0;
    return ok;
}

/* 0 absent, 1 file, 2 directory */
static int sys_kind(const char *fn)
{
#ifdef ST_POSIX
    struct stat st;
    if (stat(fn, &st) != 0) return 0;
    if (S_ISDIR(st.st_mode)) return 2;
    return 1;
#else
    FILE *f;
    f = fopen(fn, "rb");
    if (f == NULL) return 0;
    fclose(f);
    return 1;
#endif
}

/* size of a file, -1 when absent or not a regular file */
static long sys_fsize(const char *fn)
{
    FILE *f;
    long n;
    if (sys_kind(fn) != 1) return -1;
    f = fopen(fn, "rb");
    if (f == NULL) return -1;
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    n = ftell(f);
    fclose(f);
    return n;
}

/* ---- running commands ------------------------------------------------- */
static void sys_set_err(const char *s, long n)
{
    if (n > 0 && s[n - 1] == '\n') n--;
    if (sys_err != NULL) free(sys_err);
    sys_err = dupn(s, n);
}

static oop sys_command(const char *cmd)
{
#ifdef ST_POSIX
    char errname[128];
    char *full;
    FILE *p;
    SB b;
    char buf[4096];
    size_t k;
    int st;
    char *etxt;
    long elen;
    oop r;
    sprintf(errname, "/tmp/st_sys_%ld.err", (long)getpid());
    full = (char *)xmalloc(strlen(cmd) + strlen(errname) + 16);
    sprintf(full, "( %s\n) 2>%s", cmd, errname);
    flush_out();
    p = popen(full, "r");
    free(full);
    if (p == NULL) {
        sys_rc = -1;
        sys_set_err("", 0L);
        return mkstr("");
    }
    sb_init(&b);
    while ((k = fread(buf, 1, sizeof(buf), p)) > 0) sb_addn(&b, buf, (long)k);
    st = pclose(p);
    if (st == -1) sys_rc = -1;
    else if ((st & 0x7f) == 0) sys_rc = (st >> 8) & 0xff;
    else sys_rc = 128 + (st & 0x7f);
    if (b.n > 0 && b.p[b.n - 1] == '\n') b.n--;
    r = mkstrn(sb_str(&b), b.n);
    sb_free(&b);
    elen = 0;
    etxt = sys_slurp(errname, &elen);
    if (etxt != NULL) {
        sys_set_err(etxt, elen);
        free(etxt);
    } else {
        sys_set_err("", 0L);
    }
    remove(errname);
    return r;
#else
    flush_out();
    sys_rc = system(cmd);
    sys_set_err("", 0L);
    return mkstr("");
#endif
}

/* ---- class helpers ---------------------------------------------------- */

/* selector ids of the methods defined directly in class k, ascending */
static oop sys_selectors(int k)
{
    long m, n, i, j, t;
    long *ids;
    oop r;
    ids = (long *)xmalloc((size_t)(nmeth + 1) * sizeof(long));
    n = 0;
    for (m = 1; m <= nmeth; m++) {
        if (meths[m].cls == k && lookup(k, meths[m].sel) == (int)m) ids[n++] = meths[m].sel;
    }
    for (i = 1; i < n; i++) {
        t = ids[i];
        for (j = i - 1; j >= 0 && ids[j] > t; j--) ids[j + 1] = ids[j];
        ids[j + 1] = t;
    }
    r = mkarray(n);
    for (i = 0; i < n; i++) {
        oop sy_ = mksym(sel_name[ids[i]]);
        SLOT(r, i) = sy_;
    }
    free(ids);
    return r;
}

/* ---- unicode (bytes only) --------------------------------------------- */
static oop sys_unicode(oop rcv, oop a1)
{
    long op, n, j, k, ninst;
    const char *s;
    char *t;
    oop r;
    int c;
    if (!IS_INT(a1)) return 0;
    op = INTVAL(a1);
    if (op >= 1 && op <= 8) {
        if (!isstr(rcv)) return 0;
        s = strof(rcv);
        n = strlen_of(rcv);
        if (op == 6) return mkint(n);
        t = dupn(s, n);
        if (op == 5 || op == 8) {
            for (j = 0; j < n; j++) t[j] = (char)to_lower((unsigned char)t[j]);
        } else if (op == 7) {
            for (j = 0; j < n; j++) t[j] = (char)to_upper((unsigned char)t[j]);
        }
        if (kindof(rcv) == KD_SYM) r = mksym(t);
        else r = mkstrn(t, n);
        free(t);
        return r;
    }
    if (op == 9) {
        if (!isstr(rcv)) return 0;
        s = strof(rcv);
        n = strlen_of(rcv);
        r = mkarray(n);
        for (j = 0; j < n; j++) SLOT(r, j) = mkint((long)(unsigned char)s[j]);
        k = find_class("ByteArray");
        if (k > 0) objs[HI(r)].cls = (int)k;
        return r;
    }
    if (op == 10) {
        if (OK(rcv) != KD_ARR || IS_INT(rcv)) return 0;
        ninst = cls[OC(rcv)].ninst;
        n = ON(rcv) - ninst;
        if (n < 0) return 0;
        t = (char *)xmalloc((size_t)n + 1);
        for (j = 0; j < n; j++) {
            if (!IS_INT(SLOT(rcv, ninst + j))) {
                free(t);
                return 0;
            }
            c = (int)INTVAL(SLOT(rcv, ninst + j));
            if (c < 0 || c > 255) {
                free(t);
                return 0;
            }
            t[j] = (char)c;
        }
        r = mkstrn(t, n);
        free(t);
        return r;
    }
    return 0;
}

/* ---- the primitives ---------------------------------------------------- */
static oop prim_c(int p, oop rcv, long base, int na)
{
    oop a1, a2, v, r;
    long k, n, i, j, oldtop, id, b2, sv_t;
    int sv_u;
    oop sv_v;
    const char *s;
    long slen;
    char *t;
    char cb[48];

    a1 = ast[base + 1];
    a2 = ast[base + 2];

    switch (p) {
    case PR_BLOCKVALUE:
        if (IS_INT(rcv) || OK(rcv) != KD_BLK) return 0;
        if (NB(SYS_OX(rcv)) != na) {
            st_error(rcv, "wrong argument count");
            return 0;
        }
        return call_block(rcv, base, na);

    case PR_BLOCKVALUEARGS:
        if (IS_INT(rcv) || OK(rcv) != KD_BLK) return 0;
        if (IS_INT(a1) || OK(a1) != KD_ARR) return 0;
        k = cls[OC(a1)].ninst;
        n = ON(a1) - k;
        if (n < 0) return 0;
        if (NB(SYS_OX(rcv)) != n) {
            st_error(rcv, "wrong argument count");
            return 0;
        }
        b2 = asp;
        for (i = 0; i < n; i++) ast_push(SLOT(a1, k + i));
        r = call_block(rcv, b2, (int)n);
        asp = b2;
        return r;

    case PR_BLOCKNUMARGS:
        if (IS_INT(rcv) || OK(rcv) != KD_BLK) return 0;
        return mkint(NB(SYS_OX(rcv)));

    case PR_WHILETRUE:
    case PR_WHILEFALSE:
        if (IS_INT(rcv) || OK(rcv) != KD_BLK) return 0;
        for (;;) {
            v = call_block(rcv, asp, 0);
            if (unw > 0) return 0;
            if (p == PR_WHILETRUE && v != TRUE_OOP) break;
            if (p == PR_WHILEFALSE && v != FALSE_OOP) break;
            if (na >= 1) {
                v = call_block(a1, asp, 0);
                if (unw > 0) return 0;
            }
        }
        return NIL_OOP;

    case PR_ONDO:
        /* rcv on: exceptionClass do: handlerBlock */
        if (IS_INT(rcv) || OK(rcv) != KD_BLK) return 0;
        oldtop = htop;
        for (;;) {
            hid = hid + 1;
            id = hid;
            sys_hneed(oldtop + 2);
            htop = oldtop + 1;
            h_cls[htop] = a1;
            h_blk[htop] = a2;
            h_id[htop] = id;
            v = call_block(rcv, asp, 0);
            htop = oldtop;
            if (unw == UW_EXC && unw_t == id) {
                unw = UW_NONE;
                return unw_v;
            }
            if (unw == UW_RETRY && unw_t == id) {
                unw = UW_NONE;
                continue;
            }
            if (unw > 0) return 0;
            return v;
        }

    case PR_ENSURE:
    case PR_IFCURTAILED:
        if (IS_INT(rcv) || OK(rcv) != KD_BLK) return 0;
        v = call_block(rcv, asp, 0);
        if (p == PR_IFCURTAILED && unw == UW_NONE) return v;
        sv_u = unw;
        sv_t = unw_t;
        sv_v = unw_v;
        oldtop = htop;
        unw = UW_NONE;
        r = call_block(a1, asp, 0);
        htop = oldtop;
        if (unw == UW_NONE) {
            unw = sv_u;
            unw_t = sv_t;
            unw_v = sv_v;
        }
        if (unw > 0) return 0;
        return v;

    case PR_SIGNAL:
        return do_signal(rcv, htop, 1);

    case PR_RESUME:
        /* rcv resume: value */
        unw = UW_RESUME;
        unw_t = INTVAL(SLOT(rcv, 2));
        unw_v = a1;
        return 0;

    case PR_RETURN:
        k = INTVAL(SLOT(rcv, 1));
        if (k < 1 || k > htop + 50 || k >= hcap) {
            st_error(rcv, "return: outside of a handler");
            return 0;
        }
        unw = UW_EXC;
        unw_t = h_id[k];
        unw_v = a1;
        return 0;

    case PR_RETRY:
        k = INTVAL(SLOT(rcv, 1));
        if (k < 1 || k >= hcap) return 0;
        unw = UW_RETRY;
        unw_t = h_id[k];
        unw_v = NIL_OOP;
        return 0;

    case PR_PASS:
    case PR_OUTER:
        k = INTVAL(SLOT(rcv, 1));
        if (k < 1 || k >= hcap) return 0;
        v = do_signal(rcv, k - 1, 0);
        if (unw > 0) return 0;
        if (p == PR_PASS) {
            unw = UW_EXC;
            unw_t = h_id[k];
            unw_v = v;
            return 0;
        }
        return v;

    case PR_CLSNAME:
        if (IS_INT(rcv)) return 0;
        return mkstr(cls[SYS_OX(rcv)].name);

    case PR_CLSSUPER:
        if (IS_INT(rcv)) return 0;
        k = cls[SYS_OX(rcv)].super_;
        if (k == 0) return NIL_OOP;
        return cls[k].oop_;

    case PR_CLSSUBCLASS:
        if (IS_INT(rcv)) return 0;
        s = strof(a1);
        if (s[0] == '\0') return 0;
        return cls[define_class(s, (int)SYS_OX(rcv))].oop_;

    case PR_CLSADDIVAR:
        if (IS_INT(rcv)) return 0;
        s = strof(a1);
        if (s[0] == '\0') return 0;
        add_ivar((int)SYS_OX(rcv), s);
        return rcv;

    case PR_CLSIVARS:
        if (IS_INT(rcv)) return 0;
        return mkstr(cls[SYS_OX(rcv)].ivn != NULL ? cls[SYS_OX(rcv)].ivn : "");

    case PR_CLSSELECTORS:
        if (IS_INT(rcv)) return 0;
        return sys_selectors((int)SYS_OX(rcv));

    case PR_CLSINCLUDES:
        if (IS_INT(rcv)) return 0;
        s = strof(a1);
        if (s[0] == '\0') return 0;
        k = (long)SYS_OX(rcv);
        i = lookup((int)k, selid(s));
        return mkbool(i > 0 && meths[i].cls == (int)k);

    case PR_CLSADDCVAR:
        if (IS_INT(rcv)) return 0;
        s = strof(a1);
        if (s[0] == '\0') return 0;
        k = SYS_OX(rcv);
        if (cls[k].ismeta) k = cls[k].this_;
        declare_classvar((int)k, s);
        return rcv;

    case PR_CLSLOOKUP:
        if (IS_INT(rcv)) return 0;
        s = strof(a1);
        if (s[0] == '\0') return 0;
        return mkbool(lookup((int)SYS_OX(rcv), selid(s)) > 0);

    case PR_CLSSUBCLASSES:
        if (IS_INT(rcv)) return 0;
        k = SYS_OX(rcv);
        n = 0;
        for (i = 1; i <= nc; i++) {
            if (cls[i].super_ == (int)k && !cls[i].ismeta) n++;
        }
        r = mkarray(n);
        j = 0;
        for (i = 1; i <= nc && j < n; i++) {
            if (cls[i].super_ == (int)k && !cls[i].ismeta) {
                SLOT(r, j) = cls[i].oop_;
                j++;
            }
        }
        return r;

    case PR_GLOBALAT:
        s = strof(a1);
        if (s[0] == '\0' || !smap_get(&gmap, s, &k)) return 0;
        return g_val[gcell(s)];

    case PR_GLOBALATPUT:
        s = strof(a1);
        if (s[0] == '\0') return 0;
        gset(s, a2);
        return a2;

    case PR_GLOBALHAS:
        s = strof(a1);
        if (s[0] == '\0') return 0;
        return mkbool(smap_get(&gmap, s, &k));

    case PR_TRANSCRIPT:
        if (isstr(a1)) {
            outn(strof(a1), strlen_of(a1));
        } else if (IS_INT(a1)) {
            sprintf(cb, "%ld", INTVAL(a1));
            outs(cb);
        } else {
            return 0;
        }
        return rcv;

    case PR_STDINLINE:
        r = sys_stdin_line();
        if (r == 0) return NIL_OOP;
        return r;

    case PR_STDINATEND:
        return mkbool(stdin_eof != 0);

    case PR_MSECS:
        return sys_mkint_d(sys_now_ms());

    case PR_SECS:
        return sys_mkint_d((double)time(NULL));

    case PR_HALT:
        outs("halt\n");
        unw = UW_ABORT;
        return 0;

    case PR_EXIT:
        if (IS_INT(a1)) rc_exit = (int)INTVAL(a1);
        unw = UW_EXIT;
        return 0;

    case PR_ISNIL:
        return mkbool(rcv == NIL_OOP);

    case PR_DISPLAYSTRING:
        if (isstr(rcv)) return mkstrn(strof(rcv), strlen_of(rcv));
        if (numkind(rcv) > 0) return prim_a(PR_PRINTSTRING, rcv, base, 0);
        if (!IS_INT(rcv) && kindof(rcv) == KD_CHR) {
            cb[0] = (char)SYS_OX(rcv);
            return mkstrn(cb, 1L);
        }
        return 0;

    case PR_EVALSTRING:
        if (!isstr(a1)) return 0;
        return eval_string(strof(a1));

    case PR_BECOME:
        /* swap the contents of two heap objects: every reference sees the other */
        if (IS_INT(rcv) || IS_INT(a1)) return 0;
        if (rcv == a1) return rcv;
        {
            Obj tmp;
            tmp = objs[HI(rcv)];
            objs[HI(rcv)] = objs[HI(a1)];
            objs[HI(a1)] = tmp;
        }
        return rcv;

    case PR_ALLINST:
        if (IS_INT(rcv)) return 0;
        if (OK(rcv) != KD_CLS && OK(rcv) != KD_META) return 0;
        k = SYS_OX(rcv);
        j = no;
        n = 0;
        for (i = 1; i <= j; i++) {
            if (objs[i].cls == (int)k) n++;
        }
        r = mkarray(n);
        n = 0;
        for (i = 1; i <= j; i++) {
            if (objs[i].cls == (int)k) {
                SLOT(r, n) = i * 2;
                n++;
            }
        }
        return r;

    case PR_UNICODE:
        return sys_unicode(rcv, a1);

    case PR_FILEINSTR:
        if (!isstr(a1)) return 0;
        file_in(strof(a1), strlen_of(a1), "compile");
        return rcv;

    case PR_EXCINFO:
        /* a1: 1 = print trace and abort, 2 = print trace only */
        print_trace();
        if (a1 != MKINT(2L)) unw = UW_ABORT;
        return NIL_OOP;

    case PR_FORMAT:
        return 0;

    case PR_RANDOM:
        k = sys_rand_next();
        if (IS_INT(a1) && INTVAL(a1) > 0) return mkint(k % INTVAL(a1) + 1);
        return mkfloat((double)k / 2147483648.0);

    case PR_RANDSEED:
        if (!IS_INT(a1)) return 0;
        rnd_state = INTVAL(a1);
        if (rnd_state < 0) rnd_state = 0 - rnd_state;
        return rcv;

    case PR_GETENV:
        if (!isstr(a1)) return 0;
        t = getenv(strof(a1));
        return mkstr(t != NULL ? t : "");

    case PR_ARGCOUNT:
        return mkint(nsargs);

    case PR_ARGAT:
        /* sargs is 0-based: argument i is sargs[i - 1] */
        if (!IS_INT(a1)) return 0;
        k = INTVAL(a1);
        if (k < 1 || k > nsargs) return 0;
        return mkstr(sargs[k - 1]);

    case PR_FDELETE:
        if (!isstr(a1)) return 0;
        return mkbool(remove(strof(a1)) == 0);

    case PR_FRENAME:
        if (!isstr(a1) || !isstr(a2)) return 0;
        return mkbool(rename(strof(a1), strof(a2)) == 0);

    case PR_FCOPY:
        if (!isstr(a1) || !isstr(a2)) return 0;
        return mkbool(sys_copy(strof(a1), strof(a2)));

    case PR_FKIND:
        if (!isstr(a1)) return 0;
        return mkint(sys_kind(strof(a1)));

    case PR_FMKDIR:
        if (!isstr(a1)) return 0;
#ifdef ST_POSIX
        return mkbool(mkdir(strof(a1), 0777) == 0);
#else
        return 0;
#endif

    case PR_FRMDIR:
        if (!isstr(a1)) return 0;
#ifdef ST_POSIX
        return mkbool(rmdir(strof(a1)) == 0);
#else
        return 0;
#endif

    case PR_FCWD:
#ifdef ST_POSIX
        {
            char wd[4096];
            if (getcwd(wd, sizeof(wd)) == NULL) return mkstr(".");
            return mkstr(wd);
        }
#else
        return mkstr(".");
#endif

    case PR_FCHDIR:
        if (!isstr(a1)) return 0;
#ifdef ST_POSIX
        return mkbool(chdir(strof(a1)) == 0);
#else
        return 0;
#endif

    case PR_FSIZE:
        if (!isstr(a1)) return 0;
        k = sys_fsize(strof(a1));
        if (k < 0) return NIL_OOP;
        return mkint(k);

    case PR_FLIST:
        if (!isstr(a1)) return 0;
#ifdef ST_POSIX
        {
            DIR *d;
            struct dirent *de;
            d = opendir(strof(a1));
            if (d == NULL) return NIL_OOP;
            n = 0;
            while ((de = readdir(d)) != NULL) {
                if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) n++;
            }
            rewinddir(d);
            r = mkarray(n);
            j = 0;
            while (j < n && (de = readdir(d)) != NULL) {
                if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
                    {
                        oop sv_ = mkstr(de->d_name);
                        SLOT(r, j) = sv_;
                    }
                    j++;
                }
            }
            closedir(d);
            return r;
        }
#else
        return 0;
#endif

    case PR_SYSCMD:
        if (!isstr(a1)) return 0;
        return sys_command(strof(a1));

    case PR_SYSRC:
        return mkint((long)sys_rc);

    case PR_SYSERR:
        return mkstr(sys_err != NULL ? sys_err : "");

    case PR_FILEREAD:
        if (!isstr(a1)) return 0;
        t = sys_slurp(strof(a1), &slen);
        if (t == NULL) return NIL_OOP;
        r = mkstrn(t, slen);
        free(t);
        return r;

    case PR_FILEWRITE:
        /* fileWrite: path text: string append: boolean */
        if (!isstr(a1) || !isstr(a2)) return 0;
        return mkbool(sys_write(strof(a1), strof(a2), strlen_of(a2), ast[base + 3] == TRUE_OOP));

    case PR_DATESTR:
    case PR_TIMESTR:
        {
            time_t tt;
            struct tm *tmv;
            char buf[32];
            tt = time(NULL);
            tmv = localtime(&tt);
            if (tmv == NULL) return 0;
            strftime(buf, sizeof(buf), p == PR_DATESTR ? "%Y%m%d" : "%H:%M:%S", tmv);
            return mkstr(buf);
        }

    case PR_FLOATPRINT:
    case PR_BIGOP:
    default:
        return 0;
    }
    return 0;
}

/* ---- sockets: not available in the C port ------------------------------
   EXTENSION POINT.  All socket primitives (PR_SOCKNEW .. PR_SOCKSHUT, see
   src/prims.txt and lib/net.st) are routed here by prim().  To add
   networking implement them in this function only:
     - POSIX build (-DST_POSIX): socket/connect/bind/listen/accept/send/recv/
       close from <sys/socket.h>; a handle is a small integer (fd).
     - VM/370: the TCP/IP socket API of the target stack, same primitives.
   Until then Socket class>>primNew reports a clear error; the other
   primitives answer 0 (failure), so net.st falls back to its -1 / '' values. */
static oop prim_net(int p, oop rcv, long base, int na)
{
    (void)rcv;
    (void)base;
    (void)na;
    if (p == PR_SOCKNEW)
        st_error(NIL_OOP, "networking is not available in this build (C port)");
    return 0;
}
/* ===== s6_main.c ===== */
/* =========================================================================
   s6_main.c   initialisation, file reading, the interactive prompt, main
   ========================================================================= */
static char *read_file(const char *fn, long *len)
{
    FILE *f = fopen(fn, "rb");
    char *buf;
    long n = 0, cap = 0, got;
    if (f == NULL) return NULL;
    buf = NULL;
    for (;;) {
        if (n + 4096 + 1 > cap) buf = (char *)xgrow(buf, &cap, n + 4096 + 1, 1);
        got = (long)fread(buf + n, 1, 4096, f);
        if (got <= 0) break;
        n += got;
    }
    fclose(f);
    if (buf == NULL) buf = (char *)xmalloc(1);
    buf[n] = '\0';
    *len = n;
    return buf;
}

static void init_state(void)
{
    objcap = 0; slcap = 0; nodecap = 0; alcap = 0; vpcap = 0; astcap = 0;
    objs = NULL; sl = NULL; nodes = NULL; al = NULL; vp = NULL; ast = NULL;
    GROW(Obj, objs, objcap, 1);
    GROW(oop, sl, slcap, 1);
    GROW(Node, nodes, nodecap, 1);
    GROW(long, al, alcap, 1);
    GROW(oop, vp, vpcap, 4);
    GROW(oop, ast, astcap, 64);
    no = 0; nsl = 0; nnode = 0; nal = 0; nvp = 1; asp = 0;
    cs_m = (long *)xmalloc(sizeof(long) * (CDEPTH_MAX + 2));
    cs_r = (oop *)xmalloc(sizeof(oop) * (CDEPTH_MAX + 2));
    cdepth = 0;
    nc = 0; nmeth = 0; ng = 0; nsel = 0;
    gen = 1;
    unw = 0; htop = 0; hid = 0; sigcount = 0;
    nd0 = 0; ne = 0;
    smap_init(&symtab); smap_init(&cmap); smap_init(&cvmap);
    smap_init(&gmap); smap_init(&selmap); smap_init(&dmap);
    mt_init();
    GROW(Cls, cls, clscap, 1);
    GROW(Meth, meths, methcap, 1);
    g_ensure(1);
    sel_name = (char **)xgrow(sel_name, &selcap, 1, sizeof(char *));
    sel_n = (int *)xrealloc(sel_n, sizeof(int) * (size_t)selcap);
    fl_need(64);
    sc_need(16);
    marks = NULL; markcap = 0; nmk = 0;
    GROW(Mark, marks, markcap, 1);
    memset(marks, 0, sizeof(Mark) * (size_t)markcap);
    nl_name = NULL; nlcap = 0; nnl = 0;
    GROW(char *, nl_name, nlcap, 1);
    env_name = NULL; envcap = 0; envn = 0;
    env_name = (char **)xgrow(env_name, &envcap, 1, sizeof(char *));
    env_slot = (long *)xrealloc(env_slot, sizeof(long) * (size_t)envcap);
    env_lvl = (long *)xrealloc(env_lvl, sizeof(long) * (size_t)envcap);
    rnd_state = 12345;
    sargs = NULL;
    sys_err = dups("");
}

static int bracket_depth(const char *t)
{
    int d = 0, inq = 0, inc = 0;
    const char *p = t;
    while (*p) {
        char ch = *p;
        if (inq) {
            if (ch == '\'') inq = 0;
        } else if (inc) {
            if (ch == '"') inc = 0;
        } else {
            if (ch == '\'') inq = 1;
            else if (ch == '"') inc = 1;
            else if (ch == '$') { if (p[1]) p++; }
            else if (ch == '[' || ch == '(' || ch == '{') d++;
            else if (ch == ']' || ch == ')' || ch == '}') d--;
        }
        p++;
    }
    if (inq || inc) d++;
    return d;
}

static char *read_line_stdin(void)
{
    SB b;
    int c;
    sb_init(&b);
    for (;;) {
        c = getc(stdin);
        if (c == EOF) {
            if (b.n == 0) {
                sb_free(&b);
                return NULL;
            }
            break;
        }
        if (c == '\n') break;
        sb_addc(&b, c);
    }
    return sb_str(&b);
}

static void repl_loop(void)
{
    SB buf;
    char *line, *cmd;
    sb_init(&buf);
    outs("Smalltalk - type exit to leave\n");
    for (;;) {
        if (halted == 1) break;
        outs(buf.n == 0 ? "st> " : "... ");
        flush_out();
        line = read_line_stdin();
        if (line == NULL) break;
        cmd = line;
        while (*cmd == ' ' || *cmd == '\t') cmd++;
        if (buf.n == 0 && (strncmp(cmd, "exit", 4) == 0 || strncmp(cmd, "quit", 4) == 0 || strncmp(cmd, "bye", 3) == 0)) {
            char *e = cmd + strlen(cmd);
            while (e > cmd && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
            if ((e - cmd == 4 && (strncmp(cmd, "exit", 4) == 0 || strncmp(cmd, "quit", 4) == 0)) || (e - cmd == 3 && strncmp(cmd, "bye", 3) == 0)) {
                free(line);
                break;
            }
        }
        sb_adds(&buf, line);
        sb_addc(&buf, '\n');
        free(line);
        if (bracket_depth(sb_str(&buf)) <= 0) {
            repl_echo = 1;
            file_in(buf.p, buf.n, "stdin");
            repl_echo = 0;
            sb_free(&buf);
            sb_init(&buf);
        }
    }
    outs("\n");
    sb_free(&buf);
}

static const char *const lib_files[] = {
    "kernel.st", "numbers.st", "collections.st", "streams.st", "exceptions.st",
    "extras.st", "structures.st", "regex.st", "system.st", "net.st", "sunit.st",
    "process.st", "more.st", "more2.st", 0
};

static char *join_path(const char *dir, const char *f)
{
    char *p = (char *)xmalloc(strlen(dir) + strlen(f) + 2);
    sprintf(p, "%s/%s", dir, f);
    return p;
}

int main(int argc, char **argv)
{
    int ia, nfiles = 0, i;
    char **files;
    long len;
    char *text, *path;
    files = (char **)xmalloc(sizeof(char *) * (size_t)(argc + 1));
    init_state();
    libdir = NULL;
    for (ia = 1; ia < argc; ia++) {
        if (strcmp(argv[ia], "-L") == 0 && ia + 1 < argc) {
            ia++;
            libdir = argv[ia];
        } else if (strcmp(argv[ia], "-h") == 0 || strcmp(argv[ia], "--help") == 0) {
            printf("usage: st [-L libdir] [-i] [file.st ...] [-- arg ...]\n");
            return 0;
        } else if (strcmp(argv[ia], "-i") == 0) {
            repl_mode = 1;
        } else if (strcmp(argv[ia], "--") == 0) {
            sargs = (char **)xmalloc(sizeof(char *) * (size_t)(argc + 1));
            for (ia = ia + 1; ia < argc; ia++) sargs[nsargs++] = argv[ia];
        } else {
            files[nfiles++] = argv[ia];
        }
    }
    if (libdir == NULL) {
        libdir = getenv("SMALLTALK_LIB");
        if (libdir == NULL) libdir = "lib";
    }
    boot();
    for (i = 0; lib_files[i] != 0; i++) {
        path = join_path(libdir, lib_files[i]);
        text = read_file(path, &len);
        if (text == NULL && i == 0) {
            free(path);
            libdir = "../lib";
            path = join_path(libdir, lib_files[i]);
            text = read_file(path, &len);
        }
        if (text == NULL) {
            fprintf(stderr, "smalltalk: cannot read library file %s\n", path);
            return 2;
        }
        file_in(text, len, lib_files[i]);
        free(text);
        free(path);
    }
    cls_Message = find_class("Message");
    booted = 1;
    if (nfiles == 0 && repl_mode == 0) {
        SB all;
        int c;
        sb_init(&all);
        while ((c = getc(stdin)) != EOF) sb_addc(&all, c);
        file_in(sb_str(&all), all.n, "stdin");
        sb_free(&all);
    } else {
        for (i = 0; i < nfiles && !halted; i++) {
            text = read_file(files[i], &len);
            if (text == NULL) {
                outs("smalltalk: cannot read ");
                outs(files[i]);
                outs("\n");
                rc_exit = 1;
            } else {
                file_in(text, len, files[i]);
                free(text);
            }
        }
    }
    if (repl_mode == 1 && halted == 0) repl_loop();
    flush_out();
    return rc_exit;
}
