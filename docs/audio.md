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

## What the game does with sound (verified, `src/game/SoundBank.*`)

Evidence: `_free.c` 0x401753 (sound init), 0x40110e (start), 0x40c200 (`play_sound`), 0x40c440 (`stop_sound`), 0x40bff0 (`off`),
0x40cc90 (`sound_rate_set`), 0x45bf90 (level start / resume), 0x4016ae (rate -> Hz), cross-checked with the asm and the 14 projects.

- **No music subsystem.** Everything is a resident WAV inside a hashed `.dat`, owned by an element (one DirectSound buffer per element).
  "Music" is just an element (the `Observer-DM!` camera, and in levels 3-9/13 a "Scenery" ambience) whose sound is flagged looping.
  No CD audio, MIDI, streaming, panning or master volume exists. No project calls `sound_mode_set`, `sound_status`, `stop_sound_system` or
  `restart_sound_system`. `listener(e)` is used only by the camera and has no audible effect (see 3D below).
- **Sound spec** is project field 39 of an object: `rate/minDist/x/oneShot/volume`, e.g. `4096/2/50/1/-10`. rate 4096 = recorded pitch
  (SetFrequency(nominal*rate>>12), clamped 100..100000 Hz; `sound_rate_set(e, r)` changes it and it persists). volume is dB, passed as
  `dB*100` to SetVolume (0 = full, -25 = quiet; the editor says -100 = silence). oneShot = bit 0, copied to element flag 0x10:
  0 = the sound **loops**, 1 = plays once. minDist is only used by 3D.
- **When a sound starts.** `play_sound(e)` and `on(e)` both do SetCurrentPosition(0) + Play (loop unless oneShot): playing again restarts it,
  never layers it. Initially-on elements with a looping sound start when the level starts (0x45bf90); one-shots do not. `off(e)` of an on
  element stops a looping sound (a one-shot plays out). `stop_sound(e)` stops only if the element is on. Level change releases everything.
- **Music/loops per level** (loop sounds on the camera, rate/volume from the spec): level1 title 5BFC2248 (52 s, vol 0); l3 3605735E (rate 3096, -10) plus
  ambience EC6F622D (14 s); l4/l7 51F89F4C (3096, -6); l5 3605735E (4096, -10); l6/l10/l15 F5AB6CCC (-8); l8 D655EC69 (-7); l9 DD5CD494 (-10);
  l11 58686051 (-8); l12 C057FE9C (-12); l13 EFED8DF9 (-15) plus 228822D4 (3096, -25); l14 5A847C69 plus two "Bots" loops. `sound_rate_set(camera, 5096)` speeds
  the music up while global 212 = 1.
- **Effects** (all one-shots, rate 2096..8096, volume 0..-25): jump = `Play-shadow` FC966A4D (-10), die 842C473D (only when speed points > 0), apple 7C6146F0,
  rings/secret box 4F18F935, "50 pts" 89608B16 (`on`), Get-Ready CCFAF032. There is **no rolling sound**.
- **3D is off.** 0x401753 sets DS3DMODE_DISABLE when option bits 11-12 (word 0xfe1c of the .col) are 0, and all 14 levels have them 0 (flags 0x3ad..0x5fd).
  So distance attenuation, pan and Doppler never apply: volume is just the spec volume, centred. The primary buffer runs at 22 kHz (bits 9-10 = 2);
  we mix at 48 kHz instead. The options dialog with "SOUND QUALITY" is the engine's developer menu, not a player option.
- **Exe oddities kept on purpose:** `Get-Ready`'s brain calls `play_sound` every tick (30 Hz) for ~1.1 s while the banner slides, restarting
  the 0.38 s sample each time (exactly what 0x40c200 does).

## Implementation

`SoundBank` (original code) parses the spec per element, keeps one voice per element and logs events. `rb::Audio` gained `speed(handle, ratio)`
(pitch from rate), `masterVolume(hundredthsDB)` and `takePeak()`; `PCM::srcRate` carries the WAV's own rate. Hooks: `IAudio::on/off` (two lines in
`Brain.cpp` for `on`/`off`, the only `src/brain` edit), `HostSink::on/off/setRate`.

- `RB_AUDIO_DUMP=out.wav` mixes offline (no device) into a 48 kHz float32 stereo WAV, advancing exactly one 1/60 s update step per game update.
- `RB_AUDIO_LOG=1` logs `AUDIO t=<audio clock> tick=<brain tick> start|stop|rate el=.. name=.. snd=.. why=play_sound|on|level|off|stop_sound loop vol rate`.
- `RB_VOLUME=0..100` (launcher) sets the master volume; the pause menu "Volume" row (Left/Right/Enter, steps of 10) saves `volume=` in `rollerbot.cfg`.
  Env wins over the saved value. **Master only**: the original has no music/effects split, only per-sound volumes. Android launcher Settings has a Volume row (`PortPrefs.volume`, exported as `RB_VOLUME`).
- `tools/audio/analyze.py run.wav run.log [--assets dir]` (numpy) reports peak, RMS per 0.5 s, events, retriggers, music seams; `tools/audio/verify.sh` runs the two scenarios; `keys2rb.py` turns an rbbrain key schedule into `RB_KEYS`.

## Verification results (Xvfb in omtbuild, `SDL_AUDIODRIVER=dummy`, offline dump)

| run | result |
|---|---|
| level001, 3700 frames (title, then Space at frame 3500 -> level003) | music 5BFC2248 starts at t=0, RMS -21..-23 dBFS in all 123 windows (none silent), no discontinuity at the 52.25 s loop point (0.05x the p99.9 sample step); level3 music + ambience start at the level change (58.3 s). Peak -5.0 dBFS |
| level003, 1800 frames, no input | music (3605735E at 0.756x -> 19.35 s loop) and ambience (14.19 s) loop, seams 0.05x and 1.2x p99.9 (no click), 0 silent windows, peak -4.4 dBFS, 0 samples at full scale |
| level003, 2400 frames, recorded full route (`routes` lane) | apple pickups (7C6146F0, 8 apples, one sound each at 4.70 s, 5.13 s ...), jump (Play-shadow), ring + secret-box (4F18F935), die sound (PLAYER-1 x4), 50-pts `on` sound all fire once per event; peak -4.1 dBFS; level exit at tick 1146 loads level001 whose title music starts |
| level003 + `RB_VOLUME=50` | RMS 6.02 dB lower than 100% (master gain works) |
| pause menu | six items incl. `Volume: 100%` (screenshot `.shots/audio/menu.png`); the mix is frozen while it is open |

Nothing clips (max |sample| 0.63 before the clamp). Sounds that "play twice": none by different elements in one tick; the only repeats are the exe's own
`Get-Ready` retrigger above (one voice, restarted) - before this change the old HostAudio layered a new voice on every call.
Not verifiable headless: the actual audio device path (SDL dummy only), and sample-accurate timing against a real exe run (no exe audio recording exists).

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
