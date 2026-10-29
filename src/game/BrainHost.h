// BrainHost: thin plain-C++ wrapper around rb::BrainVM so that the game (which uses rb::Scene's float Vec3)
// never has to include Brain.h (rb::Vec3 would clash). No brain/scene types appear here.
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace rb {

struct ElemView {
  int index = 0, type = 0;
  std::string name, model, sound;
  float pos[3] = {0, 0, 0};     // metres
  float rotDeg[3] = {0, 0, 0};  // x, y, z; child offset already added
  float scale[3] = {1, 1, 1};
  float flat[2] = {1, 1};
  bool teleported = false;      // moved implausibly far this tick: do not interpolate
  bool on = false, visible = false, translucent = false, child = false;
};

struct HostSink {   // scene/audio side effects requested by brains
  virtual ~HostSink() = default;
  virtual void lightColor(int /*el*/, int /*r*/, int /*g*/, int /*b*/) {}
  virtual void backgroundColor(int /*r*/, int /*g*/, int /*b*/) {}
  virtual void play(int /*el*/, const std::string& /*sound*/) {}
  virtual void stop(int /*el*/) {}
};
struct HostInput {  // keyboard scancodes as the exe sees them (29 ctrl, 75/77 left/right, 57 space ...)
  virtual ~HostInput() = default;
  virtual bool key(int /*scancode*/) { return false; }
  virtual int joyX() { return 0; }
  virtual unsigned joyButtons() { return 0; }
  virtual bool any() { return false; }
};

class BrainHost {
 public:
  BrainHost();
  ~BrainHost();
  bool load(const std::string& gameDir, const std::string& project, HostSink* sink, HostInput* in, std::string* err);
  int advance(double dtSeconds);   // fixed 30 Hz brain ticks
  int count() const;
  ElemView view(int i) const;      // state after the latest tick
  // State blended between the previous and latest tick by alpha() (0..1) for display-rate rendering.
  ElemView interpolated(int i) const;
  double alpha() const;
  int credits(int i) const;        // element credits/score register (+0x5c)
  int exitCode() const;
  unsigned tickCount() const;
  std::vector<int> globals() const;            // 2048 game-state globals; carried across level loads
  void setGlobals(const std::vector<int>& g);
  int globalValue(int i) const;
  int cameraElement() const;       // first on-element of type 0 (d30 camera), -1 if none
  std::vector<std::string> unknownCommands() const;
 private:
  struct Impl;
  std::unique_ptr<Impl> d_;
};

}  // namespace rb
