#ifndef FEETECH_DRIVER__JOINT_DRIVER_HPP_
#define FEETECH_DRIVER__JOINT_DRIVER_HPP_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "feetech_driver/joint_config.hpp"
#include "feetech_driver/servo_bus.hpp"

namespace feetech_driver
{

// 複数のバスにまたがる関節をまとめて、「関節の並びと rad」で読み書きする。
// 関節の並びは、コンストラクタに渡した順のまま変わらない
class JointDriver
{
public:
  // 検査し、ポートごとにバスを開き、全関節を一度読む。トルクは入れない。
  // 読めない関節があれば std::runtime_error
  // speed [カウント/s]・acc: 位置指令と一緒に送る速度と加速度
  JointDriver(std::vector<JointConfig> joints, int speed, int acc, int baud = 1000000);
  ~JointDriver();  // トルクが入っていれば切る

  JointDriver(const JointDriver &) = delete;
  JointDriver & operator=(const JointDriver &) = delete;

  const std::vector<JointConfig> & joints() const {return joints_;}

  // 全関節の今の角度 [rad]。読めなかった関節は前回の値
  std::vector<double> read();
  // 全関節の目標 [rad]。可動域に丸めてから送る。トルクが入っていなければ std::logic_error
  void write(const std::vector<double> & rad);

  // 今の位置を保ったままトルクを入れる。
  // 入れられなければ、全関節のトルクを切ってから std::runtime_error
  void enable_torque();
  // トルクを切る。例外は投げない。全関節から応答があれば true
  bool disable_torque();
  bool torque_enabled() const {return torque_enabled_;}

  const std::vector<int> & read_failures() const {return read_failures_;}
  const std::vector<int> & clamp_counts() const {return clamp_counts_;}

private:
  // 全関節のカウントを、バスごとに 1 パケットで送る
  void send(const std::vector<int> & counts);

  std::vector<JointConfig> joints_;
  int speed_;
  int acc_;
  std::map<std::string, std::unique_ptr<ServoBus>> buses_;  // ポート → バス
  std::vector<ServoBus *> joint_bus_;  // 関節 i が載っているバス
  std::vector<double> last_rad_;       // 関節 i の最後に読めた角度
  std::vector<int> read_failures_;
  std::vector<int> clamp_counts_;
  bool torque_enabled_ = false;
};

}  // namespace feetech_driver

#endif  // FEETECH_DRIVER__JOINT_DRIVER_HPP_
