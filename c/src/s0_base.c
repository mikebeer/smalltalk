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
#define CDEPTH_MAX 20000
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
