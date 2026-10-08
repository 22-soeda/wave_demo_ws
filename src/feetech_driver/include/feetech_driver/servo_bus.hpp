#ifndef FEETECH_DRIVER__SERVO_BUS_HPP_
#define FEETECH_DRIVER__SERVO_BUS_HPP_

#include <memory>
#include <optional>
#include <string>
#include <vector>


class HLSCL;

namespace feetech_driver
{

// サーボのバス 1 本
class ServoBus
{
public:
  // ポートを開く。開けなければ std::runtime_error
  // timeout_ms: 応答を待つ時間。応答しないサーボを読むと、毎回この時間だけ待つ
  explicit ServoBus(const std::string & port, int baud = 1000000, int timeout_ms = 20);
  ~ServoBus();

  ServoBus(const ServoBus &) = delete;
  ServoBus & operator=(const ServoBus &) = delete;

  const std::string & port() const {return port_;}

  bool ping(int id);
  // 現在位置 [カウント]。読めなければ std::nullopt
  std::optional<int> read_position(int id);

  // トルクを入れる / 切る。応答がなければ false
  bool set_torque(int id, bool on);
  // 複数の軸の目標位置を 1 パケットで送る。応答はないので、届いたかは読んで確かめる
  // speed [カウント/s] は 1 以上（0 だとサーボが動かない）
  void sync_write_positions(
    const std::vector<int> & ids, const std::vector<int> & counts, int speed, int acc);

private:
  std::string port_;
  std::unique_ptr<HLSCL> hl_;
};

}  // namespace feetech_driver

#endif  // FEETECH_DRIVER__SERVO_BUS_HPP_
