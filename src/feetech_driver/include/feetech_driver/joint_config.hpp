#ifndef FEETECH_DRIVER__JOINT_CONFIG_HPP_
#define FEETECH_DRIVER__JOINT_CONFIG_HPP_

#include <string>

namespace feetech_driver
{

constexpr int kCountsPerRev = 4096;
constexpr double kPi = 3.14159265358979323846;

// deg <--> rad変換
constexpr double deg_to_rad(double deg) {return deg * kPi / 180.0;}
constexpr double rad_to_deg(double rad) {return rad * 180.0 / kPi;}

//関節一つの設定
struct JointConfig
{
  std::string name;
  std::string port;
  int id = 0;
  int home_count = 0;
  int direction = 1; // 1 or -1
  double min_rad = 0.0;
  double max_rad = 0.0;

  // 可動域に丸める
  double clamp(double rad) const;

  // rad -> count (同時に値を丸める)
  int to_count(double rad) const;

  // count -> rad
  double to_rad(int count) const;
};

//値を検査する
void validate(const JointConfig & j);

}  // namespace feetech_driver

#endif  // FEETECH_DRIVER__JOINT_CONFIG_HPP_
