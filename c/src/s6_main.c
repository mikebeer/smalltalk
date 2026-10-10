/* =========================================================================
   s6_main.c   initialisation, file reading, the interactive prompt, main
   ========================================================================= */
static char *read_file(const char *fn, long *len)
{
    FILE *f = fopen(fn, "rb");
    char *buf;
    long n = 0, cap = 0, got;
    if (f == NULL) return NULL;
    buf = NULL;
    for (;;) {
        if (n + 4096 + 1 > cap) buf = (char *)xgrow(buf, &cap, n + 4096 + 1, 1);
        got = (long)fread(buf + n, 1, 4096, f);
        if (got <= 0) break;
        n += got;
    }
    fclose(f);
    if (buf == NULL) buf = (char *)xmalloc(1);
    buf[n] = '\0';
    *len = n;
    return buf;
}

static void init_state(void)
{
    objcap = 0; slcap = 0; nodecap = 0; alcap = 0; vpcap = 0; astcap = 0;
    objs = NULL; sl = NULL; nodes = NULL; al = NULL; vp = NULL; ast = NULL;
    GROW(Obj, objs, objcap, 1);
    GROW(oop, sl, slcap, 1);
    GROW(Node, nodes, nodecap, 1);
    GROW(long, al, alcap, 1);
    GROW(oop, vp, vpcap, 4);
    GROW(oop, ast, astcap, 64);
    no = 0; nsl = 0; nnode = 0; nal = 0; nvp = 1; asp = 0;
    cs_m = (long *)xmalloc(sizeof(long) * (CDEPTH_MAX + 2));
    cs_r = (oop *)xmalloc(sizeof(oop) * (CDEPTH_MAX + 2));
    cdepth = 0;
    nc = 0; nmeth = 0; ng = 0; nsel = 0;
    gen = 1;
    unw = 0; htop = 0; hid = 0; sigcount = 0;
    nd0 = 0; ne = 0;
    smap_init(&symtab); smap_init(&cmap); smap_init(&cvmap);
    smap_init(&gmap); smap_init(&selmap); smap_init(&dmap);
    mt_init();
    GROW(Cls, cls, clscap, 1);
    GROW(Meth, meths, methcap, 1);
    g_ensure(1);
    sel_name = (char **)xgrow(sel_name, &selcap, 1, sizeof(char *));
    sel_n = (int *)xrealloc(sel_n, sizeof(int) * (size_t)selcap);
    fl_need(64);
    sc_need(16);
    marks = NULL; markcap = 0; nmk = 0;
    GROW(Mark, marks, markcap, 1);
    memset(marks, 0, sizeof(Mark) * (size_t)markcap);
    nl_name = NULL; nlcap = 0; nnl = 0;
    GROW(char *, nl_name, nlcap, 1);
    env_name = NULL; envcap = 0; envn = 0;
    env_name = (char **)xgrow(env_name, &envcap, 1, sizeof(char *));
    env_slot = (long *)xrealloc(env_slot, sizeof(long) * (size_t)envcap);
    env_lvl = (long *)xrealloc(env_lvl, sizeof(long) * (size_t)envcap);
    rnd_state = 12345;
    sargs = NULL;
    sys_err = dups("");
}

static int bracket_depth(const char *t)
{
    int d = 0, inq = 0, inc = 0;
    const char *p = t;
    while (*p) {
        char ch = *p;
        if (inq) {
            if (ch == '\'') inq = 0;
        } else if (inc) {
            if (ch == '"') inc = 0;
        } else {
            if (ch == '\'') inq = 1;
            else if (ch == '"') inc = 1;
            else if (ch == '$') { if (p[1]) p++; }
            else if (ch == '[' || ch == '(' || ch == '{') d++;
            else if (ch == ']' || ch == ')' || ch == '}') d--;
        }
        p++;
    }
    if (inq || inc) d++;
    return d;
}

static char *read_line_stdin(void)
{
    SB b;
    int c;
    sb_init(&b);
    for (;;) {
        c = getc(stdin);
        if (c == EOF) {
            if (b.n == 0) {
                sb_free(&b);
                return NULL;
            }
            break;
        }
        if (c == '\n') break;
        sb_addc(&b, c);
    }
    return sb_str(&b);
}

static void repl_loop(void)
{
    SB buf;
    char *line, *cmd;
    sb_init(&buf);
    outs("Smalltalk - type exit to leave\n");
    for (;;) {
        if (halted == 1) break;
        outs(buf.n == 0 ? "st> " : "... ");
        flush_out();
        line = read_line_stdin();
        if (line == NULL) break;
        cmd = line;
        while (*cmd == ' ' || *cmd == '\t') cmd++;
        if (buf.n == 0 && (strncmp(cmd, "exit", 4) == 0 || strncmp(cmd, "quit", 4) == 0 || strncmp(cmd, "bye", 3) == 0)) {
            char *e = cmd + strlen(cmd);
            while (e > cmd && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
            if ((e - cmd == 4 && (strncmp(cmd, "exit", 4) == 0 || strncmp(cmd, "quit", 4) == 0)) || (e - cmd == 3 && strncmp(cmd, "bye", 3) == 0)) {
                free(line);
                break;
            }
        }
        sb_adds(&buf, line);
        sb_addc(&buf, '\n');
        free(line);
        if (bracket_depth(sb_str(&buf)) <= 0) {
            repl_echo = 1;
            file_in(buf.p, buf.n, "stdin");
            repl_echo = 0;
            sb_free(&buf);
            sb_init(&buf);
        }
    }
    outs("\n");
    sb_free(&buf);
}

static const char *const lib_files[] = {
    "kernel.st", "numbers.st", "collections.st", "streams.st", "exceptions.st",
    "extras.st", "structures.st", "regex.st", "system.st", "net.st", "sunit.st",
    "process.st", "more.st", "more2.st", 0
};

static char *join_path(const char *dir, const char *f)
{
    char *p = (char *)xmalloc(strlen(dir) + strlen(f) + 2);
    sprintf(p, "%s/%s", dir, f);
    return p;
}

int main(int argc, char **argv)
{
    int ia, nfiles = 0, i;
    char **files;
    long len;
    char *text, *path;
    files = (char **)xmalloc(sizeof(char *) * (size_t)(argc + 1));
    init_state();
    libdir = NULL;
    for (ia = 1; ia < argc; ia++) {
        if (strcmp(argv[ia], "-L") == 0 && ia + 1 < argc) {
            ia++;
            libdir = argv[ia];
        } else if (strcmp(argv[ia], "-h") == 0 || strcmp(argv[ia], "--help") == 0) {
            printf("usage: st [-L libdir] [-i] [file.st ...] [-- arg ...]\n");
            return 0;
        } else if (strcmp(argv[ia], "-i") == 0) {
            repl_mode = 1;
        } else if (strcmp(argv[ia], "--") == 0) {
            sargs = (char **)xmalloc(sizeof(char *) * (size_t)(argc + 1));
            for (ia = ia + 1; ia < argc; ia++) sargs[nsargs++] = argv[ia];
        } else {
            files[nfiles++] = argv[ia];
        }
    }
    if (libdir == NULL) {
        libdir = getenv("SMALLTALK_LIB");
        if (libdir == NULL) libdir = "lib";
    }
    boot();
    for (i = 0; lib_files[i] != 0; i++) {
        path = join_path(libdir, lib_files[i]);
        text = read_file(path, &len);
        if (text == NULL && i == 0) {
            free(path);
            libdir = "../lib";
            path = join_path(libdir, lib_files[i]);
            text = read_file(path, &len);
        }
        if (text == NULL) {
            fprintf(stderr, "smalltalk: cannot read library file %s\n", path);
            return 2;
        }
        file_in(text, len, lib_files[i]);
        free(text);
        free(path);
    }
    cls_Message = find_class("Message");
    booted = 1;
    if (nfiles == 0 && repl_mode == 0) {
        SB all;
        int c;
        sb_init(&all);
        while ((c = getc(stdin)) != EOF) sb_addc(&all, c);
        file_in(sb_str(&all), all.n, "stdin");
        sb_free(&all);
    } else {
        for (i = 0; i < nfiles && !halted; i++) {
            text = read_file(files[i], &len);
            if (text == NULL) {
                outs("smalltalk: cannot read ");
                outs(files[i]);
                outs("\n");
                rc_exit = 1;
            } else {
                file_in(text, len, files[i]);
                free(text);
            }
        }
    }
    if (repl_mode == 1 && halted == 0) repl_loop();
    flush_out();
    return rc_exit;
}
