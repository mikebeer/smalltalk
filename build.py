#!/usr/bin/env python3
"""Assemble src/*.crexx into smalltalk.crexx (symbolic constants + namespace list)."""
import re, os
root = os.path.dirname(os.path.abspath(__file__))
d = os.path.join(root, 'src')
order = ['a_head', 'b_lex', 'b2_top', 'c_res', 'd_obj', 'e_eval', 'f_prim', 'g_big']
src = ''.join(open(os.path.join(d, f + '.crexx')).read() + '\n' for f in order)
consts = {}
for line in open(os.path.join(d, 'consts.txt')):
    line = line.split('#')[0].strip()
    if line:
        k, v = line.split()
        consts[k] = v
sels = []
for line in open(os.path.join(d, 'selectors.txt')):
    line = line.strip()
    if line:
        k, v = line.split(None, 1)
        sels.append(v)
        consts[k] = str(len(sels))
prims = []
for line in open(os.path.join(d, 'prims.txt')):
    line = line.strip()
    if line:
        k, v = line.split()
        prims.append(v)
        consts[k] = str(len(prims))
src = src.replace('@@PRIMNAMES@@', ' '.join(prims)).replace('@@SELBOOT@@', ' '.join(sels))
# ---- EV macro: evaluate leaf nodes inline, call ev() only for the rest
def evmacro(text):
    out = []; proc = None; need = []
    for ln in text.split('\n'):
        mm = re.match(r'^([a-z_0-9]+): procedure', ln)
        if mm: proc = len(out)
        mm = re.match(r'^(\s*)EV (\w+) = (.+)$', ln)
        if mm:
            ind, dst, ex = mm.groups()
            if proc not in need: need.append(proc)
            out += [ind + 'ev_n = ' + ex,
                    ind + 'ev_t = nt[ev_n]',
                    ind + 'if ev_t = ND_LIT then ' + dst + ' = n_a[ev_n]',
                    ind + 'else if ev_t = ND_SELF then ' + dst + ' = vp[fb + 2]',
                    ind + 'else if ev_t = ND_LOCAL & n_a[ev_n] = 0 then ' + dst + ' = vp[fb + 4 + n_b[ev_n]]',
                    ind + 'else ' + dst + ' = ev(ev_n, fb)']
        else:
            out.append(ln)
    for pi in sorted(need, reverse=True):
        j = pi + 1
        while not out[j].lstrip().startswith('arg '): j += 1
        out[j + 1:j + 1] = ['  ev_n = .int', '  ev_t = .int']
    return '\n'.join(out)
src = evmacro(src)
# ---- pack globals: the VM relinks every global register on each call, so the
# ---- fewer globals there are the cheaper a call is.  Parallel arrays are packed
# ---- into strided container arrays and scalars into SI/SS/SF arrays.
arrmap = {}      # name -> (container, stride, offset)
conts = []       # (container, type)
for line in open(os.path.join(d, 'pack.txt')):
    line = line.split('#')[0].strip()
    if not line:
        continue
    head, names = line.split(':')
    cname, ctype = head.split()
    names = names.split()
    conts.append((cname, ctype))
    for k, nm in enumerate(names):
        arrmap[nm] = (cname, len(names), k + 1)
literals = {'nil_oop': '2', 'true_oop': '4', 'false_oop': '6'}
ibody = src[src.index('init: procedure'):]
ibody = ibody[:ibody.index('\n  return\n')]
scalmap = {}
cnt = {'int': 0, 'string': 0, 'float': 0}
for ln in ibody.splitlines():
    mm = re.match(r'^  ([a-z][A-Za-z0-9_]*) = (.*)$', ln)
    if not mm:
        continue
    nm, val = mm.group(1), mm.group(2)
    if nm in scalmap or nm in arrmap or nm in literals or nm.startswith('z_'):
        continue
    if val.startswith('.'):
        continue
    ty = 'string' if val.startswith('"') else 'float' if re.match(r'^-?\d+\.\d+$', val) else 'int'
    cnt[ty] += 1
    scalmap[nm] = ({'int': 'px_si', 'string': 'px_ss', 'float': 'px_sf'}[ty], cnt[ty])
tokpat = re.compile(r'/\*.*?\*/|"[^"\n]*"|\'[^\'\n]*\'|[A-Za-z_][A-Za-z0-9_]*', re.S)
def matchbr(t, i):
    dep = 0
    while i < len(t):
        c = t[i]
        if c == '[': dep += 1
        elif c == ']':
            dep -= 1
            if dep == 0: return i
        elif c in '"\'':
            i = t.index(c, i + 1)
        i += 1
    raise SystemExit('unbalanced [')
def pk(t):
    out = []; pos = 0
    while True:
        mo = tokpat.search(t, pos)
        if not mo:
            out.append(t[pos:]); break
        out.append(t[pos:mo.start()]); tok = mo.group(0); end = mo.end()
        prev = t[mo.start() - 1] if mo.start() > 0 else ' '
        if tok[0] in '/"\'' or prev == '.':
            out.append(tok); pos = end; continue
        if tok in arrmap and t[end:end + 1] == '[':
            cl = matchbr(t, end)
            inner = pk(t[end + 1:cl])
            cn, st, off = arrmap[tok]
            if not re.match(r'^[A-Za-z0-9_]+$', inner): inner = '(' + inner + ')'
            out.append('%s[%s*%d+%d]' % (cn, inner, st, off)); pos = cl + 1; continue
        if tok in literals:
            out.append(literals[tok]); pos = end; continue
        if tok in scalmap and t[end:end + 1] != '(':
            cn, k = scalmap[tok]
            out.append('%s[%d]' % (cn, k)); pos = end; continue
        out.append(tok); pos = end
    return ''.join(out)
# drop declarations of packed arrays / literals, then rewrite everything else
lines = []
for ln in src.split('\n'):
    mm = re.match(r'^  ([a-z][A-Za-z0-9_]*) = (\.\w+\[\])\s*$', ln)
    if mm and mm.group(1) in arrmap: continue
    mm = re.match(r'^  ([a-z][A-Za-z0-9_]*) = ', ln)
    if mm and mm.group(1) in literals: continue
    lines.append(ln)
src = pk('\n'.join(lines))
decl = ''.join('  %s = .%s[]\n' % (c, t) for c, t in conts + [('px_si', 'int'), ('px_ss', 'string'), ('px_sf', 'float')])
src = src.replace('init: procedure = .void\n', 'init: procedure = .void\n' + decl, 1)
body = src[src.index('init: procedure'):]
body = body[:body.index('\n  return\n')]
names = []
for ln in body.splitlines():
    mm = re.match(r'^  ([a-z][A-Za-z0-9_]*) = ', ln)
    if mm and not mm.group(1).startswith('z_') and mm.group(1) not in names:
        names.append(mm.group(1))
lines = []
cur = 'namespace smalltalk expose'
for n in names:
    if len(cur) + len(n) > 74:
        lines.append(cur + ',')
        cur = '   ' + n
    else:
        cur += ' ' + n
lines.append(cur)
src = src.replace('@@EXPOSE@@', '\n'.join(lines))
def rep(mo):
    k = mo.group(0)
    if k in consts: return consts[k]
    raise SystemExit('unknown constant ' + k)
src = re.sub(r'\b(?:TK|ND|KD|UW|S|PR)_[A-Za-z0-9]+\b|\bSELMAX\b', rep, src)
open(os.path.join(root, 'smalltalk.crexx'), 'w').write(src)
print(len(src.splitlines()), 'lines,', len(names), 'globals')
