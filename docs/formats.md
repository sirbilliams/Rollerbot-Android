# Rollerbot asset formats (Lane B)

Scope: read-only English installation and executable printable strings; no disassembly.
The English directory contains 408 files, not 1,700 DAT files.
The extracted installation has five language directories totaling 1,721 DAT files.
Counts below refer only to English. Identification is not complete semantic decoding.

## Tool

`tools/assets/rbassets.py` uses Python 3 standard library only.

```
timeout 180s python3 tools/assets/rbassets.py "$GAME" --check
timeout 180s python3 tools/assets/rbassets.py "$GAME" --list --check
timeout 180s python3 tools/assets/rbassets.py "$GAME" --output "$PRIVATE/export"
```

Check mode performs conversions and builds the preview in memory without writing.
Export mode writes a manifest, decoded files, metadata JSON, PNG textures, OBJ geometry,
and preview.png (24 texture thumbnails and up to six wireframes).
It refuses output inside this repository or the input tree.
Names retain the original extension before the export extension to avoid collisions.
The manifest records errors individually; inspect conversion_errors even on exit zero.
Unknown layouts are retained, not silently interpreted as geometry.

## Shared obfuscation

Read each complete DWORD little-endian. Decode as
`ROR32(ciphertext XOR 0xE195FD83, 17)`.
The inverse is `ROL32(plaintext, 17) XOR 0xE195FD83`.
Bytes after the final complete DWORD are literal.
The known PCX header and long zero runs establish the transform independently.
Decoded BMP, WAV, 3DS, and project headers corroborate it.
The wrapper applies to projects, many DATs, and some named PCXs.
It is neither compression nor a file-specific encryption key.
Detection checks both clear and transformed signatures.

## Resource names and descriptors

344 English DATs comprise 69 PCX, 72 BMP, 60 WAV, 64 3DS, 11 d3m,
65 clear descriptors, one d30 camera record, quality.dat, and globvars.dat.
Hash-shaped names are eight uppercase hexadecimal digits.
Decoded PRJs already contain those names; descriptors link hashed geometry and images.
A descriptor is a CRLF-terminated, pipe-delimited record starting with version 1.
The exporter preserves every field without guessing its runtime meaning.

The name-generation function is **unresolved**.
Tests against path-like executable strings found no CRC32 matches, including case,
basename, and separator variants. Zero-seeded multiply/add hashes with multipliers
2, 3, 5, 7, 9, 17, 31, 33, 37, 39, 63, 65, 127, 129, 131, 137, 251, 257,
and 65599 also found no matches in that candidate set.
This does not prove filenames are path hashes: original paths may be absent.
Do not substitute a guessed algorithm in a loader.

## Projects and state

14 PRJs decode to Latin-1 text with CRLF line endings, a version line,
a decimal object count, and pipe-delimited object records.
Nested slash-delimited lists encode additional parameters and object relationships.
The tool emits text and lossless field arrays; field semantics remain incomplete.

14 COLs are exactly 65,536 bytes, interpreted provisionally as 16,384 LE int32 slots.
Most are sparse; one has many repeated nonzero groups.
The first nonzero slot in level001.col is byte offset 65024.
It contains both positive values and signed bit patterns.
A 256-square collision raster is **not established** by the file length.
Export JSON lists nonzero slot indices and signed values, retaining the raw file.

quality.dat is one LE int32, value 783 in this installation; bit semantics unknown.
globvars.dat is 8,192 bytes / 2,048 LE int32 slots, mostly zero.
The tool exports its nonzero indices without assigning variable names.
3drad.ini is an ordered sequence of decimal lines with a version marker and blank line;
it is not a conventional section/key INI. multilev.ini uses pipe-delimited text.

## Images

84 PCXs include 69 hashed DATs and 15 named files.
All observed PCXs use 8-bit samples, RLE scanlines, and a trailing 256-entry palette.
Decode the 128-byte header, inclusive bounds, planes, and padded bytes-per-line.
Bytes with high bits 11 introduce a six-bit repeat count and following sample.
The palette marker is 0x0c followed by 768 RGB bytes.
The decoder also handles three-plane 8-bit PCX, although none is required here.

72 BMP DATs use Windows DIB headers and BGR palettes/pixels.
The exporter supports uncompressed 8/24/32-bit images and indexed RLE8.
DD687F34.dat is the observed RLE8 case (256 by 256).
It handles padded rows, bottom-up storage, absolute runs, and delta commands.
PNG output uses RGB8, zlib scanline compression, and CRC-protected chunks.
Two named ICO files are standard icon containers and are retained as ICO.

## Geometry

10 named X files have the DirectX header `xof 0303bin 0032`.
Binary tokens have LE uint16 tags; strings/names have uint32 byte lengths.
Integer and floating-point lists carry uint32 element counts.
The tool also accepts text X and 64-bit floating-point lists.
It extracts Mesh vertices and polygon indices into OBJ.
It does not apply Frame transforms, normals, UVs, materials, or animation.
Those omissions matter for other X files; the supplied hood files convert successfully.

64 DATs are Autodesk 3DS chunk streams (root ID 0x4d4d).
Chunks have a uint16 ID and inclusive uint32 length, both LE.
Object names precede triangle-mesh subchunks.
0x4110 contains a uint16 vertex count followed by float32 XYZ triples.
0x4120 contains a uint16 face count followed by three uint16 indices and flags.
The tool exports all object geometry, preserving polygon winding.
Material groups, smoothing, UVs, and local transforms are not exported.

11 DATs start `d3m.` and contain serialized native engine data.
The DWORDs at offsets 4 and 8 behave as start/end addresses of a triangle span.
Their difference is divisible by 36 in all 11 files.
Starting at offset 64, that span contains int32 XYZ triples grouped into triangles.
The exporter extracts this provisional triangle soup in raw integer units.
Scale, remaining sections, adjacency, and native-pointer fields need runtime confirmation.
One `d30.` DAT is a 176-byte camera/settings structure; it is retained decoded.
Its complete field schema is unresolved.

## Font, tables, audio, and video

font.fnt is a 5,612-byte custom font resource, not a Windows FNT header.
Its beginning contains two uint16 values and a fixed-width face label.
A 256-byte width-like table starts at 39, followed by uint16 offsets at 295.
Glyph packing and the remaining header are unresolved; raw bytes are retained.

atan.bin contains 720 LE int32 values, increasing from 0 to 512000.
The value at index 360 is 1024; index 540 is 2472.
These fit a tangent lookup sampled at roughly eighth-degree steps, despite the name.
The exact generation/rounding rule and terminal clamp are unconfirmed.
sinusbig.bin contains 3,600 LE int32 values, range -32767 through 32768.
Its initial samples fit a sine with amplitude 32768 and 2,880 samples per turn.
sinus.bin contains 115,200 bytes; its multidimensional layout remains unresolved.
The tool exposes signed byte samples rather than asserting a false int32 interpretation.
random.bin is 2,000 bytes, exposed as 500 signed LE DWORDs; RNG semantics unknown.

61 WAVs (60 wrapped DATs and null_.wav) are RIFF/WAVE containers.
Observed payloads include mono PCM at 11,025 and 22,050 Hz, 8 or 16 bits.
Decoded WAVs are copied intact, preserving RIFF chunks.
No MIDI, compressed music, or AVI/video container was identified in the English files.
Separate disc audio/video outside this directory has not been audited.
The executable is a PE container; it is identified and retained without interpretation.

## Validation and limits

A timeout-bounded, memory-only pass classified 408/408 files (100%).
All 156 textures and 85 geometry files passed conversion with zero conversion errors.
This percentage measures container identification, not full semantic understanding.
The private export directory is outside this session's writable roots, so disk export
and the requested private preview could not be produced here.
No game bytes or converted assets were placed in the repository. No commit was made.
