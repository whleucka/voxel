#include "render/gpu_timer.hpp"

void GpuTimer::init() {
  glGenQueries(kFrames * kSpans * 2, &queries[0][0][0]);
  ready = true;
}

void GpuTimer::shutdown() {
  if (!ready) return;
  glDeleteQueries(kFrames * kSpans * 2, &queries[0][0][0]);
  ready = false;
}

void GpuTimer::stamp(int span, int edge) {
  if (!ready) return;
  glQueryCounter(queries[frame][span][edge], GL_TIMESTAMP);
  if (edge == 1) used[frame][span] = true;
}

std::array<std::optional<double>, GpuTimer::kSpans> GpuTimer::endFrame() {
  std::array<std::optional<double>, kSpans> out;
  if (!ready) return out;
  frame = (frame + 1) % kFrames;
  // The oldest frame: its slots are reused from now on.
  for (int s = 0; s < kSpans; ++s) {
    if (!used[frame][s]) continue;
    used[frame][s] = false;
    GLint in = 0;
    glGetQueryObjectiv(queries[frame][s][1], GL_QUERY_RESULT_AVAILABLE, &in);
    if (!in) continue;
    GLuint64 t0 = 0, t1 = 0;
    glGetQueryObjectui64v(queries[frame][s][0], GL_QUERY_RESULT, &t0);
    glGetQueryObjectui64v(queries[frame][s][1], GL_QUERY_RESULT, &t1);
    out[s] = static_cast<double>(t1 - t0) / 1e6;
  }
  return out;
}
