// CPU rasteriser for the in-game pause menu: draws into a 640x480 RGBA canvas with the game's own bitmap font
// (rb::Font from src/ui). Kept in its own translation unit because Ui.h and Platform.h both define rb::Action.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rb {

struct MenuModel {
  std::string title = "Paused";
  std::vector<std::string> items;   // an item starting with '#' is a section header (not selectable, drawn small)
  int selected = 0;                 // index into items (never a header)
  std::string footer;          // hint line under the panel
  std::string status;          // e.g. "Hi-score 1200"
};

// Returns false when the font cannot be decoded. rgba gets 640*480*4 bytes, transparent outside the dimmer/panel.
bool rasterizeMenu(const std::vector<uint8_t>& fontBytes, const MenuModel& m, std::vector<uint8_t>& rgba);

}  // namespace rb
