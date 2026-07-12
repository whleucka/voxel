#pragma once

#include <string>

// Loads server/game properties from an external key=value file (Minecraft's
// server.properties style) into the global g_settings.
//
//  - If the file does not exist, a fully-commented default file is written and
//    that default is then loaded (so a fresh checkout Just Works).
//  - Unknown or malformed keys are warned about and skipped; the built-in
//    default for that setting is kept.
//  - "level-seed" is resolved here: blank picks a fresh random seed each run,
//    a numeric value is used verbatim, and any other text is hashed to a seed.
void loadServerProperties(const std::string &path = "voxel.properties");

// Writes the current g_settings back to the properties file, including the
// active world_seed so the same world reloads. Returns false on write failure.
bool saveServerProperties(const std::string &path = "voxel.properties");
