// Entry point: SDL_main on Android (SDLActivity), plain main on desktop.
#include <SDL.h>   // renames main -> SDL_main where needed
#include "platform/Platform.h"
#include "game/Game.h"

int main(int, char**) {
  rb::Config cfg;
  cfg.title = "Rollerbot: A Time Journey";
  cfg.applyEnv();
  rb::Platform platform;
  if (!platform.create(cfg)) return 1;
  // Real game when the data dir has the retail files (RB_DATA), otherwise the stub shell.
  auto game = rb::makeRollerbotGame(platform.dataDir());
  if (!game) game = rb::makeStubGame();
  return platform.run(*game);
}
