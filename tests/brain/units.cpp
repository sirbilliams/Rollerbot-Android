#include "brain/Brain.h"
#include <cstdio>
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
    std::printf(fails ? "FAILED\n" : "ok\n");
    return fails;
}
