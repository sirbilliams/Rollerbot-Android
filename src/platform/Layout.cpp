#include "platform/Platform.h"

namespace rb {

Rect computeGameArea(int w, int h, Widescreen mode) {
  if (w <= 0 || h <= 0) return {0, 0, 1, 1};
  switch (mode) {
    case Widescreen::Stretch: return {0, 0, w, h};
    case Widescreen::Fill: {              // fit width, crop top/bottom equally
      int gh = int((long long)w * 3 / 4);
      if (gh < h) return {0, (h - gh) / 2, w, gh};   // taller than 4:3: letterbox instead
      return {0, (h - gh) / 2, w, gh};
    }
    case Widescreen::Panels:
    case Widescreen::Off:
    default: {
      int gw = int((long long)h * 4 / 3);
      if (gw <= w) return {(w - gw) / 2, 0, gw, h};
      int gh = int((long long)w * 3 / 4);
      return {0, (h - gh) / 2, w, gh};
    }
  }
}

void windowToGame(const Rect& a, float px, float py, float& gx, float& gy) {
  gx = (px - a.x) / float(a.w);
  gy = (py - a.y) / float(a.h);
}

void Input::setSource(int s, uint32_t m) { src_[s] = m; }

void Input::endFrame() {
  uint32_t now = src_[0] | src_[1] | src_[2];
  pressed_ |= now & ~held_;
  released_ |= held_ & ~now;
  held_ = now;
}

}  // namespace rb
