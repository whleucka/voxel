#include "render/entity_renderer.hpp"
#include "block/block_data.hpp"
#include "core/settings.hpp"
#include "entity/entity_type.hpp"
#include "entity/player_model.hpp"
#include "render/texture_util.hpp"
#include "ui/item_icon_geometry.hpp"
#include "world/world.hpp"
#include <cmath>
#include <optional>
#include <glm/gtc/matrix_transform.hpp>

namespace {

// A unit cube, 6 faces x 2 triangles. Each face lists its corners as seen
// from outside (top-left, top-right, bottom-right, bottom-left) so its texture
// reads upright and unmirrored. Layout: pos(3) normal(3) uv(2).
std::vector<float> unitCube() {
  struct Face { glm::vec3 n, tl, tr, br, bl; };
  const Face faces[] = {
      {{1, 0, 0}, {1, 1, 1}, {1, 1, 0}, {1, 0, 0}, {1, 0, 1}},
      {{-1, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}, {0, 0, 0}},
      {{0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 0, 1}, {0, 0, 1}},
      {{0, 0, -1}, {1, 1, 0}, {0, 1, 0}, {0, 0, 0}, {1, 0, 0}},
      {{0, 1, 0}, {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}},
      {{0, -1, 0}, {0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0}},
  };
  std::vector<float> v;
  auto put = [&](glm::vec3 p, glm::vec3 n, glm::vec2 uv) {
    v.insert(v.end(), {p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y});
  };
  for (const Face &f : faces) {
    put(f.tl, f.n, {0, 0}); put(f.bl, f.n, {0, 1}); put(f.br, f.n, {1, 1});
    put(f.tl, f.n, {0, 0}); put(f.br, f.n, {1, 1}); put(f.tr, f.n, {1, 0});
  }
  return v;
}

} // namespace

EntityRenderer::~EntityRenderer() { shutdown(); }

bool EntityRenderer::init() {
  shader = std::make_unique<Shader>("assets/shaders/entity.vert",
                                    "assets/shaders/entity.frag");
  std::vector<float> cube = unitCube();
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, cube.size() * sizeof(float), cube.data(), GL_STATIC_DRAW);
  const GLsizei stride = 8 * sizeof(float);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glBindVertexArray(0);

  block_tex = loadSrgbTexture("res/block_atlas.png");
  item_tex = loadSrgbTexture("res/item_atlas.png");
  return block_tex && item_tex;
}

void EntityRenderer::shutdown() {
  if (vbo) glDeleteBuffers(1, &vbo);
  if (vao) glDeleteVertexArrays(1, &vao);
  if (block_tex) glDeleteTextures(1, &block_tex);
  if (item_tex) glDeleteTextures(1, &item_tex);
  vao = vbo = block_tex = item_tex = 0;
  shader.reset();
}

void EntityRenderer::drawBox(const glm::mat4 &entity_model, glm::vec3 offset,
                             glm::vec3 size, const glm::mat4 &local) const {
  glm::mat4 m = glm::scale(glm::translate(entity_model * local, offset), size);
  shader->setMat4("uModel", m);
  glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(m)));
  glUniformMatrix3fv(glGetUniformLocation(shader->ID, "uNormalMat"), 1, GL_FALSE,
                     &normal_mat[0][0]);
  glDrawArrays(GL_TRIANGLES, 0, 36);
}

void EntityRenderer::drawLine(glm::vec3 from, glm::vec3 to, const World &world) const {
  constexpr int kSegments = 10;
  constexpr float kThickness = 0.015f;
  const float sag = 0.04f * glm::distance(from, to);
  auto at = [&](float s) {
    return glm::mix(from, to, s) - glm::vec3(0.0f, sag * 4.0f * s * (1.0f - s), 0.0f);
  };
  setLight(world, glm::mix(from, to, 0.5f));
  shader->setInt("uMode", 0);
  shader->setVec3("uColor", {0.85f, 0.85f, 0.88f});
  for (int i = 0; i < kSegments; ++i) {
    glm::vec3 a = at(static_cast<float>(i) / kSegments);
    glm::vec3 b = at(static_cast<float>(i + 1) / kSegments);
    float len = glm::distance(a, b);
    if (len < 1e-4f) continue;
    // A thin box along a->b: its z axis is the segment.
    glm::vec3 z = (b - a) / len;
    glm::vec3 up = std::abs(z.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 x = glm::normalize(glm::cross(up, z));
    glm::vec3 y = glm::cross(z, x);
    glm::mat4 m(glm::vec4(x, 0), glm::vec4(y, 0), glm::vec4(z, 0), glm::vec4(a, 1));
    drawBox(m, {-kThickness / 2, -kThickness / 2, 0.0f}, {kThickness, kThickness, len});
  }
}

void EntityRenderer::setLight(const World &world, glm::vec3 position) const {
  glm::ivec3 cell = glm::ivec3(glm::floor(position + glm::vec3(0.0f, 0.5f, 0.0f)));
  shader->setFloat("uSkyLight", world.getSkyLightAt(cell) / 15.0f);
  shader->setFloat("uBlockLight", world.getBlockLightAt(cell) / 15.0f);
}

void EntityRenderer::drawModel(const std::vector<ModelBox> &model, glm::vec3 position,
                               float yaw, bool moving, const World &world, double now,
                               bool lying) const {
  setLight(world, position);
  // Models face -Z at yaw 0; positive yaw turns toward +X.
  glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1.0f), position), -glm::radians(yaw),
                            glm::vec3(0, 1, 0));
  if (lying) {
    // Turned round, then tipped back about X: the head ends up along the
    // facing, the face up, and the body (1.8 long) centred on `position`.
    m = glm::rotate(m, glm::radians(180.0f), glm::vec3(0, 1, 0));
    m = glm::translate(m, glm::vec3(0.0f, 0.3f, -0.9f));
    m = glm::rotate(m, glm::radians(90.0f), glm::vec3(1, 0, 0));
  }
  shader->setInt("uMode", 0);
  float swing = moving && !lying ? std::sin(static_cast<float>(now) * 10.0f) * glm::radians(30.0f)
                                 : 0.0f;
  for (const ModelBox &box : model) {
    shader->setVec3("uColor", box.color);
    if (box.glow) shader->setBool("uGlow", true);
    if (box.wag && swing != 0.0f) {
      // Tails wag side to side about their front edge.
      glm::vec3 pivot = box.offset + glm::vec3(box.size.x * 0.5f, box.size.y * 0.5f, 0.0f);
      glm::mat4 local = glm::translate(glm::mat4(1.0f), pivot) *
                        glm::rotate(glm::mat4(1.0f), swing, glm::vec3(0, 1, 0)) *
                        glm::translate(glm::mat4(1.0f), -pivot);
      drawBox(m, box.offset, box.size, local);
    } else if (box.swing == 0 || swing == 0.0f) {
      drawBox(m, box.offset, box.size);
    } else {
      // Legs (and arms) swing forward and back about the top of the box.
      glm::vec3 pivot = box.offset + glm::vec3(box.size.x * 0.5f, box.size.y, box.size.z * 0.5f);
      glm::mat4 local = glm::translate(glm::mat4(1.0f), pivot) *
                        glm::rotate(glm::mat4(1.0f), swing * box.swing, glm::vec3(1, 0, 0)) *
                        glm::translate(glm::mat4(1.0f), -pivot);
      drawBox(m, box.offset, box.size, local);
    }
    if (box.glow) shader->setBool("uGlow", false);
  }
}

void EntityRenderer::draw(const std::vector<EntityView> &entities,
                          const std::vector<PlayerView> &players, const World &world,
                          const glm::mat4 &view, const glm::mat4 &projection,
                          glm::vec3 camera_pos, bool underwater, float time_of_day,
                          double now, PlayerId self, glm::vec3 self_hand) {
  if (!shader || (entities.empty() && players.empty())) return;

  SceneLighting light = Renderer::sceneLighting(time_of_day);
  shader->use();
  shader->setMat4("view", view);
  shader->setMat4("projection", projection);
  shader->setVec3("uSunDir", light.sun_dir);
  shader->setVec3("uSunColor", light.sun_color);
  shader->setVec3("uAmbientColor", light.ambient);
  shader->setVec3("uFogColor", light.fog_color);
  shader->setVec3("uWaterFogColor", light.water_fog_color);
  shader->setFloat("uFogStart", g_settings.fog_start);
  shader->setFloat("uFogEnd", g_settings.fog_end);
  shader->setFloat("uWaterFogDensity", g_settings.water_fog_density);
  shader->setVec3("uCameraPos", camera_pos);
  shader->setBool("uUnderwater", underwater);
  shader->setInt("uAtlas", 0);
  shader->setBool("uGlow", false);

  // Thin item cards are seen from both sides, so no culling for this pass.
  glDisable(GL_CULL_FACE);
  glBindVertexArray(vao);
  glActiveTexture(GL_TEXTURE0);

  for (const EntityView &e : entities) {
    setLight(world, e.position);
    shader->setFloat("uHurt", e.hurt ? 0.55f : 0.0f);
    shader->setFloat("uBurn", e.burning ? 0.45f : 0.0f);

    if (e.kind == EntityKind::DroppedItem) {
      if (!isValidItem(e.item)) continue;
      // Client-side flourish: spin and bob, out of phase per entity.
      float phase = static_cast<float>(e.id % 97);
      float spin = static_cast<float>(now) * 70.0f + phase * 37.0f;
      float bob = 0.06f + 0.05f * std::sin(static_cast<float>(now) * 2.5f + phase);
      glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1.0f), e.position),
                                glm::radians(spin), glm::vec3(0, 1, 0));
      const ItemDef &def = itemDef(e.item);
      if (def.places) {
        auto tex = block_data.find(*def.places);
        if (tex == block_data.end()) continue;
        glBindTexture(GL_TEXTURE_2D, block_tex);
        shader->setInt("uMode", 1);
        shader->setFloat("uTileSpan", 1.0f / kBlockAtlasTiles);
        shader->setVec3("uColor", glm::vec3(1.0f));
        auto tile = [](glm::ivec2 t) { return glm::vec2(t) / float(kBlockAtlasTiles); };
        glUniform2fv(glGetUniformLocation(shader->ID, "uTileTop"), 1, &tile(tex->second.top)[0]);
        glUniform2fv(glGetUniformLocation(shader->ID, "uTileSide"), 1, &tile(tex->second.side)[0]);
        glUniform2fv(glGetUniformLocation(shader->ID, "uTileBottom"), 1, &tile(tex->second.bottom)[0]);
        drawBox(m, {-0.125f, bob, -0.125f}, glm::vec3(0.25f));
      } else {
        glBindTexture(GL_TEXTURE_2D, item_tex);
        shader->setInt("uMode", 2);
        shader->setFloat("uTileSpan", 1.0f / kItemAtlasTiles);
        glm::vec2 t = iconUv(def.icon).uv0;
        glUniform2fv(glGetUniformLocation(shader->ID, "uTileSide"), 1, &t[0]);
        drawBox(m, {-0.175f, bob, -0.015f}, {0.35f, 0.35f, 0.03f});
      }
      continue;
    }

    const EntityTypeDef &def = entityType(e.kind);
    if (e.kind == EntityKind::Bobber) {
      // It rides the ripples, and dips under when something bites.
      float phase = static_cast<float>(e.id % 31);
      float dip = e.biting ? -0.14f + 0.04f * std::sin(static_cast<float>(now) * 25.0f)
                           : 0.02f * std::sin(static_cast<float>(now) * 2.0f + phase);
      glm::vec3 top = e.position + glm::vec3(0.0f, dip, 0.0f);
      drawModel(def.model, top, 0.0f, false, world, now);
      // The line, from whoever's holding the rod.
      std::optional<glm::vec3> hand;
      if (e.owner == self && self != 0) hand = self_hand;
      for (const PlayerView &p : players)
        if (p.id == e.owner) {
          float yaw = glm::radians(p.yaw);
          hand = p.position + glm::vec3(0.0f, 1.45f, 0.0f) +
                 glm::vec3(std::cos(yaw), 0.0f, std::sin(yaw)) * 0.9f;
        }
      if (hand) drawLine(*hand, top + glm::vec3(0.0f, def.height, 0.0f), world);
      continue;
    }
    drawModel(def.model, e.position, e.yaw, e.moving, world, now);

    // A fed animal shows a little bobbing heart above its head (not turned
    // with the body, so it always reads as a heart).
    if (e.happy) {
      float bob = 0.08f * std::sin(static_cast<float>(now) * 4.0f);
      glm::mat4 h = glm::translate(glm::mat4(1.0f),
                                   e.position + glm::vec3(0.0f, def.height + 0.35f + bob, 0.0f));
      shader->setFloat("uHurt", 0.0f);
      shader->setFloat("uBurn", 0.0f);
      shader->setVec3("uColor", {0.92f, 0.15f, 0.25f});
      drawBox(h, {-0.12f, 0.05f, -0.03f}, {0.10f, 0.10f, 0.06f}); // lobes
      drawBox(h, {0.02f, 0.05f, -0.03f}, {0.10f, 0.10f, 0.06f});
      drawBox(h, {-0.09f, -0.03f, -0.03f}, {0.18f, 0.09f, 0.06f}); // body
      drawBox(h, {-0.04f, -0.09f, -0.03f}, {0.08f, 0.07f, 0.06f}); // point
    }
  }

  // Other players: blocky figures in their name's colour.
  shader->setFloat("uHurt", 0.0f);
  shader->setFloat("uBurn", 0.0f);
  for (const PlayerView &p : players)
    drawModel(playerModel(playerColor(p.name)), p.position, playerModelYaw(p.yaw), p.moving,
              world, now, p.sleeping);

  glBindVertexArray(0);
  glEnable(GL_CULL_FACE);
}
