// rb::Game implementation for Rollerbot: BrainVM simulation drawn through rb::Scene, sound through rb::Audio.
#pragma once
#include <memory>
#include "platform/Platform.h"

namespace rb {
// Returns nullptr when RB_DATA does not contain the game (caller falls back to the stub).
std::unique_ptr<Game> makeRollerbotGame(const std::string& dataDir);
bool gameFilesPresent(const std::string& dataDir);
}
