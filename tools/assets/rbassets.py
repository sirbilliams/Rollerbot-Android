#!/usr/bin/env python3
"""Inspect and export Rollerbot assets. Python 3 standard library only.
Run with an external private output directory; never export into the source repo.
"""
import argparse
import collections
import json
import math
from pathlib import Path
import re
import struct
import zlib


def unwrap(b):
    """Inverse of the DWORD rotate/XOR wrapper; trailing bytes are literal."""
    out = bytearray(b)
    for i in range(0, len(b) - 3, 4):
        v = struct.unpack_from('<I', b, i)[0] ^ 0xe195fd83
        struct.pack_into('<I', out, i, ((v >> 17) | (v << 15)) & 0xffffffff)
    return bytes(out)


def magic(b):
    for sig, kind in ((b'\x0a\x05\x01\x08', 'pcx'), (b'BM', 'bmp'),
                      (b'RIFF', 'riff'), (b'MM', '3ds'), (b'xof ', 'x'),
                      (b'd3m.', 'd3m'), (b'd30.', 'd30'), (b';v1.', 'prj')):
        if b.startswith(sig):
            return ('wav' if b[8:12] == b'WAVE' else 'avi' if b[8:12] == b'AVI ' else kind) if kind == 'riff' else kind
    if b.startswith(b'1|./'):
        return 'descriptor'
    return None


def identify(path, b):
    if path.name in ('quality.dat', 'globvars.dat'):
        return path.stem, b, False
    k = magic(b)
    if k:
        return k, b, False
    d = unwrap(b)
    k = magic(d)
    if k:
        return k, d, True
    return {'.col': 'col', '.bin': 'table', '.fnt': 'font', '.ini': 'ini',
            '.exe': 'pe', '.ico': 'ico'}.get(path.suffix.lower(), 'unknown'), b, False


def png(w, h, rgb):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    rows = b''.join(b'\0' + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', w, h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')


def texture(k, b):
    if k == 'pcx':
        xmin, ymin, xmax, ymax = struct.unpack_from('<4H', b, 4)
        w, h = xmax-xmin+1, ymax-ymin+1
        planes, stride = b[65], struct.unpack_from('<H', b, 66)[0]
        if b[3] != 8 or planes not in (1, 3) or not 0 < w <= stride <= 16384 or not 0 < h <= 16384:
            raise ValueError('unsupported PCX layout')
        need = h*planes*stride
        raw = bytearray(); pos = 128
        while len(raw) < need:
            v = b[pos]; pos += 1
            count = 1
            if v >= 192:
                count = v & 63; v = b[pos]; pos += 1
            raw.extend(bytes([v])*count)
            if len(raw) > need:
                raise ValueError('PCX run exceeds raster')
        if planes == 1:
            if b[-769] != 12:
                raise ValueError('missing PCX palette')
            pal = b[-768:]
            rgb = b''.join(pal[3*raw[y*stride+x]:3*raw[y*stride+x]+3] for y in range(h) for x in range(w))
        else:
            rgb = bytes(raw[y*stride*3+c*stride+x] for y in range(h) for x in range(w) for c in range(3))
        return w, h, rgb
    off = struct.unpack_from('<I', b, 10)[0]
    w, sh = struct.unpack_from('<ii', b, 18); h = abs(sh)
    bits = struct.unpack_from('<H', b, 28)[0]
    compression = struct.unpack_from('<I', b, 30)[0]
    if bits not in (8, 24, 32) or compression not in (0, 1) or (compression == 1 and bits != 8) or not 0 < w <= 16384 or not 0 < h <= 16384:
        raise ValueError('unsupported BMP layout')
    if compression == 1:
        raster = bytearray(w*h); x = y = 0; pos = off
        while pos+2 <= len(b):
            count, value = b[pos:pos+2]; pos += 2
            if count:
                values = bytes([value])*count
            elif value == 0:
                x = 0; y += 1; continue
            elif value == 1:
                break
            elif value == 2:
                dx, dy = b[pos:pos+2]; pos += 2; x += dx; y += dy; continue
            else:
                values = b[pos:pos+value]; pos += value+(value & 1)
            if y >= h or x+len(values) > w:
                raise ValueError('RLE8 run exceeds raster')
            raster[y*w+x:y*w+x+len(values)] = values; x += len(values)
        paloff = 14+struct.unpack_from('<I', b, 14)[0]
        rgb = b''.join(b[paloff+4*raster[y*w+x]:paloff+4*raster[y*w+x]+3][::-1]
                       for y in (range(h-1,-1,-1) if sh > 0 else range(h)) for x in range(w))
        return w, h, rgb
    stride = ((w*bits+31)//32)*4
    paloff = 14+struct.unpack_from('<I', b, 14)[0]
    rgb = bytearray()
    for y in range(h):
        row = off+(h-1-y if sh > 0 else y)*stride
        for x in range(w):
            pos = row+x*(bits//8)
            if bits == 8:
                pos = paloff+4*b[pos]
            rgb.extend(b[pos:pos+3][::-1])
    if len(rgb) != w*h*3:
        raise ValueError('truncated BMP')
    return w, h, bytes(rgb)


def meshd3m(b):
    # The serialized pointer span gives the triangle soup byte length.
    start, end = struct.unpack_from('<2I', b, 4)
    size = end-start
    if size <= 0 or size % 36 or 64+size > len(b):
        raise ValueError('invalid d3m triangle span')
    vs = [tuple(float(c) for c in v) for v in struct.iter_unpack('<3i', b[64:64+size])]
    return [(vs, [tuple(range(i,i+3)) for i in range(0,len(vs),3)])]


def mesh3ds(b):
    meshes = []
    def walk(start, end, current=None):
        while start+6 <= end:
            tag, size = struct.unpack_from('<HI', b, start)
            if size < 6 or start+size > end:
                raise ValueError('invalid 3DS chunk length')
            p, stop = start+6, start+size
            if tag == 0x4000:
                p = b.index(0, p, stop)+1
                current = [[], []]; meshes.append(current); walk(p, stop, current)
            elif tag == 0x4110 and current is not None:
                n = struct.unpack_from('<H', b, p)[0]
                current[0].extend(struct.iter_unpack('<3f', b[p+2:p+2+12*n]))
            elif tag == 0x4120 and current is not None:
                n = struct.unpack_from('<H', b, p)[0]
                current[1].extend(tuple(f[:3]) for f in struct.iter_unpack('<4H', b[p+2:p+2+8*n]))
            elif tag in (0x4d4d, 0x3d3d, 0x4100):
                walk(p, stop, current)
            start = stop
    walk(0, len(b))
    return meshes


def xtokens(b):
    if b[8:12] == b'txt ':
        return re.findall(r'[A-Za-z_][\w.-]*|[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?|[{}]', b[16:].decode('latin1'))
    if b[8:12] != b'bin ':
        raise ValueError('compressed X unsupported')
    t = []; p = 16
    while p < len(b):
        code = struct.unpack_from('<H', b, p)[0]; p += 2
        if code in (1, 2):
            n = struct.unpack_from('<I', b, p)[0]; p += 4
            t.append(b[p:p+n].decode('latin1').rstrip('\0')); p += n
            if code == 2:
                p += 2
        elif code == 3:
            t.append(str(struct.unpack_from('<I', b, p)[0])); p += 4
        elif code in (6, 7):
            n = struct.unpack_from('<I', b, p)[0]; p += 4
            size = 8 if code == 7 and b[12:16] == b'0064' else 4
            fmt = 'd' if size == 8 else 'f' if code == 7 else 'I'
            t.extend(str(v[0]) for v in struct.iter_unpack('<'+fmt, b[p:p+size*n])); p += size*n
        elif code == 5:
            p += 16
        elif code in (10, 11):
            t.append('{' if code == 10 else '}')
        elif code not in (12, 13, 14, 15, 16, 17, 18, 19, 20, 31, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52):
            raise ValueError('unknown X token '+str(code))
    return t


def meshx(b):
    t = xtokens(b); meshes = []
    for i, token in enumerate(t):
        if token != 'Mesh':
            continue
        j = i+1
        if t[j] != '{':
            j += 1
        if t[j] != '{':
            continue
        j += 1
        n = int(t[j]); j += 1
        vertices = [tuple(map(float, t[j+3*k:j+3*k+3])) for k in range(n)]; j += 3*n
        n = int(t[j]); j += 1; faces = []
        for _ in range(n):
            count = int(t[j]); j += 1
            faces.append(tuple(map(int, t[j:j+count]))); j += count
        meshes.append((vertices, faces))
    return meshes


def obj(meshes):
    lines = []; base = 1
    for idx, (vertices, faces) in enumerate(meshes):
        lines.append('o mesh_'+str(idx))
        for v in vertices:
            if not all(math.isfinite(c) for c in v):
                raise ValueError('nonfinite vertex')
            lines.append('v '+' '.join(format(c, '.9g') for c in v))
        for face in faces:
            if not all(0 <= i < len(vertices) for i in face):
                raise ValueError('invalid vertex index')
            lines.append('f '+' '.join(str(base+i) for i in face))
        base += len(vertices)
    return '\n'.join(lines)+'\n'


def preview(images, meshes):
    w, h, cell = 768, 768, 128
    out = bytearray(bytes((24, 24, 28))*w*h)
    def pixel(x, y, color):
        if 0 <= x < w and 0 <= y < h:
            out[(y*w+x)*3:(y*w+x)*3+3] = bytes(color)
    for i, (iw, ih, rgb) in enumerate(images[:24]):
        scale = min(120/iw, 120/ih); tw, th = max(1,int(iw*scale)), max(1,int(ih*scale))
        ox, oy = (i%6)*cell+(cell-tw)//2, (i//6)*cell+(cell-th)//2
        for y in range(th):
            for x in range(tw):
                p = (min(ih-1,int(y/scale))*iw+min(iw-1,int(x/scale)))*3
                pixel(ox+x, oy+y, rgb[p:p+3])
    for i, group in enumerate(meshes[:6]):
        for vs, fs in group:
            points = [(x+z*.45, -y+z*.25) for x,y,z in vs]
            if not points:
                continue
            lo = [min(p[k] for p in points) for k in (0,1)]; hi = [max(p[k] for p in points) for k in (0,1)]
            s = 115/max(hi[0]-lo[0],hi[1]-lo[1],1e-9)
            points = [(int(i*128+6+(x-lo[0])*s), int(540+(y-lo[1])*s)) for x,y in points]
            for f in fs:
                for a,b in zip(f, f[1:]+f[:1]):
                    x,y = points[a]; xx,yy = points[b]; n = max(abs(xx-x),abs(yy-y),1)
                    for step in range(n+1):
                        pixel(round(x+(xx-x)*step/n),round(y+(yy-y)*step/n),(120,220,180))
    return png(w,h,out)


def metadata(kind, b, path):
    if kind in ('quality', 'globvars', 'col'):
        vals = list(struct.unpack('<%di'%(len(b)//4), b))
        return {'int32_count': len(vals), 'nonzero': [[i,v] for i,v in enumerate(vals) if v]}
    if kind == 'table':
        if path.name == 'sinus.bin':
            return {'encoding': 'signed int8', 'count': len(b), 'values': list(struct.unpack('<%db'%len(b), b))}
        return {'encoding': 'little-endian int32', 'values': list(struct.unpack('<%di'%(len(b)//4), b))}
    if kind == 'font':
        return {'header_u16': list(struct.unpack_from('<2H',b)), 'name': b[4:37].decode('latin1').rstrip(' \0'), 'size':len(b), 'layout': 'partially understood; raw retained'}
    if kind in ('prj','descriptor','ini'):
        return {'lines': [line.split('|') for line in b.decode('latin1').splitlines()]}
    if kind == 'wav':
        return {'container': 'RIFF/WAVE', 'size': len(b)}
    return {}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source', type=Path)
    ap.add_argument('--output', type=Path)
    ap.add_argument('--list', action='store_true', help='print one identification per file')
    ap.add_argument('--check', action='store_true', help='decode all in memory, write nothing')
    args = ap.parse_args()
    if args.output and not args.check:
        repo = Path(__file__).resolve().parents[2]
        if args.output.resolve().is_relative_to(repo) or args.output.resolve().is_relative_to(args.source.resolve()):
            ap.error('output must be outside repository and source tree')
        args.output.mkdir(parents=True, exist_ok=True)
    counts = collections.Counter(); rows = []; images = []; models = []
    for path in ([args.source] if args.source.is_file() else sorted(args.source.rglob('*'))):
        if not path.is_file():
            continue
        b = path.read_bytes(); kind, data, wrapped = identify(path,b); counts[kind] += 1
        relative = Path(path.name) if args.source.is_file() else path.relative_to(args.source)
        row = {'path': str(relative), 'size':len(b), 'kind':kind,'wrapped':wrapped}
        dest = args.output / relative if args.output else None
        try:
            payload = data; suffix = '.'+kind
            if kind in ('pcx','bmp'):
                image = texture(kind,data); payload = png(*image); suffix = '.png'
                row['dimensions'] = list(image[:2])
                if len(images)<24: images.append(image)
            elif kind in ('x','3ds','d3m'):
                mesh = meshx(data) if kind == 'x' else meshd3m(data) if kind == 'd3m' else mesh3ds(data)
                payload = obj(mesh).encode(); suffix = '.obj'
                row['vertices'] = sum(len(v) for v,f in mesh); row['faces'] = sum(len(f) for v,f in mesh)
                if len(models)<6: models.append(mesh)
            else:
                info = metadata(kind,data,path)
                if info:
                    row['metadata'] = {k:v for k,v in info.items() if k not in ('values','lines','nonzero')}
                    if dest and not args.check:
                        dest.parent.mkdir(parents=True,exist_ok=True)
                        dest.with_name(dest.name+'.json').write_text(json.dumps(info,indent=2))
                if kind in ('prj','descriptor','ini'): suffix = '.txt'
            row['decoded'] = kind != 'unknown'
            if dest and not args.check:
                dest.parent.mkdir(parents=True,exist_ok=True)
                dest.with_name(dest.name+suffix).write_bytes(payload)
        except (ValueError, IndexError, struct.error, OverflowError) as e:
            row['error'] = str(e)
        rows.append(row)
    report = {'total':len(rows), 'identified':len(rows)-counts['unknown'], 'counts':dict(counts),
              'conversion_errors':sum('error' in r for r in rows), 'files':rows}
    preview_bytes = preview(images,models)
    if args.output and not args.check:
        (args.output/'manifest.json').write_text(json.dumps(report,indent=2))
        (args.output/'preview.png').write_bytes(preview_bytes)
    print(json.dumps({k:v for k,v in report.items() if k != 'files'},indent=2))
    for r in rows:
        if args.list or 'error' in r or r['kind']=='unknown': print(json.dumps(r))

if __name__ == '__main__':
    main()
