#include "game/Game.h"
#include "assets/Assets.h"
#include "audio/Audio.h"
#include "SoundBank.h"
#include "game/BrainHost.h"
#include "game/MenuRaster.h"
#include "game/Save.h"
#include "platform/Bridge.h"
#include "scene/Scene.h"
#include <SDL.h>
#include <SDL_opengles2.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <sstream>
#include <chrono>

namespace rb {
namespace {

const char* envs(const char* k) { const char* v = std::getenv(k); return (v && *v) ? v : nullptr; }
const float kPi = 3.14159265358979f;

struct Model {
  bool ok = false, billboard = false;
  std::vector<MeshId> meshes;
  TextureId tex = 0;
  float aspect = 1;
};


// Minimal 2D textured-quad renderer for the HUD (640x480 canvas mapped onto the game area).
class Overlay2D {
 public:
  bool init() {
    const char* vs = "attribute vec2 p; attribute vec2 t; varying vec2 uv; void main(){ uv=t; gl_Position=vec4(p,0.0,1.0);} ";
    const char* fs = "precision mediump float; varying vec2 uv; uniform sampler2D tex; uniform vec4 clip; void main(){ gl_FragColor=texture2D(tex,uv); }";
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return false;
    prog_ = glCreateProgram();
    glAttachShader(prog_, v); glAttachShader(prog_, f);
    glBindAttribLocation(prog_, 0, "p"); glBindAttribLocation(prog_, 1, "t");
    glLinkProgram(prog_);
    GLint ok = 0; glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    return ok != 0;
  }
  // straight-alpha RGBA, top-down (the pause menu canvas)
  GLuint uploadRGBA(int w, int h, const uint8_t* rgba) {
    GLuint t = 0; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
  }
  void release(GLuint t) { if (t) glDeleteTextures(1, &t); }
  // rgba pixels, top-down; black (0,0,0) becomes transparent when key is set (D3DRM decal transparency).
  GLuint upload(int w, int h, const uint8_t* rgb, bool key) {
    std::vector<uint8_t> px(size_t(w) * h * 4);
    for (int i = 0; i < w * h; ++i) {
      px[i * 4] = rgb[i * 3]; px[i * 4 + 1] = rgb[i * 3 + 1]; px[i * 4 + 2] = rgb[i * 3 + 2];
      px[i * 4 + 3] = (key && !rgb[i * 3] && !rgb[i * 3 + 1] && !rgb[i * 3 + 2]) ? 0 : 255;
    }
    GLuint t = 0; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
  }
  void begin(const Viewport& vp) {
    vp_ = vp;
    if (vp.mode == Widescreen::Fill) {   // HUD gets its own 4:3 space fitted inside the whole window, not the cropped 3D area
      int gw = std::min(vp.winW, vp.winH * 4 / 3), gh = gw * 3 / 4;
      vp_.glArea = {(vp.winW - gw) / 2, (vp.winH - gh) / 2, gw, gh};
    }
    glViewport(0, 0, vp.winW, vp.winH);   // NDC below is window-relative
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(prog_);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vp_.glArea.x, vp_.glArea.y, vp_.glArea.w, vp_.glArea.h);
  }
  // Source rect (sx..sx+sw of the texture's tw width) drawn at canvas (x,y,w,h) in 640x480 units.
  void draw(GLuint tex, float tw, float th, float sx, float sy, float sw, float sh, float x, float y, float w, float h) {
    float x0 = vp_.glArea.x + x / 640.f * vp_.glArea.w, x1 = vp_.glArea.x + (x + w) / 640.f * vp_.glArea.w;
    float y0 = vp_.glArea.y + (1.f - y / 480.f) * vp_.glArea.h, y1 = vp_.glArea.y + (1.f - (y + h) / 480.f) * vp_.glArea.h;
    auto nx = [&](float px) { return px / float(vp_.winW) * 2.f - 1.f; };
    auto ny = [&](float py) { return py / float(vp_.winH) * 2.f - 1.f; };
    float u0 = sx / tw, u1 = (sx + sw) / tw, v0 = sy / th, v1 = (sy + sh) / th;
    float q[] = {nx(x0), ny(y0), u0, v0, nx(x1), ny(y0), u1, v0, nx(x1), ny(y1), u1, v1,
                 nx(x0), ny(y0), u0, v0, nx(x1), ny(y1), u1, v1, nx(x0), ny(y1), u0, v1};
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, q); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, q + 2);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0); glDisableVertexAttribArray(1);
  }
  void end() { glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glUseProgram(0); }
 private:
  static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type); glShaderSource(s, 1, &src, nullptr); glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[256]; glGetShaderInfoLog(s, 256, nullptr, log); SDL_Log("overlay shader: %s", log); return 0; }
    return s;
  }
  GLuint prog_ = 0;
  Viewport vp_;
};

// Level settings the exe keeps in the last 512 bytes of the .col (0x4220.. loader: 0xfe00..0xfe20, see docs/game.md).
struct LevelCfg {
  float fogStart = 150, fogEnd = 300;   // metres (0xfe00/0xfe04, scaled <<9 * 1/512)
  Color color{0, 0, 0, 1};              // 0xfe08: scene background and fog colour (ARGB)
  float skyScale = 1000;                // 0xfe0c: sky dome scale, also the far clip
  int skyLow = 120, skyHigh = 479;      // 0xfe10 / 0xfe14: texture rows spanned by the sky
  int hood = 0;                         // 0xfe18: hoodNa.x / hoodNb.x
  unsigned flags = 783;                 // 0xfe1c: bit 0x10 = fog on
  int percent = 100;                    // 0xfe20: front clip = percent/100
  std::string skyTexture;               // first extra line of the .prj: "./HASH.dat|N|"
};

// Scripted input for headless verification: RB_KEYS="75@0-300,57@100-110" (scancode@firstFrame-lastFrame).
struct KeyScript { int sc, from, to; std::chrono::steady_clock::time_point t{}; bool seen = false; };

class RollerGame : public Game, public HostSink, public HostInput {
 public:
  explicit RollerGame(std::string dir) : dir_(std::move(dir)) {}

  bool init(Platform& p) override {
    plat_ = &p;
    std::string err;
    if (!scene_.init(&err)) { SDL_Log("scene init: %s", err.c_str()); return false; }
    save_.reset(new GameSave(saveDirectory(plat_->prefDir())));
    parseLangs();
    try { store_.open(dir_); } catch (const std::exception& e) { SDL_Log("assets: %s", e.what()); return false; }
    if (const char* v = envs("RB_AUDIO_DUMP")) {   // headless: mix offline to a float WAV, one update step at a time
      audio_.init(48000, false);
      audioOk_ = dump_.open(v, 48000);
      if (!audioOk_) SDL_Log("audio dump: cannot write %s", v);
    } else {
      audioOk_ = audio_.init(48000, true);
      if (!audioOk_) { audio_.init(48000, false); }
    }
    sounds_.reset(new SoundBank(audio_, store_));
    setVolume(envs("RB_VOLUME") ? std::atoi(envs("RB_VOLUME")) : (save_->volume >= 0 ? save_->volume : 100), false);
    sounds_->setLog(envs("RB_AUDIO_LOG") != nullptr, [this] { return host_ ? host_->tickCount() : 0u; });
    hudOk_ = hud_.init();
    SDL_Log("saves in %s (hi-score %d, reached level %d)", save_->dir().c_str(), save_->highScore(), save_->reached);
    if (save_->widescreen >= 0 && save_->widescreen <= 3 && !envs("RB_WIDESCREEN")) Bridge::requestWidescreen(save_->widescreen);
    if (envs("RB_CONTINUE") && save_->lastLevel > 1) { char nm[32]; std::snprintf(nm, sizeof nm, "level%03d", save_->lastLevel); level_ = nm; }
    if (const char* v = envs("RB_LEVEL")) level_ = v;
    if (const char* v = envs("RB_FIELD")) { field_ = std::strtof(v, nullptr); fieldFixed_ = true; }
    if (const char* v = envs("RB_NOHUD")) hudOn_ = false, (void)v;
    if (const char* v = envs("RB_KEYS")) {
      std::stringstream ss(v); std::string t;
      while (std::getline(ss, t, ',')) { KeyScript k{0, 0, 1 << 30}; std::sscanf(t.c_str(), "%d@%d-%d", &k.sc, &k.from, &k.to); script_.push_back(k); }
    }
    if (const char* v = envs("RB_SHOTS")) {  // "60:/path/a.png,200:/path/b.png"
      std::stringstream ss(v); std::string t;
      while (std::getline(ss, t, ',')) { auto c = t.find(':'); if (c != std::string::npos) shots_[std::atol(t.substr(0, c).c_str())] = t.substr(c + 1); }
    }
    amb_ = scene_.createLight(LightType::Ambient, {1, 1, 1, 1});
    scene_.addLight(scene_.root(), amb_);
    cam_ = scene_.createFrame();
    return loadLevel(level_);
  }

  void update(double dt) override {
    ++frame_;
    if (audioOk_) dump_.advance(audio_, dt);
    using clk = std::chrono::steady_clock;
    auto now = clk::now();
    if (!t0_.time_since_epoch().count()) { t0_ = now; lastRender_ = now; }
    for (auto& k : script_) if (k.sc && frame_ == k.from && !k.t.time_since_epoch().count()) k.t = now;   // stamp: key becomes active
    if (runSeconds_ > 0 && std::chrono::duration<double>(now - t0_).count() > runSeconds_) { logStats(); plat_->requestQuit(); }
    if (int n = Bridge::takeLevelRequest()) {   // companion level select
      char nm[32]; std::snprintf(nm, sizeof nm, "level%03d", n);
      if (store_.find(std::string(nm) + ".prj")) loadLevel(nm);
    }
    if (Bridge::takeRestart() && host_) loadLevel(level_);
    if (int v = Bridge::takeVolume(); v >= 0) setVolume(v, true);
    if (int l = Bridge::takeLang(); l >= 0 && l < 5) switchLanguage(kLangNames[l]);
    publishStatus();
    if (!host_) return;
    const Input& in = plat_->input();
    const bool title = level_ == "level001";   // the exe's Esc on the title screen leaves the game; elsewhere it opens the menu
    if (const char* v = envs("RB_MENU")) if (frame_ == std::atol(v)) openMenu();   // test hook: open the pause menu at that frame
    if (menuOpen_) { updateMenu(in); return; }
    if (in.pressed(Action::Pause) || in.pressed(Action::Menu) || (in.pressed(Action::Back) && !title && !envs("RB_NOQUIT"))) { openMenu(); return; }
    if (in.pressed(Action::Back) && !envs("RB_NOQUIT")) plat_->requestQuit();
    host_->advance(dt);
    trackProgress();
    int ec = host_->exitCode();
    if (ec != -1 && !pendingExit_) {
      pendingExit_ = true;
      SDL_Log("brain exit(%d) at tick %u", ec, host_->tickCount());
      char name[32];
      std::snprintf(name, sizeof name, "level%03d", ec);
      std::string next = (ec >= 1 && store_.find(std::string(name) + ".prj")) ? name : level_;
      if (ec == -3) { plat_->requestQuit(); return; }   // title screen: Esc = exit to Windows
      if (!envs("RB_NOEXIT")) loadLevel(next, true);
    }
  }

  void render(const Viewport& vp) override {
    if (!host_) return;
    {
      auto n = std::chrono::steady_clock::now();
      if (lastRender_.time_since_epoch().count()) frameMs_.push_back(std::chrono::duration<double, std::milli>(n - lastRender_).count());
      lastRender_ = n;
    }
    syncFrames();
    scene_.setViewport(vp.area.x, vp.area.y, vp.area.w, vp.area.h);
    scene_.setBackground(bg_);
    scene_.setFog(cfg_.flags & 0x10, bg_, cfg_.fogStart, cfg_.fogEnd);
    scene_.render();
    if (hudOn_ && hudOk_) drawHud(vp);
    if (menuOpen_ && hudOk_) drawMenu(vp);
    auto it = shots_.find(frame_);
    if (it != shots_.end()) plat_->screenshot(it->second);
  }

  void shutdown() override {
    if (!statsDone_) logStats();
    flushSave(true);
    Bridge::publish(Status{});
    dump_.close();
    audio_.shutdown();
  }

  void logStats() {
    statsDone_ = true;
    if (frameMs_.size() < 3) return;
    std::vector<double> v(frameMs_.begin() + 1, frameMs_.end());
    std::sort(v.begin(), v.end());
    double sum = 0; for (double x : v) sum += x;
    int over = 0; for (double x : v) if (x > 25.0) ++over;
    SDL_Log("frames %zu avg %.2f ms (%.1f fps) p50 %.2f p99 %.2f max %.2f, >25ms: %d", v.size(), sum / v.size(), 1000.0 * v.size() / sum,
            v[v.size() / 2], v[size_t(v.size() * 0.99)], v.back(), over);
    if (!latMs_.empty()) {
      double a = 0, mx = 0; for (double x : latMs_) { a += x; mx = std::max(mx, x); }
      SDL_Log("input latency (step -> brain tick): n %zu avg %.1f ms max %.1f ms", latMs_.size(), a / latMs_.size(), mx);
      for (double x : latMs_) SDL_Log("  latency %.1f ms", x);
    }
  }
  // App/lid suspend or user pause (platform calls these): silence and freeze audio too.
  void pause() override { paused_ = true; flushSave(true); if (audioOk_) audio_.pauseAll(); }
  void resume() override { paused_ = false; if (audioOk_ && !menuOpen_) audio_.resumeAll(); }

  // ---- HostSink
  void lightColor(int, int, int, int) override {}   // ambient is kept fully lit (see docs/game.md)
  void backgroundColor(int r, int g, int b) override { bg_ = {r / 255.f, g / 255.f, b / 255.f, 1}; }
  // The element's sound spec comes from the project (SoundBank); the name the brain passes is the same file.
  void play(int el, const std::string&) override { if (sounds_) sounds_->play(el, "play_sound"); }
  void stop(int el) override { if (sounds_) sounds_->stop(el, "stop_sound"); }
  void setRate(int el, int q12) override { if (sounds_) sounds_->setRate(el, q12); }
  void on(int el) override { if (sounds_) sounds_->play(el, "on"); }
  void off(int el) override { if (sounds_) sounds_->off(el); }

  // ---- HostInput
  bool key(int sc) override {
    for (auto& k : script_) if (k.sc == sc && frame_ >= k.from && frame_ <= k.to) {
      if (!k.seen && k.t.time_since_epoch().count()) {   // input latency: key active (update step) -> read by a brain tick
        k.seen = true;
        latMs_.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - k.t).count());
      }
      return true;
    }
    const Input& in = plat_->input();
    switch (sc) {
      case 29: return in.held(Action::Action2) || in.held(Action::Jump);
      case 57: return padJump_ == 'a' ? in.held(Action::Accept) : padJump_ == 'x' ? in.held(Action::Jump) : (in.held(Action::Accept) || in.held(Action::Jump));
      case 28: return in.held(Action::Accept);
      case 75: return in.held(Action::Left);
      case 77: return in.held(Action::Right);
      case 72: return in.held(Action::Up);
      case 80: return in.held(Action::Down);
      case 1: return in.held(Action::Back);
      default: return false;
    }
  }
  bool any() override { const Input& in = plat_->input(); return in.held(Action::Accept) || in.held(Action::Jump); }

 private:
  // Read-only status for the second-screen companion (rb::Bridge). Globals: 220 lives, 201 fruit, 203 points.
  void publishStatus() {
    Status st;
    st.running = host_ != nullptr;
    st.level = level_.size() > 5 ? std::atoi(level_.c_str() + 5) : 0;
    if (levels_.empty())
      for (int n = 1; n < 100; ++n) {
        char nm[32]; std::snprintf(nm, sizeof nm, "level%03d.prj", n);
        if (store_.find(nm)) levels_.push_back(n);
      }
    st.levels = levels_;
    if (host_) { st.lives = host_->globalValue(220); st.fruit = host_->globalValue(201); st.points = host_->globalValue(203); }
    st.paused = paused_;
    if (save_) { st.highScore = save_->highScore(); st.reached = save_->reached; }
    st.menuOpen = menuOpen_;
    st.volume = volume_;
    for (size_t i = 0; i < langs_.size(); ++i) { int k = langIndex(langs_[i].first); if (k >= 0) st.langMask |= 1 << k; }
    st.lang = std::max(0, langIndex(lang_));
    st.widescreen = int(plat_->config().widescreen);
    Bridge::publish(st);
  }
  struct Slot { FrameId frame = 0; Model* model = nullptr; bool tried = false; bool shown = false; };

  // ---- level setup
  void readConfig(const std::string& level) {
    cfg_ = LevelCfg{};
    try {
      ProjectData pd = ProjectData::load(store_, level + ".prj");
      const auto& c = pd.collection;
      auto at = [&](size_t byteOff) -> std::int32_t { size_t i = byteOff / 4; return i < c.size() ? c[i] : 0; };
      cfg_.fogStart = float(at(0xfe00)); cfg_.fogEnd = float(at(0xfe04));
      std::uint32_t argb = std::uint32_t(at(0xfe08));
      cfg_.color = {float((argb >> 16) & 255) / 255.f, float((argb >> 8) & 255) / 255.f, float(argb & 255) / 255.f, 1};
      cfg_.skyScale = float(at(0xfe0c)); cfg_.skyLow = at(0xfe10); cfg_.skyHigh = at(0xfe14);
      if (cfg_.skyHigh <= cfg_.skyLow) cfg_.skyHigh = 479;   // 0x28 loader default
      cfg_.hood = at(0xfe18);
      if (at(0xfe1c)) cfg_.flags = std::uint32_t(at(0xfe1c));
      cfg_.percent = at(0xfe20);
      if (cfg_.percent < 1 || cfg_.percent > 100) cfg_.percent = 100;
      if (!pd.extraLines.empty()) { auto f = splitBar(pd.extraLines[0]); if (!f.empty()) cfg_.skyTexture = f[0]; }
    } catch (const std::exception& e) { SDL_Log("config %s: %s", level.c_str(), e.what()); }
    bg_ = cfg_.color;
  }

  // Sky dome (0x402216): hoodNa.x / hoodNb.x scaled, cylinder-wrapped with the level's bitmap, drawn without z-buffer
  // behind everything and following the camera position.
  void buildSky() {
    for (FrameId f : sky_) scene_.destroyFrame(f);
    sky_.clear();
    if (cfg_.skyTexture.empty()) return;
    TextureId tex = loadTexture(cfg_.skyTexture, nullptr, false);
    const float s = cfg_.skyScale - float(101 - cfg_.percent) * cfg_.skyScale * 0.001f;   // 0x471f14 minus (101-0x471f18)*...
    const float hi = (float(cfg_.skyHigh) - 238.f) / 240.f * s, lo = (float(cfg_.skyLow) - 242.f) / 240.f * s;
    const float F = hi - lo, ov = hi / F;
    for (const char half : {'a', 'b'}) {
      char name[32]; std::snprintf(name, sizeof name, "hood%d%c.x", cfg_.hood, half);
      try {
        Mesh gm = Mesh::decode(store_.read(name));
        MeshData md; Material m; m.texture = tex; md.materials.push_back(m);
        for (auto& part : gm.parts) {
          uint32_t base = uint32_t(md.positions.size());
          for (auto& v : part.vertices) {
            Vec3 f{v[0] * s, v[1] * s, v[2] * s};   // file coordinates (z = pole), as loaded by D3DRM
            // CYLINDER wrap: dir (0,1,0), up (0,0,-1), ou 0, ov, su -1, sv -1/F (0x402216)
            float ang = std::atan2(f.x, f.y);
            md.uvs.push_back({-ang / (2 * kPi), ov + (-f.z) * (-1.f / F)});
            md.positions.push_back({f.x, f.z, f.y});   // z-up file -> y-up world (GUESS: pole must point up)
          }
          for (auto& fc : part.faces)
            for (size_t k = 2; k < fc.size(); ++k) {
              MeshData::Face face{{base + fc[0], base + fc[k - 1], base + fc[k]}, 0};
              if (!envs("RB_SKYNOFLIP")) std::swap(face.v[1], face.v[2]);
              md.faces.push_back(face);
            }
        }
        MeshId id = scene_.createMesh(md);
        FrameId fr = scene_.createFrame();
        scene_.addVisual(fr, id);
        scene_.setZBuffer(fr, false);
        sky_.push_back(fr);
      } catch (const std::exception& e) { SDL_Log("sky %s: %s", name, e.what()); }
    }
  }

  bool loadLevel(const std::string& level, bool carryGlobals = false) {
    std::vector<int> g;
    if (carryGlobals && host_) g = host_->globals();
    for (auto& s : slots_) if (s.frame) scene_.destroyFrame(s.frame);
    slots_.clear(); hudTex_.clear();
    host_.reset(new BrainHost);
    std::string err;
    if (!host_->load(dir_, level, this, this, &err)) { SDL_Log("load %s: %s", level.c_str(), err.c_str()); host_.reset(); return false; }
    if (!g.empty()) host_->setGlobals(g);
    try {
      ProjectData pd = ProjectData::load(store_, level.find('.') == std::string::npos ? level + ".prj" : level);
      sounds_->load(pd, [this](int el) { return host_ && el >= 0 && el < host_->count() && host_->view(el).on; });
      sounds_->startLevel();
    } catch (const std::exception& e) { SDL_Log("sounds %s: %s", level.c_str(), e.what()); }
    level_ = level; pendingExit_ = false;
    {   // the high score lives in global 100 (as the exe keeps it) and survives sessions
      std::vector<int> g2 = host_->globals();
      if (g2.size() > 100 && save_ && g2[100] < save_->highScore()) { g2[100] = save_->highScore(); host_->setGlobals(g2); }
      int n = level.size() > 5 ? std::atoi(level.c_str() + 5) : 0;
      if (save_ && n > 0) {
        if (n > save_->reached) save_->reached = n;
        if (n > 1) save_->lastLevel = n;
        save_->touch(); flushSave(true);
      }
    }
    readConfig(level);
    buildSky();
    if (!envs("RB_FIELD")) fieldFixed_ = false;
    slots_.resize(host_->count());
    SDL_Log("loaded %s: %d elements, fog %s %.0f..%.0f", level.c_str(), host_->count(), (cfg_.flags & 0x10) ? "on" : "off", cfg_.fogStart, cfg_.fogEnd);
    return true;
  }

  // ---- persistence
  void trackProgress() {
    if (!save_ || !host_) return;
    int h = host_->globalValue(100);
    if (h > 0) save_->submitScore(h);
    if (level_ != "level001") { int p = host_->globalValue(203); if (p != save_->lastScore) { save_->lastScore = p; save_->touch(); } }
    flushSave(false);
  }
  void flushSave(bool force) {
    if (!save_ || !save_->dirty()) return;
    if (!force && ++sinceSave_ < 300) return;   // about every 5 s while something changed
    sinceSave_ = 0;
    save_->save();
  }

  // ---- pause menu (desktop Esc / Start; the companion mirrors it through rb::Bridge)
  static const char* wsName(int m) { static const char* n[] = {"Fill", "Side panels", "Stretch", "Original 4:3"}; return n[m & 3]; }
  std::vector<int> unlocked() const {
    std::vector<int> v;
    for (int n : levels_) if (n <= (save_ ? save_->reached : 1)) v.push_back(n);
    return v;
  }
  void openMenu() {
    menuOpen_ = true; menuSel_ = 0; menuDirty_ = true;
    menuLevel_ = level_.size() > 5 ? std::atoi(level_.c_str() + 5) : 1;
    flushSave(true);
    if (audioOk_) audio_.pauseAll();
  }
  void closeMenu() { menuOpen_ = false; if (audioOk_ && !paused_) audio_.resumeAll(); }
  // Menu entries by action id: 0 Resume, 1 Restart level, 2 Level select, 3 Widescreen, 4 Volume, 5 Game language (only with 2+ installed sets), 6 Quit game
  std::vector<int> menuActions() const {
    std::vector<int> a = {0, 1, 2, 3, 4};
    if (langs_.size() > 1) a.push_back(5);
    a.push_back(6);
    return a;
  }
  void updateMenu(const Input& in) {
    auto acts = menuActions();
    const int kItems = int(acts.size());
    auto lv = unlocked();
    if (in.pressed(Action::Up)) { menuSel_ = (menuSel_ + kItems - 1) % kItems; menuDirty_ = true; }
    if (in.pressed(Action::Down)) { menuSel_ = (menuSel_ + 1) % kItems; menuDirty_ = true; }
    menuSel_ = std::min(menuSel_, kItems - 1);
    int act = acts[menuSel_];
    int dx = (in.pressed(Action::Right) ? 1 : 0) - (in.pressed(Action::Left) ? 1 : 0);
    if (dx && act == 2 && !lv.empty()) {
      size_t i = std::find(lv.begin(), lv.end(), menuLevel_) - lv.begin();
      i = (i + lv.size() + dx) % lv.size();
      menuLevel_ = lv[i]; menuDirty_ = true;
    }
    bool ok = in.pressed(Action::Accept) || in.pressed(Action::Jump);
    if (dx && act == 3) cycleWidescreen(dx);
    if (dx && act == 4) setVolume(volume_ + 10 * dx, true);
    if (dx && act == 5) cycleLanguage(dx);
    if (ok) {
      switch (act) {
        case 0: closeMenu(); break;
        case 1: closeMenu(); loadLevel(level_); break;
        case 2: { char nm[32]; std::snprintf(nm, sizeof nm, "level%03d", menuLevel_);
                  if (store_.find(std::string(nm) + ".prj")) { closeMenu(); loadLevel(nm); } break; }
        case 3: cycleWidescreen(1); break;
        case 4: setVolume(volume_ >= 100 ? 0 : volume_ + 10, true); break;
        case 5: cycleLanguage(1); break;
        case 6: flushSave(true); plat_->requestQuit(); break;
      }
    } else if (in.pressed(Action::Back) || in.pressed(Action::Pause) || in.pressed(Action::Menu)) closeMenu();
  }
  // ---- game language: the installed sets come from RB_LANGS ("english=/path;french=/path"), the running one from the save or RB_LANG
  static constexpr const char* kLangNames[5] = {"english", "french", "german", "spanish", "italian"};
  static constexpr const char* kLangLabels[5] = {"EN", "FR", "DE", "ES", "IT"};
  static int langIndex(const std::string& n) { for (int i = 0; i < 5; ++i) if (n == kLangNames[i]) return i; return -1; }
  void parseLangs() {
    if (const char* v = envs("RB_LANGS")) {
      std::stringstream ss(v); std::string t;
      while (std::getline(ss, t, ';')) { auto e = t.find('='); if (e != std::string::npos) langs_.push_back({t.substr(0, e), t.substr(e + 1)}); }
    }
    std::string want = envs("RB_LANG") ? envs("RB_LANG") : (save_ ? save_->lang : "");
    for (auto& l : langs_) if (l.second == dir_) lang_ = l.first;
    if (!want.empty() && !envs("RB_DATA_FIXED")) for (auto& l : langs_) if (l.first == want && l.second != dir_) { dir_ = l.second; lang_ = l.first; }
    if (save_ && !lang_.empty()) save_->lang = lang_;   // the launcher reads it back after the game
  }
  void cycleLanguage(int d) {
    if (langs_.size() < 2) return;
    size_t cur = 0;
    for (size_t i = 0; i < langs_.size(); ++i) if (langs_[i].first == lang_) cur = i;
    switchLanguage(langs_[(cur + langs_.size() + d) % langs_.size()].first);
  }
  // Switch the whole asset set at once (art, sound and text differ per language) and restart the level.
  void switchLanguage(const std::string& name) {
    for (auto& l : langs_) {
      if (l.first != name || name == lang_) continue;
      std::string old = dir_;
      try { store_.open(l.second); } catch (const std::exception& e) { SDL_Log("language %s: %s", name.c_str(), e.what()); try { store_.open(old); } catch (const std::exception&) {} return; }
      dir_ = l.second; lang_ = name;
      models_.clear(); tex_.clear(); texAspect_.clear(); hudTex_.clear();
      sounds_.reset(new SoundBank(audio_, store_));
      sounds_->setLog(envs("RB_AUDIO_LOG") != nullptr, [this] { return host_ ? host_->tickCount() : 0u; });
      if (save_) { save_->lang = name; save_->touch(); flushSave(true); }
      SDL_Log("game language: %s (%s)", name.c_str(), dir_.c_str());
      if (host_) loadLevel(level_);
      menuDirty_ = true;
      return;
    }
  }
  // Master volume in percent (not an original control: the exe only has per-sound volumes). Applied after the mix.
  void setVolume(int pct, bool save) {
    volume_ = std::clamp(pct, 0, 100);
    int db = volume_ <= 0 ? -10000 : std::clamp(int(std::lround(2000.0 * std::log10(volume_ / 100.0))), -10000, 0);
    audio_.masterVolume(db);
    if (save && save_) { save_->volume = volume_; save_->touch(); }
    menuDirty_ = true;
  }
  void cycleWidescreen(int d) {
    int m = (int(plat_->config().widescreen) + 4 + d) % 4;
    Bridge::requestWidescreen(m);
    wsShown_ = m;
    if (save_) { save_->widescreen = m; save_->touch(); }
    menuDirty_ = true;
  }
  void drawMenu(const Viewport& vp) {
    if (menuDirty_ || !menuTex_) {
      MenuModel m;
      m.title = "Paused";
      auto acts = menuActions();
      int sel = 0;
      for (size_t k = 0; k < acts.size(); ++k) {
        if (acts[k] == 3) m.items.push_back("#Settings");
        if (int(k) == menuSel_) sel = int(m.items.size());
        switch (acts[k]) {
          case 0: m.items.push_back("Resume"); break;
          case 1: m.items.push_back("Restart level"); break;
          case 2: m.items.push_back(menuLevel_ == 1 ? "Level select: title" : "Level select: " + std::to_string(menuLevel_)); break;
          case 3: m.items.push_back(std::string("Widescreen: ") + wsName(wsShown_ >= 0 ? wsShown_ : int(plat_->config().widescreen))); break;
          case 4: m.items.push_back("Volume: " + std::to_string(volume_) + "%"); break;
          case 5: { int li = langIndex(lang_); m.items.push_back(std::string("Game language: ") + (li >= 0 ? kLangLabels[li] : "?")); break; }
          case 6: m.items.push_back("Quit game"); break;
        }
      }
      m.selected = sel;
      m.status = "Hi-score " + std::to_string(save_ ? save_->highScore() : 0);
      m.footer = "Arrows choose   Enter select   Esc back";
      std::vector<uint8_t> px;
      try {
        if (rasterizeMenu(store_.read("font.fnt"), m, px)) {
          hud_.release(menuTex_);
          menuTex_ = hud_.uploadRGBA(640, 480, px.data());
        }
      } catch (const std::exception&) {}
      menuDirty_ = false;
    }
    if (!menuTex_) return;
    hud_.begin(vp);
    if (!dimTex_) { const uint8_t d[4] = {5, 10, 28, 170}; dimTex_ = hud_.uploadRGBA(1, 1, d); }
    glScissor(0, 0, vp.winW, vp.winH);
    hud_.draw(dimTex_, 1, 1, 0, 0, 1, 1, -4000, -4000, 9000, 9000);
    glScissor(0, 0, vp.winW, vp.winH);
    hud_.draw(menuTex_, 640, 480, 0, 0, 640, 480, 0, 0, 640, 480);
    hud_.end();
  }

  // ---- model building
  static std::vector<std::string> splitBar(const std::string& t) {
    std::vector<std::string> f; size_t s = 0;
    for (size_t p; (p = t.find('|', s)) != std::string::npos; s = p + 1) f.push_back(t.substr(s, p - s));
    return f;
  }
  TextureId loadTexture(const std::string& name, float* aspect, bool key) {
    std::string ck = name + (key ? "#k" : "");
    auto it = tex_.find(ck);
    if (it != tex_.end()) { if (aspect) *aspect = texAspect_[ck]; return it->second; }
    TextureId id = 0;
    try {
      Image im = Image::decode(store_.read(name.rfind("./", 0) == 0 ? name.substr(2) : name));
      id = scene_.createTextureRGB(int(im.width), int(im.height), im.rgb.data(), key);
      texAspect_[ck] = im.height ? float(im.width) / float(im.height) : 1.f;
    } catch (const std::exception& e) { SDL_Log("texture %s: %s", name.c_str(), e.what()); }
    tex_[ck] = id;
    if (aspect) *aspect = texAspect_[ck];
    return id;
  }

  // Descriptor line (1-based exe field numbers in brackets; 0x4011c7.. / 0x40221b loader):
  //  [4] wrap type 0 colour only, 1/2 flat box projection, 3 cylinder, 4 sphere, 5 chrome
  //  [10] attach to the camera layer and draw at 50% alpha (face colour 0x80ffffff)
  //  [11] decal transparency (colour key)   [12] texture scale  k = v * 1000 / 2^28 per metre
  //  [14..25] per-axis texture offsets      [25..27] RGB for type 0     [29] projection axis 1..6
  struct Layer { std::vector<std::string> f; };
  MeshId buildLayer(const Bytes& bytes, const std::vector<std::string>& f, TextureId tex) {
    auto num = [&](size_t i) { return i < f.size() ? std::atoi(f[i].c_str()) : 0; };
    const int type = num(3), axis = std::max(1, std::min(6, num(28)));
    const bool half = num(9) != 0;
    Mesh gm = Mesh::decode(bytes);
    const bool d3m = assetKind(bytes) == "d3m";
    MeshData md;
    Material m; m.texture = type == 0 ? 0 : tex;
    if (half) m.diffuse.a = 0.5f;
    if (type == 0) m.diffuse = {num(24) / 256.f, num(25) / 256.f, num(26) / 256.f, half ? 0.5f : 1.f};
    md.materials.push_back(m);
    for (auto& part : gm.parts) {
      uint32_t base = uint32_t(md.positions.size());
      for (auto& v : part.vertices) {
        if (d3m) md.positions.push_back({v[0] * 0.001f, v[1] * 0.001f, v[2] * 0.001f});
        else md.positions.push_back({v[0], v[2], v[1]});   // 3DS (Z up, right handed) -> game (Y up)
      }
      for (auto& fc : part.faces)
        for (size_t k = 2; k < fc.size(); ++k) {
          MeshData::Face face{{base + fc[0], base + fc[k - 1], base + fc[k]}, 0};
          if (!d3m) std::swap(face.v[1], face.v[2]);
          md.faces.push_back(face);
        }
    }
    if (md.positions.empty()) return 0;
    if (type != 0 && tex) {
      const float k = float(num(11)) * 1000.f / 268435456.f;
      md.uvs.resize(md.positions.size());
      if (type <= 2) {   // flat wrap, origin 0, dir/up by axis (0x40221b), ou/ov from the offset fields
        static const float dirs[7][6] = {{0}, {0, 0, -1, 0, -1, 0}, {1, 0, 0, 0, -1, 0}, {0, 0, 1, 0, -1, 0},
                                         {-1, 0, 0, 0, -1, 0}, {0, 1, 0, 0, 0, -1}, {0, -1, 0, 0, 0, -1}};
        const float* d = dirs[axis];
        float ou = -(float(num(axis * 2 + 10)) + 127.5f) / 256.f, ov = -(float(num(axis * 2 + 11)) + 127.5f) / 256.f;
        Vec3 dir{d[0], d[1], d[2]}, up{d[3], d[4], d[5]};
        Vec3 xa = cross(up, dir);
        for (size_t i = 0; i < md.positions.size(); ++i) {
          const Vec3& p = md.positions[i];
          md.uvs[i] = {dot(p, xa) * k + ou, dot(p, up) * k + ov};
        }
      } else {   // cylinder / sphere / chrome wrap around the object origin
        static const float dirs[7][6] = {{0}, {0, 1, 0, 0, 0, 1}, {0, 1, 0, -1, 0, 0}, {0, 1, 0, 0, 0, -1},
                                         {0, 1, 0, 1, 0, 0}, {0, 0, 1, 0, -1, 0}, {0, 0, 1, 0, 1, 0}};
        const float* d = dirs[axis];
        float ov = -(float(num(axis * 2 + 12)) + 127.5f) / 256.f;
        Vec3 dir{d[0], d[1], d[2]}, up{d[3], d[4], d[5]};
        Vec3 xa = cross(up, dir);
        for (size_t i = 0; i < md.positions.size(); ++i) {
          const Vec3& p = md.positions[i];
          float ang = std::atan2(dot(p, xa), dot(p, dir)) / (2 * kPi);
          float h = dot(p, up);
          if (cylAxisIsDir_) {   // axis = dir (game Y for every cylinder case 1..4)
            ang = std::atan2(dot(p, xa), dot(p, up)) / (2 * kPi);
            h = dot(p, dir);
          }
          if (type == 3) md.uvs[i] = {ang, h * -k + ov};
          else { float r = std::sqrt(dot(p, p)); md.uvs[i] = {ang, (r > 1e-6f ? std::asin(std::max(-1.f, std::min(1.f, h / r))) / kPi : 0.f) + 0.5f}; }
        }
      }
    }
    return scene_.createMesh(md);
  }
  Model* getModel(const ElemView& e) {
    auto it = models_.find(e.model);
    if (it != models_.end()) return &it->second;
    Model& m = models_[e.model];
    if (e.type != 11 && e.type != 2) return &m;
    try {
      std::string n = e.model.rfind("./", 0) == 0 ? e.model.substr(2) : e.model;
      Bytes b = store_.read(n);
      std::string kind = assetKind(b);
      if (kind == "descriptor") {
        std::string t(b.begin(), b.end());
        size_t s = 0;
        while (s < t.size()) {
          size_t eol = t.find('\n', s); if (eol == std::string::npos) eol = t.size();
          std::string line = t.substr(s, eol - s); if (!line.empty() && line.back() == '\r') line.pop_back();
          s = eol + 1;
          auto f = splitBar(line);
          if (f.size() < 10) continue;
          const bool key = f.size() > 10 && std::atoi(f[10].c_str()) != 0;
          TextureId tex = loadTexture(f[1], nullptr, key);
          MeshId id = buildLayer(store_.read(f[2].substr(2)), f, tex);
          if (id) m.meshes.push_back(id);
        }
        m.ok = !m.meshes.empty();
      } else if (kind == "bmp" || kind == "pcx") {
        m.tex = loadTexture(e.model, &m.aspect, true);
        m.billboard = m.ok = m.tex != 0;
      }
    } catch (const std::exception& ex) { SDL_Log("model %s: %s", e.model.c_str(), ex.what()); }
    if (m.billboard) {  // unit quad in the XY plane, clockwise seen from -Z
      MeshData q;
      q.positions = {{-.5f, .5f, 0}, {.5f, .5f, 0}, {.5f, -.5f, 0}, {-.5f, -.5f, 0}};
      q.uvs = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
      q.faces = {{{0, 1, 2}, 0}, {{0, 2, 3}, 0}};
      Material mt; mt.texture = m.tex; q.materials.push_back(mt);
      m.meshes.push_back(scene_.createMesh(q));
    }
    return &m;
  }

  // 0x4012f8: SetField(240 * pct/100 / -zoom), zoom = int 12 of the d30 camera record (file offset 64 + 12*4 = -128 here).
  float cameraField(const std::string& model) {
    float zoom = 128;
    try {
      Bytes b = store_.read(model.rfind("./", 0) == 0 ? model.substr(2) : model);
      if (b.size() >= 116 && assetKind(b) == "d30") {
        std::int32_t z; std::memcpy(&z, &b[112], 4);
        if (z < 0) zoom = float(-z);
      }
    } catch (const std::exception&) {}
    return 240.f * float(cfg_.percent) / 100.f / zoom;
  }

  static Quat eulerQuat(const float* d) {
    const float r = kPi / 180.f;
    Quat qx = quatFromAxisAngle({1, 0, 0}, d[0] * r), qy = quatFromAxisAngle({0, 1, 0}, -d[1] * r), qz = quatFromAxisAngle({0, 0, 1}, d[2] * r);
    // x first, then z, then y (0x409af0). Yaw is counter-clockwise seen from above: the exe's heading is (-sin, cos).
    return qy * qz * qx;
  }

  void syncFrames() {
    int cam = host_->cameraElement();
    Vec3 camPos{0, 0, 0};
    if (cam >= 0) {
      ElemView c = host_->interpolated(cam);
      if (const char* o = envs("RB_CAM")) std::sscanf(o, "%f,%f,%f,%f,%f,%f", &c.pos[0], &c.pos[1], &c.pos[2], &c.rotDeg[0], &c.rotDeg[1], &c.rotDeg[2]);
      camPos = {c.pos[0], c.pos[1], c.pos[2]};
      if (!fieldFixed_) { fieldFixed_ = true; field_ = cameraField(c.model); }
      scene_.setPosition(cam_, camPos);
      scene_.setRotation(cam_, eulerQuat(c.rotDeg));
      // 0x4012f8: SetFront(percent/100), SetBack(sky scale); SetField(240*pct/100 / -zoom) with the d30 zoom -128 gives 1.875,
      // the main viewport default is 0.9375 (0x40..): see docs/game.md
      scene_.setCamera(cam_, field_, float(cfg_.percent) / 100.f, cfg_.skyScale);
    }
    for (FrameId f : sky_) scene_.setPosition(f, camPos);
    for (int i = 0; i < host_->count(); ++i) {
      ElemView e = host_->interpolated(i);
      Slot& s = slots_[i];
      if (!s.tried) {
        s.tried = true;
        Model* m = getModel(e);
        if (m && m->ok) {
          s.model = m;
          s.frame = scene_.createFrame();
          for (MeshId id : m->meshes) scene_.addVisual(s.frame, id);
        }
      }
      if (!s.frame) continue;
      bool show = e.on && e.visible;
      scene_.setVisible(s.frame, show);
      if (!show) continue;
      scene_.setPosition(s.frame, {e.pos[0], e.pos[1], e.pos[2]});
      if (s.model->billboard) {
        float w = e.flat[0] / 1000.f, h = e.flat[1] / 1000.f;
        scene_.setScale(s.frame, {w, h, 1});
        Vec3 p{e.pos[0], e.pos[1], e.pos[2]};
        scene_.lookAt(s.frame, p + (p - camPos), {0, 1, 0});
      } else {
        scene_.setScale(s.frame, {e.scale[0], e.scale[1], e.scale[2]});
        scene_.setRotation(s.frame, eulerQuat(e.rotDeg));
      }
    }
  }

  // ---- HUD: types 5 (2D overlay image) and 8 (counter drawn from a digit strip, value = credits register)
  struct HudTex { GLuint id = 0; int w = 0, h = 0; };
  HudTex* hudTexture(const std::string& model) {
    auto it = hudTex_.find(model);
    if (it != hudTex_.end()) return it->second.id ? &it->second : nullptr;
    HudTex t;
    try {
      Image im = Image::decode(store_.read(model.rfind("./", 0) == 0 ? model.substr(2) : model));
      t.id = hud_.upload(int(im.width), int(im.height), im.rgb.data(), true); t.w = int(im.width); t.h = int(im.height);
    } catch (const std::exception&) {}
    auto& r = hudTex_[model] = t;
    return r.id ? &r : nullptr;
  }
  void drawHud(const Viewport& vp) {
    hud_.begin(vp);
    for (int i = 0; i < host_->count(); ++i) {
      ElemView e = host_->view(i);
      if ((e.type != 5 && e.type != 8) || !e.on || !e.visible) continue;
      HudTex* t = hudTexture(e.model);
      if (!t) continue;
      if (envs("RB_DEBUG") && frame_ == 140) SDL_Log("hud e%d %s type %d pos %.0f %.0f credits %d", i, e.name.c_str(), e.type, e.pos[0], e.pos[1], host_->credits(i));
      if (e.type == 5) {
        float w = t->w * e.scale[0], h = t->h * e.scale[1];
        hud_.draw(t->id, float(t->w), float(t->h), 0, 0, float(t->w), float(t->h), e.pos[0], e.pos[1], w, h);
      } else {
        // digit strip: cells 0-9 then + - . ; cell width measured from the two strips (17 px / 15 px)
        const float cw = t->w == 211 ? 17.f : t->w == 166 ? 15.f : float(t->w) / 13.f;
        std::string txt = std::to_string(host_->credits(i));
        float x = e.pos[0];
        for (char c : txt) {
          int cell = c == '-' ? 11 : (c >= '0' && c <= '9' ? c - '0' : 12);
          hud_.draw(t->id, float(t->w), float(t->h), cell * cw, 0, cw, float(t->h), x, e.pos[1], cw, float(t->h));
          x += cw;
        }
      }
    }
    hud_.end();
  }

  std::string dir_, level_ = "level001";
  Platform* plat_ = nullptr;
  AssetStore store_;
  Scene scene_;
  Audio audio_;
  Overlay2D hud_;
  bool audioOk_ = false, hudOk_ = false, hudOn_ = true;
  std::unique_ptr<BrainHost> host_;
  std::vector<Slot> slots_;
  std::vector<FrameId> sky_;
  std::map<std::string, Model> models_;
  std::map<std::string, TextureId> tex_;
  std::map<std::string, float> texAspect_;
  std::map<std::string, HudTex> hudTex_;
  std::unique_ptr<SoundBank> sounds_;
  AudioDump dump_;
  std::vector<KeyScript> script_;
  std::map<long, std::string> shots_;
  LevelCfg cfg_;
  LightId amb_ = 0;
  FrameId cam_ = 0;
  Color bg_{0, 0, 0, 1};
  float field_ = 0.9375f;
  bool fieldFixed_ = false;
  std::chrono::steady_clock::time_point t0_{}, lastRender_{};
  std::vector<double> frameMs_, latMs_;
  double runSeconds_ = std::getenv("RB_RUNSECS") ? std::atof(std::getenv("RB_RUNSECS")) : 0;
  bool statsDone_ = false;
  bool cylAxisIsDir_ = std::getenv("RB_CYLUP") == nullptr;   // cylinders wrap around dir (game Y): checked on the alien/player faces
  long frame_ = 0;
  bool pendingExit_ = false, paused_ = false;
  int volume_ = 100;
  std::vector<int> levels_;
  std::unique_ptr<GameSave> save_;
  int sinceSave_ = 0;
  bool menuOpen_ = false, menuDirty_ = false;
  int menuSel_ = 0, menuLevel_ = 1, wsShown_ = -1;
  GLuint menuTex_ = 0, dimTex_ = 0;
  std::vector<std::pair<std::string, std::string>> langs_;   // installed language sets: name, folder
  std::string lang_;
  char padJump_ = std::getenv("RB_PADJUMP") ? std::getenv("RB_PADJUMP")[0] : 'b';   // RB_PADJUMP=a|x|both: which pad button jumps (launcher setting)
};

}  // namespace

bool gameFilesPresent(const std::string& dataDir) {
  std::error_code ec;
  return std::filesystem::exists(std::filesystem::path(dataDir) / "level001.prj", ec);
}

std::unique_ptr<Game> makeRollerbotGame(const std::string& dataDir) {
  if (!gameFilesPresent(dataDir)) return nullptr;
  return std::make_unique<RollerGame>(dataDir);
}

}  // namespace rb
