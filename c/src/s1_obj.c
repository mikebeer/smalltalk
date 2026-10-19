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
    m->cap = (m->cnt * 2 > oc) ? oc * 2 : oc;   /* only rehash when mostly tombstones */
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
    long i, ix;
    if (ofree > 0) {
        ix = ofree;
        ofree = objs[ix].x;
    } else {
        no++;
        GROW(Obj, objs, objcap, no);
        ix = no;
    }
    gc_alloc += (double)sizeof(Obj) + 16.0 + (double)n * (double)sizeof(oop);
    objs[ix].cls = cls_;
    objs[ix].kind = kind;
    objs[ix].n = n;
    objs[ix].x = 0;
    objs[ix].s = NULL;
    objs[ix].slen = 0;
    objs[ix].f = 0.0;
    objs[ix].slot = nsl + 1;
    if (n > 0) {
        GROW(oop, sl, slcap, nsl + n);
        for (i = 1; i <= n; i++) {
            nsl++;
            sl[nsl] = NIL_OOP;
        }
    }
    return ix * 2L;
}

static void set_str(oop o, const char *s, long n)
{
    char *p = dupn(s, n);
    if (objs[HI(o)].s != NULL) free(objs[HI(o)].s);
    objs[HI(o)].s = p;
    objs[HI(o)].slen = n;
    gc_alloc += (double)n + 16.0;
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
            gc_alloc += (double)n + 16.0;
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
