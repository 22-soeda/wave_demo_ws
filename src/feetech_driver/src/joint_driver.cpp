#include "feetech_driver/joint_driver.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace feetech_driver
{

JointDriver::JointDriver(std::vector<JointConfig> joints, int speed, int acc, int baud)
: joints_(std::move(joints)), speed_(speed), acc_(acc)
{
  validate_joints(joints_);
  if (speed_ <= 0) {
    throw std::invalid_argument("speed は 1 以上にする（0 だとサーボが動かない）");
  }

  const size_t n = joints_.size();
  for (const auto & j : joints_) {
    if (buses_.count(j.port) == 0) {
      buses_[j.port] = std::make_unique<ServoBus>(j.port, baud);
    }
    joint_bus_.push_back(buses_[j.port].get());
  }
  last_rad_.assign(n, 0.0);
  read_failures_.assign(n, 0);
  clamp_counts_.assign(n, 0);

  // 全関節を一度読む。配線や電源の問題を起動時に見つけ、「前回の値」を必ず持つため
  std::string unreadable;
  for (size_t i = 0; i < n; ++i) {
    const auto count = joint_bus_[i]->read_position(joints_[i].id);
    if (count) {
      last_rad_[i] = joints_[i].to_rad(*count);
    } else {
      unreadable += " " + joints_[i].name;
    }
  }
  if (!unreadable.empty()) {
    throw std::runtime_error("位置を読めない関節があります:" + unreadable);
  }
}

JointDriver::~JointDriver()
{
  if (torque_enabled_) {
    disable_torque();
  }
}

std::vector<double> JointDriver::read()
{
  for (size_t i = 0; i < joints_.size(); ++i) {
    const auto count = joint_bus_[i]->read_position(joints_[i].id);
    if (count) {
      last_rad_[i] = joints_[i].to_rad(*count);
    } else {
      ++read_failures_[i];  // 前回の値のまま
    }
  }
  return last_rad_;
}

void JointDriver::write(const std::vector<double> & rad)
{
  if (!torque_enabled_) {
    throw std::logic_error("トルクが入っていないので、指令を送りません");
  }
  if (rad.size() != joints_.size()) {
    throw std::invalid_argument("指令の数が関節の数と違います");
  }
  std::vector<int> counts(joints_.size());
  for (size_t i = 0; i < joints_.size(); ++i) {
    if (joints_[i].clamp(rad[i]) != rad[i]) {
      ++clamp_counts_[i];
    }
    counts[i] = joints_[i].to_count(rad[i]);  // 中で丸める
  }
  send(counts);
}

void JointDriver::enable_torque()
{
  // 1. 全関節の今の位置を読む。読めない関節や、可動域の外にいる関節があれば入れない
  std::vector<int> counts(joints_.size());
  for (size_t i = 0; i < joints_.size(); ++i) {
    const JointConfig & j = joints_[i];
    const auto count = joint_bus_[i]->read_position(j.id);
    if (!count) {
      throw std::runtime_error(j.name + ": 位置を読めないので、トルクを入れません");
    }
    const double rad = j.to_rad(*count);
    if (j.clamp(rad) != rad) {
      throw std::runtime_error(
              j.name + ": 可動域の外にいます（" + std::to_string(rad_to_deg(rad)) +
              " deg）。手で可動域の中に戻してから、やり直してください");
    }
    counts[i] = *count;
    last_rad_[i] = rad;
  }

  // 2. 目標を今の位置にする（入れた瞬間に跳ねないように）
  send(counts);

  // 3. トルクを入れる。1 つでも失敗したら、全部切ってから投げる
  torque_enabled_ = true;
  for (size_t i = 0; i < joints_.size(); ++i) {
    if (!joint_bus_[i]->set_torque(joints_[i].id, true)) {
      disable_torque();
      throw std::runtime_error(
              joints_[i].name + ": トルクを入れられません（応答なし）。全関節のトルクを切りました");
    }
  }
}

bool JointDriver::disable_torque()
{
  bool all_ok = true;
  for (size_t i = 0; i < joints_.size(); ++i) {
    if (!joint_bus_[i]->set_torque(joints_[i].id, false)) {
      all_ok = false;  // 応答がなくても、残りの関節は切りにいく
    }
  }
  torque_enabled_ = false;
  return all_ok;
}

void JointDriver::send(const std::vector<int> & counts)
{
  for (const auto & [port, bus] : buses_) {
    std::vector<int> ids;
    std::vector<int> bus_counts;
    for (size_t i = 0; i < joints_.size(); ++i) {
      if (joint_bus_[i] == bus.get()) {
        ids.push_back(joints_[i].id);
        bus_counts.push_back(counts[i]);
      }
    }
    bus->sync_write_positions(ids, bus_counts, speed_, acc_);
  }
}

}  // namespace feetech_driver
