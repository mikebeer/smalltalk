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

/* a new frame at the end of vp; its size is recorded for the collector */
static long frame_new(long size)
{
    long fb = nvp + 1;
    nvp += size;
    GROW(oop, vp, vpcap, nvp + 1);
    GROW(int, vsz, vszcap, nvp + 1);
    vsz[fb] = (int)size;
    return fb;
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
    return frame_new(size);
}

/* frames being executed are collector roots */
static void act_push(long fb)
{
    nact++;
    GROW(long, act_f, actcap, nact);
    act_f[nact - 1] = fb;
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
        nf = frame_new(size);
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
        GROW(oop, ast, astcap, base + 8);   /* primitives read ast[base+1..base+3] unconditionally */
        r = prim_fn(p, rcv, base, na);
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
    act_push(nf);
    GC_POINT();
    r = ev(meths[m].body, nf);
    nact--;
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
    act_push(bf);
    GC_POINT();
    r = ev(NA(node), bf);
    nact--;
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
    long x, y, s, slot, base, vsl;
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
                GC_POINT();
                vp[slot] = mkint(x);
                v = ev(NC(n), fb);
                if (unw > 0) return 0;
                x += s;
            }
        } else {
            while (x >= y) {
                GC_POINT();
                vp[slot] = mkint(x);
                v = ev(NC(n), fb);
                if (unw > 0) return 0;
                x += s;
            }
        }
        return a;
    }
    /* general numbers: use sends; the operands are kept on the argument stack */
    ast_push(a);
    ast_push(b);
    ast_push(st);
    ast_push(a);
    vsl = asp;
    base = asp;
    v = a;
    for (;;) {
        asp = base;
        ast[vsl] = v;
        GC_POINT();
        ast_push(b);
        if (!is_neg(st)) r = send_sel(v, S_LE, base, 1);
        else r = send_sel(v, S_GE, base, 1);
        asp = base;
        if (unw > 0) {
            asp = base - 4;
            return 0;
        }
        if (r != TRUE_OOP) break;
        vp[slot] = v;
        r = ev(NC(n), fb);
        if (unw > 0) {
            asp = base - 4;
            return 0;
        }
        asp = base;
        ast_push(st);
        v = send_sel(v, S_ADD, base, 1);
        asp = base;
        if (unw > 0) {
            asp = base - 4;
            return 0;
        }
    }
    asp = base - 4;
    return a;
}

static oop do_cascade(long n, long fb)
{
    oop rcv, v, r;
    long i, k, j, base, cb0;
    EVAL(rcv, NA(n), fb);
    if (unw > 0) return 0;
    ast_push(rcv);
    cb0 = asp;
    r = NIL_OOP;
    for (i = NB(n); i < NB(n) + NC(n); i++) {
        k = al[i];
        base = asp;
        for (j = 0; j < NI(k); j++) {
            EVAL(v, al[NB(k) + j], fb);
            if (unw > 0) {
                asp = cb0 - 1;
                return 0;
            }
            ast_push(v);
        }
        r = send_sel(rcv, (int)NC(k), base, (int)NI(k));
        asp = base;
        if (unw > 0) {
            asp = cb0 - 1;
            return 0;
        }
    }
    asp = cb0 - 1;
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
            GC_POINT();
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
            GC_POINT();
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
            GC_POINT();
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
        ast_push(v);
        for (i = 0; i < c; i++) {
            oop e;
            e = ev(al[NA(n) + i], fb);
            if (unw > 0) {
                asp--;
                return 0;
            }
            sl[objs[HI(v)].slot + i] = e;
        }
        asp--;
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
    /* the receiver stays on the argument stack (a collector root) during the send */
    asp++;
    if (asp >= astcap) ast = (oop *)xgrow(ast, &astcap, asp, sizeof(oop));
    ast[asp] = rcv;
    base = asp;
    f = nodes[n].b;
    for (i = 0; i < na; i++) {
        EVAL(v, al[f + i], fb);
        if (unw > 0) {
            asp = base - 1;
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
                if (r >= SI_MIN && r <= SI_MAX) { asp = base - 1; return MKINT(r); }
                break;
            case S_SUB:
                r = x - y;
                if (r >= SI_MIN && r <= SI_MAX) { asp = base - 1; return MKINT(r); }
                break;
            case S_MUL:
                if (x > -SI_MULLIM && x < SI_MULLIM && y > -SI_MULLIM && y < SI_MULLIM) {
                    asp = base - 1;
                    return MKINT(x * y);
                }
                break;
            case S_LT: asp = base - 1; return x < y ? TRUE_OOP : FALSE_OOP;
            case S_GT: asp = base - 1; return x > y ? TRUE_OOP : FALSE_OOP;
            case S_LE: asp = base - 1; return x <= y ? TRUE_OOP : FALSE_OOP;
            case S_GE: asp = base - 1; return x >= y ? TRUE_OOP : FALSE_OOP;
            case S_EQ: asp = base - 1; return x == y ? TRUE_OOP : FALSE_OOP;
            case S_NE: asp = base - 1; return x != y ? TRUE_OOP : FALSE_OOP;
            case S_IDIV:
                if (y != 0) {
                    long q, rm;
                    floor_divmod(x, y, &q, &rm);
                    asp = base - 1;
                    return MKINT(q);
                }
                break;
            case S_MOD:
                if (y != 0) {
                    long q, rm;
                    floor_divmod(x, y, &q, &rm);
                    asp = base - 1;
                    return MKINT(rm);
                }
                break;
            default:
                break;
            }
        }
        if (sel == S_IDENT) {
            asp = base - 1;
            return rcv == v ? TRUE_OOP : FALSE_OOP;
        }
        if (sel == S_NIDENT) {
            asp = base - 1;
            return rcv == v ? FALSE_OOP : TRUE_OOP;
        }
    }
    if (c == 0) c = cof(rcv);
    m = lookup(c, sel);
    if (m == 0) v = send_dnu(rcv, sel, base, (int)na);
    else v = invoke(m, rcv, base, (int)na);
    asp = base - 1;
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
