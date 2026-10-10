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
