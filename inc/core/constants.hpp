#pragma once

#include <string>

constexpr int kChunkHeight = 256;
constexpr int kChunkDepth = 16;
constexpr int kChunkWidth = 16;

constexpr int kMaxAllowedChunks = 12000;
constexpr int kChunkPreloadRadius = 20;
constexpr int kRenderDistance = 300;
constexpr int kMaxChunksPerFrame = 16;
constexpr int kMaxUploadsPerFrame = 4;

constexpr int kScreenHeight = 900;
constexpr int kScreenWidth = 1600;

constexpr int kSeaLevel = (kChunkHeight * 2) / 5;
constexpr int kSnowLevel = 130;
constexpr int kTreeLevel = 180;

constexpr int kBiomeTreeBorder = 4;
constexpr float kBiomeFreq = 0.04f;
constexpr float kBiomeAmp = 35.0f;
constexpr float kBiomeOctaves = 4.0f;
constexpr float kBiomeLacunarity = 1.2f;
constexpr float kBiomeGain = 0.5f;

// Player dimensions (AABB)
constexpr float kPlayerHeight = 1.8f;
constexpr float kPlayerWidth = 0.6f;           // AABB width and depth
constexpr float kPlayerHalfWidth = kPlayerWidth / 2.0f;
constexpr float kPlayerEyeHeight = 1.62f;      // eye level within player box
constexpr float kPlayerCrouchEyeHeight = 1.27f; // eye level when crouching
constexpr float kPlayerCrouchHeight = 1.5f;    // AABB height when crouching

// Player speeds
constexpr float kPlayerMoveSpeed = 4.317f;     // Minecraft walk speed (blocks/s)
constexpr float kPlayerSprintSpeed = 5.612f;   // Minecraft sprint speed (blocks/s)
constexpr float kPlayerCrouchSpeed = 1.295f;   // Minecraft sneak speed (blocks/s)
constexpr float kPlayerWaterSpeed = 2.2f;       // swim speed
constexpr float kPlayerWaterSprintSpeed = 4.0f; // fast swim

// Jump and gravity
constexpr float kPlayerJumpStrength = 8.0f;    // upward velocity on jump
constexpr float kPlayerWaterJumpStrength = 2.5f; // swim-up velocity
constexpr float kGravityForce = -28.0f;        // gravity acceleration
constexpr float kWaterGravityMultiplier = 0.15f; // reduced gravity in water
constexpr float kTerminalVelocity = -60.0f;    // max fall speed
constexpr float kWaterDrag = 0.8f;             // water velocity damping per frame

// Interaction
constexpr float kPlayerReach = 5.0f;           // block interaction reach distance
constexpr int kHotbarSlots = 9;                // number of hotbar slots

// Collision
constexpr float kCollisionEpsilon = 0.001f;    // small offset to prevent z-fighting with block faces

inline const std::string kAppTitle = "3D Voxel Engine";
