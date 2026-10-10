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
