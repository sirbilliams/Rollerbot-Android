// GameSave: high score survives a reload, never lowers; progress and settings persist.
#include "game/Save.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)
int main() {
  std::string d = (std::filesystem::temp_directory_path() / "rb_save_test").string();
  std::filesystem::remove_all(d);
  {
    rb::GameSave s(d);
    CHECK(s.highScore() == 0 && s.reached == 1);
    CHECK(s.submitScore(500) && !s.submitScore(400));
    s.reached = 7; s.lastLevel = 6; s.widescreen = 2; s.bottomScreen = false;
    CHECK(s.save());
  }
  rb::GameSave t(d);
  CHECK(t.highScore() == 500 && t.reached == 7 && t.lastLevel == 6 && t.widescreen == 2 && !t.bottomScreen);
  CHECK(std::filesystem::file_size(d + "/globvars.dat") == 8192);
  setenv("RB_SAVE", "/x/y", 1);
  CHECK(rb::saveDirectory("") == "/x/y");
  std::filesystem::remove_all(d);
  std::puts("ok");
  return 0;
}
