#include "world/world_save.hpp"

#include <cctype>
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
constexpr char kInvMagic[4]    = {'V', 'X', 'I', 'N'};
constexpr char kGuestMagic[4]  = {'V', 'X', 'G', 'P'};

bool safeName(const std::string &name) {
  if (name.empty() || name.size() > 16) return false;
  for (char c : name)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
  return true;
}

void writePlayer(std::ostream &o, const WorldSave::PlayerData &p) {
  wr(o, p.position.x);
  wr(o, p.position.y);
  wr(o, p.position.z);
  wr(o, p.yaw);
  wr(o, p.pitch);
  wr(o, p.selected_slot);
  wr(o, p.fly_mode);
}

bool readPlayer(std::istream &in, WorldSave::PlayerData &p) {
  return rd(in, p.position.x) && rd(in, p.position.y) && rd(in, p.position.z) &&
         rd(in, p.yaw) && rd(in, p.pitch) && rd(in, p.selected_slot) && rd(in, p.fly_mode);
}

void writeSlots(std::ostream &o, std::span<const ItemStack> slots) {
  wr(o, static_cast<uint16_t>(slots.size()));
  for (const ItemStack &s : slots) {
    wr(o, static_cast<uint16_t>(s.empty() ? ItemId::NONE : s.id));
    wr(o, static_cast<uint16_t>(s.empty() ? 0 : s.count));
  }
}

// Items this build doesn't know load as empty slots.
bool readSlots(std::istream &in, std::vector<ItemStack> &out) {
  uint16_t slot_count = 0;
  if (!rd(in, slot_count)) return false;
  std::vector<ItemStack> slots(slot_count);
  for (ItemStack &s : slots) {
    uint16_t id, count;
    if (!rd(in, id) || !rd(in, count)) return false;
    ItemId item = static_cast<ItemId>(id);
    if (isValidItem(item) && count > 0) s = {item, count};
  }
  out = std::move(slots);
  return true;
}

} // namespace

namespace WorldSave {

std::string worldDir(const std::string &name) {
  return "saves/" + (name.empty() ? std::string("world") : name);
}

// ─── level.dat ──────────────────────────────────────────────────────────────
bool loadLevel(const std::string &dir, LevelData &out) {
  std::ifstream in(dir + "/level.dat", std::ios::binary);
  if (!in || !checkHeader(in, kLevelMagic)) return false;
  if (!(rd(in, out.seed) && rd(in, out.noise_offset.x) && rd(in, out.noise_offset.y) &&
        rd(in, out.time_of_day)))
    return false;
  uint8_t mode = 0;
  out.mode = rd(in, mode) && mode == static_cast<uint8_t>(GameMode::Creative)
                 ? GameMode::Creative
                 : GameMode::Survival;
  uint8_t worldgen = 0;
  out.worldgen = rd(in, worldgen) ? worldgen : worldgen::kOriginal;
  return true;
}

bool loadPlayer(const std::string &dir, PlayerData &out) {
  std::ifstream in(dir + "/player.dat", std::ios::binary);
  if (!in || !checkHeader(in, kPlayerMagic)) return false;
  return readPlayer(in, out);
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

// ─── level.dat (alone) ───────────────────────────────────────────────────────

bool saveLevel(const std::string &dir, const LevelData &level) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) return false;
  std::ofstream o(dir + "/level.dat", std::ios::binary | std::ios::trunc);
  if (!o) return false;
  writeHeader(o, kLevelMagic);
  wr(o, level.seed);
  wr(o, level.noise_offset.x);
  wr(o, level.noise_offset.y);
  wr(o, level.time_of_day);
  wr(o, static_cast<uint8_t>(level.mode));
  wr(o, level.worldgen);
  return static_cast<bool>(o);
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

  if (!saveLevel(dir, level)) return false;
  {
    std::ofstream o(dir + "/player.dat", std::ios::binary | std::ios::trunc);
    if (!o) return false;
    writeHeader(o, kPlayerMagic);
    writePlayer(o, player);
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

// ─── inventory.dat ──────────────────────────────────────────────────────────
bool loadInventory(const std::string &dir, std::vector<ItemStack> &out) {
  std::ifstream in(dir + "/inventory.dat", std::ios::binary);
  if (!in || !checkHeader(in, kInvMagic)) return false;
  return readSlots(in, out);
}

bool saveInventory(const std::string &dir, std::span<const ItemStack> slots) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) return false;

  std::ofstream o(dir + "/inventory.dat", std::ios::binary | std::ios::trunc);
  if (!o) return false;
  writeHeader(o, kInvMagic);
  writeSlots(o, slots);
  return static_cast<bool>(o);
}

// ─── players/<name>.dat ─────────────────────────────────────────────────────

bool saveGuest(const std::string &dir, const std::string &name, const GuestRecord &rec) {
  if (!safeName(name)) return false;
  std::error_code ec;
  std::filesystem::create_directories(dir + "/players", ec);
  if (ec) return false;
  std::ofstream o(dir + "/players/" + name + ".dat", std::ios::binary | std::ios::trunc);
  if (!o) return false;
  writeHeader(o, kGuestMagic);
  writePlayer(o, rec.player);
  writeSlots(o, rec.inventory);
  return static_cast<bool>(o);
}

bool loadGuests(const std::string &dir, std::map<std::string, GuestRecord> &out) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path folder = fs::path(dir) / "players";
  if (!fs::is_directory(folder, ec)) return true;
  for (const fs::directory_entry &entry : fs::directory_iterator(folder, ec)) {
    if (entry.path().extension() != ".dat") continue;
    std::string name = entry.path().stem().string();
    std::ifstream in(entry.path(), std::ios::binary);
    GuestRecord rec;
    if (!safeName(name) || !in || !checkHeader(in, kGuestMagic) ||
        !readPlayer(in, rec.player) || !readSlots(in, rec.inventory)) {
      std::cerr << "[save] skipping unreadable " << entry.path() << "\n";
      continue;
    }
    out[name] = std::move(rec);
  }
  return !ec;
}

} // namespace WorldSave
