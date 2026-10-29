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
int BrainHost::advance(double dt) { return d_->vm.advance(dt); }
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
  v.pos[0] = float(e.pos.x) / kMt; v.pos[1] = float(e.pos.y) / kMt; v.pos[2] = float(e.pos.z) / kMt;
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
