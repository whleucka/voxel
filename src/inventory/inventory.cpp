#include "inventory/inventory.hpp"
#include <utility>

void Inventory::set(int i, ItemStack s) {
  if (!validSlot(i)) return;
  slots_[i] = s.empty() ? ItemStack{} : s;
}

ItemStack Inventory::add(ItemStack stack) {
  if (stack.empty()) return {};

  for (ItemStack &s : slots_) {
    if (stack.empty()) break;
    if (!s.empty() && s.id == stack.id) mergeInto(s, stack);
  }
  for (ItemStack &s : slots_) {
    if (stack.empty()) break;
    if (s.empty()) mergeInto(s, stack);
  }
  return stack;
}

ItemStack Inventory::take(int i, uint16_t n) {
  if (!validSlot(i)) return {};
  return split(slots_[i], n);
}

bool Inventory::move(int from, int to, uint16_t count) {
  if (!validSlot(from) || !validSlot(to) || from == to) return false;
  ItemStack &src = slots_[from];
  ItemStack &dst = slots_[to];
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
