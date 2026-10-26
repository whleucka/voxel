#pragma once

#include <cstdint>
#include <random>

// Deterministic randomness for world generation.
//
// Chunk::init() reseeds this from (world seed, chunk position) before it
// generates, and a chunk is generated start-to-finish on one thread. A
// thread_local generator therefore gives each chunk the same sequence no
// matter which worker builds it or in what order, so a chunk is identical on
// every reload and on every machine sharing the seed.
namespace worldgen {

inline std::mt19937 &rng() {
  thread_local std::mt19937 gen;
  return gen;
}

// splitmix64 finalizer: neighbouring chunks get unrelated streams.
inline uint64_t mix(uint64_t h) {
  h += 0x9E3779B97F4A7C15ull;
  h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ull;
  h = (h ^ (h >> 27)) * 0x94D049BB133111EBull;
  return h ^ (h >> 31);
}

inline void seedChunk(uint32_t world_seed, int cx, int cz) {
  uint64_t h = mix(world_seed);
  h = mix(h ^ static_cast<uint32_t>(cx));
  h = mix(h ^ static_cast<uint32_t>(cz));
  rng().seed(static_cast<uint32_t>(h ^ (h >> 32)));
}

// Drop-in for rand(): a non-negative int. mt19937's output sequence is fixed
// by the standard, so results don't depend on the compiler or C library.
inline int rand() { return static_cast<int>(rng()() >> 1); }

} // namespace worldgen
