#ifndef IO__GIMBAL_PROTOCOL_HPP
#define IO__GIMBAL_PROTOCOL_HPP

#include <cstdint>

namespace io
{
struct __attribute__((packed)) GimbalToVision
{
  uint8_t head[2] = {'S', 'P'};
  uint8_t mode = 0;
  float q[4]{};
  float yaw = 0;
  float yaw_vel = 0;
  float pitch = 0;
  float pitch_vel = 0;
  float bullet_speed = 0;
  uint16_t bullet_count = 0;
  uint16_t crc16 = 0;
};

struct __attribute__((packed)) VisionToGimbal
{
  uint8_t head[2] = {'S', 'P'};
  uint8_t mode = 0;
  float yaw = 0;
  float yaw_vel = 0;
  float yaw_acc = 0;
  float pitch = 0;
  float pitch_vel = 0;
  float pitch_acc = 0;
  uint16_t crc16 = 0;
};

static_assert(sizeof(GimbalToVision) == 43, "MC-02 state frame must stay 43 bytes");
static_assert(sizeof(VisionToGimbal) == 29, "Vision command frame must stay 29 bytes");

VisionToGimbal make_vision_command(
  bool control, bool fire, float yaw, float yaw_vel, float yaw_acc, float pitch,
  float pitch_vel, float pitch_acc);

VisionToGimbal make_bareboard_aim_command(
  float yaw, float yaw_vel, float yaw_acc, float pitch, float pitch_vel, float pitch_acc);

VisionToGimbal make_bareboard_control_command(
  bool has_target, bool link_online, bool plan_control, float yaw, float yaw_vel, float yaw_acc,
  float pitch, float pitch_vel, float pitch_acc);

bool check_vision_command(const VisionToGimbal & command);

}  // namespace io

#endif  // IO__GIMBAL_PROTOCOL_HPP
