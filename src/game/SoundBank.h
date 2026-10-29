// SoundBank: the per-element sound behaviour of Rollerbot.exe on top of rb::Audio (see docs/audio.md).
// Every element owns at most one DirectSound buffer described by project field 39 ("rate/minDist/x/oneShot/volume").
// No 3D positioning is applied: the exe sets DS3DMODE_DISABLE unless option bits 11-12 are set, and no shipped level sets them.
#pragma once
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "assets/Assets.h"
#include "audio/Audio.h"

namespace rb {

struct SoundSpec {
  std::string file;      // "./HASH.dat" (empty = element has no sound)
  std::string name;      // element name (debug log)
  int rate = 4096;       // initial rate, 4096 = recorded pitch (field 1; 0x401753 -> +0x84)
  bool oneShot = false;  // field 4 bit 0 -> element flag 0x10: Play without DSBPLAY_LOOPING
  int volume = 0;        // field 5, dB * 100 passed to SetVolume (0x401753)
};

class SoundBank {
 public:
  SoundBank(Audio& audio, const AssetStore& store) : a_(audio), store_(store) {}
  ~SoundBank();
  // Parse the project's elements; drops all voices. isOn(el) tells the element's current on flag.
  void load(const ProjectData& pd, std::function<bool(int)> isOn);
  // Level start (0x45bf90): loops of elements that are on start, everything else is stopped.
  void startLevel();
  void play(int el, const char* why);    // play_sound (0x40c200), on() (0x40110e): restart from zero
  void stop(int el, const char* why);    // stop_sound (0x40c440): only if the element is on
  void off(int el);                      // off() (0x40bff0): stops a looping sound
  void setRate(int el, int q12);         // sound_rate_set (0x40cc90)
  // Debug log (RB_AUDIO_LOG): one line per event, stamped with the audio clock and the brain tick.
  void setLog(bool on, std::function<unsigned()> tick) { log_ = on; tick_ = std::move(tick); }
 private:
  struct Slot { SoundSpec spec; std::shared_ptr<const PCM> pcm; Audio::Handle h = 0; int rate = 4096; bool tried = false; };
  Slot* slot(int el);
  void start(int el, const char* why);
  Audio& a_;
  const AssetStore& store_;
  std::vector<Slot> slots_;
  std::function<bool(int)> isOn_;
  std::function<unsigned()> tick_;
  std::map<std::string, std::shared_ptr<const PCM>> cache_;
  bool log_ = false;
};

// RB_AUDIO_DUMP: offline mix to a 32-bit float stereo WAV, advanced by the game's fixed update step.
class AudioDump {
 public:
  bool open(const std::string& path, int rate);
  void advance(Audio& a, double dt);
  void close();
  ~AudioDump() { close(); }
 private:
  std::FILE* f_ = nullptr;
  int rate_ = 48000;
  double carry_ = 0;
  unsigned long frames_ = 0;
};

}  // namespace rb
