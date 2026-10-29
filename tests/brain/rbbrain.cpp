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

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: rbbrain <gamedir> <project> [--ticks N] [--trace] [--keys a,b] [--summary]\n"); return 2; }
    std::string game = argv[1], proj = argv[2];
    int ticks = 100; bool trace = false, summary = false; std::set<int> keys;
    std::map<int,std::set<int>> schedule;
    for (int i = 3; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--trace")) trace = true;
        else if (!std::strcmp(argv[i], "--summary")) summary = true;
        else if (!std::strcmp(argv[i], "--keys") && i + 1 < argc) { std::stringstream ss(argv[++i]); std::string t;
            while (std::getline(ss,t,';')) {
                auto c=t.find(':'); int tick=c==std::string::npos ? 0 : std::atoi(t.c_str());
                std::stringstream ks(c==std::string::npos ? t : t.substr(c+1)); std::string key;
                while(std::getline(ks,key,',')) if(!key.empty()) schedule[tick].insert(std::atoi(key.c_str()));
                if(!schedule.count(tick)) schedule[tick]={};
            } }
    }
    try {
        AssetStore store; store.open(game);
        if (proj.find('.') == std::string::npos) proj += ".prj";
        ProjectData pd = ProjectData::load(store, proj);
        MockScene scene;
        MockAudio audio; MockInput input; input.keys = keys; input.any = !keys.empty();
        BrainVM vm; vm.setServices(&scene, &audio, &input);
        vm.load(pd, game);
        for (auto& m : vm.compileErrors()) std::printf("compile error: %s\n", m.c_str());
        std::printf("project %s objects %zu brains %d\n", proj.c_str(), pd.objects.size(), vm.brainCount());
        struct Snap { Vec3 pos, rot; std::uint32_t flags; std::int32_t credits; };
        std::vector<Snap> prev;
        auto snap = [&] { prev.clear(); for (auto& e : vm.world().elements) prev.push_back({e.pos, e.rot, e.flags, e.credits}); };
        snap();
        std::array<std::int32_t, kMaxGlobals> pg = vm.world().globals;
        std::size_t evShown = 0;
        for (int t = 0; t < ticks && !vm.finished(); ++t) {
            auto ki=schedule.find(t); if(ki!=schedule.end()) { input.keys=ki->second; input.any=!input.keys.empty(); }
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
        for (const auto& e : w.elements) if(e.id&8) std::printf("player %u pos %.3f %.3f %.3f yaw %.3f radius %d collided %u\n",e.index,e.pos.x/double(kMt),e.pos.y/double(kMt),e.pos.z/double(kMt),normalizedDeg(e.rot.y),e.radius,e.collided);
        std::printf("state score %d lives %d fruit %d next %s\n",w.globals[0],w.globals[220],w.globals[201],BrainVM::projectForExit(w.exitCode).c_str());
        int moved = 0, on = 0;
        for (size_t i = 0; i < w.elements.size(); ++i) { moved += (w.elements[i].pos.x || w.elements[i].pos.y || w.elements[i].pos.z); on += (w.elements[i].flags & 1); }
        if (summary || !trace) {
            std::printf("ticks %u exit %d elements-on %d collision-pairs %zu unknown-commands:", w.tickCount, w.exitCode, on, vm.collisionPairs());
            for (auto& u : vm.unknownCommands()) std::printf(" %s", u.c_str());
            std::printf("\n");
        }
    } catch (const std::exception& ex) { std::fprintf(stderr, "error: %s\n", ex.what()); return 1; }
    return 0;
}
