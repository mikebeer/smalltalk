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
    act_push(fb);
    v = ev(seq, fb);
    nact--;
    return v;
}

/* run a top-level statement (a raw node) */
static void run_doit(long st)
{
    oop v, er;
    long eb, ab0;
    cdepth = 0;
    unw = UW_NONE;
    htop = 0;
    ab0 = asp;
    v = run_seq_in(cls_UndefinedObject, st, NULL);
    ast_push(v);   /* the value stays a collector root while the scheduler runs */
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
    asp = ab0;
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
        act_push(fb);
        v = ev(seq, fb);
        nact--;
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
