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
