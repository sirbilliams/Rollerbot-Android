// Headless brain runner: rbbrain <gamedir> <project> [--ticks N] [--trace] [--keys 57,29|@file] [--summary] [--sweep]
// Whole game: rbbrain <gamedir> --route-chain <route-directory>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <fstream>
#include <sstream>
#include "brain/Brain.h"
using namespace rb;
static double normalizedDeg(std::int32_t a) { return double(((a % kTurn) + kTurn) % kTurn) / kDg; }

int runFullRoutes(int, char**);
int main(int argc, char** argv) {
    if(argc==4&&!std::strcmp(argv[2], "--route-chain")){
        char mode[]="--replay";char* args[]={argv[0],argv[1],argv[3],mode};return runFullRoutes(4,args);
    }
    if (argc < 3) { std::fprintf(stderr, "usage: rbbrain <gamedir> <project> [--ticks N] [--trace] [--keys a,b] [--summary] [--sweep]\n"); return 2; }
    std::string game = argv[1], proj = argv[2];
    int ticks = 100; bool trace = false, summary = false, sweep = false; std::set<int> keys;
    std::map<int,std::set<int>> schedule;
    for (int i = 3; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--trace")) trace = true;
        else if (!std::strcmp(argv[i], "--sweep")) sweep = true;
        else if (!std::strcmp(argv[i], "--summary")) summary = true;
        else if (!std::strcmp(argv[i], "--keys") && i + 1 < argc) { std::string keyText=argv[++i];
            if(!keyText.empty()&&keyText[0]=='@') { std::ifstream f(keyText.substr(1));if(!f){std::fprintf(stderr,"cannot open key file\n");return 2;}keyText.assign(std::istreambuf_iterator<char>(f),{}); }
            std::stringstream ss(keyText); std::string t;
            while (std::getline(ss,t,';')) {
                auto c=t.find(':'); int tick=c==std::string::npos ? 0 : std::atoi(t.c_str());
                std::stringstream ks(c==std::string::npos ? t : t.substr(c+1)); std::string key;
                while(std::getline(ks,key,',')) if(key.find_first_not_of(" \r\n\t")!=key.npos) schedule[tick].insert(std::atoi(key.c_str()));
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
        int livesLost = 0, nonfinite = 0;
        std::map<int, int> exits;
        for (int t = 0; t < ticks && (sweep || !vm.finished()); ++t) {
            if (sweep) vm.world().exitCode = -1;
            int oldLives = vm.world().globals[220];
            auto ki=schedule.find(t); if(ki!=schedule.end()) { input.keys=ki->second; input.any=!input.keys.empty(); }
            vm.tick();
            if (sweep) {
                livesLost += std::max(0, oldLives - vm.world().globals[220]);
                if (vm.finished()) ++exits[vm.world().exitCode];
                for (const auto& e : vm.world().elements) {
                    auto tr = elementTransform(e);
                    for (int axis = 0; axis < 3; ++axis)
                        nonfinite += !std::isfinite(tr.pos[axis]) || !std::isfinite(tr.rotDeg[axis]) || !std::isfinite(tr.scale[axis]);
                }
            }
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
            if (sweep) { vm.clearEvents(); audio.log.clear(); evShown = 0; }
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
        if (sweep) {
            std::printf("SWEEP ticks=%u compile=%zu nonfinite=%d lives_lost=%d exits=", w.tickCount, vm.compileErrors().size(), nonfinite, livesLost);
            for (const auto& x : exits) std::printf("%d:%d,", x.first, x.second);
            std::printf(" unknown=");
            for (const auto& x : vm.unknownCommandCounts()) std::printf("%s:%llu,", x.first.c_str(), (unsigned long long)x.second);
            std::printf("\n");
            if (!vm.compileErrors().empty() || nonfinite || !vm.unknownCommands().empty() || w.tickCount != unsigned(ticks)) return 1;
        }
    } catch (const std::exception& ex) { std::fprintf(stderr, "error: %s\n", ex.what()); return 1; }
    return 0;
}
