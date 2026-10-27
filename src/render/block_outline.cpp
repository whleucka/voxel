#include "render/block_outline.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
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
                                                glm::vec3 dir) {
  RaycastResult ray = world.raycast(eye, dir, kPlayerReach);
  if (!ray.hit) return std::nullopt;
  return ray.block_pos;
}

std::vector<float> BlockOutline::mesh() {
  const glm::vec3 color(0.05f);
  const float lo = -kInflate, hi = 1.0f + kInflate, t = kThickness / 2;
  std::vector<float> v;
  // Four bars along each axis, one at each corner of the other two.
  for (int axis = 0; axis < 3; ++axis) {
    int a = (axis + 1) % 3, b = (axis + 2) % 3;
    for (float pa : {lo, hi})
      for (float pb : {lo, hi}) {
        glm::vec3 from(0.0f), to(0.0f);
        from[axis] = lo - t;
        to[axis] = hi + t;
        from[a] = pa - t, to[a] = pa + t;
        from[b] = pb - t, to[b] = pb + t;
        addBox(v, from, to, color);
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
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
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

void BlockOutline::draw(glm::ivec3 block, const glm::mat4 &view,
                        const glm::mat4 &projection) const {
  if (!shader || !vao) return;
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
