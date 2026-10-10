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
