#ifndef IO__TCP_FRAME_CAMERA_HPP
#define IO__TCP_FRAME_CAMERA_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace io
{
// Receives SPB1 packets from tools/wsl_bridge/mvs_camera_bridge.py. Payloads are packed BGR8.
class TcpFrameCamera
{
public:
  TcpFrameCamera(const std::string & host, uint16_t port);
  ~TcpFrameCamera();

  TcpFrameCamera(const TcpFrameCamera &) = delete;
  TcpFrameCamera & operator=(const TcpFrameCamera &) = delete;

  bool read_for(
    cv::Mat & image, std::chrono::steady_clock::time_point & timestamp,
    std::chrono::milliseconds timeout);
  void read(cv::Mat & image, std::chrono::steady_clock::time_point & timestamp);

private:
  int socket_ = -1;
  std::vector<uint8_t> buffer_;
  size_t consumed_ = 0;

  bool receive_more(std::chrono::steady_clock::time_point deadline);
};
}  // namespace io

#endif  // IO__TCP_FRAME_CAMERA_HPP
