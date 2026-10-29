// rb::Bridge - tiny thread-safe meeting point between the running game and a second-screen UI
// (the Android companion panel reads it through JNI; a desktop UI could too). Read-only status in one
// direction, a few requests in the other. The game publishes with publish() and polls take*().
#pragma once
#include <string>
#include <vector>

namespace rb {

struct Status {
  bool running = false;
  int level = 0;                 // number in levelNNN.prj, 0 = none
  std::vector<int> levels;       // numbers of the levels present in the game folder
  int lives = 0, fruit = 0, points = 0;   // brain globals 220, 201, 203 (see docs/brain.md)
  bool paused = false;           // by the user (companion) or by app suspend
  int widescreen = 0;            // rb::Widescreen
  int highScore = 0;             // global 100, persisted
  int reached = 1;               // highest unlocked level number
  bool menuOpen = false;         // the in-game pause menu is showing
};

class Bridge {
 public:
  static void publish(const Status& s);
  static Status status();
  // requests from the UI side
  static void requestLevel(int n);       // load levelNNN
  static int takeLevelRequest();         // 0 = none
  static void requestPause(bool on);
  static bool pauseRequested();
  static void requestWidescreen(int m);  // -1 = none
  static int takeWidescreen();
  static void requestRestart();          // restart the current level
  static bool takeRestart();
  // "level|lives|fruit|points|paused|widescreen|1,2,3|highScore|reached|menu" for the JNI side
  static std::string statusLine();
};

}  // namespace rb
