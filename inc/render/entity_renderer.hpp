#pragma once

#include "render/renderer.hpp"
#include "render/shader.hpp"
#include "entity/entity_type.hpp"
#include "sim/protocol.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class World;

// Draws entity box models: one unit cube, scaled and placed per box, with a
// flat colour or atlas tiles. Dropped items are drawn from their item's art
// (a mini block or a flat card) and spin and bob as a client-side effect.
class EntityRenderer {
public:
  EntityRenderer() = default;
  ~EntityRenderer();
  EntityRenderer(const EntityRenderer &) = delete;
  EntityRenderer &operator=(const EntityRenderer &) = delete;

  bool init(); // needs a GL context
  void shutdown();

  void draw(const std::vector<EntityView> &entities, const std::vector<PlayerView> &players,
            const World &world, const glm::mat4 &view, const glm::mat4 &projection,
            glm::vec3 camera_pos, bool underwater, float time_of_day, double now);

private:
  std::unique_ptr<Shader> shader;
  GLuint vao = 0, vbo = 0;
  GLuint block_tex = 0, item_tex = 0;

  // Box models (creatures and players), lit at `position`.
  void drawModel(const std::vector<ModelBox> &model, glm::vec3 position, float yaw,
                 bool moving, const World &world, double now) const;
  void setLight(const World &world, glm::vec3 position) const;
  // `local` is applied before the box is placed (e.g. a leg's swing).
  void drawBox(const glm::mat4 &entity_model, glm::vec3 offset, glm::vec3 size,
               const glm::mat4 &local = glm::mat4(1.0f)) const;
};
