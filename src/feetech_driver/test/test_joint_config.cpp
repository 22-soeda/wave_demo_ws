#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <utility>

#include "feetech_driver/joint_config.hpp"

using feetech_driver::JointConfig;
using feetech_driver::deg_to_rad;
using feetech_driver::validate;

namespace
{

// テスト用の関節。可動域はわざと非対称（上下の取り違えを見つけるため）
JointConfig make_joint(int direction)
{
  JointConfig j;
  j.name = "test";
  j.port = "/dev/null";
  j.id = 8;
  j.home_count = 2000;
  j.direction = direction;
  j.min_rad = deg_to_rad(-30.0);
  j.max_rad = deg_to_rad(60.0);
  return j;
}

}  // namespace

TEST(JointConfig, ZeroIsHome)
{
  const JointConfig j = make_joint(1);
  EXPECT_EQ(j.to_count(0.0), 2000);
  EXPECT_DOUBLE_EQ(j.to_rad(2000), 0.0);
}

TEST(JointConfig, Direction)
{
  // 45 deg = 512 カウント
  EXPECT_EQ(make_joint(1).to_count(deg_to_rad(45.0)), 2512);
  EXPECT_EQ(make_joint(-1).to_count(deg_to_rad(45.0)), 1488);
}

TEST(JointConfig, ClampToLimits)
{
  for (const int dir : {1, -1}) {
    const JointConfig j = make_joint(dir);
    EXPECT_EQ(j.to_count(deg_to_rad(100.0)), j.to_count(deg_to_rad(60.0)));
    EXPECT_EQ(j.to_count(deg_to_rad(-100.0)), j.to_count(deg_to_rad(-30.0)));
  }
}

TEST(JointConfig, ReadIsNotClamped)
{
  // 可動域（〜60 deg）の外にいても、読んだ値はそのまま返す
  EXPECT_NEAR(make_joint(1).to_rad(2000 + 1024), deg_to_rad(90.0), 1e-9);
}

TEST(JointConfig, RoundTrip)
{
  const JointConfig j = make_joint(-1);
  const double half_count = feetech_driver::kPi / 4096;  // 0.5 カウント [rad]
  for (double deg = -30.0; deg <= 60.0; deg += 7.3) {
    EXPECT_NEAR(j.to_rad(j.to_count(deg_to_rad(deg))), deg_to_rad(deg), half_count);
  }
}

TEST(JointConfig, NanThrows)
{
  EXPECT_THROW(make_joint(1).to_count(std::nan("")), std::invalid_argument);
}

TEST(Validate, AcceptsGoodConfig)
{
  EXPECT_NO_THROW(validate(make_joint(1)));
  EXPECT_NO_THROW(validate(make_joint(-1)));
}

TEST(Validate, RejectsBadConfig)
{
  JointConfig j = make_joint(1);
  j.id = 254;  // 254 は全サーボ宛て（ブロードキャスト）
  EXPECT_THROW(validate(j), std::invalid_argument);

  j = make_joint(1);
  j.direction = 2;
  EXPECT_THROW(validate(j), std::invalid_argument);

  j = make_joint(1);
  j.home_count = 4096;
  EXPECT_THROW(validate(j), std::invalid_argument);

  j = make_joint(1);
  std::swap(j.min_rad, j.max_rad);  // 上下が逆
  EXPECT_THROW(validate(j), std::invalid_argument);

  j = make_joint(1);
  j.min_rad = deg_to_rad(10.0);  // 0 が可動域の外
  EXPECT_THROW(validate(j), std::invalid_argument);

  j = make_joint(1);
  j.home_count = 4000;  // +60 deg の端が 4000 + 683 になり、4095 を超える
  EXPECT_THROW(validate(j), std::invalid_argument);
}
