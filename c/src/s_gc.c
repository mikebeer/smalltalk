/* =========================================================================
   s_gc.c   precise mark-and-sweep garbage collector

   Heap objects are entries of the object table objs[]; an oop is the index
   (times two), so objects never move and the table is swept in place.  A
   collection runs only at a safe point (GC_POINT: method and block entry,
   loop iterations), when every live reference is in one of
     - the globals, class table, symbols, literals of the syntax tree,
     - the argument stack ast[] (receivers and arguments of sends in
       progress are kept there), the call stack, the handler stack,
     - the frames being executed (act_f[]) and the frames reachable from
       blocks (their slots, outer and home frames).
   C locals of primitives are never roots: a primitive allocates without
   collecting; only the callbacks it makes (blocks, sends) are safe points
   and the few callers that hold an oop across one push it on ast[].

   mark:  explicit stack (no recursion), object and frame mark bytes
   sweep: free objects go to a list chained through .x, their strings are
          freed; dead dictionary entries are recycled; frames are rebuilt
          into the free lists; the slot array sl[] is compacted when more
          than half of it is dead.
   The trigger is the volume allocated since the last collection, compared
   with max(GC_MIN, live volume after the last one).
   ========================================================================= */
#define GC_MIN 4000000.0

static unsigned char *gm;     /* object mark bytes */
static unsigned char *gfm;    /* frame mark bytes, indexed by frame start */
static long *gstk;            /* work stack: > 0 object index, < 0 frame start */
static long gsp, gscap;

static void gpush(long v)
{
    gsp++;
    if (gsp >= gscap) gstk = (long *)xgrow(gstk, &gscap, gsp, sizeof(long));
    gstk[gsp] = v;
}

static void gmark(oop x)
{
    long i;
    if (IS_INT(x) || x <= 0) return;
    i = HI(x);
    if (i < 1 || i > no) return;
    if (objs[i].kind == KD_FREE || gm[i]) return;
    gm[i] = 1;
    gpush(i);
}

static void gmark_frame(long f)
{
    if (f < 2 || f > nvp || gfm[f]) return;
    gfm[f] = 1;
    gpush(-f);
}

static void gdrain(void)
{
    long t, i, j, e, d, prev, nx, f, sz;
    while (gsp > 0) {
        t = gstk[gsp];
        gsp--;
        if (t < 0) {
            f = -t;
            sz = vsz[f];
            gmark(vp[f + 2]);
            for (j = 4; j < sz; j++) gmark(vp[f + j]);
            gmark_frame(vp[f]);
            gmark_frame(vp[f + 1]);
            continue;
        }
        i = t;
        for (j = 0; j < objs[i].n; j++) gmark(sl[objs[i].slot + j]);
        if (objs[i].kind == KD_BLK) {
            gmark_frame(objs[i].slot);
        } else if (objs[i].kind == KD_DICT && objs[i].x > 0) {
            d = objs[i].x;
            e = dd_head[d];
            prev = 0;
            while (e > 0) {
                nx = de_next[e];
                if (de_live[e] == 1) {
                    gmark(de_key[e]);
                    gmark(de_val[e]);
                    prev = e;
                } else {
                    /* removed entry: unlink and recycle */
                    if (prev == 0) dd_head[d] = nx;
                    else de_next[prev] = nx;
                    if (dd_tail[d] == e) dd_tail[d] = prev;
                    de_next[e] = de_free;
                    de_free = e;
                }
                e = nx;
            }
        }
    }
}

/* a dictionary that died: forget its entries in the lookup map and recycle them */
static void gc_release_dict(long d)
{
    long e, nx;
    char *k;
    e = dd_head[d];
    while (e > 0) {
        nx = de_next[e];
        if (de_live[e] == 1) {
            k = dict_ckey(d, de_key[e]);
            smap_del(&dmap, k);
            free(k);
        }
        de_live[e] = 0;
        de_next[e] = de_free;
        de_free = e;
        e = nx;
    }
    dd_head[d] = dd_free;
    dd_tail[d] = 0;
    dd_n[d] = 0;
    dd_free = d;
}

static void gc_compact_slots(void)
{
    oop *ns;
    long i, j, k, live, cap;
    live = 0;
    for (i = 1; i <= no; i++) {
        if (objs[i].kind != KD_FREE && objs[i].n > 0) live += objs[i].n;
    }
    cap = live + 64;
    ns = (oop *)xmalloc((size_t)cap * sizeof(oop));
    ns[0] = 0;
    k = 0;
    for (i = 1; i <= no; i++) {
        if (objs[i].kind == KD_FREE || objs[i].n <= 0) continue;
        j = objs[i].slot;
        memcpy(ns + k + 1, sl + j, (size_t)objs[i].n * sizeof(oop));
        objs[i].slot = k + 1;
        k += objs[i].n;
    }
    free(sl);
    sl = ns;
    slcap = cap;
    nsl = k;
}

static void gc_run(void)
{
    long i, f, sz, deadslots, liveobj;
    double livebytes;

    gc_count++;
    gm = (unsigned char *)xmalloc((size_t)no + 2);
    memset(gm, 0, (size_t)no + 2);
    gfm = (unsigned char *)xmalloc((size_t)nvp + 2);
    memset(gfm, 0, (size_t)nvp + 2);
    gsp = 0;

    /* roots */
    gmark(NIL_OOP);
    gmark(TRUE_OOP);
    gmark(FALSE_OOP);
    for (i = 1; i <= ng; i++) gmark(g_val[i]);
    for (i = 0; i < 256; i++) gmark(chr_oop[i]);
    for (i = 1; i <= nc; i++) gmark(cls[i].oop_);
    for (i = 1; i <= no; i++) {
        int k = objs[i].kind;
        if (k == KD_SYM || k == KD_CLS || k == KD_META || k == KD_CHR) gmark(i * 2L);
    }
    for (i = 1; i <= nnode; i++) {
        if (nodes[i].t == ND_LIT) gmark(nodes[i].a);
    }
    for (i = 1; i <= asp; i++) gmark(ast[i]);
    for (i = 1; i <= cdepth; i++) gmark(cs_r[i]);
    gmark(unw_v);
    for (i = 1; i <= hmax; i++) {
        gmark(h_cls[i]);
        gmark(h_blk[i]);
    }
    for (i = 0; i < nact; i++) gmark_frame(act_f[i]);
    gdrain();

    /* dead dictionaries, then the objects */
    for (i = 1; i <= no; i++) {
        if (!gm[i] && objs[i].kind == KD_DICT && objs[i].x > 0) gc_release_dict(objs[i].x);
    }
    deadslots = 0;
    liveobj = 0;
    livebytes = 0.0;
    ofree = 0;
    for (i = no; i >= 1; i--) {
        if (objs[i].kind == KD_FREE) {
            objs[i].x = ofree;
            ofree = i;
            deadslots += 0;
            continue;
        }
        if (gm[i]) {
            liveobj++;
            livebytes += (double)sizeof(Obj) + 16.0 + (double)objs[i].n * (double)sizeof(oop) + (double)objs[i].slen;
            continue;
        }
        if (objs[i].s != NULL) {
            free(objs[i].s);
            objs[i].s = NULL;
        }
        deadslots += objs[i].n;
        gc_freed++;
        objs[i].kind = KD_FREE;
        objs[i].cls = 0;
        objs[i].n = 0;
        objs[i].slen = 0;
        objs[i].x = ofree;
        ofree = i;
    }

    /* frames: everything not marked goes back to the free lists */
    for (i = 0; i < flcap; i++) fl_head[i] = 0;
    for (f = 2; f <= nvp; f += sz) {
        sz = vsz[f];
        if (!gfm[f]) {
            vp[f] = fl_head[sz];
            fl_head[sz] = f;
        } else {
            livebytes += (double)sz * (double)sizeof(oop);
        }
    }

    /* slots: compact when the dead part is large (always under gcstress) */
    if (deadslots * 2 > nsl || (gc_stress > 0 && deadslots > 0)) gc_compact_slots();

    free(gm);
    free(gfm);
    gm = NULL;
    gfm = NULL;
    gc_alloc = 0.0;
    gc_next = livebytes > GC_MIN ? livebytes : GC_MIN;
    if (gc_stress > 0) gc_next = 0.0;
    if (gc_verbose) {
        fprintf(stderr, "gc %ld: %ld live objects, %.0f live bytes, next at %.0f\n", gc_count, liveobj, livebytes, gc_next);
    }
}

static void gc_point(void)
{
    if (!gc_on) return;
    if (gc_stress > 0) {
        gc_tick++;
        if (gc_tick < gc_stress) return;
        gc_tick = 0;
    }
    gc_run();
}

static void gc_init(void)
{
    gc_alloc = 0.0;
    gc_next = GC_MIN;
    if (!gc_on) gc_next = 1.0e300;
    if (gc_stress > 0) gc_next = 0.0;
}
