#pragma once

#include <string>

constexpr int kChunkHeight = 256;
constexpr int kChunkDepth = 16;
constexpr int kChunkWidth = 16;

constexpr int kScreenHeight = 600;
constexpr int kScreenWidth = 800;

constexpr int kSeaLevel    = (kChunkHeight * 2) / 10;
constexpr int kSnowLevel = 85;

constexpr float kBiomeFreq = 0.04f;
constexpr float kBiomeAmp = 35.0f;
constexpr float kBiomeOctaves = 4.0f;
constexpr float kBiomeLacunarity = 1.2f;
constexpr float kBiomeGain = 0.5f;

constexpr float kPlayerHeight = 1.5f;
constexpr float kPlayerRadius = 0.5f;
constexpr float kPlayerMoveSpeed = 5.0f;
constexpr float kPlayerSprintSpeed = 10.f;
constexpr float kPlayerJumpStrength = 5.0f;
constexpr float kPlayerWaterSpeed = 2.5f;
constexpr float kPlayerWaterJumpStrength = 1.0f;

constexpr float kGravityForce = -20.0f;

inline const std::string kAppTitle = "3D Voxel Engine";
