#include "game/Save.h"
#include "ui/Ui.h"
#include <SDL.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace rb {
namespace fs = std::filesystem;

struct GameSave::Impl {
  explicit Impl(const std::string& d) : store(d) {}
  SaveStore store;
};

std::string saveDirectory(const std::string& androidPrefDir) {
  if (const char* v = std::getenv("RB_SAVE")) if (*v) return v;
#ifdef __ANDROID__
  if (!androidPrefDir.empty()) return androidPrefDir;
#else
  (void)androidPrefDir;
#endif
  if (const char* x = std::getenv("XDG_DATA_HOME")) if (*x) return std::string(x) + "/rollerbot";
  if (const char* h = std::getenv("HOME")) if (*h) return std::string(h) + "/.local/share/rollerbot";
  return "./rollerbot-save";
}

GameSave::GameSave(const std::string& dir) : dir_(dir), d_(std::make_shared<Impl>(dir)) {
  try { d_->store.load(); } catch (const std::exception& e) { SDL_Log("save: %s (starting fresh)", e.what()); d_ = std::make_shared<Impl>(dir); }
  std::ifstream f(fs::path(dir) / "rollerbot.cfg");
  std::string line;
  while (std::getline(f, line)) {
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq);
    int v = std::atoi(line.c_str() + eq + 1);
    if (k == "reached") reached = std::max(1, v);
    else if (k == "last") lastLevel = v;
    else if (k == "score") lastScore = v;
    else if (k == "widescreen") widescreen = v;
    else if (k == "bottom") bottomScreen = v != 0;
    else if (k == "volume") volume = v;
    else if (k == "lang") lang = line.substr(eq + 1);
  }
}

int GameSave::highScore() const { return std::max(0, d_->store.highScore()); }

bool GameSave::submitScore(int score) {
  if (score <= d_->store.highScore()) return false;
  d_->store.submitScore(score);
  dirty_ = true;
  return true;
}

bool GameSave::save() {
  try {
    d_->store.save();
    fs::path p = fs::path(dir_) / "rollerbot.cfg", t = fs::path(dir_) / "rollerbot.cfg.tmp";
    {
      std::ofstream o(t, std::ios::trunc);
      o << "reached=" << reached << "\nlast=" << lastLevel << "\nscore=" << lastScore << "\nwidescreen=" << widescreen
        << "\nbottom=" << (bottomScreen ? 1 : 0) << "\nvolume=" << volume << "\nlang=" << lang << "\n";
      if (!o) throw std::runtime_error("write failed");
    }
    fs::rename(t, p);
    dirty_ = false;
    return true;
  } catch (const std::exception& e) {
    SDL_Log("save to %s failed: %s", dir_.c_str(), e.what());
    dirty_ = false;   // do not retry every frame
    return false;
  }
}

}  // namespace rb
