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
