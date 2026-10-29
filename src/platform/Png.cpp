// Minimal PNG writer: stored (uncompressed) deflate blocks, so no zlib/libpng needed.
#include "platform/Platform.h"
#include <cstdio>
#include <vector>

namespace rb {
namespace {
uint32_t crcTable[256];
void initCrc() {
  if (crcTable[1]) return;
  for (uint32_t n = 0; n < 256; n++) {
    uint32_t c = n;
    for (int k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
    crcTable[n] = c;
  }
}
uint32_t crc(uint32_t c, const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; i++) c = crcTable[(c ^ p[i]) & 0xff] ^ (c >> 8);
  return c;
}
void be32(std::vector<uint8_t>& v, uint32_t x) {
  for (int s = 24; s >= 0; s -= 8) v.push_back(uint8_t(x >> s));
}
void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
  be32(out, uint32_t(data.size()));
  std::vector<uint8_t> td(type, type + 4);
  td.insert(td.end(), data.begin(), data.end());
  out.insert(out.end(), td.begin(), td.end());
  be32(out, crc(0xffffffffu, td.data(), td.size()) ^ 0xffffffffu);
}
}  // namespace

bool writePng(const std::string& path, const uint8_t* rgba, int w, int h) {
  if (!rgba || w <= 0 || h <= 0) return false;
  initCrc();
  std::vector<uint8_t> raw;
  raw.reserve(size_t(h) * (size_t(w) * 4 + 1));
  for (int y = 0; y < h; y++) {
    raw.push_back(0);  // filter none
    raw.insert(raw.end(), rgba + size_t(y) * w * 4, rgba + size_t(y + 1) * w * 4);
  }
  std::vector<uint8_t> z = {0x78, 0x01};
  uint32_t a = 1, b = 0;
  for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  for (size_t pos = 0; pos < raw.size();) {
    size_t n = raw.size() - pos < 65535 ? raw.size() - pos : 65535;
    z.push_back(pos + n == raw.size() ? 1 : 0);
    z.push_back(uint8_t(n)); z.push_back(uint8_t(n >> 8));
    z.push_back(uint8_t(~n)); z.push_back(uint8_t((~n) >> 8));
    z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
    pos += n;
  }
  be32(z, (b << 16) | a);
  std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
  std::vector<uint8_t> ihdr;
  be32(ihdr, uint32_t(w)); be32(ihdr, uint32_t(h));
  ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
  chunk(out, "IHDR", ihdr);
  chunk(out, "IDAT", z);
  chunk(out, "IEND", {});
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
  std::fclose(f);
  return ok;
}

}  // namespace rb
