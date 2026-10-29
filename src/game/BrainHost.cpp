#include "game/BrainHost.h"
#include "brain/Brain.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace rb {

namespace {
// Collision radii and ground queries backed by the real meshes (same idea as tests/brain/rbbrain.cpp).
struct HostScene : IScene {
  AssetStore* store = nullptr;
  HostSink* sink = nullptr;
  std::map<std::string, Mesh> meshes;
  std::map<std::string, std::int32_t> radii;

  const Mesh* mesh(const std::string& model, bool* d3m = nullptr) {
    auto it = meshes.find(model);
    if (it == meshes.end()) {
      Mesh me;
      try {
        std::string n = model.rfind("./", 0) == 0 ? model.substr(2) : model;
        Bytes b = store->read(n);
        if (assetKind(b) == "descriptor") {  // "1|tex|mesh|..." -> first layer's mesh
          std::string t(b.begin(), b.end());
          std::vector<std::string> f; size_t s = 0;
          for (size_t p; (p = t.find('|', s)) != std::string::npos; s = p + 1) f.push_back(t.substr(s, p - s));
          if (f.size() > 2) b = store->read(f[2].substr(2));
        }
        me = Mesh::decode(b);
      } catch (const std::exception&) {}
      it = meshes.emplace(model, std::move(me)).first;
    }
    return it->second.parts.empty() ? nullptr : &it->second;
  }
  std::int32_t modelRadius(const std::string& m) override {
    auto it = radii.find(m);
    if (it != radii.end()) return it->second;
    double r = 0;
    if (auto* me = mesh(m)) for (auto& p : me->parts) for (auto& v : p.vertices) r = std::max(r, std::sqrt(double(v[0]) * v[0] + double(v[1]) * v[1] + double(v[2]) * v[2]));
    return radii[m] = std::int32_t(r * 1000);
  }
  std::int32_t groundLevel(const World& w, int e) override {
    if (w.groundElement < 0) return 1;
    const Element& g = w.elements[w.groundElement];
    const Element& o = w.elements[e];
    auto* me = mesh(g.model);
    if (!me) return 1;
    double px = double(o.pos.x - g.pos.x) / kMt, pz = double(o.pos.z - g.pos.z) / kMt;
    for (auto& p : me->parts) {
      double k = 1.0;
      for (auto& v : p.vertices) if (std::fabs(v[0]) > 5000 || std::fabs(v[2]) > 5000) { k = 1e-3; break; }  // d3m raw units
      for (auto& f : p.faces)
        for (size_t i = 2; i < f.size(); ++i) {
          auto &a = p.vertices[f[0]], &b = p.vertices[f[i - 1]], &c = p.vertices[f[i]];
          auto s = [&](const std::array<float, 3>& u, const std::array<float, 3>& v) {
            return (px - u[0] * k) * (v[2] * k - u[2] * k) - (pz - u[2] * k) * (v[0] * k - u[0] * k); };
          double d1 = s(a, b), d2 = s(b, c), d3 = s(c, a);
          bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
          if (!(neg && pos)) return 1;
        }
    }
    return 0;
  }
  void backgroundColor(int r, int g, int b) override { if (sink) sink->backgroundColor(r, g, b); }
  void lightColor(int el, int r, int g, int b) override { if (sink) sink->lightColor(el, r, g, b); }
};
struct HostAudio : IAudio {
  HostSink* sink = nullptr;
  void play(int e, const std::string& s) override { if (sink) sink->play(e, s); }
  void stop(int e) override { if (sink) sink->stop(e); }
  void setRate(int e, int r) override { if (sink) sink->setRate(e, r); }
  void on(int e) override { if (sink) sink->on(e); }
  void off(int e) override { if (sink) sink->off(e); }
};
struct HostIn : IInput {
  HostInput* in = nullptr;
  bool keyboard(int sc) override { return in && in->key(sc); }
  int joyAxisX() override { return in ? in->joyX() : 0; }
  unsigned joyButtons() override { return in ? in->joyButtons() : 0; }
  bool anyInput() override { return in && in->any(); }
};
}  // namespace

struct BrainHost::Impl {
  AssetStore store;
  HostScene scene;
  HostAudio audio;
  HostIn input;
  BrainVM vm;
  std::vector<ElemView> prev;   // views before the latest tick
  double acc = 0;
};

BrainHost::BrainHost() : d_(new Impl) {}
BrainHost::~BrainHost() = default;

bool BrainHost::load(const std::string& gameDir, const std::string& project, HostSink* sink, HostInput* in, std::string* err) {
  try {
    d_->store.open(gameDir);
    std::string proj = project.find('.') == std::string::npos ? project + ".prj" : project;
    ProjectData pd = ProjectData::load(d_->store, proj);
    d_->scene.store = &d_->store; d_->scene.sink = sink;
    d_->audio.sink = sink; d_->input.in = in;
    d_->vm.setServices(&d_->scene, &d_->audio, &d_->input);
    d_->vm.load(pd, gameDir);
    return true;
  } catch (const std::exception& e) {
    if (err) *err = e.what();
    return false;
  }
}
int BrainHost::advance(double dt) {
  const double step = 1.0 / kTicksPerSecond;
  d_->acc += dt;
  int n = 0;
  while (d_->acc >= step && n < 8) {
    d_->prev.resize(count());
    for (int i = 0; i < count(); ++i) d_->prev[i] = view(i);
    d_->vm.tick();
    d_->acc -= step;
    ++n;
  }
  if (d_->acc > step) d_->acc = step;
  return n;
}
double BrainHost::alpha() const { return d_->acc * kTicksPerSecond; }
int BrainHost::credits(int i) const { return d_->vm.world().elements[i].credits; }
ElemView BrainHost::interpolated(int i) const {
  ElemView c = view(i);
  if (size_t(i) >= d_->prev.size()) return c;
  const ElemView& p = d_->prev[i];
  float a = float(alpha());
  float dx = c.pos[0] - p.pos[0], dy = c.pos[1] - p.pos[1], dz = c.pos[2] - p.pos[2];
  if (dx * dx + dy * dy + dz * dz > 30.f * 30.f || p.on != c.on) return c;   // teleport / on-off edge
  for (int k = 0; k < 3; ++k) {
    c.pos[k] = p.pos[k] + (c.pos[k] - p.pos[k]) * a;
    float dr = c.rotDeg[k] - p.rotDeg[k];
    dr -= 360.f * std::floor(dr / 360.f + 0.5f);   // shortest way round
    if (std::fabs(dr) < 90.f) c.rotDeg[k] = p.rotDeg[k] + dr * a;
    c.scale[k] = p.scale[k] + (c.scale[k] - p.scale[k]) * a;
  }
  return c;
}
int BrainHost::count() const { return int(d_->vm.world().elements.size()); }
int BrainHost::exitCode() const { return d_->vm.world().exitCode; }
unsigned BrainHost::tickCount() const { return d_->vm.world().tickCount; }
std::vector<int> BrainHost::globals() const { auto& g = d_->vm.world().globals; return {g.begin(), g.end()}; }
void BrainHost::setGlobals(const std::vector<int>& g) { for (size_t i = 0; i < g.size() && i < size_t(kMaxGlobals); ++i) d_->vm.setGlobal(int(i), g[i]); }
int BrainHost::globalValue(int i) const { return d_->vm.world().globals[i & 0x7ff]; }
std::vector<std::string> BrainHost::unknownCommands() const { return {d_->vm.unknownCommands().begin(), d_->vm.unknownCommands().end()}; }

ElemView BrainHost::view(int i) const {
  const Element& e = d_->vm.world().elements[i];
  ElemView v;
  v.index = i; v.type = e.type; v.name = e.name; v.model = e.model; v.sound = e.sound;
  bool child = (e.flags & kFlagChild) != 0;
  v.child = child;
  Vec3 r = e.rot;
  if (child) { r.x += e.childRot.x; r.y += e.childRot.y; r.z += e.childRot.z; }  // parent rot + stored offset
  const float pu = (e.type == 5 || e.type == 8) ? 1.f : float(kMt);   // overlays are in pixels
  v.pos[0] = float(e.pos.x) / pu; v.pos[1] = float(e.pos.y) / pu; v.pos[2] = float(e.pos.z) / pu;
  v.rotDeg[0] = float(r.x) / kDg; v.rotDeg[1] = float(r.y) / kDg; v.rotDeg[2] = float(r.z) / kDg;
  for (int k = 0; k < 3; ++k) v.scale[k] = float(e.scale[k]) / 1000.0f;
  v.flat[0] = float(e.flatScale[0]); v.flat[1] = float(e.flatScale[1]);
  v.on = (e.flags & kFlagOn) != 0; v.visible = (e.flags & kFlagVisible) != 0; v.translucent = (e.flags & kFlagTranslucent) != 0;
  return v;
}
int BrainHost::cameraElement() const {
  const auto& els = d_->vm.world().elements;
  for (size_t i = 0; i < els.size(); ++i)
    if (els[i].type == 0 && (els[i].flags & kFlagOn)) return int(i);
  return -1;
}

}  // namespace rb
