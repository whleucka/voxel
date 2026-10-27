#include "item/item_stack.hpp"
#include <utility>
#include <algorithm>

namespace {

void normalize(ItemStack &s) {
  if (s.empty()) s = {};
}

} // namespace

uint16_t ItemStack::space() const {
  if (empty()) return kDefaultMaxStack;
  uint16_t max = itemDef(id).max_stack;
  return count >= max ? 0 : max - count;
}

uint16_t mergeInto(ItemStack &dst, ItemStack &src) {
  if (src.empty()) return 0;
  if (!dst.empty() && dst.id != src.id) return 0;

  uint16_t max = itemDef(src.id).max_stack;
  uint16_t have = dst.empty() ? 0 : dst.count;
  uint16_t moved = std::min<uint16_t>(src.count, have >= max ? 0 : max - have);
  if (moved == 0) return 0;

  dst = {src.id, static_cast<uint16_t>(have + moved)};
  src.count -= moved;
  normalize(src);
  return moved;
}

ItemStack split(ItemStack &s, uint16_t n) {
  if (s.empty() || n == 0) return {};
  uint16_t taken = std::min(n, s.count);
  ItemStack out{s.id, taken};
  s.count -= taken;
  normalize(s);
  return out;
}

bool moveStack(ItemStack &src, ItemStack &dst, uint16_t count) {
  if (src.empty()) return false;
  uint16_t n = (count == 0 || count > src.count) ? src.count : count;

  if (dst.empty() || dst.id == src.id) {
    ItemStack moving = split(src, n);
    uint16_t moved = mergeInto(dst, moving);
    mergeInto(src, moving); // anything that didn't fit goes back
    return moved > 0;
  }

  // Different items: only a whole-stack move can swap them.
  if (n != src.count) return false;
  std::swap(src, dst);
  return true;
}
