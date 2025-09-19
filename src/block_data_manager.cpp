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

glm::vec2 BlockDataManager::getUV(BlockType type, BlockFace face) const {
  // map BlockType -> JSON key
  std::string type_str;
  switch (type) {
  case BlockType::DIRT:
    type_str = "dirt";
    break;
  case BlockType::GRASS:
    type_str = "grass";
    break;
  case BlockType::STONE:
    type_str = "stone";
    break;
  case BlockType::SAND:
    type_str = "sand";
    break;
  case BlockType::COBBLESTONE:
    type_str = "cobblestone";
    break;
  case BlockType::BEDROCK:
    type_str = "bedrock";
    break;
  case BlockType::WATER:
    type_str = "water";
    break;
  case BlockType::SNOW:
    type_str = "snow";
    break;
  case BlockType::SNOW_STONE:
    type_str = "snow-stone";
    break;
  case BlockType::SNOW_DIRT:
    type_str = "snow-dirt";
    break;
  case BlockType::SANDSTONE:
    type_str = "sandstone";
    break;
  default:
    type_str = "dirt";
    break;
  }

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
