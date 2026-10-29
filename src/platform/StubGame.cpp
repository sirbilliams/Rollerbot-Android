// Stub game for the empty shell: gradient bars, a moving square, input indicators, touch dots.
#include "platform/Platform.h"

#include <SDL_opengles2.h>
#include <cmath>

namespace rb {
namespace {

class StubGame : public Game {
 public:
  bool init(Platform& p) override { plat_ = &p; return true; }
  void update(double dt) override {
    t_ += dt;
    const Input& in = plat_->input();
    if (in.held(Action::Left)) px_ -= 0.4 * dt;
    if (in.held(Action::Right)) px_ += 0.4 * dt;
    if (in.held(Action::Up)) py_ -= 0.4 * dt;
    if (in.held(Action::Down)) py_ += 0.4 * dt;
    if (in.pressed(Action::Back)) plat_->requestQuit();
  }
  void render(const Viewport& vp) override {
    glEnable(GL_SCISSOR_TEST);
    // Background: 8 colour bars
    static const float bars[8][3] = {{.75f,.75f,.75f},{.75f,.75f,0},{0,.75f,.75f},{0,.75f,0},
                                     {.75f,0,.75f},{.75f,0,0},{0,0,.75f},{.1f,.1f,.1f}};
    for (int i = 0; i < 8; i++) fill(vp, i / 8.f, 0.f, 1.f / 8, 0.7f, bars[i][0], bars[i][1], bars[i][2]);
    // Lower strip: checker to expose scaling
    for (int y = 0; y < 3; y++)
      for (int x = 0; x < 32; x++)
        fill(vp, x / 32.f, 0.7f + y * 0.1f, 1.f / 32, 0.1f,
             ((x + y) & 1) ? 0.9f : 0.15f, ((x + y) & 1) ? 0.9f : 0.15f, ((x + y) & 1) ? 0.9f : 0.2f);
    // Border markers so cropping is visible: red corners
    float m = 0.02f;
    fill(vp, 0, 0, m, m, 1, 0, 0); fill(vp, 1 - m, 0, m, m, 1, 0, 0);
    fill(vp, 0, 1 - m, m, m, 1, 0, 0); fill(vp, 1 - m, 1 - m, m, m, 1, 0, 0);
    // Moving square (orbit + input offset)
    float cx = 0.5f + float(px_) + 0.2f * float(std::cos(t_ * 2));
    float cy = 0.35f + float(py_) + 0.15f * float(std::sin(t_ * 2));
    fill(vp, cx - 0.04f, cy - 0.05f, 0.08f, 0.1f, 1, 1, 1);
    // Input indicators: one cell per action, lit while held
    const Input& in = plat_->input();
    for (int a = 0; a < int(Action::Count); a++)
      fill(vp, 0.03f + a * 0.04f, 0.04f, 0.03f, 0.04f,
           in.held(Action(a)) ? 1.f : 0.2f, in.held(Action(a)) ? 0.9f : 0.2f, 0.f);
    // Touch dots
    for (int i = 0; i < Input::kMaxTouches; i++) {
      const TouchPoint& tp = in.touch(i);
      if (tp.down) fill(vp, tp.x - 0.02f, tp.y - 0.027f, 0.04f, 0.054f, 1, 0.5f, 0);
    }
    glDisable(GL_SCISSOR_TEST);
  }
  void pause() override {}
  void resume() override {}

 private:
  // Rect in game-area coords (0..1, y down), drawn with scissor + clear (no shaders needed).
  static void fill(const Viewport& vp, float x, float y, float w, float h, float r, float g, float b) {
    int sx = vp.glArea.x + int(std::floor(x * vp.glArea.w));
    int ex = vp.glArea.x + int(std::ceil((x + w) * vp.glArea.w));
    int top = vp.glArea.y + vp.glArea.h - int(std::floor(y * vp.glArea.h));
    int bot = vp.glArea.y + vp.glArea.h - int(std::ceil((y + h) * vp.glArea.h));
    glScissor(sx, bot, ex - sx, top - bot);
    glClearColor(r, g, b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  Platform* plat_ = nullptr;
  double t_ = 0, px_ = 0, py_ = 0;
};

}  // namespace

std::unique_ptr<Game> makeStubGame() { return std::make_unique<StubGame>(); }

}  // namespace rb
