#include "net/wire.hpp"
#include "item/item.hpp"
#include <cmath>
#include <cstring>
#include <type_traits>
#include <utility>

namespace wire {
namespace {

constexpr uint8_t kMagic[4] = {'V', 'O', 'X', 'L'};
constexpr size_t kMaxString = 1024;

// ─── Primitive writer / reader ─────────────────────────────────────────
// Every message struct lists its fields once, in fields() below; the Writer
// and Reader walk that list, so encoding and decoding can't drift apart.

struct Writer {
  std::vector<uint8_t> out;

  void bytes(const void *p, size_t n) {
    const auto *b = static_cast<const uint8_t *>(p);
    out.insert(out.end(), b, b + n);
  }
  template <class U> void le(U v) {
    for (size_t i = 0; i < sizeof(U); ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
  }

  void put(bool v) { out.push_back(v ? 1 : 0); }
  void put(uint8_t v) { out.push_back(v); }
  void put(uint16_t v) { le(v); }
  void put(uint32_t v) { le(v); }
  void put(int32_t v) { le(static_cast<uint32_t>(v)); }
  void put(float v) {
    uint32_t u;
    std::memcpy(&u, &v, 4);
    le(u);
  }
  template <class E> requires std::is_enum_v<E> void put(E v) {
    put(static_cast<std::underlying_type_t<E>>(v));
  }
  void put(const std::string &s) {
    put(static_cast<uint16_t>(s.size()));
    bytes(s.data(), s.size());
  }
  void put(const glm::vec2 &v) { put(v.x), put(v.y); }
  void put(const glm::vec3 &v) { put(v.x), put(v.y), put(v.z); }
  void put(const glm::ivec3 &v) { put(v.x), put(v.y), put(v.z); }
  template <class T> void put(const std::vector<T> &v) {
    put(static_cast<uint32_t>(v.size()));
    for (const T &x : v) put(x);
  }
  template <class T, size_t N> void put(const std::array<T, N> &a) {
    for (const T &x : a) put(x);
  }
  template <class T> requires std::is_class_v<T> void put(const T &v);

  template <class... T> void operator()(T &...v) { (put(v), ...); }
};

struct Reader {
  std::span<const uint8_t> in;
  size_t pos = 0;
  bool ok = true;

  bool need(size_t n) {
    if (!ok || in.size() - pos < n) ok = false;
    return ok;
  }
  template <class U> U le() {
    if (!need(sizeof(U))) return U{};
    U v = 0;
    for (size_t i = 0; i < sizeof(U); ++i) v |= static_cast<U>(U(in[pos + i]) << (8 * i));
    pos += sizeof(U);
    return v;
  }

  void get(bool &v) {
    uint8_t b = le<uint8_t>();
    if (b > 1) ok = false;
    v = b == 1;
  }
  void get(uint8_t &v) { v = le<uint8_t>(); }
  void get(uint16_t &v) { v = le<uint16_t>(); }
  void get(uint32_t &v) { v = le<uint32_t>(); }
  void get(int32_t &v) { v = static_cast<int32_t>(le<uint32_t>()); }
  void get(float &v) {
    uint32_t u = le<uint32_t>();
    std::memcpy(&v, &u, 4);
    if (!std::isfinite(v)) ok = false, v = 0.0f;
  }
  void get(BlockType &v) {
    uint8_t raw = le<uint8_t>();
    v = static_cast<BlockType>(raw);
    if (raw > static_cast<uint8_t>(kLastBlockType)) ok = false, v = BlockType::AIR;
  }
  void get(ItemId &v) {
    v = static_cast<ItemId>(le<uint16_t>());
    if (v != ItemId::NONE && !isValidItem(v)) ok = false, v = ItemId::NONE;
  }
  void get(EntityKind &v) {
    v = static_cast<EntityKind>(le<uint16_t>());
    if (!isValidEntityKind(v)) ok = false, v = EntityKind::TestCreature;
  }
  void get(std::string &s) {
    uint16_t n = le<uint16_t>();
    if (n > kMaxString || !need(n)) return void(ok = false);
    s.assign(reinterpret_cast<const char *>(in.data() + pos), n);
    pos += n;
  }
  void get(glm::vec2 &v) { get(v.x), get(v.y); }
  void get(glm::vec3 &v) { get(v.x), get(v.y), get(v.z); }
  void get(glm::ivec3 &v) { get(v.x), get(v.y), get(v.z); }
  template <class T> void get(std::vector<T> &v) {
    uint32_t n = le<uint32_t>();
    // Every element takes at least one byte: a bigger count is a lie.
    if (!ok || n > in.size() - pos) return void(ok = false);
    v.resize(n);
    for (T &x : v) {
      get(x);
      if (!ok) return;
    }
  }
  template <class T, size_t N> void get(std::array<T, N> &a) {
    for (T &x : a) get(x);
  }
  template <class T> requires std::is_class_v<T> void get(T &v);

  template <class... T> void operator()(T &...v) { (get(v), ...); }
};

// ─── Message fields, in wire order ─────────────────────────────────────
// A Command or Event alternative without a fields() overload doesn't compile.

template <class IO> void fields(IO &io, ItemStack &m) { io(m.id, m.count, m.wear); }
template <class IO> void fields(IO &io, BlockEdit &m) { io(m.pos, m.type); }
template <class IO> void fields(IO &io, EntityView &m) {
  io(m.id, m.kind, m.position, m.yaw, m.item, m.hurt, m.happy);
}
template <class IO> void fields(IO &io, PlayerView &m) {
  io(m.id, m.name, m.position, m.yaw, m.pitch, m.crouch);
}

// Commands
template <class IO> void fields(IO &io, Join &m) { io(m.name); }
template <class IO> void fields(IO &io, MovePlayer &m) {
  io(m.position, m.yaw, m.pitch, m.fly, m.crouch, m.on_ground, m.sprinting, m.in_water,
     m.eyes_underwater, m.respawn);
}
template <class IO> void fields(IO &io, BreakBlock &m) { io(m.pos); }
template <class IO> void fields(IO &io, PlaceBlock &m) { io(m.pos); }
template <class IO> void fields(IO &io, SelectSlot &m) {
  int32_t slot = m.slot;
  io(slot);
  m.slot = slot;
}
template <class IO> void fields(IO &, Save &) {}
template <class IO> void fields(IO &io, SetTimeOfDay &m) { io(m.seconds); }
template <class IO> void fields(IO &io, GiveItem &m) { io(m.id, m.count); }
template <class IO> void fields(IO &io, MoveItems &m) {
  int32_t from = m.from, to = m.to;
  io(from, to, m.count);
  m.from = from, m.to = to;
}
template <class IO> void fields(IO &io, DropItem &m) { io(m.whole_stack); }
template <class IO> void fields(IO &io, AttackEntity &m) { io(m.id); }
template <class IO> void fields(IO &io, SpawnDebugEntity &m) { io(m.kind); }
template <class IO> void fields(IO &io, InteractEntity &m) { io(m.id); }
template <class IO> void fields(IO &, EatItem &) {}
template <class IO> void fields(IO &io, Craft &m) { io(m.recipe, m.all); }
template <class IO> void fields(IO &io, OpenContainer &m) { io(m.pos); }
template <class IO> void fields(IO &, CloseContainer &) {}

// Events
template <class IO> void fields(IO &io, Joined &m) {
  int32_t slot = m.slot;
  uint8_t mode = static_cast<uint8_t>(m.mode);
  io(m.id, m.position, m.yaw, m.pitch, slot, m.fly, mode);
  m.slot = slot;
  m.mode = mode == static_cast<uint8_t>(GameMode::Creative) ? GameMode::Creative
                                                             : GameMode::Survival;
}
template <class IO> void fields(IO &io, BlockChanged &m) { io(m.pos, m.type); }
template <class IO> void fields(IO &io, PlayerJoined &m) { io(m.id, m.name); }
template <class IO> void fields(IO &io, PlayerLeft &m) { io(m.id); }
template <class IO> void fields(IO &io, CommandRejected &m) { io(m.reason); }
template <class IO> void fields(IO &io, InventoryContents &m) { io(m.slots); }
template <class IO> void fields(IO &io, KnownItems &m) { io(m.ids); }
template <class IO> void fields(IO &io, ContainerView &m) {
  io(m.pos, m.open, m.input, m.fuel, m.output, m.burn, m.cook);
}
template <class IO> void fields(IO &io, VitalsChanged &m) {
  int32_t h = m.health, s = m.stamina, b = m.breath;
  io(h, s, b, m.can_sprint, m.dead);
  m.health = h, m.stamina = s, m.breath = b;
}
template <class IO> void fields(IO &io, Respawned &m) { io(m.position, m.respawn); }
template <class IO> void fields(IO &io, EntitySnapshot &m) { io(m.tick, m.entities, m.players); }
template <class IO> void fields(IO &io, KnockedBack &m) { io(m.velocity); }
template <class IO> void fields(IO &io, JoinRefused &m) { io(m.reason); }
template <class IO> void fields(IO &io, Welcome &m) {
  io(m.seed, m.noise_offset, m.time_of_day, m.time_scale, m.edits, m.worldgen);
}
template <class IO> void fields(IO &io, TimeSync &m) { io(m.time_of_day, m.time_scale); }
template <class IO> void fields(IO &io, Disconnected &m) { io(m.reason); }

template <class T> requires std::is_class_v<T> void Writer::put(const T &v) {
  fields(*this, const_cast<T &>(v));
}
template <class T> requires std::is_class_v<T> void Reader::get(T &v) { fields(*this, v); }

// ─── Variants ──────────────────────────────────────────────────────────

template <class V> std::vector<uint8_t> encodeVariant(const V &v) {
  Writer w;
  w.put(static_cast<uint8_t>(v.index()));
  std::visit([&](const auto &alt) { w.put(alt); }, v);
  return std::move(w.out);
}

template <class V, size_t... I>
std::optional<V> decodeAt(Reader &r, size_t index, std::index_sequence<I...>) {
  std::optional<V> out;
  // Exactly one I matches the index and decodes that alternative.
  ((index == I ? (void)[&] {
     std::variant_alternative_t<I, V> alt{};
     r.get(alt);
     out.emplace(std::in_place_index<I>, std::move(alt));
   }()
               : void()),
   ...);
  return out;
}

template <class V> std::optional<V> decodeVariant(std::span<const uint8_t> payload) {
  Reader r{payload};
  uint8_t index = r.le<uint8_t>();
  if (!r.ok || index >= std::variant_size_v<V>) return std::nullopt;
  auto out = decodeAt<V>(r, index, std::make_index_sequence<std::variant_size_v<V>>{});
  if (!r.ok || r.pos != payload.size()) return std::nullopt;
  return out;
}

} // namespace

std::vector<uint8_t> encode(const Command &cmd) { return encodeVariant(cmd); }
std::vector<uint8_t> encode(const Event &ev) { return encodeVariant(ev); }
std::optional<Command> decodeCommand(std::span<const uint8_t> p) { return decodeVariant<Command>(p); }
std::optional<Event> decodeEvent(std::span<const uint8_t> p) { return decodeVariant<Event>(p); }

std::vector<uint8_t> hello() {
  Writer w;
  w.bytes(kMagic, 4);
  w.put(kProtocolVersion);
  return std::move(w.out);
}

std::optional<uint16_t> readHello(std::span<const uint8_t> payload) {
  if (payload.size() != 6 || std::memcmp(payload.data(), kMagic, 4) != 0) return std::nullopt;
  Reader r{payload, 4};
  return r.le<uint16_t>();
}

std::vector<uint8_t> frame(std::span<const uint8_t> payload) {
  Writer w;
  w.put(static_cast<uint32_t>(payload.size()));
  w.bytes(payload.data(), payload.size());
  return std::move(w.out);
}

void FrameReader::feed(std::span<const uint8_t> bytes) {
  if (bad_) return;
  // Drop consumed bytes now and then, so the buffer doesn't grow forever.
  if (start > 0 && start * 2 > buf.size()) {
    buf.erase(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(start));
    start = 0;
  }
  buf.insert(buf.end(), bytes.begin(), bytes.end());
}

std::optional<std::vector<uint8_t>> FrameReader::next() {
  if (bad_ || buf.size() - start < 4) return std::nullopt;
  uint32_t n = 0;
  for (int i = 0; i < 4; ++i) n |= uint32_t(buf[start + i]) << (8 * i);
  if (n > kMaxFrame) {
    bad_ = true;
    return std::nullopt;
  }
  if (buf.size() - start - 4 < n) return std::nullopt;
  auto first = buf.begin() + static_cast<std::ptrdiff_t>(start + 4);
  std::vector<uint8_t> payload(first, first + n);
  start += 4 + n;
  return payload;
}

} // namespace wire
