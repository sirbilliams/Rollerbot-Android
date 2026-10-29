"""Convert the test renderer's P6 output using only the standard library."""
import pathlib, struct, sys, zlib
source, target = map(pathlib.Path, sys.argv[1:])
magic, dimensions, maximum, pixels = source.read_bytes().split(b'\n', 3)
assert magic == b'P6' and maximum == b'255'
w, h = map(int, dimensions.split())
assert len(pixels) == w*h*3
def chunk(kind, data):
    return struct.pack('>I', len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
rows = b''.join(b'\0'+pixels[y*w*3:(y+1)*w*3] for y in range(h))
target.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))
