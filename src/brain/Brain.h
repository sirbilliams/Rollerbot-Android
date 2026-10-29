// Brain VM + simulation (lane brain). Original code; behaviour reconstructed from Rollerbot.exe
// (3D Rad 3.0). Exe addresses are cited in docs/brain.md and in comments.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "assets/Assets.h"

namespace rb {

// ---- fixed point (units from the compiler's unit suffix converters 0x426ac0..0x426bf0) ----
constexpr std::int32_t kMt = 512000;   // "1mt": thousandths << 9
constexpr std::int32_t kDg = 512;      // "1dg": 184320 units per turn (0x2d000)
constexpr std::int32_t kTurn = 184320; // 360 degrees
constexpr std::int32_t kTicksPerSecond = 30; // guess from the "ms"/"rm" converters
constexpr int kMaxGlobals = 2048;      // global(n) masks n with 0x7ff (0x40a010)
constexpr std::int32_t kElementRefBase = 0x10000000; // synthetic "pointer" base; ref = base + index*0x400
constexpr std::uint32_t kElementStride = 0x400;      // sizeof(element) in the exe

// Element flag bits at +0x58 (from handlers on/off/show/hide/collisions_*/translucent, 0x40bff0..).
enum ElementFlags : std::uint32_t {
    kFlagOn = 0x1,         // brain runs and element exists in the simulation
    kFlagPaused = 0x2,     // brain skipped by the frame loop (0x4105bd)
    kFlagChild = 0x8,      // set by child_rot; cleared on every frame start (0x4104da)
    kFlagNoHide = 0x10,    // off() skips the hide hook when set
    kFlagTranslucent = 0x20,
    kFlagVisible = 0x40,
    kFlagCollide = 0x80,
    kFlagSphere2 = 0x100,
};

struct Vec3 { std::int32_t x = 0, y = 0, z = 0; };

struct Element {
    std::uint32_t index = 0;
    std::string name, model, sound;
    std::int32_t type = 0;           // project field 35 (2=3D object,5=sprite,8=counter,11=model...) guess
    Vec3 pos, rot, vel, spin;        // +0x0c +0x18 +0x24 +0x30
    std::int32_t scale[3] = {1000, 1000, 1000};
    std::int32_t flatScale[2] = {1000, 1000};
    std::uint32_t flags = kFlagOn | kFlagVisible | kFlagCollide;
    std::int32_t credits = 0;        // +0x5c, also score()/score_set()/score_add()
    std::uint32_t id = 0;            // +0x60 collision class bits (project field 30)
    std::uint32_t collided = 0;      // +0x64 OR of the ids of elements hit this frame
    std::int32_t radius = 0;         // +0x68 collision radius in 1/1000 mt
    std::uint32_t runCount = 0;      // +0xac
    std::int32_t soundRate = 0;
    std::vector<std::int32_t> vars;  // brain variable memory
    int brain = -1;                  // index into BrainVM programs, -1 = none
};

class World {
public:
    std::vector<Element> elements;
    std::array<std::int32_t, kMaxGlobals> globals{};
    std::int32_t exitCode = -1;      // exit(n) -> 0x57f2dc; -1 means keep running
    std::uint32_t tickCount = 0;     // 0x57f1dc, drives every_tics
    int groundElement = -1;          // ground_set()
    int listener = -1;
    std::uint32_t randomPtr = 0;
};

// ---- services the VM talks to; all have headless defaults (see MockScene etc.) ----
struct IScene {
    virtual ~IScene() = default;
    virtual void lightColor(int, int, int, int) {}
    virtual void lightIntensity(int, int) {}
    virtual void backgroundColor(int, int, int) {}
    virtual void elementEvent(int /*index*/, const char* /*what*/) {} // on/off/show/hide/translucent
    // Height queries; return 0 when no ground is below (water/void), >0 otherwise (raw units above ground).
    virtual std::int32_t groundLevel(const World&, int /*element*/) { return 1; }
    // Collision radius (1/1000 mt, before scale) for a model; 0 = unknown.
    virtual std::int32_t modelRadius(const std::string& /*model*/) { return 0; }
};
struct IAudio {
    virtual ~IAudio() = default;
    virtual void play(int /*element*/, const std::string& /*sound*/) {}
    virtual void stop(int /*element*/) {}
    virtual void setRate(int /*element*/, int /*rate*/) {}
};
struct IInput {
    virtual ~IInput() = default;
    virtual bool keyboard(int /*scancode*/) { return false; }   // keyboard(n) -> -1 when held
    virtual int joyAxisX() { return 0; }                          // -1000..1000
    virtual unsigned joyButtons() { return 0; }
    virtual bool anyInput() { return false; }
};

// Scene-facing transform of an element, in scene units (metres) and degrees, for mapping onto rb::Scene
// frames (position, euler -> quaternion, scale). Rotation order is the exe's: X, then Z, then Y
// (see rotateChain in Brain.cpp, 0x409af0); the caller converts handedness.
struct ElementTransform { float pos[3]; float rotDeg[3]; float scale[3]; bool visible; bool on; };
inline ElementTransform elementTransform(const Element& e) {
    ElementTransform t;
    t.pos[0] = float(e.pos.x) / kMt; t.pos[1] = float(e.pos.y) / kMt; t.pos[2] = float(e.pos.z) / kMt;
    t.rotDeg[0] = float(e.rot.x) / kDg; t.rotDeg[1] = float(e.rot.y) / kDg; t.rotDeg[2] = float(e.rot.z) / kDg;
    for (int i = 0; i < 3; ++i) t.scale[i] = float(e.scale[i]) / 1000.0f;
    t.visible = (e.flags & kFlagVisible) != 0; t.on = (e.flags & kFlagOn) != 0;
    return t;
}

struct TickEvent { std::uint32_t tick; int element; std::string text; };

class Brain; // compiled program (opaque)

class BrainVM {
public:
    BrainVM();
    ~BrainVM();
    // The only place that knows where project text comes from. Takes the parsed project.
    void load(const ProjectData& project, const std::filesystem::path& gameDir);
    void setServices(IScene* s, IAudio* a, IInput* i);
    // Inject an event (e.g. level-transition request or a global set from the UI lane).
    void setGlobal(int index, std::int32_t v) { world_.globals[index & 0x7ff] = v; }
    // One 1/30 s simulation step: animation, collisions, input, then every element's brain in index order.
    void tick();
    // dt-based driver: accumulates real seconds, runs whole ticks, returns number run.
    int advance(double dtSeconds);
    World& world() { return world_; }
    const World& world() const { return world_; }
    const std::vector<TickEvent>& events() const { return events_; }
    void clearEvents() { events_.clear(); }
    const std::set<std::string>& unknownCommands() const { return unknown_; }
    bool finished() const { return world_.exitCode != -1; }
    int brainCount() const { return int(programs_.size()); }
private:
    friend class Brain;
    struct Impl;
    World world_;
    IScene* scene_; IAudio* audio_; IInput* input_;
    std::vector<std::shared_ptr<Brain>> programs_;
    std::vector<TickEvent> events_;
    std::set<std::string> unknown_;
    std::vector<std::int8_t> sinus_;       // sinus.bin, byte table
    std::vector<std::int32_t> sinusBig_;   // sinusbig.bin
    std::vector<std::int32_t> randomTab_;  // random.bin
    std::vector<std::uint8_t> randomBytes_;
    double accum_ = 0;
    std::map<std::pair<int,int>, int> collisionMap_;
    std::set<int> hitColors_;
    std::vector<std::uint8_t> collisionPairs_;
    friend struct Runner;
    void runCollisions();
    void runElement(int index);
    // exposed for the interpreter
public:
    std::int32_t callCommand(const std::string& name, const std::vector<std::int32_t>& a, int self);
    std::int32_t sinB(std::int32_t angle) const;
    std::int32_t cosB(std::int32_t angle) const;
    std::int32_t sinBig(std::int32_t idx) const;
    std::uint32_t nextRandom();
    IScene* sceneP() { return scene_; }
};

// Headless mocks (also used by the tests).
struct MockScene : IScene {};
struct MockAudio : IAudio { std::vector<std::string> log; void play(int e, const std::string& s) override { log.push_back("play " + std::to_string(e) + " " + s); } void stop(int e) override { log.push_back("stop " + std::to_string(e)); } };
struct MockInput : IInput {
    std::set<int> keys; bool any = false;
    bool keyboard(int sc) override { return keys.count(sc) != 0; }
    bool anyInput() override { return any; }
};

// Brain text -> program, exposed so tools/tests can compile a single brain and report errors.
std::shared_ptr<Brain> compileBrain(const std::vector<std::string>& lines, std::vector<std::string>* errors);
std::int32_t parseUnitNumber(const std::string& tok, bool* ok = nullptr);

} // namespace rb
