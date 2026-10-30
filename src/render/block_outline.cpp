#include "render/block_outline.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace {

// Appends an axis-aligned box [lo, hi] as 12 triangles.
void addBox(std::vector<float> &v, glm::vec3 lo, glm::vec3 hi, glm::vec3 color) {
  const glm::vec3 c[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z},
                          {lo.x, hi.y, lo.z}, {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z},
                          {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
  const int quads[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
                           {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
  auto put = [&](int i) { v.insert(v.end(), {c[i].x, c[i].y, c[i].z, color.r, color.g, color.b}); };
  for (const auto &q : quads) {
    put(q[0]); put(q[1]); put(q[2]);
    put(q[0]); put(q[2]); put(q[3]);
  }
}

} // namespace

std::optional<glm::ivec3> BlockOutline::target(const World &world, glm::vec3 eye,
                                                glm::vec3 dir, float reach) {
  RaycastResult ray = world.raycast(eye, dir, std::min(reach, kPlayerReach));
  if (!ray.hit) return std::nullopt;
  return ray.block_pos;
}

std::vector<DoorBox> BlockOutline::boxes(const World &world, glm::ivec3 pos) {
  constexpr float s = 1.0f / 16.0f;
  const BlockType b = world.getBlockAt(glm::vec3(pos) + 0.5f);
  // The box round everything drawn in the cell, no higher than the cell (a
  // fence stops bodies higher than it's drawn).
  auto bounds = [](const CellBoxes &cell) {
    DoorBox out{glm::vec3(1.0f), glm::vec3(0.0f)};
    for (const DoorBox &box : cell) {
      out.lo = glm::min(out.lo, box.lo);
      out.hi = glm::max(out.hi, box.hi);
    }
    out.hi.y = std::min(out.hi.y, 1.0f);
    return out;
  };

  if (isStair(b)) {
    CellBoxes cell = blockBoxes(b);
    return {cell.begin(), cell.end()};
  }
  if (isDoor(b)) { // the panel, over both halves
    DoorBox panel = doorPanel(b);
    if (doorTop(b)) panel.lo.y = -1.0f;
    else panel.hi.y = 2.0f;
    return {panel};
  }
  if (isBed(b)) { // the whole bed, both ends
    const glm::vec3 other = glm::vec3(bedOther(pos, b) - pos);
    DoorBox bed{glm::min(glm::vec3(0.0f), other), glm::max(glm::vec3(1.0f), other + 1.0f)};
    bed.hi.y = kBedHeight;
    return {bed};
  }
  if (isGate(b)) {
    DoorBox bar = blockBoxes(gateBlock(gateAlongX(b), false)).box[0];
    bar.hi.y = 1.0f;
    return {bar};
  }
  if (isFence(b)) return {bounds(world.boxesAt(pos))};
  if (isLadder(b)) {
    const glm::ivec3 wall = ladderSupport(b);
    DoorBox panel{{0, 0, 0}, {1, 1, 1}};
    if (wall.z < 0) panel.hi.z = s;
    else if (wall.z > 0) panel.lo.z = 1 - s;
    else if (wall.x > 0) panel.lo.x = 1 - s;
    else panel.hi.x = s;
    return {panel};
  }
  if (isTorch(b)) { // the stick, leaning on its wall like the drawn one
    glm::vec3 at(0.0f);
    const glm::ivec3 wall = torchSupport(b);
    if (wall.y == 0) at = glm::vec3(wall) * 0.35f + glm::vec3(0.0f, 0.2f, 0.0f);
    return {{at + glm::vec3(6 * s, 0, 6 * s), at + glm::vec3(10 * s, 10 * s, 10 * s)}};
  }
  if (isCake(b)) {
    const float end = s + 14 * s * static_cast<float>(cakeSlices(b)) / kCakeSlices;
    return {{{s, 0, s}, {end, kCakeHeight, 1 - s}}};
  }
  if (isCrop(b) || isPlant(b)) return {{{2 * s, 0, 2 * s}, {14 * s, 14 * s, 14 * s}}};
  if (b == BlockType::CACTUS) return {{{s, 0, s}, {1 - s, 1, 1 - s}}};
  if (CellBoxes cell = blockBoxes(b); cell.count > 0) return {bounds(cell)};
  return {{{0, 0, 0}, {1, 1, 1}}};
}

std::vector<float> BlockOutline::mesh(const std::vector<DoorBox> &shape) {
  const glm::vec3 color(0.05f);
  const float t = kThickness / 2;
  std::vector<float> v;
  for (const DoorBox &box : shape) {
    const glm::vec3 lo = box.lo - kInflate, hi = box.hi + kInflate;
    // Four bars along each axis, one at each corner of the other two.
    for (int axis = 0; axis < 3; ++axis) {
      int a = (axis + 1) % 3, b = (axis + 2) % 3;
      for (float pa : {lo[a], hi[a]})
        for (float pb : {lo[b], hi[b]}) {
          glm::vec3 from(0.0f), to(0.0f);
          from[axis] = lo[axis] - t;
          to[axis] = hi[axis] + t;
          from[a] = pa - t, to[a] = pa + t;
          from[b] = pb - t, to[b] = pb + t;
          addBox(v, from, to, color);
        }
    }
  }
  return v;
}

BlockOutline::~BlockOutline() { shutdown(); }

bool BlockOutline::init() {
  shader = std::make_unique<Shader>("assets/shaders/basic_color.vert",
                                    "assets/shaders/basic_color.frag");
  std::vector<float> v = mesh();
  vertex_count = static_cast<GLsizei>(v.size() / 6);
  uploaded = {{{0, 0, 0}, {1, 1, 1}}};
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_DYNAMIC_DRAW);
  const GLsizei stride = 6 * sizeof(float);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
  return true;
}

void BlockOutline::shutdown() {
  if (vbo) glDeleteBuffers(1, &vbo);
  if (vao) glDeleteVertexArrays(1, &vao);
  vao = vbo = 0;
  shader.reset();
}

void BlockOutline::draw(glm::ivec3 block, const std::vector<DoorBox> &shape,
                        const glm::mat4 &view, const glm::mat4 &projection) {
  if (!shader || !vao) return;
  // A new shape: rebuild the frame (a few hundred vertices, only on change).
  auto same = [](const DoorBox &a, const DoorBox &b) { return a.lo == b.lo && a.hi == b.hi; };
  if (!std::equal(shape.begin(), shape.end(), uploaded.begin(), uploaded.end(), same)) {
    std::vector<float> v = mesh(shape);
    vertex_count = static_cast<GLsizei>(v.size() / 6);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_DYNAMIC_DRAW);
    uploaded = shape;
  }
  shader->use();
  shader->setMat4("model", glm::translate(glm::mat4(1.0f), glm::vec3(block)));
  shader->setMat4("view", view);
  shader->setMat4("projection", projection);
  glDisable(GL_CULL_FACE); // the bars are tiny: draw both sides, never mind winding
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, vertex_count);
  glBindVertexArray(0);
  glEnable(GL_CULL_FACE);
}
