#include "camera.hpp"

#include <stdexcept>

#ifdef SP_ENABLE_HIKROBOT
#include "hikrobot/hikrobot.hpp"
#endif
#ifdef SP_ENABLE_MINDVISION
#include "mindvision/mindvision.hpp"
#endif
#include "tools/yaml.hpp"

namespace io
{
Camera::Camera(const std::string & config_path)
{
  auto yaml = tools::load(config_path);
  auto camera_name = tools::read<std::string>(yaml, "camera_name");
  auto exposure_ms = tools::read<double>(yaml, "exposure_ms");

  if (camera_name == "mindvision") {
#ifdef SP_ENABLE_MINDVISION
    auto gamma = tools::read<double>(yaml, "gamma");
    auto vid_pid = tools::read<std::string>(yaml, "vid_pid");
    camera_ = std::make_unique<MindVision>(exposure_ms, gamma, vid_pid);
#else
    throw std::runtime_error("MindVision backend is disabled in this build");
#endif
  }

  else if (camera_name == "hikrobot") {
#ifdef SP_ENABLE_HIKROBOT
    auto gain = tools::read<double>(yaml, "gain");
    auto vid_pid = tools::read<std::string>(yaml, "vid_pid");
    camera_ = std::make_unique<HikRobot>(exposure_ms, gain, vid_pid);
#else
    throw std::runtime_error("HikRobot backend is disabled in this build");
#endif
  }

  else {
    throw std::runtime_error("Unknow camera_name: " + camera_name + "!");
  }
}

void Camera::read(cv::Mat & img, std::chrono::steady_clock::time_point & timestamp)
{
  camera_->read(img, timestamp);
}

bool Camera::read_for(
  cv::Mat & img, std::chrono::steady_clock::time_point & timestamp,
  std::chrono::milliseconds timeout)
{
  return camera_->read_for(img, timestamp, timeout);
}

}  // namespace io
