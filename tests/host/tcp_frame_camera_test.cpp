#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "io/tcp_frame_camera.hpp"

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      std::fprintf(stderr, "FAIL:%d: %s\n", __LINE__, #condition);             \
      return 1;                                                                 \
    }                                                                           \
  } while (false)

int main()
{
  const int reservation = ::socket(AF_INET, SOCK_STREAM, 0);
  CHECK(reservation >= 0);

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  CHECK(::bind(reservation, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);

  socklen_t address_size = sizeof(address);
  CHECK(::getsockname(reservation, reinterpret_cast<sockaddr *>(&address), &address_size) == 0);
  const auto port = ntohs(address.sin_port);
  ::close(reservation);

  const auto start = std::chrono::steady_clock::now();
  bool threw = false;
  try {
    io::TcpFrameCamera camera("127.0.0.1", port);
  } catch (const std::runtime_error &) {
    threw = true;
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;

  CHECK(threw);
  CHECK(elapsed < std::chrono::seconds(1));
  std::cout << "TCP frame bridge connection failure returned promptly\n";
  return 0;
}
