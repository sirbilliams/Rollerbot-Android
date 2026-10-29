// rb::Platform - window + GLES2 context, fixed-step loop, abstract input, 4:3 game area.
// Public API of the platform lane. No SDL types leak through this header.
#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace rb {

// ---- Input -----------------------------------------------------------------
// Abstract actions; keyboard, SDL gamepad and touch all feed the same set.
enum class Action : uint8_t {
  Up, Down, Left, Right,
  Accept,      // keyboard Enter/Space/Z, pad A
  Back,        // Esc/X, pad B
  Jump,        // Space, pad X
  Action2,     // Ctrl/C, pad Y
  Pause,       // P, pad Start
  Menu,        // Tab, pad Back/Select
  Count
};

struct TouchPoint {
  bool down = false;
  float x = 0, y = 0;  // normalised game-area coords, 0..1 (may be outside in Fill/Off bars)
};

class Input {
 public:
  static constexpr int kMaxTouches = 5;
  bool held(Action a) const { return (held_ >> unsigned(a)) & 1u; }
  bool pressed(Action a) const { return (pressed_ >> unsigned(a)) & 1u; }   // edge this frame
  bool released(Action a) const { return (released_ >> unsigned(a)) & 1u; }
  const TouchPoint& touch(int i) const { return touches_[i]; }
  // Used by Platform (and tests) to feed state; each source ORs into its own mask.
  void setSource(int source, uint32_t heldMask);   // 0 keyboard, 1 pad, 2 touch/virtual
  void setTouch(int i, bool down, float x, float y) { touches_[i] = {down, x, y}; }
  void beginFrame() { pressed_ = released_ = 0; }
  void endFrame();
 private:
  uint32_t src_[3] = {0, 0, 0};
  uint32_t held_ = 0, pressed_ = 0, released_ = 0;
  TouchPoint touches_[kMaxTouches];
};

// ---- Widescreen / game area -------------------------------------------------
enum class Widescreen { Fill = 0, Panels = 1, Stretch = 2, Off = 3 };  // same order as SpongeBob's setting

struct Rect { int x, y, w, h; };
// Where the game's 4:3 image is drawn in a window of winW x winH (GL origin top-left here).
//  Fill: scale to the window width, pan/crop vertically. Panels/Off: 4:3 pillarboxed (Panels
//  additionally has the shell paint side panels). Stretch: whole window.
Rect computeGameArea(int winW, int winH, Widescreen mode);
// Window pixel -> normalised game-area coord.
void windowToGame(const Rect& area, float px, float py, float& gx, float& gy);

struct Viewport {
  Rect area;            // game area in window pixels, y from the top
  Rect glArea;          // same, y from the bottom (for glViewport)
  int winW = 0, winH = 0;
  Widescreen mode = Widescreen::Fill;
};

// ---- Game interface ---------------------------------------------------------
class Platform;
class Game {
 public:
  virtual ~Game() = default;
  virtual bool init(Platform& p) = 0;     // GL context is current
  virtual void update(double dt) = 0;     // fixed step, seconds
  virtual void render(const Viewport& vp) = 0;  // draw into vp.glArea; shell clears and paints bars
  virtual void pause() {}                 // app/lid suspend, focus loss
  virtual void resume() {}
  virtual void shutdown() {}
};

struct Config {
  std::string title = "Rollerbot";
  int width = 1280, height = 720;
  bool fullscreen = false;
  double step = 1.0 / 60.0;
  Widescreen widescreen = Widescreen::Fill;
  // Overridable by env: RB_WIDESCREEN=fill|panels|stretch|off, RB_FULLSCREEN=1, RB_SIZE=WxH,
  // RB_FRAMES=N (exit after N frames), RB_SHOT=path.png (screenshot of the last frame), RB_DATA=game dir.
  void applyEnv();
};

class Platform {
 public:
  Platform();
  ~Platform();
  bool create(const Config& cfg);             // window + GLES2 context
  int run(Game& game);                        // returns process exit code
  void requestQuit();
  const Input& input() const;
  const Viewport& viewport() const;
  const Config& config() const;
  std::string dataDir() const;                // RB_DATA or ./ (game files, read-only)
  std::string prefDir() const;                // writable saves/settings
  bool screenshot(const std::string& pngPath);  // reads the framebuffer now
  struct Impl;
 private:
  std::unique_ptr<Impl> d_;
};

// Writes an RGBA8 image (top row first) as PNG. Dependency-free (stored deflate).
bool writePng(const std::string& path, const uint8_t* rgba, int w, int h);

// Stub game used by main.cpp until the lanes plug in: clear + test pattern.
std::unique_ptr<Game> makeStubGame();

}  // namespace rb
