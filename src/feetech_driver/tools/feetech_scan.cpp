// バスの ID 1〜20 を探し、応答したサーボの現在位置を表示し続ける。
// 読み取りだけで、トルクは入れない。Ctrl-C で終わる。
//   ros2 run feetech_driver feetech_scan /dev/feetech_right

#include <chrono>
#include <cstdio>
#include <exception>
#include <thread>
#include <vector>

#include "feetech_driver/servo_bus.hpp"

namespace
{

constexpr int kMaxId = 20;

}  // namespace

int main(int argc, char ** argv)
{
  if (argc != 2) {
    std::fprintf(stderr, "使い方: feetech_scan <ポート>（例 /dev/feetech_right）\n");
    return 2;
  }

  try {
    feetech_driver::ServoBus bus(argv[1]);

    std::vector<int> ids;
    for (int id = 1; id <= kMaxId; ++id) {
      if (bus.ping(id)) {
        ids.push_back(id);
      }
    }
    if (ids.empty()) {
      std::printf("応答したサーボはありません（電源と配線を確かめる）\n");
      return 1;
    }

    while (true) {
      for (const int id : ids) {
        const auto pos = bus.read_position(id);
        if (pos) {
          std::printf("ID%-2d %5d   ", id, *pos);
        } else {
          std::printf("ID%-2d  ----   ", id);
        }
      }
      std::printf("\n");
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  } catch (const std::exception & e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
