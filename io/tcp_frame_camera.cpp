#include "tcp_frame_camera.hpp"
#include <opencv2/imgcodecs.hpp>

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace io
{
namespace
{
constexpr size_t kHeaderSize = 16;
constexpr uint32_t kMaxDimension = 8192;
constexpr uint32_t kMaxPayload = 256U * 1024U * 1024U;

uint32_t read_network_u32(const uint8_t * p)
{
  uint32_t value;
  std::memcpy(&value, p, sizeof(value));
  return ntohl(value);
}
}  // namespace

TcpFrameCamera::TcpFrameCamera(const std::string & host, uint16_t port)
{
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo * addresses = nullptr;
  const auto service = std::to_string(port);
  const int lookup = getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses);
  if (lookup != 0) throw std::runtime_error("Frame bridge DNS lookup failed: " + std::string(gai_strerror(lookup)));

  for (auto * address = addresses; address != nullptr; address = address->ai_next) {
    socket_ = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (socket_ < 0) continue;
    if (::connect(socket_, address->ai_addr, address->ai_addrlen) == 0) break;
    ::close(socket_);
    socket_ = -1;
  }
  freeaddrinfo(addresses);
  if (socket_ < 0) throw std::runtime_error("Cannot connect to the Windows MVS frame bridge");
}

TcpFrameCamera::~TcpFrameCamera()
{
  if (socket_ >= 0) ::close(socket_);
}

bool TcpFrameCamera::receive_more(std::chrono::steady_clock::time_point deadline)
{
  const auto now = std::chrono::steady_clock::now();
  if (now >= deadline) return false;
  const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
  pollfd descriptor{socket_, POLLIN, 0};
  int ready;
  do {
    ready = ::poll(&descriptor, 1, static_cast<int>(std::max<int64_t>(1, remaining.count())));
  } while (ready < 0 && errno == EINTR);
  if (ready == 0) return false;
  if (ready < 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
    throw std::runtime_error("Windows MVS frame bridge disconnected");

  if (consumed_ != 0 && (consumed_ > buffer_.size() / 2 || buffer_.size() >= kMaxPayload)) {
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(consumed_));
    consumed_ = 0;
  }
  std::array<uint8_t, 65536> chunk{};
  const auto count = ::recv(socket_, chunk.data(), chunk.size(), 0);
  if (count == 0) throw std::runtime_error("Windows MVS frame bridge closed the connection");
  if (count < 0) {
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return true;
    throw std::runtime_error("Failed reading from Windows MVS frame bridge");
  }
  buffer_.insert(buffer_.end(), chunk.begin(), chunk.begin() + count);
  return true;
}

bool TcpFrameCamera::read_for(
  cv::Mat & image, std::chrono::steady_clock::time_point & timestamp,
  std::chrono::milliseconds timeout)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (true) {
    const auto available = buffer_.size() - consumed_;
    if (available >= kHeaderSize) {
      const auto * header = buffer_.data() + consumed_;
      const bool is_bgr8 = std::memcmp(header, "SPB1", 4) == 0;
      const bool is_jpeg = std::memcmp(header, "SPJ1", 4) == 0;
      if (!is_bgr8 && !is_jpeg) throw std::runtime_error("Invalid frame bridge packet magic");
      const uint32_t width = read_network_u32(header + 4);
      const uint32_t height = read_network_u32(header + 8);
      const uint32_t payload_size = read_network_u32(header + 12);
      const uint64_t expected = static_cast<uint64_t>(width) * height * 3U;
      if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension ||
          payload_size == 0 || payload_size > kMaxPayload || (is_bgr8 && payload_size != expected))
        throw std::runtime_error("Invalid dimensions or payload length from frame bridge");

      const size_t packet_size = kHeaderSize + payload_size;
      if (available >= packet_size) {
        if (is_bgr8) {
          image.create(static_cast<int>(height), static_cast<int>(width), CV_8UC3);
          std::memcpy(image.data, header + kHeaderSize, payload_size);
        } else {
          std::vector<uint8_t> encoded(header + kHeaderSize, header + packet_size);
          image = cv::imdecode(encoded, cv::IMREAD_COLOR);
          if (image.empty() || image.cols != static_cast<int>(width) ||
              image.rows != static_cast<int>(height))
            throw std::runtime_error("Could not decode frame bridge JPEG payload");
        }
        consumed_ += packet_size;
        timestamp = std::chrono::steady_clock::now();
        return true;
      }
    }

    if (!receive_more(deadline)) return false;
  }
}

void TcpFrameCamera::read(cv::Mat & image, std::chrono::steady_clock::time_point & timestamp)
{
  while (!read_for(image, timestamp, std::chrono::seconds(1))) {
  }
}
}  // namespace io
