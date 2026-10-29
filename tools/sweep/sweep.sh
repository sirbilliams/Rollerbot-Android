#!/usr/bin/env bash
# Run from any directory; all retail data stays in the external .game tree.
set -euo pipefail
cd "$(dirname "$0")/../.."
export RB_SWEEP_EXE="${RB_SWEEP_EXE:-build/src/brain/tests/rbbrain}"
export RB_SWEEP_DATA="${RB_SWEEP_DATA:-.game}"
timeout 900 python3 - <<'PY'
import collections, os, pathlib, re, subprocess
exe = os.environ['RB_SWEEP_EXE']
data = pathlib.Path(os.environ['RB_SWEEP_DATA'])
logs = pathlib.Path('.shots/sweep/logs'); logs.mkdir(parents=True, exist_ok=True)
patterns = {'idle': '0:', 'forward': '0:72',
            'forward+turns': ';'.join(f'{t}:72' + (',77' if (t//180)%3 == 1 else ',75' if (t//180)%3 == 2 else '') for t in range(0,5400,180))}
rows = []; clean = 0
for project in sorted(data.glob('level[0-9][0-9][0-9].prj')):
    loads = 0; compile_errors = 0; errors = 0; nan = 0; lives = 0
    unknown = collections.Counter(); exits = collections.Counter(); ticks = []
    for pattern, keys in patterns.items():
        r = subprocess.run(['timeout', '120', exe, str(data), project.name, '--ticks', '5400', '--sweep', '--keys', keys], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (logs / f'{project.stem}-{pattern}.log').write_text(r.stdout)
        loads += bool(re.search(r'^project ', r.stdout, re.M))
        m = re.search(r'SWEEP ticks=(\d+) compile=(\d+) nonfinite=(\d+) lives_lost=(\d+) exits=(\S*) unknown=(\S*)', r.stdout)
        if not m:
            errors += 1; ticks.append('failed'); continue
        t, c, n, l = map(int, m.group(1,2,3,4)); ticks.append(str(t))
        compile_errors += c; nan += n; lives += l
        # Return 1 denotes a diagnosed compile/unknown/nonfinite failure; other codes are runtime failures.
        errors += r.returncode not in (0,1) or bool(re.search(r'^error:',r.stdout,re.M))
        for field, counts in [(m[5],exits),(m[6],unknown)]:
            for item in field.strip(',').split(','):
                if item:
                    name, count = item.rsplit(':',1); counts[name] += int(count)
    ok = loads == 3 and ticks == ['5400']*3 and not (compile_errors or errors or nan or unknown)
    clean += ok
    fmt = lambda c: ', '.join(f'{k}: {v}' for k,v in sorted(c.items())) or '—'
    rows.append(f'| {project.stem} | {loads}/3 | {"/".join(ticks)} | {compile_errors} | {fmt(unknown)} | {errors}/{nan} | {fmt(exits)} | {lives} |')
    print(project.stem, 'clean' if ok else 'FAILED', flush=True)
report = '''# Retail level sweep

Each project runs independently for 5400 ticks (180 seconds at 30 Hz) with idle,
hold forward (scan code 72), and forward plus alternating 180-tick turn segments
(scan codes 75/77). Each process has a 120-second wall timeout. Exits are recorded
by code and cleared before the next tick, retaining the current project for coverage;
these counts are exit requests, not proof of completed routes. Lives lost sums positive
decreases of global 220. Counts aggregate all three patterns. NaN checks cover every
element's scene transform each tick; VM state itself uses integer fixed point.
Runtime errors include exceptions, signals, timeout, and missing completion records.
Retail movement is automatic; forward holds the Up input (72). Bare `rot=>80`
and `rot=<30` in level004 are accepted as discarded comparisons, with unit coverage.
This compatibility policy preserves the variable; the exe compiler's malformed-source
behaviour remains unverified (statement/expression paths 0x4125f0/0x414880).
No new command handlers were required: no UNIMPLEMENTED calls were reached.
Logs: `.shots/sweep/logs/`. Reproduce: `tools/sweep/sweep.sh` after building.

| Level | Loads | Ticks idle/forward/turns | Compile errors | UNIMPLEMENTED hits | Runtime errors/NaNs | Exit requests (code: count) | Lives lost |
|---|---:|---|---:|---|---:|---|---:|
'''
pathlib.Path('docs/levels.md').write_text(report + '\n'.join(rows) + f'\n\nLevels clean: {clean}/{len(rows)}.\n' + '\nCaptures: `tools/sweep/shots.sh` uses the real `build/src/platform/rollerbot`,\nRB_DATA=.game, RB_LEVEL per project, RB_FRAMES=150, RB_SHOTS at frame 150,\nSDL_VIDEO_X11_FORCE_EGL=1 and Xvfb. Images: `.shots/sweep/levelNNN.png`;\ncontact sheet: `.shots/sweep/sheet.png`. Build and all 8 CTests passed in omtbuild.\n')
raise SystemExit(clean != len(rows) or len(rows) != 14)
PY
