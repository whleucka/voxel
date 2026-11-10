#pragma once

#include "render/blast_effects.hpp"
#include "render/renderer.hpp"
#include "render/shader.hpp"
#include "entity/entity_type.hpp"
#include "sim/protocol.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string_view>
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

  // `self` is the local player (not in `players`); their fishing line starts
  // at `self_hand`.
  void draw(const std::vector<EntityView> &entities, const std::vector<PlayerView> &players,
            const World &world, const glm::mat4 &view, const glm::mat4 &projection,
            glm::vec3 camera_pos, bool underwater, float time_of_day, double now,
            PlayerId self = 0, glm::vec3 self_hand = glm::vec3(0.0f));

  // An explosion's puff of cubes (see BlastEffects).
  void drawPuff(const std::vector<BlastEffects::Cube> &cubes, const World &world,
                const glm::mat4 &view, const glm::mat4 &projection, glm::vec3 camera_pos,
                bool underwater, float time_of_day);

  // A character slowly turning in the noon light, on a clear background, for
  // the Character page. Returns the texture it's in. Needs no world.
  GLuint drawPreview(const PlayerLook &look, std::string_view name, int width, int height,
                     double now);

private:
  std::unique_ptr<Shader> shader;
  GLuint vao = 0, vbo = 0;
  GLuint block_tex = 0, item_tex = 0;
  GLuint preview_fbo = 0, preview_tex = 0, preview_depth = 0;
  int preview_width = 0, preview_height = 0;

  void setScene(const glm::mat4 &view, const glm::mat4 &projection, glm::vec3 camera_pos,
                bool underwater, float time_of_day) const;
  void resizePreview(int width, int height);

  // Box models (creatures and players), lit at `position`. Lying, the model
  // is laid on its back with its head along `yaw`, centred on `position`.
  // Pets: collar boxes take colour `collar` (none drawn at 0), and sitting
  // lowers the body by `sit_drop` with the legs tucked away. `wagging`: the
  // tail wags whether or not it's moving.
  void drawModel(const std::vector<ModelBox> &model, glm::vec3 position, float yaw,
                 bool moving, const World &world, double now, bool lying = false,
                 uint8_t collar = 0, float sit_drop = 0.0f, bool wagging = false) const;
  // The same, lit by whatever's set already.
  void drawBoxes(const std::vector<ModelBox> &model, glm::vec3 position, float yaw, bool moving,
                 double now, bool lying, uint8_t collar, float sit_drop, bool wagging) const;
  void setLight(const World &world, glm::vec3 position) const;
  mutable glm::vec3 sun_dir{0.0f, 1.0f, 0.0f}; // from the last setScene, for cloud shade
  // `local` is applied before the box is placed (e.g. a leg's swing).
  void drawBox(const glm::mat4 &entity_model, glm::vec3 offset, glm::vec3 size,
               const glm::mat4 &local = glm::mat4(1.0f)) const;
  // A fishing line from a hand to a bobber, sagging a little.
  void drawLine(glm::vec3 from, glm::vec3 to, const World &world) const;
};
