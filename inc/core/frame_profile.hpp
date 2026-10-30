#pragma once

#include <array>
#include <chrono>

// Where the frame's time goes, for the debug panel: each stage's average over
// the last kWindow frames, and its worst frame among them (a hitch shows there
// long after it has gone out of the average).
class FrameProfile {
public:
  enum Stage {
    // On the CPU, in the order a frame runs them
    Update,  // input, movement and mining
    Game,    // the session: the Game's ticks when hosting, then its events
    Uploads, // finished chunk meshes sent to the GPU
    Scene,   // the world, entities and outline submitted
    Hud,     // the HUD and panels built and submitted
    Swap,    // presenting: the wait for vsync or for the GPU to catch up
    // On the GPU, a few frames late
    GpuScene,   // the world, shadow map included
    GpuShadows, // the shadow map
    GpuHud,
    kStages
  };
  static constexpr int kWindow = 60;

  static const char *name(Stage s);

  // Adds time to a stage of the frame in progress.
  void add(Stage s, double ms) { current[s] += ms; }
  // Closes the frame in progress into the window.
  void endFrame();

  double average(Stage s) const;
  double worst(Stage s) const;

  // Adds the time until it goes out of scope.
  class Scoped {
  public:
    Scoped(FrameProfile &p, Stage s)
        : profile(p), stage(s), start(std::chrono::steady_clock::now()) {}
    ~Scoped() {
      std::chrono::duration<double, std::milli> d = std::chrono::steady_clock::now() - start;
      profile.add(stage, d.count());
    }
    Scoped(const Scoped &) = delete;
    Scoped &operator=(const Scoped &) = delete;

  private:
    FrameProfile &profile;
    Stage stage;
    std::chrono::steady_clock::time_point start;
  };

private:
  using Times = std::array<double, kStages>;
  Times current{};
  std::array<Times, kWindow> frames{};
  int next = 0;   // where the next frame goes
  int filled = 0; // frames in the window so far
};
