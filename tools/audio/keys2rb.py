#!/usr/bin/env python3
"""Convert an rbbrain key schedule ("tick:key,key;tick:;...") into RB_KEYS ("sc@fromFrame-toFrame,...").
usage: keys2rb.py schedule.keys [framesPerTick=2]   (the game steps 60 Hz, brains tick at 30 Hz)"""
import sys
sched = open(sys.argv[1]).read().strip()
k = int(sys.argv[2]) if len(sys.argv) > 2 else 2
ev = []
for part in sched.split(';'):
    if not part.strip(): continue
    t, _, keys = part.partition(':')
    ev.append((int(t), {int(x) for x in keys.split(',') if x}))
out, held, last = [], {}, 0
for t, ks in ev:
    for sc in list(held):
        if sc not in ks: out.append(f'{sc}@{held.pop(sc)*k}-{t*k-1}')
    for sc in ks:
        if sc not in held: held[sc] = t
    last = t
for sc, t in held.items(): out.append(f'{sc}@{t*k}-{1<<30}')
print(','.join(out))
print(f'last schedule tick {last} = frame {last*k}', file=sys.stderr)
