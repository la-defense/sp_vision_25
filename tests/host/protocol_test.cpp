#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>

#include "io/gimbal/protocol.hpp"
#include "tools/crc.hpp"

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      std::fprintf(stderr, "FAIL:%d: %s\n", __LINE__, #condition);             \
      return 1;                                                                 \
    }                                                                           \
  } while (false)

int main()
{
  static_assert(sizeof(io::GimbalToVision) == 43);
  static_assert(sizeof(io::VisionToGimbal) == 29);

  const auto aim = io::make_bareboard_aim_command(0.1F, 0.2F, 0.3F, -0.1F, -0.2F, -0.3F);
  CHECK(aim.head[0] == 'S' && aim.head[1] == 'P');
  CHECK(aim.mode == 1);
  CHECK(io::check_vision_command(aim));
  CHECK(aim.yaw == 0.1F && aim.pitch == -0.1F);

  const auto neutral = io::make_vision_command(false, true, 0, 0, 0, 0, 0, 0);
  CHECK(neutral.mode == 0);
  CHECK(io::check_vision_command(neutral));

  const auto normal_fire = io::make_vision_command(true, true, 0, 0, 0, 0, 0, 0);
  CHECK(normal_fire.mode == 2);
  CHECK(io::check_vision_command(normal_fire));

  const auto valid_bareboard = io::make_bareboard_control_command(
    true, true, true, 0.1F, 0.2F, 0.3F, -0.1F, -0.2F, -0.3F);
  CHECK(valid_bareboard.mode == 1);
  CHECK(io::check_vision_command(valid_bareboard));
  CHECK(valid_bareboard.yaw == 0.1F && valid_bareboard.pitch == -0.1F);

  const auto no_target = io::make_bareboard_control_command(
    false, true, true, 0.1F, 0.2F, 0.3F, -0.1F, -0.2F, -0.3F);
  const auto stale_link = io::make_bareboard_control_command(
    true, false, true, 0.1F, 0.2F, 0.3F, -0.1F, -0.2F, -0.3F);
  const auto invalid_plan = io::make_bareboard_control_command(
    true, true, false, 0.1F, 0.2F, 0.3F, -0.1F, -0.2F, -0.3F);
  const auto non_finite_plan = io::make_bareboard_control_command(
    true, true, true, NAN, 0.2F, 0.3F, -0.1F, -0.2F, -0.3F);
  CHECK(no_target.mode == 0 && io::check_vision_command(no_target));
  CHECK(stale_link.mode == 0 && io::check_vision_command(stale_link));
  CHECK(invalid_plan.mode == 0 && io::check_vision_command(invalid_plan));
  CHECK(non_finite_plan.mode == 0 && io::check_vision_command(non_finite_plan));

  auto corrupted = aim;
  corrupted.yaw_vel += 1.0F;
  CHECK(!io::check_vision_command(corrupted));

  const uint8_t known_bytes[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(tools::get_crc16(known_bytes, sizeof(known_bytes)) == 0x6f91);

  std::cout << "SP frame layout, command modes, and CRC checks passed\n";
  return 0;
}
