#include "block_data_manager.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

void BlockDataManager::load(const std::string &path) {
  // sensible empty default so operator[] won’t explode later
  // Maybe json was a bad idea?
  data = nlohmann::json::object();
  data["blocks"] = nlohmann::json::object();

  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "[BlockDataManager] Failed to open '" << path
              << "' (cwd=" << std::filesystem::current_path() << ")\n";
    return; // keep empty object
  }

  // parse without throwing
  nlohmann::json j =
      nlohmann::json::parse(file, /*cb*/ nullptr, /*allow_exceptions*/ false);
  if (j.is_discarded() || !j.is_object()) {
    std::cerr << "[BlockDataManager] JSON parse failed for '" << path << "'\n";
    return; // keep empty object
  }

  data = std::move(j);
  if (!data.contains("blocks") || !data["blocks"].is_object()) {
    data["blocks"] = nlohmann::json::object();
  }
}

static std::string BlockTypeToString(BlockType type) {
  switch (type) {
  case BlockType::AIR:
    return "air";
  case BlockType::DIRT:
    return "dirt";
  case BlockType::GRASS:
    return "grass";
  case BlockType::STONE:
    return "stone";
  case BlockType::SAND:
    return "sand";
  case BlockType::COBBLESTONE:
    return "cobblestone";
  case BlockType::BEDROCK:
    return "bedrock";
  case BlockType::WATER:
    return "water";
  case BlockType::SNOW:
    return "snow";
  case BlockType::SNOW_STONE:
    return "snow-stone";
  case BlockType::SNOW_DIRT:
    return "snow-dirt";
  case BlockType::SANDSTONE:
    return "sandstone";
  case BlockType::TREE_LEAF:
    return "tree-leaf";
  case BlockType::TREE:
    return "tree";
  default:
    return "dirt";
  }
}

glm::vec2 BlockDataManager::getUV(BlockType type, BlockFace face) const {
  const std::string type_str = BlockTypeToString(type);

  if (!data.is_object())
    return {0, 0};
  auto itBlocks = data.find("blocks");
  if (itBlocks == data.end() || !itBlocks->is_object())
    return {0, 0};
  auto itType = itBlocks->find(type_str);
  if (itType == itBlocks->end() || !itType->is_object())
    return {0, 0};

  const auto &block_data = *itType;

  auto pick2 = [&](const char *k) -> glm::vec2 {
    if (block_data.contains(k) && block_data[k].is_array() &&
        block_data[k].size() >= 2) {
      return {block_data[k][0].get<float>(), block_data[k][1].get<float>()};
    }
    return {0, 0};
  };

  // prefer single "uv", else face-specific
  if (block_data.contains("uv") && block_data["uv"].is_array() &&
      block_data["uv"].size() >= 2) {
    return {block_data["uv"][0].get<float>(), block_data["uv"][1].get<float>()};
  }

  switch (face) {
  case BlockFace::TOP:
    return pick2("uv_top");
  case BlockFace::BOTTOM:
    return pick2("uv_bottom");
  default:
    return pick2("uv_side");
  }
}

bool BlockDataManager::isOpaque(BlockType type) const {
  const std::string type_str = BlockTypeToString(type);
  if (!data.is_object()) return false;
  auto itBlocks = data.find("blocks");
  if (itBlocks == data.end() || !itBlocks->is_object()) return false;
  auto itType = itBlocks->find(type_str);
  if (itType == itBlocks->end() || !itType->is_object()) return false;

  const auto &block_data = *itType;
  if (block_data.contains("opaque") && block_data["opaque"].is_boolean()) {
    return block_data["opaque"].get<bool>();
  }
  return false;
}

bool BlockDataManager::isTransparent(BlockType type) const {
  const std::string type_str = BlockTypeToString(type);
  if (!data.is_object()) return false;
  auto itBlocks = data.find("blocks");
  if (itBlocks == data.end() || !itBlocks->is_object()) return false;
  auto itType = itBlocks->find(type_str);
  if (itType == itBlocks->end() || !itType->is_object()) return false;

  const auto &block_data = *itType;
  if (block_data.contains("transparent") && block_data["transparent"].is_boolean()) {
    return block_data["transparent"].get<bool>();
  }
  return false;
}

bool BlockDataManager::isFluid(BlockType type) const {
  const std::string type_str = BlockTypeToString(type);
  if (!data.is_object()) return false;
  auto itBlocks = data.find("blocks");
  if (itBlocks == data.end() || !itBlocks->is_object()) return false;
  auto itType = itBlocks->find(type_str);
  if (itType == itBlocks->end() || !itType->is_object()) return false;

  const auto &block_data = *itType;
  if (block_data.contains("fluid") && block_data["fluid"].is_boolean()) {
    return block_data["fluid"].get<bool>();
  }
  return false;
}

bool BlockDataManager::isSolid(BlockType type) const {
  const std::string type_str = BlockTypeToString(type);
  if (!data.is_object()) return false;
  auto itBlocks = data.find("blocks");
  if (itBlocks == data.end() || !itBlocks->is_object()) return false;
  auto itType = itBlocks->find(type_str);
  if (itType == itBlocks->end() || !itType->is_object()) return false;

  const auto &block_data = *itType;
  if (block_data.contains("solid") && block_data["solid"].is_boolean()) {
    return block_data["solid"].get<bool>();
  }
  return false;
}

bool BlockDataManager::isSelectable(BlockType type) const {
  const std::string type_str = BlockTypeToString(type);
  if (!data.is_object()) return false;
  auto itBlocks = data.find("blocks");
  if (itBlocks == data.end() || !itBlocks->is_object()) return false;
  auto itType = itBlocks->find(type_str);
  if (itType == itBlocks->end() || !itType->is_object()) return false;

  const auto &block_data = *itType;
  if (block_data.contains("selectable") && block_data["selectable"].is_boolean()) {
    return block_data["selectable"].get<bool>();
  }
  return false;
}
