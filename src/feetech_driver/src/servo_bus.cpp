#include "feetech_driver/servo_bus.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "scservo/HLSCL.h"

namespace feetech_driver
{

namespace
{

// 位置指令と一緒に書く「目標トルク」（HLS では電流の上限 0〜2047）。
// SDK の既定は 0 で、0 だとまったく動かない。上限いっぱいにして制限しない
constexpr u16 kGoalTorque = 2047;

}  // namespace


ServoBus::ServoBus(const std::string & port, int baud, int timeout_ms)
: port_(port), hl_(std::make_unique<HLSCL>())
{
  if (!hl_->begin(baud, port_.c_str())) {
    throw std::runtime_error(
            port_ + " を開けません。ポートがあるか（ls -l /dev/feetech_*）、"
            "dialout グループに入っているか（groups）を確かめてください");
  }
  hl_->IOTimeOut = timeout_ms;  // begin() が 100ms に戻すので、その後で設定する
}

ServoBus::~ServoBus()
{
  hl_->end();
}

bool ServoBus::ping(int id)
{
  return hl_->Ping(static_cast<u8>(id)) != -1;
}

std::optional<int> ServoBus::read_position(int id)
{
  // ReadPos(id) は失敗すると -1 を返すが、多回転モードでは -1 も正しい位置になりうる。
  // そこで FeedBack(id) の戻り値で成否を見て、いま読んだ値を ReadPos(-1) で取り出す
  if (hl_->FeedBack(id) == -1) {
    return std::nullopt;
  }
  return hl_->ReadPos(-1);
}

bool ServoBus::set_torque(int id, bool on)
{
  return hl_->EnableTorque(static_cast<u8>(id), on ? 1 : 0) == 1;
}

void ServoBus::sync_write_positions(
  const std::vector<int> & ids, const std::vector<int> & counts, int speed, int acc)
{
  if (ids.size() != counts.size()) {
    throw std::invalid_argument("ids と counts の長さが違います");
  }
  if (speed <= 0) {
    throw std::invalid_argument("speed は 1 以上にする（0 だとサーボが動かない）");
  }
  const size_t n = ids.size();
  if (n == 0) {
    return;
  }
  // SDK は C の配列で受け取るので、SDK の型の vector に詰め替える
  std::vector<u8> id_buf(n);
  std::vector<s16> pos_buf(n);
  std::vector<u16> spd_buf(n, static_cast<u16>(speed));
  std::vector<u8> acc_buf(n, static_cast<u8>(acc));
  std::vector<u16> trq_buf(n, kGoalTorque);
  for (size_t i = 0; i < n; ++i) {
    id_buf[i] = static_cast<u8>(ids[i]);
    pos_buf[i] = static_cast<s16>(counts[i]);
  }
  hl_->SyncWritePosEx(
    id_buf.data(), static_cast<u8>(n), pos_buf.data(), spd_buf.data(), acc_buf.data(),
    trq_buf.data());
}

}  // namespace feetech_driver
