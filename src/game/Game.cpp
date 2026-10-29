#include "game/Game.h"
#include "assets/Assets.h"
#include "audio/Audio.h"
#include "game/BrainHost.h"
#include "scene/Scene.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <sstream>

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

// Scripted input for headless verification: RB_KEYS="75@0-300,57@100-110" (scancode@firstFrame-lastFrame).
struct KeyScript { int sc, from, to; };

class RollerGame : public Game, public HostSink, public HostInput {
 public:
  explicit RollerGame(std::string dir) : dir_(std::move(dir)) {}

  bool init(Platform& p) override {
    plat_ = &p;
    std::string err;
    if (!scene_.init(&err)) { SDL_Log("scene init: %s", err.c_str()); return false; }
    try { store_.open(dir_); } catch (const std::exception& e) { SDL_Log("assets: %s", e.what()); return false; }
    audioOk_ = audio_.init(48000, true);
    if (!audioOk_) { audio_.init(48000, false); }
    if (const char* v = envs("RB_LEVEL")) level_ = v;
    if (const char* v = envs("RB_TILE")) tile_ = std::strtof(v, nullptr);
    if (const char* v = envs("RB_FIELD")) field_ = std::strtof(v, nullptr);
    if (const char* v = envs("RB_FOG")) std::sscanf(v, "%f,%f", &fogStart_, &fogEnd_);
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
    if (!host_) return;
    const Input& in = plat_->input();
    if (in.pressed(Action::Back) && !envs("RB_NOQUIT")) plat_->requestQuit();
    host_->advance(dt);
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
    syncFrames();
    scene_.setViewport(vp.area.x, vp.area.y, vp.area.w, vp.area.h);
    scene_.setBackground(bg_);
    scene_.setFog(true, bg_, fogStart_, fogEnd_);
    scene_.render();
    auto it = shots_.find(frame_);
    if (it != shots_.end()) plat_->screenshot(it->second);
  }

  void shutdown() override { audio_.shutdown(); }

  // ---- HostSink
  void lightColor(int el, int r, int g, int b) override { (void)el; (void)r; (void)g; (void)b; }
  void backgroundColor(int r, int g, int b) override { bg_ = {r / 255.f, g / 255.f, b / 255.f, 1}; }
  void play(int el, const std::string& sound) override {
    if (sound.empty() || sound.find("null_") != std::string::npos) return;
    auto it = pcm_.find(sound);
    if (it == pcm_.end()) {
      std::shared_ptr<const PCM> pcm;
      try { Bytes b = store_.read(sound.rfind("./", 0) == 0 ? sound.substr(2) : sound); pcm = audio_.loadWav(b.data(), b.size()); } catch (const std::exception&) {}
      it = pcm_.emplace(sound, pcm).first;
    }
    if (it->second) voices_[el] = audio_.play(it->second, false);
  }
  void stop(int el) override { auto it = voices_.find(el); if (it != voices_.end()) audio_.stop(it->second); }

  // ---- HostInput
  bool key(int sc) override {
    for (auto& k : script_) if (k.sc == sc && frame_ >= k.from && frame_ <= k.to) return true;
    const Input& in = plat_->input();
    switch (sc) {
      case 29: return in.held(Action::Action2) || in.held(Action::Jump);
      case 57: return in.held(Action::Accept) || in.held(Action::Jump);
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
  struct Slot { FrameId frame = 0; Model* model = nullptr; bool tried = false; bool shown = false; };

  bool loadLevel(const std::string& level, bool carryGlobals = false) {
    std::vector<int> g;
    if (carryGlobals && host_) g = host_->globals();
    for (auto& s : slots_) if (s.frame) scene_.destroyFrame(s.frame);
    slots_.clear(); voices_.clear();
    host_.reset(new BrainHost);
    std::string err;
    if (!host_->load(dir_, level, this, this, &err)) { SDL_Log("load %s: %s", level.c_str(), err.c_str()); host_.reset(); return false; }
    if (!g.empty()) host_->setGlobals(g);
    level_ = level; pendingExit_ = false;
    slots_.resize(host_->count());
    SDL_Log("loaded %s: %d elements", level.c_str(), host_->count());
    return true;
  }

  // ---- model building
  static std::vector<std::string> splitBar(const std::string& t) {
    std::vector<std::string> f; size_t s = 0;
    for (size_t p; (p = t.find('|', s)) != std::string::npos; s = p + 1) f.push_back(t.substr(s, p - s));
    return f;
  }
  TextureId loadTexture(const std::string& name, float* aspect = nullptr) {
    auto it = tex_.find(name);
    if (it != tex_.end()) { if (aspect) *aspect = texAspect_[name]; return it->second; }
    TextureId id = 0;
    try {
      Image im = Image::decode(store_.read(name.rfind("./", 0) == 0 ? name.substr(2) : name));
      id = scene_.createTextureRGB(int(im.width), int(im.height), im.rgb.data(), true);
      texAspect_[name] = im.height ? float(im.width) / float(im.height) : 1.f;
    } catch (const std::exception& e) { SDL_Log("texture %s: %s", name.c_str(), e.what()); }
    tex_[name] = id;
    if (aspect) *aspect = texAspect_[name];
    return id;
  }
  // 3DS is right-handed with Z up: game coordinates are (x, z, y). Planar UVs project along game Y.
  MeshId buildMesh(const Bytes& bytes, int wrap, TextureId tex) {
    Mesh gm = Mesh::decode(bytes);
    const bool d3m = assetKind(bytes) == "d3m";
    MeshData md;
    Material m; m.texture = tex; md.materials.push_back(m);
    for (auto& part : gm.parts) {
      uint32_t base = uint32_t(md.positions.size());
      for (auto& v : part.vertices) {
        if (d3m) md.positions.push_back({v[0] * 0.001f, v[1] * 0.001f, v[2] * 0.001f});
        else md.positions.push_back({v[0], v[2], v[1]});
      }
      for (auto& f : part.faces)
        for (size_t k = 2; k < f.size(); ++k) {
          MeshData::Face fc{{base + f[0], base + f[k - 1], base + f[k]}, 0};
          if (flip_ ^ d3m) std::swap(fc.v[1], fc.v[2]);
          md.faces.push_back(fc);
        }
    }
    if (md.positions.empty()) return 0;
    Vec3 mn = md.positions[0], mx = mn;
    for (auto& p : md.positions) {
      mn = {std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
      mx = {std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
    }
    Vec3 ext = mx - mn;
    auto sx = ext.x > 1e-6f ? ext.x : 1.f, sy = ext.y > 1e-6f ? ext.y : 1.f, sz = ext.z > 1e-6f ? ext.z : 1.f;
    md.uvs.resize(md.positions.size());
    for (size_t i = 0; i < md.positions.size(); ++i) {
      const Vec3& p = md.positions[i];
      if (wrap == 3) md.uvs[i] = {0.5f + std::atan2(p.x - (mn.x + sx * .5f), p.z - (mn.z + sz * .5f)) / (2 * kPi), 1.f - (p.y - mn.y) / sy};
      else md.uvs[i] = {p.x / tile_, -p.z / tile_};
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
          if (f.size() < 4) continue;
          TextureId tex = loadTexture(f[1]);
          Bytes mb = store_.read(f[2].substr(2));
          MeshId id = buildMesh(mb, std::atoi(f[3].c_str()), tex);
          if (id) m.meshes.push_back(id);
        }
        m.ok = !m.meshes.empty();
      } else if (kind == "bmp" || kind == "pcx") {
        m.tex = loadTexture(e.model, &m.aspect);
        m.billboard = m.ok = m.tex != 0;
      } else if (kind == "3ds" || kind == "d3m") {
        MeshId id = buildMesh(b, 2, 0);
        if (id) { m.meshes.push_back(id); m.ok = true; }
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

  static Quat eulerQuat(const float* d) {
    const float r = kPi / 180.f;
    Quat qx = quatFromAxisAngle({1, 0, 0}, d[0] * r), qy = quatFromAxisAngle({0, 1, 0}, -d[1] * r), qz = quatFromAxisAngle({0, 0, 1}, d[2] * r);
    // brains rotate x first, then z, then y (0x409af0). Yaw is counter-clockwise seen from above: the exe's heading
    // vector is (-sin, cos) (sinus.bin row/byte), so Y is negated; pitch is standard (+ = look down).
    return qy * qz * qx;
  }

  void syncFrames() {
    int cam = host_->cameraElement();
    Vec3 camPos{0, 0, 0};
    if (cam >= 0) {
      ElemView c = host_->view(cam);
      if (const char* o = envs("RB_CAM")) std::sscanf(o, "%f,%f,%f,%f,%f,%f", &c.pos[0], &c.pos[1], &c.pos[2], &c.rotDeg[0], &c.rotDeg[1], &c.rotDeg[2]);
      camPos = {c.pos[0], c.pos[1], c.pos[2]};
      scene_.setPosition(cam_, camPos);
      scene_.setRotation(cam_, eulerQuat(c.rotDeg));
      scene_.setCamera(cam_, field_, 0.5f, 3000.f);
      if (envs("RB_DEBUG") && frame_ % 20 == 0) SDL_Log("cam e%d pos %.2f %.2f %.2f rot %.1f %.1f %.1f", cam, c.pos[0], c.pos[1], c.pos[2], c.rotDeg[0], c.rotDeg[1], c.rotDeg[2]);
    }
    for (int i = 0; i < host_->count(); ++i) {
      ElemView e = host_->view(i);
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

  std::string dir_, level_ = "level001";
  Platform* plat_ = nullptr;
  AssetStore store_;
  Scene scene_;
  Audio audio_;
  bool audioOk_ = false;
  std::unique_ptr<BrainHost> host_;
  std::vector<Slot> slots_;
  std::map<std::string, Model> models_;
  std::map<std::string, TextureId> tex_;
  std::map<std::string, float> texAspect_;
  std::map<std::string, std::shared_ptr<const PCM>> pcm_;
  std::map<int, Audio::Handle> voices_;
  std::vector<KeyScript> script_;
  std::map<long, std::string> shots_;
  LightId amb_ = 0;
  FrameId cam_ = 0;
  Color bg_{0.35f, 0.5f, 0.8f, 1};
  float field_ = 0.9375f, fogStart_ = 150.f, fogEnd_ = 900.f, tile_ = 40.f;
  bool flip_ = !envs("RB_NOFLIP");
  long frame_ = 0;
  bool pendingExit_ = false;
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
