// Entry point: SDL_main on Android (SDLActivity), plain main on desktop.
#include <SDL.h>   // renames main -> SDL_main where needed
#include "platform/Platform.h"

int main(int, char**) {
  rb::Config cfg;
  cfg.title = "Rollerbot: A Time Journey";
  cfg.applyEnv();
  rb::Platform platform;
  if (!platform.create(cfg)) return 1;
  auto game = rb::makeStubGame();   // lanes replace this when they land
  return platform.run(*game);
}
