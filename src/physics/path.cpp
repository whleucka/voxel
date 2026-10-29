#include "physics/path.hpp"
#include "block/block_data.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>
#include <tuple>
#include <unordered_map>

namespace {

// Air, or something walked through: an open door (a closed one is a wall),
// a crop, a plant or a torch.
bool clear(const World &world, glm::ivec3 c) {
  return isPassable(world.getBlockAt(glm::vec3(c) + 0.5f));
}

bool bodyClear(const World &world, glm::ivec3 c, int height) {
  for (int i = 0; i < height; ++i)
    if (!clear(world, c + glm::ivec3(0, i, 0))) return false;
  return true;
}

// Mid-air endpoints (a jumping player, a falling creature) count as the ground
// below them.
std::optional<glm::ivec3> snapDown(const World &world, glm::ivec3 c, const PathParams &p) {
  for (int d = 0; d <= p.max_drop; ++d) {
    glm::ivec3 at = c - glm::ivec3(0, d, 0);
    if (standable(world, at, p.height_cells)) return at;
    if (!clear(world, at)) break;
  }
  return std::nullopt;
}

int64_t key(glm::ivec3 c) {
  return (static_cast<int64_t>(c.x & 0xFFFFF) << 40) |
         (static_cast<int64_t>(c.z & 0xFFFFF) << 20) | (c.y & 0xFFFFF);
}

int heuristic(glm::ivec3 a, glm::ivec3 b) {
  return std::abs(a.x - b.x) + std::abs(a.z - b.z);
}

} // namespace

// On solid ground, but never on a bed: creatures go round them.
bool standable(const World &world, glm::ivec3 cell, int height_cells) {
  return world.isSolidBlock(cell.x, cell.y - 1, cell.z) &&
         !isBed(world.getBlockAt(glm::vec3(cell) + glm::vec3(0.5f, -0.5f, 0.5f))) &&
         bodyClear(world, cell, height_cells);
}

std::optional<std::vector<glm::ivec3>> findPath(const World &world, glm::ivec3 from,
                                                glm::ivec3 to, const PathParams &p) {
  glm::vec2 span(to.x - from.x, to.z - from.z);
  if (glm::length(span) > p.max_range) return std::nullopt;
  auto start = snapDown(world, from, p);
  auto goal = snapDown(world, to, p);
  if (!start || !goal) return std::nullopt;
  if (*start == *goal) return std::vector<glm::ivec3>{};

  struct Node {
    glm::ivec3 cell;
    int g;
    int64_t parent;
  };
  std::unordered_map<int64_t, Node> nodes;
  // (f, order, key): the insertion order breaks ties, so paths are stable.
  using Open = std::tuple<int, int, int64_t>;
  std::priority_queue<Open, std::vector<Open>, std::greater<Open>> open;
  int order = 0;

  nodes[key(*start)] = {*start, 0, key(*start)};
  open.push({heuristic(*start, *goal), order++, key(*start)});

  const glm::ivec3 dirs[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
  int expanded = 0;
  while (!open.empty()) {
    auto [f, o, k] = open.top();
    open.pop();
    Node cur = nodes[k];
    if (f - heuristic(cur.cell, *goal) > cur.g) continue; // stale entry

    if (cur.cell == *goal) {
      std::vector<glm::ivec3> path;
      for (int64_t at = k; at != key(*start); at = nodes[at].parent)
        path.push_back(nodes[at].cell);
      std::reverse(path.begin(), path.end());
      return path;
    }
    if (++expanded > p.max_nodes) return std::nullopt;

    auto visit = [&](glm::ivec3 next) {
      int g = cur.g + 1;
      auto [it, fresh] = nodes.try_emplace(key(next), Node{next, g, k});
      if (!fresh) {
        if (g >= it->second.g) return;
        it->second = {next, g, k};
      }
      open.push({g + heuristic(next, *goal), order++, key(next)});
    };

    for (glm::ivec3 d : dirs) {
      glm::ivec3 side = cur.cell + d;
      if (standable(world, side, p.height_cells)) {
        visit(side);
        continue;
      }
      // Up one: room to jump above our own cell too.
      glm::ivec3 up = side + glm::ivec3(0, 1, 0);
      if (standable(world, up, p.height_cells) &&
          clear(world, cur.cell + glm::ivec3(0, p.height_cells, 0))) {
        visit(up);
        continue;
      }
      // Down: walk off the edge and fall to the first ground below.
      if (!bodyClear(world, side, p.height_cells)) continue;
      for (int drop = 1; drop <= p.max_drop; ++drop) {
        glm::ivec3 down = side - glm::ivec3(0, drop, 0);
        if (!clear(world, down)) break;
        if (standable(world, down, p.height_cells)) {
          visit(down);
          break;
        }
      }
    }
  }
  return std::nullopt;
}
