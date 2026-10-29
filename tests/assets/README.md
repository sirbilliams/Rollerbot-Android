# Asset validation

Configure with `cmake -S . -B build -DRB_GAME_DIR=/private/game`, build,
then run `timeout 180 ctest --test-dir build --output-on-failure`.
The game installation is read only and no decoded retail bytes are written.
The expected inventory is the English retail release (14 projects/COLs, 344 DATs).
Each project, descriptor, image, geometry and PCM sound is checked in memory;
opaque state and camera records receive size checks. Missing model sentinel
0000002D.dat is explicitly excluded from resource resolution.
The filename scanner collects unique extension-bearing byte strings from the
EXE and decoded PRJs; an independent signed-byte implementation checks hashes
and hexadecimal round trips. Original pre-hash paths are not recoverable from
hashed project references alone.

ASan/UBSan builds can use `-fsanitize=address,undefined`. In a ptrace-managed
sandbox set `ASAN_OPTIONS=detect_leaks=0`; LeakSanitizer cannot operate there.
