#pragma once

class GameClock {
public:
  float time_of_day = 12 * 3600; // noon (12h * 3600s)
  float scale = 2000.0f;
  //float scale = 72.0f; // 20 min day

  void update(float dt) {
    time_of_day += dt * scale;

    // wrap around after 24h
    if (time_of_day >= 86400.0f)
      time_of_day -= 86400.0f;
  }

  int hour() const { return static_cast<int>(time_of_day / 3600.0f); }
  int minute() const { return (static_cast<int>(time_of_day / 60)) % 60; }
  float fractionOfDay() const { return time_of_day / 86400.0f; }
};
