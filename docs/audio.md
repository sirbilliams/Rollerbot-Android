# Audio and movies

The audio lane exposes `src/audio/Audio.h`; integrate with `add_subdirectory(src/audio)` and link `rb_audio`. SDL2 is the only runtime dependency. No SDL_mixer or external codec libraries. All code is original C++17. Private inputs and decompiler output remain outside this repository.

## Original DirectSound behavior

Evidence: private `decomp/rb/raw/_free.c`, cross-checked with executable disassembly. Several implementation bodies are incorrectly attached to jump-alias headings; addresses below distinguish known implementation addresses from aliases. Static analysis establishes engine capability, not which project invokes every capability.

* Setup 0x45c170: default DirectSound device; foreground HWND, desktop fallback; cooperative level 2 (priority). Primary descriptor flags 0x91 (primary, 3D, volume). PCM stereo 16-bit primary format: setting bits 9–10 select disabled / 11025 / 22050 / 44100 Hz. Primary QueryInterface obtains IDirectSound3DListener. Distance factor 1.2 and rolloff 0.7 are immediate. Disabled sound releases the device. Two 256-entry pointer tables hold secondary buffers and their 3D interfaces.
* File loader 0x45c0d0 (alias 0x401465) calls alias 0x401406: secondary descriptor flags 0x240b2 (3D, volume, frequency, static storage, improved cursor, mute at maximum distance). WAV parsing has resource/file-mapping paths; creation is followed by one full Lock/copy/Unlock (alias 0x4011f4). This is resident sample playback, with no observed refill/ring-buffer streaming. Each object owns one buffer, so replay restarts that object's voice rather than duplicating it.
* Alias 0x401753 initializes a sound object: GetFrequency, save nominal frequency, SetFrequency, SetMinDistance from project field, SetMaxDistance to 100000000, SetMode disabled (2) or normal (0) according to sound options; volume is a signed project integer times 100 (DirectSound hundredths of dB). Saves primary volume for restoration.
* Start (object helper, duplicated under aliases and implementation headings): SetCurrentPosition(0), Play(0,0,flags), with flags 1 for looping and 0 for one-shot; updates 3D position and velocity and commits listener deferred settings. Stop uses buffer Stop, gated by loaded/enabled object state. GetStatus detects playback completion; GetCurrentPosition reads cursors.
* Runtime listener update sets listener position and orientation. Object updates set deferred positions/velocities and maximum distance (including a temporary far distance). CommitDeferredSettings occurs per update. Position and velocity use separate engine unit conversion constants. No secondary SetPan call was found: stereo placement comes from DirectSound3D. Listener velocity/Doppler configuration was not found. Do not infer streamed music, MIDI, CD audio or a separate music subsystem from this path.
* Global pause helpers save primary volume and set it to -10000, then restore it; this mutes without freezing DirectSound cursors. Another helper relocates sound sources far away and starts looping or stops buffers. Cleanup 0x404e50 restores primary volume, stops/releases each secondary, releases each 3D interface, then primary and device.

## COM method audit

Every reference to the primary, secondary, listener and secondary-3D tables in the recovered C was inspected, including duplicate alias bodies. IDirectSoundBuffer vtable byte offsets: 0 QueryInterface, 8 Release, 0x18 GetVolume, 0x20 GetFrequency, 0x24 GetStatus, 0x2c Lock, 0x30 Play, 0x34 SetCurrentPosition, 0x38 SetFormat, 0x3c SetVolume, 0x44 SetFrequency, 0x48 Stop, 0x4c Unlock. Secondary GetCurrentPosition is 0x10 (not 0x20). No SetPan (0x40), Restore (0x50), notifications or streaming service was identified in the recovered buffer paths. Lock/Unlock arguments are corrupted by decompiler stack recovery; the semantic identification uses COM slot layout and the copy operation.

3D buffer slots: 0x20 GetMaxDistance, 0x40 SetMaxDistance, 0x44 SetMinDistance, 0x48 SetMode, 0x4c SetPosition, 0x50 SetVelocity. Listener: 0x30 SetDistanceFactor, 0x34 SetOrientation, 0x38 SetPosition, 0x3c SetRolloffFactor, 0x44 CommitDeferredSettings. Per-site recovered-heading inventory follows; alias headings are not trusted implementation addresses.

| Recovered heading | Interfaces referenced |
|---|---|
| 0x40110e | secondary, 3D buffer, listener |
| 0x4013de | secondary, 3D buffer |
| 0x401465 | secondary, 3D buffer |
| 0x4015d2 | secondary |
| 0x40164a | 3D buffer, listener |
| 0x401753 | primary, secondary, 3D buffer |
| 0x401834 | primary |
| 0x401c08 | secondary |
| 0x4020e0 | primary, secondary, 3D buffer, listener |
| 0x402257 | listener |
| 0x402545 | primary, secondary, 3D buffer |
| 0x40c200 | secondary, 3D buffer, listener |
| 0x40c440 | secondary |
| 0x40ff20 | primary |
| 0x40ff90 | primary |
| 0x433ba0 | listener |
| 0x4340f0 | 3D buffer, listener |
| 0x435880 | secondary |
| 0x435ad0 | primary, secondary, 3D buffer |
| 0x45bf90 | secondary, 3D buffer |
| 0x45c0d0 | secondary, 3D buffer |
| 0x45e070 | primary, secondary, 3D buffer |

## AVI and codec inventory

0x404bd0 checks file existence and VfW version >= 0x010a. Synchronous branch creates MCIWnd at 0x404ca2, style 0x0a, sizes it, sends MCI_PLAY (0x806), polls MCIWnd get-mode (0x46a) for MCI_MODE_PLAY (0x20e), allows input interruption, repeats according to a global repeat count, and destroys the window. Asynchronous branch at 0x404d99 uses style 0x08000000, starts playback and retains HWND at 0x57ef74. Windows MCI owns AVI demux, codecs, audio and presentation timing; no game-side frame/audio streaming is visible here.

A recursive extension and RIFF-magic scan of **every regular file** in private `disc/` and extracted `game/` found **zero AVI or other video files**. Disc has ten RIFF WAVE utility/installer sounds; game has one RIFF PAL palette and no standalone RIFF WAVE. Cabinet contents are represented by the supplied extracted game tree; cabinets themselves were not independently unpacked by this lane. No retail video codec can truthfully be reported. Hashed game DAT containers are the asset lane's responsibility; an embedded movie inside an opaque container is not ruled out.

The sibling FFmpeg script was reviewed: its minimal build enables Bink only, which is not evidence that Rollerbot uses Bink. With no observed compressed codec, this lane implements a tiny BI_RGB 24/32-bit AVI + PCM 8/16-bit mono/stereo decoder. Other codecs fail explicitly; no guessed compressed decoder is shipped. It checks RIFF bounds, padding, format, dimensions, frame lengths and audio alignment. Movie audio is returned as stereo float PCM and can be passed to Audio::play. Frames have RGBA pixels and stream PTS. Movie mediaClock is the next decode PTS, not a wall clock; caller schedules frames against Audio::mediaClock and calls Movie::pause alongside pauseAll. EOF is decode=false; teardown clears all media.

## Replacement behavior and limits

Own SDL float-stereo callback mixer: shared immutable PCM, independent cursors, linear resampling, looping, clamped sum, DirectSound-style volume/pan units. Mutations and callback synchronize with a mutex; this is functional, not a hard real-time guarantee. WAV uses SDL_LoadWAV and SDL_AudioCVT. Stop discards a voice; play creates a new voice at sample zero. pauseAll freezes cursors and sample clock, intentionally suitable for platform suspension (original primary mute did not freeze). mediaClock counts produced frames, excluding pauses, and includes up to one SDL hardware buffer of latency.

Position uses listener-relative inverse distance with rolloff 0.7 and horizontal stereo balance; it approximates DirectSound3D. Orientation, velocity/Doppler, per-voice min/max distances, original frequency-control commands and exact engine coordinate conversion are not exposed yet. Stereo sources also receive this approximation. Movie files are resident and capped at 512 MiB; no asynchronous movie streaming or compressed AVI support. These differences must be resolved if project traces require them.

## Validation

Already running inside `/run/.containerenv` name `omtbuild`. All builds/tests bounded by timeout:

```
timeout 120 cmake -S src/audio -B /tmp/rb-audio-build
timeout 120 cmake --build /tmp/rb-audio-build -j8
timeout 30 env SDL_AUDIODRIVER=dummy ctest --test-dir /tmp/rb-audio-build --output-on-failure
timeout 60 python3 tests/audio/validate.py /tmp/rb-audio-build/tests/rb_audio_test .shots/audio /home/bill/Projects/rollerbot-private/disc /home/bill/Projects/rollerbot-private/game
```

Fixtures verify loop output, pan asymmetry, mute, pause clock, WAV round-trip, invalid WAV, dummy SDL device lifecycle, synthetic BI_RGB first-frame pixels/PNG, PCM availability, movie pause/EOF/teardown and nonexistent files. `.shots/audio/` holds mixed WAV, generated original AVI/PNG and private inventory metadata (untracked). No retail video frames exist to export.
