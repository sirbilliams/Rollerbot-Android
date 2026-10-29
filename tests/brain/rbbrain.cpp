// Headless brain runner: rbbrain <gamedir> <project> [--ticks N] [--trace] [--keys 57,29] [--summary]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include "brain/Brain.h"
using namespace rb;
static double normalizedDeg(std::int32_t a) { return double(((a % kTurn) + kTurn) % kTurn) / kDg; }

// Scene backed by the real meshes: collision radii from model bounds, ground by XZ footprint of the ground mesh.
struct MeshScene : IScene {
    AssetStore* store = nullptr;
    std::map<std::string, Mesh> meshes;
    std::map<std::string, std::int32_t> radii;
    const Mesh* mesh(const std::string& m) {
        auto it = meshes.find(m);
        if (it != meshes.end()) return it->second.parts.empty() ? nullptr : &it->second;
        Mesh me;
        try { std::string n = m.rfind("./", 0) == 0 ? m.substr(2) : m; me = Mesh::decode(store->read(n)); } catch (const std::exception&) {}
        return &(meshes[m] = std::move(me));
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
        for (auto& p : me->parts)
            for (auto& f : p.faces) {
                for (size_t k = 2; k < f.size(); ++k) {
                    auto &a = p.vertices[f[0]], &b = p.vertices[f[k - 1]], &c = p.vertices[f[k]];
                    auto s = [&](const std::array<float, 3>& u, const std::array<float, 3>& v) { return (px - u[0]) * (v[2] - u[2]) - (pz - u[2]) * (v[0] - u[0]); };
                    double d1 = s(a, b), d2 = s(b, c), d3 = s(c, a);
                    bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
                    if (!(neg && pos)) return 1;
                }
            }
        return 0;
    }
};

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: rbbrain <gamedir> <project> [--ticks N] [--trace] [--keys a,b] [--summary]\n"); return 2; }
    std::string game = argv[1], proj = argv[2];
    int ticks = 100; bool trace = false, summary = false; std::set<int> keys;
    for (int i = 3; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--trace")) trace = true;
        else if (!std::strcmp(argv[i], "--summary")) summary = true;
        else if (!std::strcmp(argv[i], "--keys") && i + 1 < argc) { std::stringstream ss(argv[++i]); std::string t; while (std::getline(ss, t, ',')) keys.insert(std::atoi(t.c_str())); }
    }
    try {
        AssetStore store; store.open(game);
        ProjectData pd = ProjectData::load(store, proj);
        MeshScene scene; scene.store = &store;
        MockAudio audio; MockInput input; input.keys = keys; input.any = !keys.empty();
        BrainVM vm; vm.setServices(&scene, &audio, &input);
        vm.load(pd, game);
        std::printf("project %s objects %zu brains %d\n", proj.c_str(), pd.objects.size(), vm.brainCount());
        struct Snap { Vec3 pos, rot; std::uint32_t flags; std::int32_t credits; };
        std::vector<Snap> prev;
        auto snap = [&] { prev.clear(); for (auto& e : vm.world().elements) prev.push_back({e.pos, e.rot, e.flags, e.credits}); };
        snap();
        std::array<std::int32_t, kMaxGlobals> pg = vm.world().globals;
        std::size_t evShown = 0;
        for (int t = 0; t < ticks && !vm.finished(); ++t) {
            vm.tick();
            if (trace) {
                std::printf("tick %d\n", t + 1);
                const auto& w = vm.world();
                for (int g = 0; g < kMaxGlobals; ++g) if (w.globals[g] != pg[g]) { std::printf("  global[%d] %d -> %d\n", g, pg[g], w.globals[g]); pg[g] = w.globals[g]; }
                for (size_t i = 0; i < w.elements.size(); ++i) {
                    const Element& e = w.elements[i]; const Snap& s = prev[i];
                    if (std::memcmp(&e.pos, &s.pos, sizeof(Vec3)) || std::memcmp(&e.rot, &s.rot, sizeof(Vec3)) || e.flags != s.flags || e.credits != s.credits)
                        std::printf("  e%zu %s pos %.3f %.3f %.3f rot %.1f flags %02x credits %d\n", i, e.name.c_str(), e.pos.x / double(kMt), e.pos.y / double(kMt), e.pos.z / double(kMt), normalizedDeg(e.rot.y), e.flags, e.credits);
                }
                const auto& evs = vm.events();
                for (; evShown < evs.size(); ++evShown) std::printf("  event e%d %s\n", evs[evShown].element, evs[evShown].text.c_str());
                snap();
            }
        }
        const auto& w = vm.world();
        int moved = 0, on = 0;
        for (size_t i = 0; i < w.elements.size(); ++i) { moved += (w.elements[i].pos.x || w.elements[i].pos.y || w.elements[i].pos.z); on += (w.elements[i].flags & 1); }
        if (summary || !trace) {
            std::printf("ticks %u exit %d elements-on %d unknown-commands:", w.tickCount, w.exitCode, on);
            for (auto& u : vm.unknownCommands()) std::printf(" %s", u.c_str());
            std::printf("\n");
        }
    } catch (const std::exception& ex) { std::fprintf(stderr, "error: %s\n", ex.what()); return 1; }
    return 0;
}
