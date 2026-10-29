#include "platform/Bridge.h"
#include <atomic>
#include <mutex>

namespace rb {
namespace {
std::mutex mu;
Status cur;
std::atomic<int> levelReq{0}, wsReq{-1}, volReq{-1}, langReq{-1};
std::atomic<bool> pauseReq{false}, restartReq{false};
}  // namespace

void Bridge::publish(const Status& s) { std::lock_guard<std::mutex> l(mu); cur = s; }
Status Bridge::status() { std::lock_guard<std::mutex> l(mu); return cur; }
void Bridge::requestLevel(int n) { levelReq = n; }
int Bridge::takeLevelRequest() { return levelReq.exchange(0); }
void Bridge::requestPause(bool on) { pauseReq = on; }
bool Bridge::pauseRequested() { return pauseReq; }
void Bridge::requestWidescreen(int m) { wsReq = m; }
int Bridge::takeWidescreen() { return wsReq.exchange(-1); }

void Bridge::requestVolume(int pct) { volReq = pct; }
int Bridge::takeVolume() { return volReq.exchange(-1); }
void Bridge::requestLang(int idx) { langReq = idx; }
int Bridge::takeLang() { return langReq.exchange(-1); }
void Bridge::requestRestart() { restartReq = true; }
bool Bridge::takeRestart() { return restartReq.exchange(false); }

std::string Bridge::statusLine() {
  Status s = status();
  std::string o = std::to_string(s.level) + "|" + std::to_string(s.lives) + "|" + std::to_string(s.fruit) + "|" +
                  std::to_string(s.points) + "|" + (s.paused ? "1" : "0") + "|" + std::to_string(s.widescreen) + "|";
  for (size_t i = 0; i < s.levels.size(); i++) o += (i ? "," : "") + std::to_string(s.levels[i]);
  o += "|" + std::to_string(s.highScore) + "|" + std::to_string(s.reached) + "|" + (s.menuOpen ? "1" : "0") + "|" + std::to_string(s.volume) + "|" + std::to_string(s.lang) + "|" + std::to_string(s.langMask);
  return o;
}
}  // namespace rb
