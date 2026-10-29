#include "SoundBank.h"
#include <SDL.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace rb {

namespace {
std::vector<std::string> splitOn(const std::string& s, char c) {
  std::vector<std::string> v; std::stringstream ss(s); std::string t;
  while (std::getline(ss, t, c)) v.push_back(t);
  return v;
}
}  // namespace

SoundBank::~SoundBank() = default;

void SoundBank::load(const ProjectData& pd, std::function<bool(int)> isOn) {
  for (auto& s : slots_) if (s.h) a_.stop(s.h);
  slots_.clear(); isOn_ = std::move(isOn);
  slots_.resize(pd.objects.size());
  for (size_t i = 0; i < pd.objects.size(); ++i) {
    const ObjectRecord& o = pd.objects[i];
    SoundSpec& sp = slots_[i].spec;
    sp.name = o.name;
    if (o.sound.empty() || o.sound == "-" || o.sound.find("null_") != std::string::npos) continue;   // 0x401753 skips the null sound
    sp.file = o.sound;
    if (o.fields.size() > 38) {   // 0x401753: project field 0x27 (1-based), '/'-separated: 1 rate, 2 min distance, 4 one-shot bit, 5 volume
      auto f = splitOn(o.fields[38], '/');
      if (f.size() > 0) sp.rate = std::atoi(f[0].c_str());
      if (f.size() > 3) sp.oneShot = (std::atoi(f[3].c_str()) & 1) != 0;
      if (f.size() > 4) sp.volume = std::atoi(f[4].c_str());
    }
    slots_[i].rate = sp.rate;
  }
}

SoundBank::Slot* SoundBank::slot(int el) {
  if (el < 0 || size_t(el) >= slots_.size() || slots_[el].spec.file.empty()) return nullptr;
  Slot& s = slots_[el];
  if (!s.tried) {
    s.tried = true;
    auto it = cache_.find(s.spec.file);
    if (it == cache_.end()) {
      std::shared_ptr<const PCM> pcm;
      try { Bytes b = store_.read(s.spec.file.rfind("./", 0) == 0 ? s.spec.file.substr(2) : s.spec.file); pcm = a_.loadWav(b.data(), b.size()); }
      catch (const std::exception&) {}
      if (!pcm) SDL_Log("audio: cannot load %s", s.spec.file.c_str());
      it = cache_.emplace(s.spec.file, pcm).first;
    }
    s.pcm = it->second;
  }
  return s.pcm ? &s : nullptr;
}

static float ratioFor(int srcRate, int q12) {   // 0x4016ae: freq = nominal * rate >> 12, clamped to 100..100000 Hz
  long long f = (long long)srcRate * q12 >> 12;
  f = std::clamp<long long>(f, 100, 100000);
  return float(double(f) / double(std::max(1, srcRate)));
}

void SoundBank::start(int el, const char* why) {
  Slot* s = slot(el);
  if (!s) return;
  if (s->h) a_.stop(s->h);   // one buffer per element: starting restarts it (SetCurrentPosition(0) + Play)
  s->h = a_.play(s->pcm, !s->spec.oneShot);
  a_.volume(s->h, std::clamp(s->spec.volume * 100, -10000, 0));
  a_.speed(s->h, ratioFor(s->pcm->srcRate, s->rate));
  if (log_) SDL_Log("AUDIO t=%.3f tick=%u start el=%d name=%s snd=%s why=%s loop=%d vol=%d rate=%d", a_.mediaClock(), tick_ ? tick_() : 0, el,
                    s->spec.name.c_str(), s->spec.file.c_str(), why, s->spec.oneShot ? 0 : 1, s->spec.volume, s->rate);
}

void SoundBank::play(int el, const char* why) { start(el, why); }

void SoundBank::stop(int el, const char* why) {
  Slot* s = slot(el);
  if (!s || !isOn_ || !isOn_(el)) return;   // 0x40c440 needs the on flag
  if (s->h) a_.stop(s->h);
  s->h = 0;
  if (log_) SDL_Log("AUDIO t=%.3f tick=%u stop el=%d name=%s why=%s", a_.mediaClock(), tick_ ? tick_() : 0, el, s->spec.name.c_str(), why);
}

void SoundBank::off(int el) {
  Slot* s = slot(el);
  if (!s || s->spec.oneShot) return;   // 0x40bff0: only a looping sound is stopped; a one-shot plays out
  if (s->h) a_.stop(s->h);
  s->h = 0;
  if (log_) SDL_Log("AUDIO t=%.3f tick=%u stop el=%d name=%s why=off", a_.mediaClock(), tick_ ? tick_() : 0, el, s->spec.name.c_str());
}

void SoundBank::setRate(int el, int q12) {
  Slot* s = slot(el);
  if (!s) return;
  s->rate = q12;
  if (s->h) a_.speed(s->h, ratioFor(s->pcm->srcRate, q12));
  if (log_) SDL_Log("AUDIO t=%.3f tick=%u rate el=%d name=%s rate=%d", a_.mediaClock(), tick_ ? tick_() : 0, el, s->spec.name.c_str(), q12);
}

void SoundBank::startLevel() {
  for (size_t i = 0; i < slots_.size(); ++i) {
    if (slots_[i].spec.file.empty()) continue;
    if (!slots_[i].spec.oneShot && isOn_ && isOn_(int(i))) start(int(i), "level");
  }
}

// ---- RB_AUDIO_DUMP
static void put32(std::FILE* f, uint32_t v) { std::fwrite(&v, 4, 1, f); }
static void put16(std::FILE* f, uint16_t v) { std::fwrite(&v, 2, 1, f); }

bool AudioDump::open(const std::string& path, int rate) {
  close();
  f_ = std::fopen(path.c_str(), "wb");
  if (!f_) return false;
  rate_ = rate; carry_ = 0; frames_ = 0;
  std::fwrite("RIFF", 1, 4, f_); put32(f_, 0); std::fwrite("WAVEfmt ", 1, 8, f_);
  put32(f_, 16); put16(f_, 3); put16(f_, 2); put32(f_, uint32_t(rate)); put32(f_, uint32_t(rate * 8)); put16(f_, 8); put16(f_, 32);   // IEEE float, stereo
  std::fwrite("data", 1, 4, f_); put32(f_, 0);
  return true;
}
void AudioDump::advance(Audio& a, double dt) {
  if (!f_) return;
  carry_ += dt * rate_;
  size_t n = size_t(carry_);
  carry_ -= double(n);
  std::vector<float> buf(n * 2);
  a.mix(buf.data(), n);
  std::fwrite(buf.data(), 4, buf.size(), f_);
  frames_ += n;
}
void AudioDump::close() {
  if (!f_) return;
  uint32_t bytes = uint32_t(frames_ * 8);
  std::fseek(f_, 4, SEEK_SET); put32(f_, 36 + bytes);
  std::fseek(f_, 40, SEEK_SET); put32(f_, bytes);
  std::fclose(f_); f_ = nullptr;
}

}  // namespace rb
