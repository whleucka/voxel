#include "world/world_save.hpp"

#include "core/constants.hpp"
#include "item/item.hpp"
#include <cctype>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

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
constexpr char kKnownMagic[4]  = {'V', 'X', 'D', 'I'};
constexpr char kFurnaceMagic[4] = {'V', 'X', 'F', 'U'};
constexpr char kChestMagic[4]   = {'V', 'X', 'C', 'H'};
constexpr char kFarmMagic[4]    = {'V', 'X', 'F', 'A'};
constexpr char kBoatMagic[4]    = {'V', 'X', 'B', 'O'};

constexpr uint32_t kChunkVolume = kChunkWidth * kChunkDepth * kChunkHeight;

// Writes a file whole or not at all: into <path>.tmp, flushed to disk, then
// renamed over the old one. A crash or a full disk mid-save leaves the last
// good file in place rather than half of a new one.
template <class F> bool writeAtomic(const std::string &path, F &&write) {
  namespace fs = std::filesystem;
  const std::string tmp = path + ".tmp";
  std::error_code ec;
  fs::create_directories(fs::path(path).parent_path(), ec);
  if (ec) return false;
  bool ok;
  {
    std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
    if (!o) return false;
    write(o);
    o.close();
    ok = static_cast<bool>(o);
  }
  if (ok) {
    int fd = ::open(tmp.c_str(), O_RDONLY);
    ok = fd >= 0 && ::fsync(fd) == 0;
    if (fd >= 0) ::close(fd);
  }
  if (ok) fs::rename(tmp, path, ec);
  if (!ok || ec) {
    fs::remove(tmp, ec);
    std::cerr << "[save] couldn't write '" << path << "'\n";
    return false;
  }
  return true;
}

// A loaded stack as this build can hold it: unknown items as nothing, and no
// more than the item stacks to.
ItemStack loadedStack(uint16_t id, uint16_t count) {
  ItemId item = static_cast<ItemId>(id);
  if (!isValidItem(item) || count == 0) return {};
  return {item, std::min(count, itemDef(item).max_stack)};
}

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

// Fails on a position or look that isn't a number, as the wire does.
bool readPlayer(std::istream &in, WorldSave::PlayerData &p) {
  return rd(in, p.position.x) && rd(in, p.position.y) && rd(in, p.position.z) &&
         rd(in, p.yaw) && rd(in, p.pitch) && rd(in, p.selected_slot) &&
         rd(in, p.fly_mode) && std::isfinite(p.position.x) && std::isfinite(p.position.y) &&
         std::isfinite(p.position.z) && std::isfinite(p.yaw) && std::isfinite(p.pitch);
}

// The respawn bed: a flag, then the foot cell if there is one.
void writeBed(std::ostream &o, const WorldSave::PlayerData &p) {
  wr(o, static_cast<uint8_t>(p.bed ? 1 : 0));
  if (!p.bed) return;
  wr(o, static_cast<int32_t>(p.bed->x));
  wr(o, static_cast<int32_t>(p.bed->y));
  wr(o, static_cast<int32_t>(p.bed->z));
}

// Missing (an older file): no bed.
void readBed(std::istream &in, WorldSave::PlayerData &p) {
  uint8_t has = 0;
  int32_t x = 0, y = 0, z = 0;
  if (rd(in, has) && has == 1 && rd(in, x) && rd(in, y) && rd(in, z)) p.bed = glm::ivec3(x, y, z);
}

// The map pins: a count, then each one's cell and colour.
void writePins(std::ostream &o, const WorldSave::PlayerData &p) {
  wr(o, static_cast<uint8_t>(p.pins.size()));
  for (const Pin &pin : p.pins) {
    wr(o, static_cast<int32_t>(pin.pos.x));
    wr(o, static_cast<int32_t>(pin.pos.y));
    wr(o, static_cast<int32_t>(pin.pos.z));
    wr(o, pin.colour);
  }
}

// Missing (an older file): none. Too many, or a colour out of range or
// already taken, and the extra pins are dropped.
void readPins(std::istream &in, WorldSave::PlayerData &p) {
  uint8_t n = 0;
  if (!rd(in, n)) return;
  for (uint8_t i = 0; i < n; ++i) {
    int32_t x = 0, y = 0, z = 0;
    uint8_t colour = 0;
    if (!rd(in, x) || !rd(in, y) || !rd(in, z) || !rd(in, colour)) return;
    bool taken = false;
    for (const Pin &pin : p.pins) taken |= pin.colour == colour;
    if (colour < kMaxPins && !taken && p.pins.size() < kMaxPins)
      p.pins.push_back({glm::ivec3(x, y, z), colour});
  }
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
    s = loadedStack(id, count);
  }
  out = std::move(slots);
  return true;
}

// Tool wear, slot by slot: a trailing section, so files from before tools
// load (as unworn) and older builds stop reading before it.
void writeWear(std::ostream &o, std::span<const ItemStack> slots) {
  wr(o, static_cast<uint16_t>(slots.size()));
  for (const ItemStack &s : slots) wr(o, static_cast<uint16_t>(s.empty() ? 0 : s.wear));
}

void readWear(std::istream &in, std::vector<ItemStack> &slots) {
  uint16_t n = 0;
  if (!rd(in, n)) return;
  for (uint16_t i = 0; i < n; ++i) {
    uint16_t wear = 0;
    if (!rd(in, wear)) return;
    if (i < slots.size() && !slots[i].empty()) slots[i].wear = wear;
  }
}

void writeIds(std::ostream &o, std::span<const ItemId> ids) {
  wr(o, static_cast<uint32_t>(ids.size()));
  for (ItemId id : ids) wr(o, static_cast<uint16_t>(id));
}

bool readIds(std::istream &in, std::vector<ItemId> &out) {
  uint32_t n = 0;
  if (!rd(in, n) || n > 4096) return false;
  out.clear();
  for (uint32_t i = 0; i < n; ++i) {
    uint16_t v = 0;
    if (!rd(in, v)) return false;
    if (isValidItem(static_cast<ItemId>(v))) out.push_back(static_cast<ItemId>(v));
  }
  return true;
}

void writeStack(std::ostream &o, const ItemStack &s) {
  wr(o, static_cast<uint16_t>(s.id));
  wr(o, s.count);
}

bool readStack(std::istream &in, ItemStack &s) {
  uint16_t id = 0, count = 0;
  if (!rd(in, id) || !rd(in, count)) return false;
  s = loadedStack(id, count);
  return true;
}

} // namespace

namespace WorldSave {

std::string worldDir(const std::string &name) {
  return "saves/" + (name.empty() ? std::string("world") : name);
}

void setAside(const std::string &dir, const std::string &file) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path from = fs::path(dir) / file;
  if (!fs::exists(from, ec)) return;
  fs::path to = from;
  to += ".unreadable";
  fs::rename(from, to, ec);
  if (ec) std::cerr << "[save] couldn't set aside " << from << ": " << ec.message() << "\n";
  else std::cerr << "[save] " << from << " won't load; kept as " << to << "\n";
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
  if (!readPlayer(in, out)) return false;
  readBed(in, out);
  readPins(in, out);
  return true;
}

bool loadEdits(const std::string &dir, WorldEdits &out) {
  std::ifstream in(dir + "/edits.dat", std::ios::binary);
  if (!in || !checkHeader(in, kEditMagic)) return false;

  uint32_t chunk_count = 0;
  if (!rd(in, chunk_count)) return false;
  for (uint32_t c = 0; c < chunk_count; ++c) {
    int32_t cx, cz;
    uint32_t edit_count;
    // A chunk can't hold more edits than it has blocks.
    if (!rd(in, cx) || !rd(in, cz) || !rd(in, edit_count) || edit_count > kChunkVolume)
      return false;
    ChunkEdits &ce = out[{cx, cz}];
    for (uint32_t e = 0; e < edit_count; ++e) {
      uint32_t idx;
      uint8_t  block;
      if (!rd(in, idx) || !rd(in, block)) return false;
      // Blocks from a newer build, or cells outside the chunk, are dropped.
      if (idx < kChunkVolume && block <= static_cast<uint8_t>(kLastBlockType)) ce[idx] = block;
    }
  }
  return true;
}

// ─── level.dat (alone) ───────────────────────────────────────────────────────

bool saveLevel(const std::string &dir, const LevelData &level) {
  return writeAtomic(dir + "/level.dat", [&](std::ostream &o) {
    writeHeader(o, kLevelMagic);
    wr(o, level.seed);
    wr(o, level.noise_offset.x);
    wr(o, level.noise_offset.y);
    wr(o, level.time_of_day);
    wr(o, static_cast<uint8_t>(level.mode));
    wr(o, level.worldgen);
  });
}

// ─── save ────────────────────────────────────────────────────────────────────
bool save(const std::string &dir, const LevelData &level,
          const PlayerData &player, const WorldEdits &edits) {
  return saveLevel(dir, level) &&
         writeAtomic(dir + "/player.dat",
                     [&](std::ostream &o) {
                       writeHeader(o, kPlayerMagic);
                       writePlayer(o, player);
                       writeBed(o, player);
                       writePins(o, player);
                     }) &&
         writeAtomic(dir + "/edits.dat", [&](std::ostream &o) {
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
         });
}

// ─── inventory.dat ──────────────────────────────────────────────────────────
bool loadInventory(const std::string &dir, std::vector<ItemStack> &out) {
  std::ifstream in(dir + "/inventory.dat", std::ios::binary);
  if (!in || !checkHeader(in, kInvMagic)) return false;
  if (!readSlots(in, out)) return false;
  readWear(in, out);
  return true;
}

bool saveInventory(const std::string &dir, std::span<const ItemStack> slots) {
  return writeAtomic(dir + "/inventory.dat", [&](std::ostream &o) {
    writeHeader(o, kInvMagic);
    writeSlots(o, slots);
    writeWear(o, slots);
  });
}

// ─── discovered.dat ─────────────────────────────────────────────────────────

bool loadDiscovered(const std::string &dir, std::vector<ItemId> &out) {
  std::ifstream in(dir + "/discovered.dat", std::ios::binary);
  if (!in || !checkHeader(in, kKnownMagic)) return false;
  return readIds(in, out);
}

bool saveDiscovered(const std::string &dir, std::span<const ItemId> ids) {
  return writeAtomic(dir + "/discovered.dat", [&](std::ostream &o) {
    writeHeader(o, kKnownMagic);
    writeIds(o, ids);
  });
}

// ─── furnaces.dat ───────────────────────────────────────────────────────────

bool loadFurnaces(const std::string &dir, std::vector<FurnaceRecord> &out) {
  std::ifstream in(dir + "/furnaces.dat", std::ios::binary);
  if (!in || !checkHeader(in, kFurnaceMagic)) return false;
  uint32_t n = 0;
  if (!rd(in, n) || n > 1'000'000) return false;
  out.clear();
  for (uint32_t i = 0; i < n; ++i) {
    FurnaceRecord r;
    Furnace &f = r.furnace;
    if (!rd(in, r.pos.x) || !rd(in, r.pos.y) || !rd(in, r.pos.z) || !readStack(in, f.input) ||
        !readStack(in, f.fuel) || !readStack(in, f.output) || !rd(in, f.burn_ms) ||
        !rd(in, f.burn_total_ms) || !rd(in, f.cook_ms))
      return false;
    out.push_back(r);
  }
  return true;
}

bool saveFurnaces(const std::string &dir, std::span<const FurnaceRecord> furnaces) {
  return writeAtomic(dir + "/furnaces.dat", [&](std::ostream &o) {
    writeHeader(o, kFurnaceMagic);
    wr(o, static_cast<uint32_t>(furnaces.size()));
    for (const FurnaceRecord &r : furnaces) {
      const Furnace &f = r.furnace;
      wr(o, r.pos.x);
      wr(o, r.pos.y);
      wr(o, r.pos.z);
      writeStack(o, f.input);
      writeStack(o, f.fuel);
      writeStack(o, f.output);
      wr(o, f.burn_ms);
      wr(o, f.burn_total_ms);
      wr(o, f.cook_ms);
    }
  });
}

// ─── chests.dat ─────────────────────────────────────────────────────────────

bool loadChests(const std::string &dir, std::vector<ChestRecord> &out) {
  std::ifstream in(dir + "/chests.dat", std::ios::binary);
  if (!in || !checkHeader(in, kChestMagic)) return false;
  uint32_t n = 0;
  if (!rd(in, n) || n > 1'000'000) return false;
  out.clear();
  for (uint32_t i = 0; i < n; ++i) {
    ChestRecord r;
    if (!rd(in, r.pos.x) || !rd(in, r.pos.y) || !rd(in, r.pos.z)) return false;
    // Each stack with its wear: chests hold tools.
    for (ItemStack &s : r.chest) {
      uint16_t wear = 0;
      if (!readStack(in, s) || !rd(in, wear)) return false;
      if (!s.empty()) s.wear = wear;
    }
    out.push_back(r);
  }
  return true;
}

bool saveChests(const std::string &dir, std::span<const ChestRecord> chests) {
  return writeAtomic(dir + "/chests.dat", [&](std::ostream &o) {
    writeHeader(o, kChestMagic);
    wr(o, static_cast<uint32_t>(chests.size()));
    for (const ChestRecord &r : chests) {
      wr(o, r.pos.x);
      wr(o, r.pos.y);
      wr(o, r.pos.z);
      for (const ItemStack &s : r.chest) {
        writeStack(o, s);
        wr(o, s.wear);
      }
    }
  });
}

// ─── players/<name>.dat ─────────────────────────────────────────────────────

bool saveGuest(const std::string &dir, const std::string &name, const GuestRecord &rec) {
  if (!safeName(name)) return false;
  return writeAtomic(dir + "/players/" + name + ".dat", [&](std::ostream &o) {
    writeHeader(o, kGuestMagic);
    writePlayer(o, rec.player);
    writeSlots(o, rec.inventory);
    writeIds(o, rec.discovered);
    writeWear(o, rec.inventory);
    writeBed(o, rec.player);
    writePins(o, rec.player);
  });
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
    if (!safeName(name)) continue; // not one of ours
    if (!in || !checkHeader(in, kGuestMagic) || !readPlayer(in, rec.player) ||
        !readSlots(in, rec.inventory)) {
      in.close();
      setAside(folder.string(), entry.path().filename().string());
      continue;
    }
    if (!readIds(in, rec.discovered)) rec.discovered.clear(); // from before crafting
    else {
      readWear(in, rec.inventory); // from before tools: unworn
      readBed(in, rec.player);     // from before beds: none
      readPins(in, rec.player);    // from before the map: none
    }
    out[name] = std::move(rec);
  }
  return !ec;
}


// ─── farms.dat ──────────────────────────────────────────────────────────────

bool loadFarms(const std::string &dir, Farms &out) {
  std::ifstream in(dir + "/farms.dat", std::ios::binary);
  if (!in || !checkHeader(in, kFarmMagic)) return false;
  Farms farms;
  uint32_t n = 0;
  if (!rd(in, n) || n > 1'000'000) return false;
  for (uint32_t i = 0; i < n; ++i) {
    CropRecord r;
    if (!rd(in, r.pos.x) || !rd(in, r.pos.y) || !rd(in, r.pos.z) || !rd(in, r.left_ms))
      return false;
    farms.crops.push_back(r);
  }
  if (!rd(in, n) || n > 1'000'000) return false;
  for (uint32_t i = 0; i < n; ++i) {
    SoilRecord r;
    uint8_t by_water = 0;
    if (!rd(in, r.pos.x) || !rd(in, r.pos.y) || !rd(in, r.pos.z) || !rd(in, r.wet_ms) ||
        !rd(in, by_water))
      return false;
    r.by_water = by_water != 0;
    farms.soil.push_back(r);
  }
  out = std::move(farms);
  return true;
}

bool saveFarms(const std::string &dir, const Farms &farms) {
  return writeAtomic(dir + "/farms.dat", [&](std::ostream &o) {
    writeHeader(o, kFarmMagic);
    wr(o, static_cast<uint32_t>(farms.crops.size()));
    for (const CropRecord &r : farms.crops) {
      wr(o, r.pos.x);
      wr(o, r.pos.y);
      wr(o, r.pos.z);
      wr(o, r.left_ms);
    }
    wr(o, static_cast<uint32_t>(farms.soil.size()));
    for (const SoilRecord &r : farms.soil) {
      wr(o, r.pos.x);
      wr(o, r.pos.y);
      wr(o, r.pos.z);
      wr(o, r.wet_ms);
      wr(o, static_cast<uint8_t>(r.by_water ? 1 : 0));
    }
  });
}

// ─── boats.dat ──────────────────────────────────────────────────────────────

bool loadBoats(const std::string &dir, std::vector<BoatRecord> &out) {
  std::ifstream in(dir + "/boats.dat", std::ios::binary);
  if (!in || !checkHeader(in, kBoatMagic)) return false;
  uint32_t n = 0;
  if (!rd(in, n) || n > 100'000) return false;
  std::vector<BoatRecord> boats;
  for (uint32_t i = 0; i < n; ++i) {
    BoatRecord r;
    if (!rd(in, r.position.x) || !rd(in, r.position.y) || !rd(in, r.position.z) ||
        !rd(in, r.yaw))
      return false;
    if (!std::isfinite(r.position.x) || !std::isfinite(r.position.y) ||
        !std::isfinite(r.position.z) || !std::isfinite(r.yaw))
      return false;
    boats.push_back(r);
  }
  // Then who sat where. A file from before riders were kept ends here.
  if (in.peek() != std::char_traits<char>::eof()) {
    for (BoatRecord &r : boats)
      for (BoatRecord::Seat &seat : r.seats) {
        uint8_t rider = 0, len = 0;
        if (!rd(in, rider) || rider > static_cast<uint8_t>(BoatRecord::Rider::Guest) ||
            !rd(in, len))
          return false;
        seat.rider = static_cast<BoatRecord::Rider>(rider);
        seat.name.resize(len);
        if (len && !in.read(seat.name.data(), len)) return false;
        if (seat.rider == BoatRecord::Rider::Guest && !safeName(seat.name)) return false;
      }
  }
  out = std::move(boats);
  return true;
}

bool saveBoats(const std::string &dir, std::span<const BoatRecord> boats) {
  return writeAtomic(dir + "/boats.dat", [&](std::ostream &o) {
    writeHeader(o, kBoatMagic);
    wr(o, static_cast<uint32_t>(boats.size()));
    for (const BoatRecord &r : boats) {
      wr(o, r.position.x);
      wr(o, r.position.y);
      wr(o, r.position.z);
      wr(o, r.yaw);
    }
    for (const BoatRecord &r : boats)
      for (const BoatRecord::Seat &seat : r.seats) {
        const std::string name = seat.name.substr(0, 255);
        wr(o, static_cast<uint8_t>(seat.rider));
        wr(o, static_cast<uint8_t>(name.size()));
        o.write(name.data(), static_cast<std::streamsize>(name.size()));
      }
  });
}

} // namespace WorldSave
