#include "platform/Platform.h"

#include <SDL.h>
#include <SDL_opengles2.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace rb {

namespace {
const char* env(const char* k) { const char* v = std::getenv(k); return (v && *v) ? v : nullptr; }
std::string lower(std::string s) { for (auto& c : s) c = char(std::tolower((unsigned char)c)); return s; }
}  // namespace

void Config::applyEnv() {
  if (const char* v = env("RB_WIDESCREEN")) {
    std::string m = lower(v);
    if (m == "fill") widescreen = Widescreen::Fill;
    else if (m == "panels") widescreen = Widescreen::Panels;
    else if (m == "stretch") widescreen = Widescreen::Stretch;
    else if (m == "off") widescreen = Widescreen::Off;
  }
  if (const char* v = env("RB_FULLSCREEN")) fullscreen = std::atoi(v) != 0;
  if (const char* v = env("RB_SIZE")) {
    int w, h;
    if (std::sscanf(v, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) { width = w; height = h; }
  }
}

struct Platform::Impl {
  Config cfg;
  SDL_Window* win = nullptr;
  SDL_GLContext gl = nullptr;
  SDL_GameController* pad = nullptr;
  Input input;
  Viewport vp;
  Game* game = nullptr;
  bool quit = false, paused = false;
  uint32_t keyMask = 0, padMask = 0;
  long maxFrames = -1;
  std::string shotPath;

  void updateViewport() {
    int w, h;
    SDL_GL_GetDrawableSize(win, &w, &h);
    vp.winW = w; vp.winH = h; vp.mode = cfg.widescreen;
    vp.area = computeGameArea(w, h, cfg.widescreen);
    vp.glArea = {vp.area.x, h - (vp.area.y + vp.area.h), vp.area.w, vp.area.h};
  }
  static uint32_t bit(Action a) { return 1u << unsigned(a); }

  void setPausedState(bool p) {
    if (p == paused || !game) return;
    paused = p;
    if (p) game->pause(); else game->resume();
  }

  void openPad() {
    if (pad) return;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
      if (SDL_IsGameController(i) && (pad = SDL_GameControllerOpen(i))) break;
  }

  void recomputePad() {
    uint32_t m = 0;
    if (pad) {
      auto b = [&](SDL_GameControllerButton x) { return SDL_GameControllerGetButton(pad, x) != 0; };
      if (b(SDL_CONTROLLER_BUTTON_DPAD_UP)) m |= bit(Action::Up);
      if (b(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) m |= bit(Action::Down);
      if (b(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) m |= bit(Action::Left);
      if (b(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) m |= bit(Action::Right);
      if (b(SDL_CONTROLLER_BUTTON_A)) m |= bit(Action::Accept);
      if (b(SDL_CONTROLLER_BUTTON_B)) m |= bit(Action::Back);
      if (b(SDL_CONTROLLER_BUTTON_X)) m |= bit(Action::Jump);
      if (b(SDL_CONTROLLER_BUTTON_Y)) m |= bit(Action::Action2);
      if (b(SDL_CONTROLLER_BUTTON_START)) m |= bit(Action::Pause);
      if (b(SDL_CONTROLLER_BUTTON_BACK)) m |= bit(Action::Menu);
      const int dz = 12000;
      int ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
      int ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
      if (ax < -dz) m |= bit(Action::Left);
      if (ax > dz) m |= bit(Action::Right);
      if (ay < -dz) m |= bit(Action::Up);
      if (ay > dz) m |= bit(Action::Down);
    }
    padMask = m;
    input.setSource(1, m);
  }

  static uint32_t keyBit(SDL_Scancode sc) {
    switch (sc) {
      case SDL_SCANCODE_UP: case SDL_SCANCODE_W: return bit(Action::Up);
      case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S: return bit(Action::Down);
      case SDL_SCANCODE_LEFT: case SDL_SCANCODE_A: return bit(Action::Left);
      case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_D: return bit(Action::Right);
      case SDL_SCANCODE_RETURN: case SDL_SCANCODE_Z: return bit(Action::Accept);
      case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_X: case SDL_SCANCODE_AC_BACK: return bit(Action::Back);
      case SDL_SCANCODE_SPACE: return bit(Action::Jump);
      case SDL_SCANCODE_C: case SDL_SCANCODE_LCTRL: return bit(Action::Action2);
      case SDL_SCANCODE_P: return bit(Action::Pause);
      case SDL_SCANCODE_TAB: return bit(Action::Menu);
      default: return 0;
    }
  }

  void touch(int idx, bool down, float winX, float winY) {
    if (idx < 0 || idx >= Input::kMaxTouches) return;
    float gx, gy;
    windowToGame(vp.area, winX, winY, gx, gy);
    input.setTouch(idx, down, gx, gy);
  }

  void handle(const SDL_Event& e) {
    switch (e.type) {
      case SDL_QUIT: quit = true; break;
      case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) updateViewport();
        break;
      case SDL_KEYDOWN: case SDL_KEYUP: {
        uint32_t b = keyBit(e.key.keysym.scancode);
        if (e.type == SDL_KEYDOWN) keyMask |= b; else keyMask &= ~b;
        input.setSource(0, keyMask);
        break;
      }
      case SDL_CONTROLLERDEVICEADDED: openPad(); break;
      case SDL_CONTROLLERDEVICEREMOVED:
        if (pad && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
          SDL_GameControllerClose(pad); pad = nullptr;
        }
        recomputePad(); openPad(); break;
      case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP: case SDL_CONTROLLERAXISMOTION:
        recomputePad(); break;
      case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_FINGERUP: {
        // Slot by order of the touch's finger id (0..4); fine for the shell, lanes only need positions.
        int idx = int(e.tfinger.fingerId % Input::kMaxTouches);
        touch(idx, e.type != SDL_FINGERUP, e.tfinger.x * vp.winW, e.tfinger.y * vp.winH);
        break;
      }
      case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEMOTION:
        if (e.type != SDL_MOUSEMOTION || (e.motion.state & SDL_BUTTON_LMASK)) {
          if (e.motion.which != SDL_TOUCH_MOUSEID)  // desktop mouse acts as touch 0
            touch(0, e.type == SDL_MOUSEMOTION ? true : e.button.state == SDL_PRESSED,
                  float(e.type == SDL_MOUSEMOTION ? e.motion.x : e.button.x),
                  float(e.type == SDL_MOUSEMOTION ? e.motion.y : e.button.y));
        }
        break;
      default: break;
    }
  }
};

namespace {
// Android lifecycle events arrive from SDL's callback thread-ish path and must be handled at once.
int lifecycleWatch(void* ud, SDL_Event* e) {
  auto* d = static_cast<Platform::Impl*>(ud);
  switch (e->type) {
    case SDL_APP_WILLENTERBACKGROUND: d->setPausedState(true); break;   // incl. lid close / screen off
    case SDL_APP_DIDENTERFOREGROUND: d->setPausedState(false); break;
    case SDL_APP_TERMINATING: d->quit = true; break;
    default: break;
  }
  return 1;
}
}  // namespace

Platform::Platform() : d_(new Impl) {}
Platform::~Platform() {
  if (d_->pad) SDL_GameControllerClose(d_->pad);
  if (d_->gl) SDL_GL_DeleteContext(d_->gl);
  if (d_->win) SDL_DestroyWindow(d_->win);
  SDL_Quit();
}

bool Platform::create(const Config& cfg) {
  d_->cfg = cfg;
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
    SDL_Log("SDL_Init: %s", SDL_GetError());
    return false;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
  if (cfg.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#ifdef __ANDROID__
  flags |= SDL_WINDOW_FULLSCREEN;
#endif
  d_->win = SDL_CreateWindow(cfg.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             cfg.width, cfg.height, flags);
  if (!d_->win) { SDL_Log("SDL_CreateWindow: %s", SDL_GetError()); return false; }
  d_->gl = SDL_GL_CreateContext(d_->win);
  if (!d_->gl) { SDL_Log("SDL_GL_CreateContext: %s", SDL_GetError()); return false; }
  SDL_GL_SetSwapInterval(1);
  d_->updateViewport();
  d_->openPad();
  SDL_AddEventWatch(lifecycleWatch, d_.get());
  if (const char* v = env("RB_FRAMES")) d_->maxFrames = std::atol(v);
  if (const char* v = env("RB_SHOT")) d_->shotPath = v;
  if (!d_->shotPath.empty() && d_->maxFrames < 0) d_->maxFrames = 120;
  return true;
}

int Platform::run(Game& game) {
  Impl& d = *d_;
  d.game = &game;
  if (!game.init(*this)) { SDL_Log("game init failed"); return 1; }
  const bool deterministic = d.maxFrames >= 0;   // headless/test runs: exactly one step per frame
  uint64_t freq = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter();
  double acc = 0;
  long frame = 0;
  int rc = 0;
  while (!d.quit) {
    d.input.beginFrame();
    SDL_Event e;
    while (SDL_PollEvent(&e)) d.handle(e);
    d.recomputePad();

    uint64_t now = SDL_GetPerformanceCounter();
    acc += double(now - last) / double(freq);
    last = now;
    if (d.paused) { acc = 0; SDL_Delay(50); continue; }   // suspended: no updates, no rendering
    int steps = 0;
    if (deterministic) { steps = 1; acc = 0; }
    else {
      acc = std::min(acc, d.cfg.step * 5);
      while (acc >= d.cfg.step && steps < 5) { acc -= d.cfg.step; steps++; }
    }
    for (int i = 0; i < steps; i++) { d.input.endFrame(); game.update(d.cfg.step); d.input.beginFrame(); }
    if (steps == 0) d.input.endFrame();

    d.updateViewport();
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, d.vp.winW, d.vp.winH);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (d.vp.mode == Widescreen::Panels) {            // shell-painted side panels
      glEnable(GL_SCISSOR_TEST);
      int stripes = 8;
      for (int i = 0; i < stripes; i++) {
        float t = float(i) / stripes;
        glClearColor(0.10f + 0.10f * t, 0.12f + 0.08f * t, 0.22f + 0.12f * t, 1.f);
        int y0 = d.vp.winH * i / stripes, y1 = d.vp.winH * (i + 1) / stripes;
        glScissor(0, y0, d.vp.glArea.x, y1 - y0);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(d.vp.glArea.x + d.vp.glArea.w, y0, d.vp.winW, y1 - y0);
        glClear(GL_COLOR_BUFFER_BIT);
      }
      glDisable(GL_SCISSOR_TEST);
    }
    glViewport(d.vp.glArea.x, d.vp.glArea.y, d.vp.glArea.w, d.vp.glArea.h);
    game.render(d.vp);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, d.vp.winW, d.vp.winH);

    frame++;
    if (d.maxFrames >= 0 && frame >= d.maxFrames) {
      if (!d.shotPath.empty()) {
        if (screenshot(d.shotPath)) SDL_Log("screenshot: %s", d.shotPath.c_str());
        else { SDL_Log("screenshot failed: %s", d.shotPath.c_str()); rc = 2; }
      }
      d.quit = true;
    }
    SDL_GL_SwapWindow(d.win);
  }
  game.shutdown();
  d.game = nullptr;
  return rc;
}

void Platform::requestQuit() { d_->quit = true; }
const Input& Platform::input() const { return d_->input; }
const Viewport& Platform::viewport() const { return d_->vp; }
const Config& Platform::config() const { return d_->cfg; }

std::string Platform::dataDir() const {
  if (const char* v = env("RB_DATA")) return v;
  return ".";
}

std::string Platform::prefDir() const {
  char* p = SDL_GetPrefPath("sirbilliams", "rollerbot");
  std::string s = p ? p : "./";
  if (p) SDL_free(p);
  return s;
}

bool Platform::screenshot(const std::string& pngPath) {
  int w = d_->vp.winW, h = d_->vp.winH;
  std::vector<uint8_t> px(size_t(w) * h * 4), flipped(px.size());
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  for (int y = 0; y < h; y++)
    std::memcpy(&flipped[size_t(y) * w * 4], &px[size_t(h - 1 - y) * w * 4], size_t(w) * 4);
  return writePng(pngPath, flipped.data(), w, h);
}

}  // namespace rb
