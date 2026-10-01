#include "protocol.hpp"

#include <cmath>

#include "tools/crc.hpp"

namespace io
{
VisionToGimbal make_vision_command(
  bool control, bool fire, float yaw, float yaw_vel, float yaw_acc, float pitch,
  float pitch_vel, float pitch_acc)
{
  VisionToGimbal command{};
  command.mode = control ? (fire ? 2 : 1) : 0;
  command.yaw = yaw;
  command.yaw_vel = yaw_vel;
  command.yaw_acc = yaw_acc;
  command.pitch = pitch;
  command.pitch_vel = pitch_vel;
  command.pitch_acc = pitch_acc;
  command.crc16 = tools::get_crc16(
    reinterpret_cast<const uint8_t *>(&command), sizeof(command) - sizeof(command.crc16));
  return command;
}

VisionToGimbal make_bareboard_aim_command(
  float yaw, float yaw_vel, float yaw_acc, float pitch, float pitch_vel, float pitch_acc)
{
  return make_vision_command(true, false, yaw, yaw_vel, yaw_acc, pitch, pitch_vel, pitch_acc);
}

VisionToGimbal make_bareboard_control_command(
  bool has_target, bool link_online, bool plan_control, float yaw, float yaw_vel, float yaw_acc,
  float pitch, float pitch_vel, float pitch_acc)
{
  const bool finite = std::isfinite(yaw) && std::isfinite(yaw_vel) && std::isfinite(yaw_acc) &&
                      std::isfinite(pitch) && std::isfinite(pitch_vel) && std::isfinite(pitch_acc);
  if (!has_target || !link_online || !plan_control || !finite) {
    return make_vision_command(false, false, 0, 0, 0, 0, 0, 0);
  }
  return make_bareboard_aim_command(yaw, yaw_vel, yaw_acc, pitch, pitch_vel, pitch_acc);
}

bool check_vision_command(const VisionToGimbal & command)
{
  return tools::check_crc16(
    reinterpret_cast<const uint8_t *>(&command), sizeof(command));
}
}  // namespace io
