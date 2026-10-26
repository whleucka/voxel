#include "world/world_list.hpp"
#include "core/seed.hpp"
#include "world/world_save.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>

namespace fs = std::filesystem;

namespace {

// The world's folder, only if the name is valid (so it can't climb out).
std::optional<fs::path> folderFor(const fs::path &root, const std::string &name) {
  if (!validWorldName(name)) return std::nullopt;
  fs::path p = root / name;
  if (p.parent_path() != root) return std::nullopt;
  return p;
}

bool hasLevel(const fs::path &dir) {
  WorldSave::LevelData level;
  return WorldSave::loadLevel(dir.string(), level);
}

uintmax_t folderSize(const fs::path &dir) {
  uintmax_t total = 0;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec)) total += it->file_size(ec);
  return total;
}

std::tm localTime(std::chrono::system_clock::time_point t) {
  std::time_t tt = std::chrono::system_clock::to_time_t(t);
  std::tm out{};
  localtime_r(&tt, &out);
  return out;
}

} // namespace

bool validWorldName(std::string_view name) {
  if (name.empty() || name.size() > 32 || name.front() == ' ' || name.back() == ' ')
    return false;
  return std::all_of(name.begin(), name.end(), [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '_' || c == '-';
  });
}

std::vector<WorldInfo> listWorlds(const fs::path &root) {
  std::vector<WorldInfo> out;
  std::error_code ec;
  for (const fs::directory_entry &entry : fs::directory_iterator(root, ec)) {
    std::string name = entry.path().filename().string();
    if (!entry.is_directory(ec) || !validWorldName(name)) continue;
    WorldSave::LevelData level;
    if (!WorldSave::loadLevel(entry.path().string(), level)) continue;
    WorldInfo info;
    info.name = name;
    info.seed = level.seed;
    info.last_played = fs::last_write_time(entry.path() / "level.dat", ec);
    info.size_bytes = folderSize(entry.path());
    out.push_back(std::move(info));
  }
  std::sort(out.begin(), out.end(), [](const WorldInfo &a, const WorldInfo &b) {
    return a.last_played != b.last_played ? a.last_played > b.last_played : a.name < b.name;
  });
  return out;
}

std::optional<std::string> createWorld(const fs::path &root, const std::string &name,
                                       std::string_view seed_text) {
  auto dir = folderFor(root, name);
  if (!dir) return "World names are 1-32 letters, digits, spaces, _ or -";
  std::error_code ec;
  if (fs::exists(*dir, ec)) return "There's already a world called '" + name + "'";
  WorldSave::LevelData level;
  level.seed = resolveSeed(seed_text);
  level.noise_offset = noiseOffsetFor(level.seed);
  if (!WorldSave::saveLevel(dir->string(), level)) return "Couldn't create '" + name + "'";
  return std::nullopt;
}

std::optional<std::string> renameWorld(const fs::path &root, const std::string &from,
                                       const std::string &to) {
  auto src = folderFor(root, from);
  auto dst = folderFor(root, to);
  if (!dst) return "World names are 1-32 letters, digits, spaces, _ or -";
  if (!src || !hasLevel(*src)) return "There's no world called '" + from + "'";
  if (from == to) return std::nullopt;
  std::error_code ec;
  if (fs::exists(*dst, ec)) return "There's already a world called '" + to + "'";
  fs::rename(*src, *dst, ec);
  if (ec) return "Couldn't rename '" + from + "'";
  return std::nullopt;
}

std::optional<std::string> deleteWorld(const fs::path &root, const std::string &name) {
  auto dir = folderFor(root, name);
  // Only a real world folder: never a stray path or something that isn't a save.
  if (!dir || !hasLevel(*dir)) return "There's no world called '" + name + "'";
  std::error_code ec;
  fs::remove_all(*dir, ec);
  if (ec) return "Couldn't delete '" + name + "'";
  return std::nullopt;
}

std::chrono::system_clock::time_point toSystemTime(fs::file_time_type t) {
  return std::chrono::clock_cast<std::chrono::system_clock>(t);
}

std::string lastPlayedText(std::chrono::system_clock::time_point when,
                           std::chrono::system_clock::time_point now) {
  using namespace std::chrono;
  auto ago = now - when;
  if (ago < minutes(1)) return "just now";
  if (ago < hours(1)) {
    long m = duration_cast<minutes>(ago).count();
    return std::to_string(m) + (m == 1 ? " minute ago" : " minutes ago");
  }
  std::tm w = localTime(when), n = localTime(now);
  char clock[8];
  std::strftime(clock, sizeof clock, "%H:%M", &w);

  // Calendar days between the two, in local time.
  std::tm wd = w, nd = n;
  wd.tm_hour = nd.tm_hour = 12;
  wd.tm_min = nd.tm_min = wd.tm_sec = nd.tm_sec = 0;
  wd.tm_isdst = nd.tm_isdst = -1;
  long days = std::lround(std::difftime(std::mktime(&nd), std::mktime(&wd)) / 86400.0);
  if (days <= 0) return std::string("today ") + clock;
  if (days == 1) return std::string("yesterday ") + clock;
  if (days < 30) return std::to_string(days) + " days ago";
  char date[16];
  std::strftime(date, sizeof date, "%Y-%m-%d", &w);
  return date;
}
