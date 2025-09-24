#pragma once

#include "block_type.hpp"
#include "json.hpp"
#include <glm/vec2.hpp>
#include <string>

using json = nlohmann::json;

enum class BlockFace { TOP, BOTTOM, LEFT, RIGHT, FRONT, BACK };

class BlockDataManager {
public:
  static BlockDataManager &getInstance() {
    static BlockDataManager instance;
    return instance;
  }

  void load(const std::string &path);
  glm::vec2 getUV(BlockType type, BlockFace face) const;

  bool isOpaque(BlockType type) const;
  bool isTransparent(BlockType type) const;
  bool isFluid(BlockType type) const;

private:
  BlockDataManager() = default;
  ~BlockDataManager() = default;
  BlockDataManager(const BlockDataManager &) = delete;
  BlockDataManager &operator=(const BlockDataManager &) = delete;

  json data;
};
