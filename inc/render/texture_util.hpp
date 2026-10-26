#pragma once

#include <glad/glad.h>
#include <string>

// Loads a PNG as an sRGB texture with nearest filtering and clamped edges,
// matching the block atlas, so sampled colours come back as painted through
// the sRGB framebuffer. Returns 0 (and logs) on failure. Needs a GL context.
GLuint loadSrgbTexture(const std::string &path);
