#include "game/MenuRaster.h"
#include "ui/Ui.h"
#include <algorithm>
#include <cctype>

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
  const int rowH = fh * 2 + 8, hdrH = fh + 14;
  int n = int(m.items.size()), body = 0;
  for (const auto& it : m.items) body += (!it.empty() && it[0] == '#') ? hdrH : rowH;
  int w = 420, h = 48 + fh * 2 + body + 2 * (fh + 8) + 12;
  int x = (640 - w) / 2, y = (480 - h) / 2 - 6;
  c.fillRect(x - 6, y - 6, w + 12, h + 12, 0xffb02cff);       // toy-bright orange frame
  c.fillRect(x - 3, y - 3, w + 6, h + 6, 0x2ec4b6ff);         // teal inner frame
  c.fillRect(x, y, w, h, 0x14234cf0);
  c.fillRect(x, y, w, fh * 2 + 20, 0x1f3a7aff);
  int tw = f.width(m.title) * 2;
  f.draw(c, m.title, x + (w - tw) / 2, y + 10, 0xffd23fff, 2);
  int iy = y + fh * 2 + 32;
  int cy = iy;
  for (int i = 0; i < n; ++i) {
    const std::string& it = m.items[i];
    if (!it.empty() && it[0] == '#') {   // section header: small caps label with a rule
      std::string t = it.substr(1);
      for (auto& ch : t) ch = char(std::toupper((unsigned char)ch));
      f.draw(c, t, x + 22, cy + 4, 0x2ec4b6ff, 1);
      c.fillRect(x + 28 + f.width(t), cy + 4 + fh / 2, w - 56 - f.width(t), 2, 0x2ec4b680);
      cy += hdrH;
      continue;
    }
    bool sel = i == m.selected;
    if (sel) {
      c.fillRect(x + 14, cy - 2, w - 28, rowH - 2, 0xff5a3cff);
      c.fillRect(x + 18, cy + 2, w - 36, rowH - 10, 0xff8a4dff);
    }
    int iw = f.width(it) * 2;
    f.draw(c, it, x + (w - iw) / 2, cy + 2, sel ? 0xffffffff : 0xcfe0ffff, 2);
    cy += rowH;
  }
  int sy = cy + 4;
  if (!m.status.empty()) f.draw(c, m.status, (640 - f.width(m.status)) / 2, sy, 0xffd23fff, 1), sy += fh + 10;
  if (!m.footer.empty()) f.draw(c, m.footer, (640 - f.width(m.footer)) / 2, sy, 0xaabbddff, 1);
  return true;
}

}  // namespace rb
