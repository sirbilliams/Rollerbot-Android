// Headless checks: layout math, input edges, PNG writer. No window needed.
#include "platform/Platform.h"
#include "platform/Bridge.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main() {
  using namespace rb;
  Rect r = computeGameArea(1920, 1080, Widescreen::Off);
  CHECK(r.w == 1440 && r.h == 1080 && r.x == 240 && r.y == 0);
  r = computeGameArea(1920, 1080, Widescreen::Panels);
  CHECK(r.w == 1440 && r.x == 240);
  r = computeGameArea(1920, 1080, Widescreen::Fill);
  CHECK(r.w == 1920 && r.h == 1440 && r.x == 0 && r.y == -180);
  r = computeGameArea(1920, 1080, Widescreen::Stretch);
  CHECK(r.w == 1920 && r.h == 1080);
  r = computeGameArea(800, 800, Widescreen::Fill);          // narrower than 4:3: letterbox
  CHECK(r.w == 800 && r.h == 600 && r.y == 100);
  r = computeGameArea(600, 800, Widescreen::Off);           // portrait
  CHECK(r.w == 600 && r.h == 450 && r.y == 175);
  float gx, gy;
  windowToGame({240, 0, 1440, 1080}, 960, 540, gx, gy);
  CHECK(std::abs(gx - 0.5f) < 1e-4f && std::abs(gy - 0.5f) < 1e-4f);

  Input in;
  in.beginFrame();
  in.setSource(0, 1u << unsigned(Action::Accept));
  in.endFrame();
  CHECK(in.held(Action::Accept) && in.pressed(Action::Accept) && !in.released(Action::Accept));
  in.beginFrame(); in.endFrame();
  CHECK(in.held(Action::Accept) && !in.pressed(Action::Accept));
  in.beginFrame(); in.setSource(0, 0); in.endFrame();
  CHECK(!in.held(Action::Accept) && in.released(Action::Accept));

  Status st; st.level = 3; st.lives = 5; st.fruit = 7; st.points = 9; st.paused = true; st.widescreen = 2; st.levels = {1, 3, 4};
  Bridge::publish(st);
  CHECK(Bridge::statusLine() == "3|5|7|9|1|2|1,3,4|0|1|0");
  Bridge::requestLevel(4); CHECK(Bridge::takeLevelRequest() == 4 && Bridge::takeLevelRequest() == 0);
  Bridge::requestWidescreen(1); CHECK(Bridge::takeWidescreen() == 1 && Bridge::takeWidescreen() == -1);
  Bridge::requestPause(true); CHECK(Bridge::pauseRequested()); Bridge::requestPause(false);

  std::vector<uint8_t> px(4 * 3 * 4, 200);
  const char* tmp = std::getenv("TMPDIR");
  std::string path = std::string(tmp ? tmp : "/tmp") + "/rb_platform_test.png";
  CHECK(writePng(path, px.data(), 4, 3));
  FILE* f = std::fopen(path.c_str(), "rb");
  unsigned char sig[8] = {};
  CHECK(f && std::fread(sig, 1, 8, f) == 8 && sig[1] == 'P');
  if (f) { std::fclose(f); std::remove(path.c_str()); }
  std::printf(fails ? "platform tests FAILED\n" : "platform tests ok\n");
  return fails ? 1 : 0;
}
