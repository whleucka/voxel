#pragma once

#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include "json.hpp"
#include "block_type.hpp"

using json = nlohmann::json;

enum class BlockFace {
    TOP,
    BOTTOM,
    LEFT,
    RIGHT,
    FRONT,
    BACK
};

class BlockDataManager {
public:
    static BlockDataManager& getInstance() {
        static BlockDataManager instance;
        return instance;
    }

    void load(const std::string& path);
    glm::vec2 getUV(BlockType type, BlockFace face) const;

private:
    BlockDataManager() = default;
    ~BlockDataManager() = default;
    BlockDataManager(const BlockDataManager&) = delete;
    BlockDataManager& operator=(const BlockDataManager&) = delete;

    json data;
};
