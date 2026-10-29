#!/usr/bin/env python3
"""Analyse an RB_AUDIO_DUMP wav (float32 stereo) with the RB_AUDIO_LOG lines of the same run.

usage: analyze.py run.wav run.log [--assets DIR]   (DIR = rbassets.py export, only for sample lengths)
Prints: peak, RMS per window, event table, retriggers (same element restarted while still audible),
music loop seam check. Exit status 1 if the mix peaks at or above 0 dBFS.
"""
import re, struct, sys, os, wave
import numpy as np

def read_f32(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'RIFF' and b[8:12] == b'WAVE'
    off, fmt, data = 12, None, None
    while off + 8 <= len(b):
        cid, sz = b[off:off+4], struct.unpack_from('<I', b, off+4)[0]
        if cid == b'fmt ': fmt = struct.unpack_from('<HHIIHH', b, off+8)
        if cid == b'data': data = b[off+8:off+8+sz]; break
        off += 8 + sz + (sz & 1)
    assert fmt[0] == 3 and fmt[1] == 2 and fmt[5] == 32, fmt
    return fmt[2], np.frombuffer(data, '<f4').reshape(-1, 2)

EV = re.compile(r'AUDIO t=([\d.]+) tick=(\d+) (start|stop|rate) el=(\d+) name=(.*?)(?: snd=(\S+))?(?: why=(\S+))?(?: loop=(\d))?(?: vol=(-?\d+))?(?: rate=(\d+))?$')

def read_log(path):
    ev = []
    for l in open(path, errors='replace'):
        m = EV.search(l.strip())
        if m:
            t, tick, act, el, name, snd, why, loop, vol, rate = m.groups()
            ev.append(dict(t=float(t), tick=int(tick), act=act, el=int(el), name=name, snd=snd, why=why,
                           loop=None if loop is None else int(loop), vol=None if vol is None else int(vol),
                           rate=None if rate is None else int(rate)))
    return ev

def dur(assets, snd):
    if not assets or not snd: return None
    p = os.path.join(assets, os.path.basename(snd) + '.wav')
    if not os.path.exists(p): return None
    w = wave.open(p); return w.getnframes() / w.getframerate()

def main():
    a = sys.argv[1:]
    assets = None
    if '--assets' in a: i = a.index('--assets'); assets = a[i+1]; del a[i:i+2]
    sr, x = read_f32(a[0]); ev = read_log(a[1])
    secs = len(x) / sr
    peak = float(np.abs(x).max())
    print(f'{a[0]}: {secs:.1f} s, {sr} Hz, peak {peak:.3f} = {20*np.log10(max(peak,1e-9)):.1f} dBFS (pre-clamp), '
          f'samples >= 1.0: {int((np.abs(x) >= 1.0).sum())}')
    mono = x.mean(axis=1)
    win = int(sr * 0.5)
    rms = [float(np.sqrt((mono[i:i+win]**2).mean())) for i in range(0, len(mono) - win + 1, win)]
    print('RMS per 0.5 s (dBFS): ' + ' '.join(f'{20*np.log10(max(r,1e-6)):.0f}' for r in rms))
    print('silent windows (< -90 dBFS):', sum(1 for r in rms if r < 3e-5), 'of', len(rms))
    starts = [e for e in ev if e['act'] == 'start']
    print(f'events: {len(starts)} starts, {sum(1 for e in ev if e["act"]=="stop")} stops, {sum(1 for e in ev if e["act"]=="rate")} rate changes')
    # per sound summary
    by = {}
    for e in starts: by.setdefault((e['name'], e['snd'], e['loop'], e['vol']), []).append(e)
    print('start events per (element, sound, loop, vol): first t, count, why')
    for (n, s, lp, v), L in sorted(by.items(), key=lambda kv: kv[1][0]['t']):
        d = dur(assets, s)
        print(f'  {n[:22]:22} {s} loop={lp} vol={v:4} len={d and round(d,2)}  t0={L[0]["t"]:.2f} n={len(L)} {sorted(set(e["why"] for e in L))}')
    # retriggers: same element restarted while its previous (one-shot) voice would still be playing
    rt = {}
    last = {}
    for e in ev:
        if e['act'] == 'start':
            p = last.get(e['el'])
            if p:
                d = dur(assets, p['snd']) or 0
                ratio = 4096 / max(1, p.get('rate_now') or 4096)
                if e['loop'] == 0 and e['t'] - p['t'] < d * ratio * 0.999:
                    rt.setdefault((e['name'], e['snd']), []).append(e['t'])
            e['rate_now'] = e['rate']; last[e['el']] = e
        elif e['act'] == 'rate' and e['el'] in last:
            last[e['el']]['rate_now'] = e['rate']
        elif e['act'] == 'stop': last.pop(e['el'], None)
    print('retriggers (same element restarted while still playing; the exe restarts too, 0x40c200):')
    for k, v in rt.items(): print(f'  {k[0][:22]:22} {k[1]} x{len(v)} between t={v[0]:.2f}..{v[-1]:.2f}')
    if not rt: print('  none')
    # same sound started twice in the same tick by different elements
    tk = {}
    for e in starts: tk.setdefault((e['tick'], e['snd']), []).append(e['el'])
    dup = {k: v for k, v in tk.items() if len(set(v)) > 1}
    print('same sound started by several elements in one tick:', dup if dup else 'none')
    # music loop seam: loops start at t=0 and wrap at k*len/ratio
    for e in starts:
        d = dur(assets, e['snd'])
        if e['loop'] == 1 and d and d > 5:
            ratio = (e['rate'] or 4096) / 4096
            Lp = d / ratio
            seams = [e['t'] + k * Lp for k in range(1, 4) if e['t'] + k * Lp < secs - 0.1]
            msg = []
            dif = np.abs(np.diff(mono)); ref = float(np.percentile(dif, 99.9)) or 1e-9
            for s in seams:
                i = int(s * sr); loc = float(dif[max(0, i-3):i+3].max())
                msg.append(f'{s:.2f}s jump {loc/ref:.2f}x p99.9')
            print(f'  music {e["name"][:16]} {e["snd"]} loop length {Lp:.2f} s; seams: ' + ('; '.join(msg) if msg else 'run too short'))
    sys.exit(1 if peak >= 1.0 else 0)

main()
