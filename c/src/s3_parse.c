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
