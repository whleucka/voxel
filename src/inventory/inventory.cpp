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
  return moveStack(slots_[from], slots_[to], count);
}
