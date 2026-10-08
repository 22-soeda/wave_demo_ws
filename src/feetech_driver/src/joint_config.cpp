#include "feetech_driver/joint_config.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace feetech_driver
{

namespace
{

constexpr double kCountsPerRad = kCountsPerRev / (2.0 * kPi);

void check(bool ok, const JointConfig & j, const std::string & what)
{
  if (!ok) {
    throw std::invalid_argument(j.name + ": " + what);
  }
}

}  // namespace

double JointConfig::clamp(double rad) const
{
  return std::clamp(rad, min_rad, max_rad);
}

int JointConfig::to_count(double rad) const
{
  if (!std::isfinite(rad)) {
    throw std::invalid_argument(name + ": 指令が有限の値ではありません");
  }
  return home_count + static_cast<int>(std::lround(direction * clamp(rad) * kCountsPerRad));
}

double JointConfig::to_rad(int count) const
{
  return direction * (count - home_count) / kCountsPerRad;
}

void validate(const JointConfig & j)
{
  check(0 <= j.id && j.id <= 253, j, "id は 0〜253");
  check(j.direction == 1 || j.direction == -1, j, "direction は 1 か -1");
  check(0 <= j.home_count && j.home_count < kCountsPerRev, j, "home_count は 0〜4095");
  check(j.min_rad < j.max_rad, j, "min_rad < max_rad にする");
  check(j.min_rad <= 0.0 && 0.0 <= j.max_rad, j, "T ポーズ（0 rad）が可動域の外です");
  for (const double lim : {j.min_rad, j.max_rad}) {
    const int c = j.to_count(lim);
    check(0 <= c && c < kCountsPerRev, j, "可動域の端がカウント 0〜4095 の外に出ます");
  }
}

}  // namespace feetech_driver
