#pragma once

#include "block/block_type.hpp"
#include "chunk/chunk.hpp"
#include "render/camera.hpp"
#include "render/renderer.hpp"
#include "robin_hood/robin_hood.h"
#include "util/lock.hpp"
#include "util/thread_pool.hpp"
#include "world/block_edit.hpp"
#include "world/world_save.hpp"
#include <atomic>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <mutex>
#include <span>
#include <functional>
#include <vector>

// Result of a DDA raycast into the voxel world
struct RaycastResult {
  bool hit = false;
  glm::ivec3 block_pos{0};   // position of the block that was hit
  glm::ivec3 normal{0};      // face normal (for placement: place at block_pos + normal)
  float distance = 0.0f;     // distance from ray origin to hit
  BlockType block_type = BlockType::AIR;
};

class World {
public:
  World();
  ~World();

  void init();
  void update(float delta_time);
  // viewer_pos drives shadow/cloud placement and fog; it is the local
  // player's feet position.
  // `under_water` runs between the solid blocks and the blended water and
  // glass, so what it draws (entities) shows through the water, tinted. It
  // runs without the chunk lock held, so it may read the world.
  void render(glm::mat4 &view, glm::mat4 &projection, float timeOfDay,
              int viewportWidth, int viewportHeight,
              const glm::vec3 &viewer_pos, bool viewer_underwater,
              const std::function<void()> &under_water = {});

  // Chunks stream in around each point (one per player). Loading only reacts
  // when a point crosses a chunk boundary; the first call also preloads.
  void setInterestPoints(std::span<const glm::vec3> points);
  glm::vec3 findSpawn() const;
  void addChunk(int x, int z);
  // Generates a chunk on the calling thread and inserts it without queueing a
  // mesh build or GPU upload. For tests, which run without a GL context.
  void loadChunkSync(int x, int z);
  std::shared_ptr<Chunk> getChunk(int x, int z);
  std::shared_ptr<const Chunk> getChunk(int x, int z) const;
  size_t getChunkCount() const;

  // ─── Persistence ─────────────────────────────────────────────────────────
  // Installed before chunks load; replayed onto each freshly generated chunk.
  void setLoadedEdits(WorldEdits e);
  // The same from a flat list (a guest's Welcome), before any chunk loads.
  void setLoadedEdits(std::span<const BlockEdit> list);
  // Thread-safe copy of all player edits, for writing to disk.
  WorldEdits snapshotEdits() const;
  // The same edits as a flat list in world coordinates (for Welcome).
  std::vector<BlockEdit> editList() const;
  // The player edit at a block, loaded chunk or not (nothing if unedited).
  std::optional<BlockType> editAt(const glm::ivec3 &pos) const;
  // An edit made in another copy of this world (a guest mirroring the host).
  // Unlike setBlockAt it is never lost: an edit to a chunk that isn't loaded
  // yet is recorded and appears when the chunk streams in. Main thread only.
  void applyEdit(const glm::ivec3 &pos, BlockType type);
  // Applies recorded remote edits whose chunks have finished loading. Called
  // by update(); tests call it directly.
  void flushPendingEdits();

  const RenderStats &getRenderStats() const { return renderer->getStats(); }
  BlockType getBlockAt(const glm::vec3& worldPos) const;
  bool isSolidBlock(int bx, int by, int bz) const;
  void setBlockAt(const glm::ivec3& worldPos, BlockType type);

  // Light levels (0-15) at a world block position — used by the debug panel.
  uint8_t getSkyLightAt(const glm::ivec3& worldPos) const;
  uint8_t getBlockLightAt(const glm::ivec3& worldPos) const;

  // DDA raycast: find the first solid block along a ray
  RaycastResult raycast(const glm::vec3& origin, const glm::vec3& direction,
                        float max_distance) const;

private:
  void populateChunk(Chunk &chunk, const ChunkKey &key);
  void rebuildChunk(int cx, int cz);
  // Cross-chunk block-light: recompute a 5x5 chunk box around (ccx,ccz) into a
  // temp buffer and persist the inner 3x3. Correct when light must also go
  // down (a torch placed, broken or walled in), but costly: for edits only.
  void relightBlockRegion(int ccx, int ccz);
  // Lets block light cross the borders between `origin` and its loaded
  // neighbours, and on through any chunk that brightens. Cheap, and safe on
  // any thread; light only rises. Returns the chunks other than `origin`
  // whose light changed, which need remeshing.
  std::vector<ChunkKey> spreadBorderLight(const ChunkKey &origin);
  // True if some emitter is close enough to shine on `pos`.
  bool emitterInReach(const glm::ivec3 &pos) const;
  void preloadChunks();
  bool isChunkInFrustum(const glm::vec4 planes[6], const glm::vec3 &min, const glm::vec3 &max);
  std::vector<glm::ivec2> generateSpiralOrder(int radius);
  std::vector<glm::ivec2> spiral_offsets;
  void updateLoadedChunks();
  void unloadChunk(int x, int z);

  ThreadPool gen_pool{6};
  ThreadPool mesh_pool{6};
  mutable SharedMutex chunks_mutex;

  std::unique_ptr<Renderer> renderer;

  robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash>
      chunks;
  std::queue<std::shared_ptr<Chunk>> mesh_queue;
  std::queue<std::shared_ptr<Chunk>> upload_queue;

  std::vector<glm::ivec2> interest_chunks; // chunk coords of each interest point

  float cloud_time = 0.0f; // accumulated time for cloud drift

  // Player block edits, recorded in setBlockAt() and replayed onto freshly
  // generated chunks. Guarded because generation runs on worker threads.
  mutable std::mutex edits_mutex;
  WorldEdits edits;
  // Remote edits to chunks that were missing when they arrived. A chunk may
  // be generating at that moment, past the point where it reads `edits`, so
  // these are checked again once it is loaded. Main thread only.
  std::vector<BlockEdit> pending_edits;

};
