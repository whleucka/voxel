#include "world/world_save.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

// ─── POD binary helpers ────────────────────────────────────────────────────
template <class T> void wr(std::ostream &o, const T &v) {
  o.write(reinterpret_cast<const char *>(&v), sizeof(T));
}
template <class T> bool rd(std::istream &i, T &v) {
  return static_cast<bool>(i.read(reinterpret_cast<char *>(&v), sizeof(T)));
}

// Every file starts with a 4-byte magic and a version so a stale/foreign file
// is rejected rather than misread.
constexpr uint32_t kVersion = 1;

void writeHeader(std::ostream &o, const char magic[4]) {
  o.write(magic, 4);
  wr(o, kVersion);
}

bool checkHeader(std::istream &i, const char magic[4]) {
  char m[4];
  if (!i.read(m, 4)) return false;
  if (std::memcmp(m, magic, 4) != 0) return false;
  uint32_t ver;
  if (!rd(i, ver) || ver != kVersion) return false;
  return true;
}

constexpr char kLevelMagic[4]  = {'V', 'X', 'L', 'V'};
constexpr char kPlayerMagic[4] = {'V', 'X', 'P', 'L'};
constexpr char kEditMagic[4]   = {'V', 'X', 'E', 'D'};

} // namespace

namespace WorldSave {

std::string worldDir(const std::string &name) {
  return "saves/" + (name.empty() ? std::string("world") : name);
}

// ─── level.dat ──────────────────────────────────────────────────────────────
bool loadLevel(const std::string &dir, LevelData &out) {
  std::ifstream in(dir + "/level.dat", std::ios::binary);
  if (!in || !checkHeader(in, kLevelMagic)) return false;
  return rd(in, out.seed) && rd(in, out.noise_offset.x) &&
         rd(in, out.noise_offset.y) && rd(in, out.time_of_day);
}

bool loadPlayer(const std::string &dir, PlayerData &out) {
  std::ifstream in(dir + "/player.dat", std::ios::binary);
  if (!in || !checkHeader(in, kPlayerMagic)) return false;
  return rd(in, out.position.x) && rd(in, out.position.y) &&
         rd(in, out.position.z) && rd(in, out.yaw) && rd(in, out.pitch) &&
         rd(in, out.selected_slot) && rd(in, out.fly_mode);
}

bool loadEdits(const std::string &dir, WorldEdits &out) {
  std::ifstream in(dir + "/edits.dat", std::ios::binary);
  if (!in || !checkHeader(in, kEditMagic)) return false;

  uint32_t chunk_count = 0;
  if (!rd(in, chunk_count)) return false;
  for (uint32_t c = 0; c < chunk_count; ++c) {
    int32_t cx, cz;
    uint32_t edit_count;
    if (!rd(in, cx) || !rd(in, cz) || !rd(in, edit_count)) return false;
    ChunkEdits &ce = out[{cx, cz}];
    ce.reserve(edit_count);
    for (uint32_t e = 0; e < edit_count; ++e) {
      uint32_t idx;
      uint8_t  block;
      if (!rd(in, idx) || !rd(in, block)) return false;
      ce[idx] = block;
    }
  }
  return true;
}

// ─── save ────────────────────────────────────────────────────────────────────
bool save(const std::string &dir, const LevelData &level,
          const PlayerData &player, const WorldEdits &edits) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    std::cerr << "[save] cannot create '" << dir << "': " << ec.message()
              << "\n";
    return false;
  }

  {
    std::ofstream o(dir + "/level.dat", std::ios::binary | std::ios::trunc);
    if (!o) return false;
    writeHeader(o, kLevelMagic);
    wr(o, level.seed);
    wr(o, level.noise_offset.x);
    wr(o, level.noise_offset.y);
    wr(o, level.time_of_day);
  }
  {
    std::ofstream o(dir + "/player.dat", std::ios::binary | std::ios::trunc);
    if (!o) return false;
    writeHeader(o, kPlayerMagic);
    wr(o, player.position.x);
    wr(o, player.position.y);
    wr(o, player.position.z);
    wr(o, player.yaw);
    wr(o, player.pitch);
    wr(o, player.selected_slot);
    wr(o, player.fly_mode);
  }
  {
    std::ofstream o(dir + "/edits.dat", std::ios::binary | std::ios::trunc);
    if (!o) return false;
    writeHeader(o, kEditMagic);
    wr(o, static_cast<uint32_t>(edits.size()));
    for (const auto &[key, ce] : edits) {
      wr(o, static_cast<int32_t>(key.x));
      wr(o, static_cast<int32_t>(key.z));
      wr(o, static_cast<uint32_t>(ce.size()));
      for (const auto &[idx, block] : ce) {
        wr(o, idx);
        wr(o, block);
      }
    }
  }
  return true;
}

} // namespace WorldSave
