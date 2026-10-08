#include "feetech_driver/servo_bus.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "scservo/HLSCL.h"

namespace feetech_driver
{

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

}  // namespace feetech_driver
