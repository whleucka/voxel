#include "core/constants.hpp"
#include "core/engine.hpp"

int main() {
  Engine engine(kScreenWidth, kScreenHeight, kAppTitle);
  if (!engine.init())
    return -1;
  engine.run();
  return 0;
}
