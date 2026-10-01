#include <cmath>
#include <csignal>
#include <chrono>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <opencv2/opencv.hpp>

#include "io/camera.hpp"
#include "io/gimbal/gimbal.hpp"
#include "io/tcp_frame_camera.hpp"
#include "tasks/auto_aim/planner/planner.hpp"
#include "tasks/auto_aim/solver.hpp"
#include "tasks/auto_aim/tracker.hpp"
#include "tasks/auto_aim/yolo.hpp"
#include "tools/logger.hpp"

namespace
{
volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int)
{
  stop_requested = 1;
}

bool finite_plan(const auto_aim::Plan & plan)
{
  return std::isfinite(plan.yaw) && std::isfinite(plan.yaw_vel) && std::isfinite(plan.yaw_acc) &&
         std::isfinite(plan.pitch) && std::isfinite(plan.pitch_vel) && std::isfinite(plan.pitch_acc);
}

void send_neutral(io::Gimbal & gimbal)
{
  gimbal.send(false, false, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F);
}

class NeutralOnExit
{
public:
  explicit NeutralOnExit(io::Gimbal & gimbal) : gimbal_(gimbal) {}
  ~NeutralOnExit() { send_neutral(gimbal_); }

  NeutralOnExit(const NeutralOnExit &) = delete;
  NeutralOnExit & operator=(const NeutralOnExit &) = delete;

private:
  io::Gimbal & gimbal_;
};
}  // namespace

int main(int argc, char * argv[])
{
  const std::string keys =
    "{help h usage ? |                 | print usage }"
    "{config-path c  | configs/demo.yaml | yaml configuration path }"
    "{source s       | demo              | input source: demo or camera }"
    "{demo-path      | assets/demo/demo  | demo video/text prefix }"
    "{port           |                   | serial port override }"
    "{bridge-host    |                   | Windows MVS bridge host (source=bridge) }"
    "{bridge-port    | 5001              | Windows MVS bridge TCP port }"
    "{max-frames     | 0                 | stop after N frames; 0 means no limit }";

  cv::CommandLineParser cli(argc, argv, keys);
  if (cli.has("help")) {
    cli.printMessage();
    return 0;
  }

  const auto config_path = cli.get<std::string>("config-path");
  const auto source = cli.get<std::string>("source");
  const auto demo_path = cli.get<std::string>("demo-path");
  const auto serial_port = cli.get<std::string>("port");
  const auto bridge_host = cli.get<std::string>("bridge-host");
  const auto bridge_port = cli.get<int>("bridge-port");
  const auto max_frames = cli.get<int>("max-frames");
  if (!cli.check() || (source != "demo" && source != "camera" && source != "bridge") ||
      max_frames < 0 || bridge_port < 1 || bridge_port > 65535 ||
      (source == "bridge" && bridge_host.empty())) {
    cli.printErrors();
    return 2;
  }

  std::signal(SIGINT, request_stop);
  std::signal(SIGTERM, request_stop);

  try {
    io::Gimbal gimbal(config_path, serial_port);
    NeutralOnExit neutral_on_exit(gimbal);

    std::unique_ptr<io::Camera> camera;
    std::unique_ptr<io::TcpFrameCamera> bridge_camera;
    cv::VideoCapture video;
    std::ifstream demo_pose;
    if (source == "camera") {
      camera = std::make_unique<io::Camera>(config_path);
    } else if (source == "bridge") {
      bridge_camera = std::make_unique<io::TcpFrameCamera>(bridge_host, static_cast<uint16_t>(bridge_port));
    } else {
      video.open(demo_path + ".avi");
      demo_pose.open(demo_path + ".txt");
      if (!video.isOpened() || !demo_pose) {
        throw std::runtime_error("Cannot open demo video or its matching quaternion file");
      }
    }

    auto_aim::YOLO yolo(config_path, false);
    auto_aim::Solver solver(config_path);
    auto_aim::Tracker tracker(config_path, solver);
    auto_aim::Planner planner(config_path);

    cv::Mat image;
    std::chrono::steady_clock::time_point timestamp;
    const auto replay_origin = std::chrono::steady_clock::now();
    unsigned long frame_count = 0;
    unsigned long target_frame_count = 0;
    unsigned long command_count = 0;
    unsigned long neutral_count = 0;
    auto last_report = std::chrono::steady_clock::now();
    auto last_stale_report = std::chrono::steady_clock::time_point{};

    tools::logger()->info(
      "Bare-board HIL started: source={}, camera frames use timed reads; "
      "all outgoing commands force fire=false, idle MCU mode is allowed.", source);

    while (!stop_requested && (max_frames == 0 || frame_count < static_cast<unsigned long>(max_frames))) {
      Eigen::Quaterniond orientation;
      if (source == "demo") {
        if (!video.read(image) || image.empty()) break;

        double seconds = 0.0;
        double qw = 1.0, qx = 0.0, qy = 0.0, qz = 0.0;
        if (!(demo_pose >> seconds >> qw >> qx >> qy >> qz) || !std::isfinite(seconds)) {
          tools::logger()->warn("Demo ended before a matching quaternion was available.");
          send_neutral(gimbal);
          ++neutral_count;
          break;
        }
        timestamp = replay_origin + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                      std::chrono::duration<double>(seconds));
        orientation = Eigen::Quaterniond(qw, qx, qy, qz);
        if (!orientation.coeffs().allFinite() || orientation.norm() < 1e-6) {
          tools::logger()->warn("Invalid quaternion at demo frame {}.", frame_count);
          send_neutral(gimbal);
          ++neutral_count;
          ++frame_count;
          continue;
        }
        orientation.normalize();
      } else {
        const bool received = source == "camera" ?
                                camera->read_for(image, timestamp, std::chrono::milliseconds(200)) :
                                bridge_camera->read_for(image, timestamp, std::chrono::milliseconds(200));
        if (!received || image.empty()) {
          send_neutral(gimbal);
          ++neutral_count;
          continue;
        }
        if (!gimbal.online()) {
          send_neutral(gimbal);
          ++neutral_count;
          const auto now = std::chrono::steady_clock::now();
          if (last_stale_report == std::chrono::steady_clock::time_point{} ||
              now - last_stale_report >= std::chrono::seconds(1)) {
            tools::logger()->warn("MC-02 state stream stale; camera frame received but control revoked.");
            last_stale_report = now;
          }
          continue;
        }
        orientation = gimbal.orientation();
      }

      ++frame_count;
      solver.set_R_gimbal2world(orientation);
      auto armors = yolo.detect(image, static_cast<int>(frame_count));
      auto targets = tracker.track(armors, timestamp);

      if (!targets.empty()) {
        ++target_frame_count;
        const auto gimbal_state = gimbal.state();
        const auto plan = planner.plan(targets.front(), gimbal_state.bullet_speed);
        if (plan.control && finite_plan(plan) && gimbal.online()) {
          // The bool overload maps this to protocol mode 1. The fire argument is a literal false.
          gimbal.send(true, false, plan.yaw, plan.yaw_vel, plan.yaw_acc,
                      plan.pitch, plan.pitch_vel, plan.pitch_acc);
          ++command_count;
          if (command_count == 1 || command_count % 100 == 0) {
            tools::logger()->info(
              "Aim frame={} targets={} mode={} yaw={:.4f} pitch={:.4f} fire=forced-off",
              frame_count, targets.size(), static_cast<int>(gimbal.mode()), plan.yaw, plan.pitch);
          }
        } else {
          send_neutral(gimbal);
          ++neutral_count;
        }
      } else {
        send_neutral(gimbal);
        ++neutral_count;
      }

      const auto now = std::chrono::steady_clock::now();
      if (now - last_report >= std::chrono::seconds(2)) {
        tools::logger()->info(
          "HIL progress: frames={} target_frames={} aim_frames={} neutral_frames={} mc02_online={}",
          frame_count, target_frame_count, command_count, neutral_count, gimbal.online());
        last_report = now;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    send_neutral(gimbal);
    tools::logger()->info(
      "Bare-board HIL stopped: frames={} target_frames={} aim_frames={} neutral_frames={}; "
      "final neutral frame sent.",
      frame_count, target_frame_count, command_count, neutral_count);
  } catch (const std::exception & e) {
    tools::logger()->critical("Bare-board HIL failed: {}", e.what());
    return 1;
  }

  return 0;
}
