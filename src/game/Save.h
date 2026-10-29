// Persistent player data for the real game: high score (global 100 of globvars.dat, as the exe keeps it, via rb::SaveStore)
// plus a small rollerbot.cfg (key=value) with progress and settings. No UI/platform types are exposed.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rb {

// Writable save folder: RB_SAVE, else the platform preference folder on Android, else $XDG_DATA_HOME/rollerbot
// (Flatpak sets that to its data dir) or ~/.local/share/rollerbot.
std::string saveDirectory(const std::string& androidPrefDir);

class GameSave {
 public:
  explicit GameSave(const std::string& dir);
  int highScore() const;
  bool submitScore(int score);          // raises the high score only; true when it changed
  int reached = 1;                      // highest level number unlocked
  int lastLevel = 1;                    // level to continue from (0/1 = none)
  int lastScore = 0;
  int widescreen = -1;                  // rb::Widescreen, -1 = never chosen
  bool bottomScreen = true;
  bool dirty() const { return dirty_; }
  void touch() { dirty_ = true; }
  bool save();                          // false (and a log line) when the folder is not writable
  const std::string& dir() const { return dir_; }
 private:
  struct Impl;
  std::string dir_;
  bool dirty_ = false;
  std::shared_ptr<Impl> d_;
};

}  // namespace rb
