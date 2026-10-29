#include "brain/Brain.h"
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <chrono>
using namespace rb;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
int main() {
    CHECK(parseUnitNumber("1mt") == 512000);
    CHECK(parseUnitNumber("5.5dg") == 2816);
    CHECK(parseUnitNumber("10.093mt") == 10093 * 512);
    CHECK(parseUnitNumber("1ms") == 17067 / 1);
    std::vector<std::string> errs;
    auto b = compileBrain({"var a=0", "var b=1mt/3", "if a=0 and b>5", "a=a+1", "else", "a=7", "endif", "hide(element)"}, &errs);
    CHECK(errs.empty());
    ProjectData pd; ObjectRecord o; o.name = "t"; o.position = {1, 2, 3}; o.fields.assign(50, "0"); o.fields[32] = "1";
    pd.objects.push_back(o);
    BrainRecord br; br.objectIndex = 0; br.lines = {"var a=0", "var b=1mt/3", "if a=0 and b>5", "a=a+1", "else", "a=7", "endif", "position_x_set(element,position_x(element)+1mt)", "global_add(5,1)"};
    pd.brains.push_back(br);
    BrainVM vm; vm.load(pd, "/nonexistent");
    vm.tick(); vm.tick();
    CHECK(vm.world().elements[0].pos.x == 3 * kMt);
    CHECK(vm.world().globals[5] == 2);
    CHECK(vm.world().elements[0].vars[0] == 7);
    // Compiler's reduction order differs from C: division is reduced before multiplication.
    pd.brains[0].lines={"var a=0", "a=8*6/4", "if collision(element,2)0", "global_set(1,99)", "endif"};
    BrainVM expressions; expressions.load(pd,"/nonexistent"); expressions.world().elements[0].collided=2;
    expressions.tick(); CHECK(expressions.world().elements[0].vars[0]==8); CHECK(expressions.world().globals[1]==99);
    auto ref=kElementRefBase;
    expressions.world().elements.push_back(expressions.world().elements[0]);
    expressions.world().elements[1].index=1; expressions.world().elements[1].pos={2*kMt,2*kMt,3*kMt};
    expressions.callCommand("track",{ref,ref+1024},0);
    CHECK(expressions.world().elements[0].rot.y==-90*kDg);
    expressions.world().elements[1].pos={kMt,3*kMt,3*kMt};
    expressions.callCommand("track",{ref,ref+1024},0); CHECK(expressions.world().elements[0].rot.x==-90*kDg);
    expressions.world().elements[1].pos.y=kMt;
    expressions.callCommand("track",{ref,ref+1024},0); CHECK(expressions.world().elements[0].rot.x==90*kDg);

    // Generated geometry and random bytes are original fixtures, never retail assets.
    auto dir=std::filesystem::temp_directory_path()/("rb-brain-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    { std::ofstream f(dir/"floor.x"); f << "xof 0303txt 0032\nMesh { 3; -10;2;-10;, 10;4;-10;, 0;4;10;; 1; 3;0,1,2;; }"; }
    { std::ofstream f(dir/"multilev.ini"); f << "title|background|\n||\nloading|default|\n"; }
    { std::ofstream f(dir/"random.bin",std::ios::binary); for(int i=0;i<2000;++i) f.put(char(i)); }
    ProjectData geometry; auto ground=o; ground.model="floor.x"; ground.type=12;
    geometry.objects={ground,o}; geometry.objects[1].model="floor.x"; geometry.objects[1].position={0,8,0};
    BrainVM meshVM; meshVM.load(geometry,dir);
    auto height=meshVM.callCommand("ground_level",{ref+1024},1);
    CHECK(height==std::int32_t(3.5*kMt));
    CHECK(meshVM.callCommand("agl",{ref+1024},1)==std::int32_t(4.5*kMt));
    meshVM.world().elements[1].pos.x=20*kMt; CHECK(meshVM.callCommand("ground_level",{ref+1024},1)==0);
    CHECK(BrainVM::selectLevel(dir,3).loadingScreen=="loading"); CHECK(BrainVM::projectForExit(-3).empty());
    meshVM.world().updateCount=7; meshVM.world().randomPtr=1997;
    auto r=meshVM.nextRandom(); CHECK(meshVM.world().randomPtr==1);
    CHECK(r==((7u+(7u<<16)+0x04030201u)>>1));
    auto cursor=meshVM.world().randomPtr; CHECK(meshVM.callCommand("random",{5,2},0)==5); CHECK(cursor==meshVM.world().randomPtr);
    meshVM.setGlobal(220,2); meshVM.setGlobal(0,500); meshVM.load(geometry,dir);
    CHECK(meshVM.world().globals[220]==2); CHECK(meshVM.world().globals[0]==500);
    { std::ofstream f(dir/"level003.prj"); f << ";v1.0\n1\n";
      for(size_t i=0;i<o.fields.size();++i) { if(i) f << '|'; f << (i==0 ? "fixture" : i==1 ? "floor.x" : o.fields[i]); }
      f << "\n@0\n"; }
    { std::ofstream f(dir/"level003.col",std::ios::binary); }
    meshVM.world().exitCode=3; CHECK(meshVM.transition(dir));
    CHECK(meshVM.world().globals[0]==500 && meshVM.world().globals[220]==2);
    CHECK(meshVM.world().exitCode==-1 && meshVM.world().tickCount==0);
    meshVM.world().exitCode=-3; CHECK(!meshVM.transition(dir));
    meshVM.load(geometry,dir);
    meshVM.world().elements[0].radius=1000; meshVM.world().elements[1].radius=1000;
    meshVM.world().elements[0].id=8; meshVM.world().elements[1].id=128;
    meshVM.world().elements[0].pos={0,0,0}; meshVM.world().elements[1].pos={kMt,0,0};
    meshVM.callCommand("collision_map_set",{ref,ref+1024,5},0); meshVM.tick();
    CHECK(meshVM.callCommand("collision",{ref,128},0)==128); CHECK(meshVM.callCommand("collision_color",{27},0)==-1);
    meshVM.callCommand("scale_set",{ref,0,0,0},0); CHECK(meshVM.world().elements[0].radius==1000);
    meshVM.callCommand("collisions_off",{ref+1024},1); meshVM.tick(); CHECK(meshVM.world().elements[0].collided==0);
    meshVM.callCommand("collisions_on",{ref+1024},1);
    meshVM.callCommand("scale_set",{ref,1000,1000,1000},0);
    meshVM.world().elements[1].pos.x=15*kMt;
    meshVM.callCommand("box_collisions_on",{ref},0); meshVM.callCommand("box_collisions_on",{ref+1024},1);
    meshVM.tick(); CHECK(meshVM.world().elements[0].collided==128);
    meshVM.world().elements[1].pos.x=25*kMt; meshVM.tick(); CHECK(meshVM.world().elements[0].collided==0);
    meshVM.world().elements[1].pos.x=15*kMt; meshVM.callCommand("box_collisions_off",{ref+1024},1);
    meshVM.tick(); CHECK(meshVM.world().elements[0].collided==0);
    std::filesystem::remove_all(dir);
    std::printf(fails ? "FAILED\n" : "ok\n");
    return fails;
}
