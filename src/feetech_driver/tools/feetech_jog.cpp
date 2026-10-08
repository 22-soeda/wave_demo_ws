// 1 軸だけを、今の位置から指定したカウントだけ動かす。
// --allow-torque を付けたときだけトルクを入れる。終わるとき（Ctrl-C・エラーを含む）はトルクを切る。
//   ros2 run feetech_driver feetech_jog /dev/feetech_right 10 50                 # 表示だけ
//   ros2 run feetech_driver feetech_jog /dev/feetech_right 10 50 --allow-torque  # 動かす

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <thread>

#include "feetech_driver/servo_bus.hpp"

namespace
{

constexpr int kMaxDelta = 200;       // 1 回に動かせる量 [カウント]（約 18 deg）
constexpr int kSpeed = 200;          // [カウント/s]（約 18 deg/s）
constexpr int kAcc = 10;             // 加速度。小さいほどゆっくり動き出す
constexpr int kTolerance = 10;       // 着いたとみなす誤差 [カウント]
constexpr double kTimeoutSec = 5.0;  // 着くのを待つ時間の上限

volatile std::sig_atomic_t g_stop = 0;

void on_sigint(int)
{
  g_stop = 1;
}

// スコープを抜けるとき（return・例外のどちらでも）に、必ずトルクを切る
struct TorqueOffGuard
{
  feetech_driver::ServoBus & bus;
  int id;

  ~TorqueOffGuard()
  {
    const bool ok = bus.set_torque(id, false);
    std::printf("ID%d: トルクを切りました%s\n", id, ok ? "" : "（応答なし。電源を切って確かめる）");
  }
};

}  // namespace

int main(int argc, char ** argv)
{
  const bool allow_torque = (argc == 5 && std::string(argv[4]) == "--allow-torque");
  if (argc != 4 && !allow_torque) {
    std::fprintf(stderr, "使い方: feetech_jog <ポート> <ID> <動かす量 [カウント]> [--allow-torque]\n");
    return 2;
  }
  const int id = std::atoi(argv[2]);
  const int delta = std::atoi(argv[3]);
  if (id < 1 || id > 253) {
    std::fprintf(stderr, "ID は 1〜253\n");
    return 2;
  }
  if (std::abs(delta) > kMaxDelta) {
    std::fprintf(stderr, "動かす量は ±%d カウントまで\n", kMaxDelta);
    return 2;
  }

  try {
    feetech_driver::ServoBus bus(argv[1]);

    const auto start = bus.read_position(id);
    if (!start) {
      std::fprintf(stderr, "ID%d の位置を読めません\n", id);
      return 1;
    }
    const int target = *start + delta;
    std::printf("ID%d: 今 %d → 目標 %d\n", id, *start, target);
    if (target < 0 || target > 4095) {
      std::fprintf(stderr, "目標が 0〜4095 の外です\n");
      return 1;
    }
    if (!allow_torque) {
      std::printf("--allow-torque がないので動かしません\n");
      return 0;
    }

    std::signal(SIGINT, on_sigint);

    // 目標を今の位置にしてからトルクを入れる（入れた瞬間に跳ねないように）
    bus.sync_write_positions({id}, {*start}, kSpeed, kAcc);
    TorqueOffGuard guard{bus, id};  // ここから先は、どう抜けてもトルクが切れる
    if (!bus.set_torque(id, true)) {
      std::fprintf(stderr, "ID%d: トルクを入れられません（応答なし）\n", id);
      return 1;
    }

    bus.sync_write_positions({id}, {target}, kSpeed, kAcc);

    // 着くまで待つ
    const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::duration<double>(kTimeoutSec);
    int now = *start;
    while (!g_stop && std::chrono::steady_clock::now() < deadline) {
      const auto pos = bus.read_position(id);
      if (pos) {
        now = *pos;
        if (std::abs(now - target) <= kTolerance) {
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::printf("ID%d: 今 %d（目標との差 %d）%s\n", id, now, now - target, g_stop ? " Ctrl-C で止めました" : "");
  } catch (const std::exception & e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
  return 0;
}
