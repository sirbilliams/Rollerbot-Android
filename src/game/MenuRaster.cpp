#include "game/MenuRaster.h"
#include "ui/Ui.h"
#include <algorithm>

namespace rb {
namespace {

struct Canvas : IRenderer2D {
  std::vector<uint8_t>& px;
  explicit Canvas(std::vector<uint8_t>& p) : px(p) {}
  void fillRect(int x, int y, int w, int h, uint32_t c) override {   // 0xRRGGBBAA, source-over
    int a = c & 255;
    for (int j = std::max(0, y); j < std::min(480, y + h); ++j)
      for (int i = std::max(0, x); i < std::min(640, x + w); ++i) {
        uint8_t* d = &px[(size_t(j) * 640 + i) * 4];
        int da = d[3], oa = a + da * (255 - a) / 255;
        if (!oa) continue;
        for (int k = 0; k < 3; ++k) {
          int s = (c >> (24 - 8 * k)) & 255;
          d[k] = uint8_t((s * a + d[k] * da * (255 - a) / 255) / oa);
        }
        d[3] = uint8_t(oa);
      }
  }
  void overlay(std::string_view, int, int, int, int) override {}
};

}  // namespace

bool rasterizeMenu(const std::vector<uint8_t>& fontBytes, const MenuModel& m, std::vector<uint8_t>& rgba) {
  Font f;
  try { f = Font::decode(fontBytes); } catch (const std::exception&) { return false; }
  rgba.assign(size_t(640) * 480 * 4, 0);
  Canvas c(rgba);
  const int fh = std::max(8, f.height());
  const int rowH = fh * 2 + 10;
  int n = int(m.items.size());
  int w = 400, h = 52 + fh * 2 + n * rowH + 2 * (fh + 10) + 14;
  int x = (640 - w) / 2, y = (480 - h) / 2 - 10;
  c.fillRect(x - 6, y - 6, w + 12, h + 12, 0xffb02cff);       // toy-bright orange frame
  c.fillRect(x - 3, y - 3, w + 6, h + 6, 0x2ec4b6ff);         // teal inner frame
  c.fillRect(x, y, w, h, 0x14234cf0);
  c.fillRect(x, y, w, fh * 2 + 24, 0x1f3a7aff);
  int tw = f.width(m.title) * 2;
  f.draw(c, m.title, x + (w - tw) / 2, y + 12, 0xffd23fff, 2);
  int iy = y + fh * 2 + 40;
  for (int i = 0; i < n; ++i) {
    bool sel = i == m.selected;
    int ry = iy + i * rowH;
    if (sel) {
      c.fillRect(x + 14, ry - 4, w - 28, rowH - 4, 0xff5a3cff);
      c.fillRect(x + 18, ry, w - 36, rowH - 12, 0xff8a4dff);
    }
    int iw = f.width(m.items[i]) * 2;
    f.draw(c, m.items[i], x + (w - iw) / 2, ry + 2, sel ? 0xffffffff : 0xcfe0ffff, 2);
  }
  int sy = iy + n * rowH + 6;
  if (!m.status.empty()) f.draw(c, m.status, (640 - f.width(m.status)) / 2, sy, 0xffd23fff, 1), sy += fh + 10;
  if (!m.footer.empty()) f.draw(c, m.footer, (640 - f.width(m.footer)) / 2, sy, 0xaabbddff, 1);
  return true;
}

}  // namespace rb
