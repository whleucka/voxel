#pragma once
#include <algorithm>

// Paces a message sent every frame (the player's MovePlayer) to at most `hz`
// a second, however fast frames come. Uncapped frame rates (vsync off: 300+
// FPS) otherwise send hundreds of tiny packets a second, which a guest's
// Wi-Fi can't keep up with: the backlog delays everything sent after them.
class SendRate {
public:
  explicit SendRate(double hz) : interval_(1.0 / hz) {}

  // Whether to send now. A frame that's a little early for its slot still
  // gets it, so 60 FPS at 60 Hz sends every frame despite jitter.
  bool due(double now) {
    if (now + interval_ * 0.5 < next_) return false;
    next_ = std::max(next_, now) + interval_; // on schedule; a stall doesn't bank sends
    return true;
  }

  void reset() { next_ = 0.0; }

private:
  double interval_;
  double next_ = 0.0;
};
