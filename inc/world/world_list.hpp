#pragma once

#include "core/game_mode.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The worlds under a saves folder, for the title menu: listing them and
// creating, renaming and deleting one. Every operation works on a single,
// validated name directly under `root` and never follows a path outside it.
// Errors come back as a short sentence for the menu to show.

struct WorldInfo {
  std::string name;
  uint32_t seed = 0;
  GameMode mode = GameMode::Survival;
  std::filesystem::file_time_type last_played{}; // level.dat's modified time
  uintmax_t size_bytes = 0;
};

// Worlds with a readable level.dat, most recently played first.
std::vector<WorldInfo> listWorlds(const std::filesystem::path &root);

// 1-32 letters, digits, spaces, '_' or '-'; no leading or trailing space.
bool validWorldName(std::string_view name);

// A new world with this seed text (blank = random, a number as-is, other text
// hashed): its level.dat is written now, so it lists and loads with it.
std::optional<std::string> createWorld(const std::filesystem::path &root,
                                       const std::string &name, std::string_view seed_text,
                                       GameMode mode = GameMode::Survival);
// Rewrites only the mode in the world's level.dat.
std::optional<std::string> setWorldMode(const std::filesystem::path &root,
                                        const std::string &name, GameMode mode);
std::optional<std::string> renameWorld(const std::filesystem::path &root,
                                       const std::string &from, const std::string &to);
std::optional<std::string> deleteWorld(const std::filesystem::path &root,
                                       const std::string &name);

// "just now", "5 minutes ago", "today 14:05", "yesterday 09:30",
// "3 days ago", or a date, in local time.
std::string lastPlayedText(std::chrono::system_clock::time_point when,
                           std::chrono::system_clock::time_point now);
std::chrono::system_clock::time_point toSystemTime(std::filesystem::file_time_type t);
