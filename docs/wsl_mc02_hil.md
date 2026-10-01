# WSL 2 deployment and MC-02 bench HIL

Baseline: official `TongjiSuperPower/sp_vision_25` commit `bd9f5e798fa3c6dd3b483ae6627796afb41c608d` (recheck upstream HEAD when cloning). The Windows staging checkout is `D:\RoboMasterRelated\OpenSourseProject\sp_vision_25-upstream`; the existing personal checkout `sp_vision_25-main` was left untouched. Final build/run checkout belongs under the WSL Linux home directory.

## Windows prerequisites and current installation

Windows 10 Pro for Workstations (build 19045) now reports a running hypervisor. The WSL and Virtual Machine Platform optional features are enabled; wsl --update completed, default WSL version is 2, and Ubuntu 22.04.5 LTS is installed as WSL 2. Its default Linux user is mc02.

The USB ACM node is root-owned in this WSL image, so the direct-device examples use `sudo -E`. If the `mc02` sudo password is unavailable, reset it from Windows with `wsl -d Ubuntu-22.04 -u root -- passwd mc02` and choose a new one.

usbipd-win 5.3.0 is installed. Current device IDs were camera 2BDF:0001 (BUSID 2-13), MC-02 CDC 0483:5740 (BUSID 3-3), and Horco CMSIS-DAP FAED:4870 (BUSID 3-4). BUSIDs can change after replugging; run usbipd list before each bind/attach operation. The camera was successfully used through the Windows MVS bridge fallback. The MC-02 CDC was attached directly to WSL and appeared as /dev/ttyACM0.

After a physical USB replug, the Windows OpenOCD session detects the Horco v0.2 CMSIS-DAP, reports SWD DPIDR `0x6ba02477`, and can halt/read/resume the MC-02. The post-HIL RX, CRC, status-TX, and USB busy counters are recorded below.

The official clone lives in the WSL Linux filesystem at /home/mc02/src/sp_vision_25, on commit bd9f5e798fa3c6dd3b483ae6627796afb41c608d. The prepared overlay was applied there. The Windows staging checkout is D:\RoboMasterRelated\OpenSourseProject\sp_vision_25-upstream; the existing personal checkout sp_vision_25-main remains untouched.

The older inbox wsl.exe does not implement --version or --status; use wsl -l -v to check distro state.

## Ubuntu dependencies and OpenVINO

```bash
sudo apt update
sudo apt install -y git build-essential cmake ninja-build pkg-config \
  libopencv-dev libfmt-dev libeigen3-dev libspdlog-dev libyaml-cpp-dev \
  nlohmann-json3-dev libceres-dev libusb-1.0-0-dev usbutils socat openocd curl
```

The project baseline uses the OpenVINO 2024.6 C++ Runtime. Intel's Ubuntu 22.04 x86_64 archive instructions:

```bash
cd ~/Downloads
curl -L https://storage.openvinotoolkit.org/repositories/openvino/packages/2024.6/linux/l_openvino_toolkit_ubuntu22_2024.6.0.17404.4c0f47d2335_x86_64.tgz -o openvino_2024.6.0.tgz
tar -xf openvino_2024.6.0.tgz
sudo mkdir -p /opt/intel
sudo mv l_openvino_toolkit_ubuntu22_2024.6.0.17404.4c0f47d2335_x86_64 /opt/intel/openvino_2024.6.0
cd /opt/intel/openvino_2024.6.0
sudo -E ./install_dependencies/install_openvino_dependencies.sh -y -c=core -c=dev -c=python
source /opt/intel/openvino_2024.6.0/setupvars.sh
```

`SP_OPENVINO_CMAKE_DIR` is a CMake cache path. The checked-in WSL preset defaults it to `/opt/intel/openvino_2024.6.0/runtime/cmake`; pass a different path to CMake to use another installation.

## Build the required targets

```bash
cd ~/src/sp_vision_25
source /opt/intel/openvino_2024.6.0/setupvars.sh
cmake --preset wsl-hil
cmake --build --preset wsl-hil-targets -j2
```

The preset builds `standard_mpc`, offline `auto_aim_test`, and `bareboard_hil`; it enables the auto-buff library because `standard_mpc` links it, selects the HikRobot backend, disables MindVision, skips unrelated applications/tests, and uses CPU inference from `configs/demo.yaml`.

Offline demo check (GUI is not required for the HIL entry point; `auto_aim_test` opens its existing visualization window):

```bash
source /opt/intel/openvino_2024.6.0/setupvars.sh
./build/wsl-hil/auto_aim_test --config-path=configs/demo.yaml --end-index=150 assets/demo/demo
```

## Direct USB connection

On Windows, refresh the BUSIDs before binding. Current observed IDs were camera `2BDF:0001` at `2-13` and MC-02 CDC `0483:5740` at `3-3`; BUSIDs can change after replugging.

```powershell
usbipd list
usbipd bind --busid 2-13
usbipd attach --wsl --busid 2-13
usbipd bind --busid 3-3
usbipd attach --wsl --busid 3-3
```

In Ubuntu, verify with lsusb and ls -l /dev/ttyACM*. Use the actual device node for the MC-02. In this setup, USB device nodes are root-only, so direct-device HIL examples use sudo -E.

```bash
source /opt/intel/openvino_2024.6.0/setupvars.sh
sudo -E ./build/wsl-hil/bareboard_hil --config-path=configs/demo.yaml --source=camera --port=/dev/ttyACM0
sudo -E ./build/wsl-hil/bareboard_hil --config-path=configs/demo.yaml --source=demo --port=/dev/ttyACM0
```

The camera path uses fresh MC-02 quaternion data; the demo path reads the matching quaternion samples from `assets/demo/demo.txt`. The bench accepts MCU idle mode. Every aiming frame uses the `mode=1` send path with `fire=false`; missing targets, stale MCU state, camera timeouts, normal exit, and SIGINT/SIGTERM send a neutral `mode=0` frame. Firmware timeout clearing is an additional fail-safe.

## Windows MVS and COM bridge fallback

Use these only when a device cannot be used reliably through usbipd. The MVS bridge has been exercised locally against the installed Windows MVS 5.1.0 SDK and produces JPEG packets; `serial_to_tcp.py` forwards bytes unchanged and keeps the SP frame/CRC protocol intact. Windows Python needs the MVS Python bindings plus `pyserial`, NumPy, and OpenCV (`python -m pip install pyserial numpy opencv-python`).

Find the WSL VM's current IP with `hostname -I` in Ubuntu. In PowerShell from the Windows staging checkout, start each bridge in its own terminal and pass that exact address as `--allow-peer` (the bridge rejects all other clients):

```powershell
$WSL_IP = (wsl -d Ubuntu-22.04 hostname -I).Trim().Split(' ')[0]
python tools\wsl_bridge\mvs_camera_bridge.py --host 0.0.0.0 --allow-peer $WSL_IP --port 5001 --vid 2bdf --pid 0001
python tools\wsl_bridge\serial_to_tcp.py --port COM14 --host 0.0.0.0 --allow-peer $WSL_IP --tcp-port 5002
```

In WSL, the default route gateway is the Windows host address for the WSL NAT network. Create a PTY for the unchanged serial driver and run camera HIL:

```bash
source /opt/intel/openvino_2024.6.0/setupvars.sh
WSL_GATEWAY=$(ip -4 route show default | awk '{print $3; exit}')
socat pty,raw,echo=0,link=/tmp/mc02-gimbal,mode=666 tcp:${WSL_GATEWAY}:5002
```

In a second WSL terminal:

```bash
source /opt/intel/openvino_2024.6.0/setupvars.sh
WSL_GATEWAY=$(ip -4 route show default | awk '{print $3; exit}')
./build/wsl-hil/bareboard_hil --config-path=configs/demo.yaml --source=bridge \
  --bridge-host="$WSL_GATEWAY" --bridge-port=5001 --port=/tmp/mc02-gimbal
```

If Windows Firewall blocks the WSL-side connection, add a private-network inbound TCP rule scoped to this `$WSL_IP` and ports 5001/5002, or return to usbipd direct attachment.

## Verification results (2026-10-01)

- WSL build: CMake configured successfully with GCC 11.4, OpenCV 4.5.4, Ceres 2.0, libusb, and OpenVINO 2024.6 CPU runtime. cmake --build --preset wsl-hil-targets -j2 succeeded for standard_mpc, auto_aim_test, and bareboard_hil. One missing OpenCV imgcodecs include was fixed in io/tcp_frame_camera.cpp.
- Demo self-aim: bareboard_hil processed 300 demo frames with the matching quaternion file. It reported 268 target frames and 268 finite aim frames (yaw about 2.33–2.57 rad, pitch about -0.13–0.06 rad), with 32 neutral frames and a final neutral frame. The MC-02 state stream remained online and the board reported idle mode. Every aim log said fire=forced-off; no motor or firing hardware was connected.
- MC-02 direct USB: 0483:5740 attached through usbipd, cdc_acm created /dev/ttyACM0, and the HIL reader parsed fresh status frames with valid CRCs (mc02_online=true). The first run exposed the upstream serial reader's zero-timeout busy loop, which repeatedly closed the port while sends were in progress. io/gimbal/gimbal.cpp now uses a 20 ms bounded serial timeout and reconnects after 10 consecutive read failures; the rebuilt HIL run completed without serial write errors.
- Camera direct USB: Linux lsusb saw 2BDF:0001, but the bundled Linux MVS SDK repeatedly returned MV_CC_EnumDevices 0x80000006. Per the fallback plan, the Windows MVS 5.1.0 bridge streamed JPEG frames over TCP to TcpFrameCamera in WSL. The HIL recognizer processed 100 camera frames in about 3.4 seconds (roughly 29 fps); no armor was present, and all frames caused neutral commands.
- Serial receive bridge scripts were separately validated on Windows before this WSL run. The current camera bridge accepted only the configured WSL peer 172.30.227.104; the WSL-side Windows gateway at test time was 172.30.224.1. These IPs are transient.
- After replugging CMSIS-DAP, a 300-frame WSL HIL run was bracketed with SWD counter reads. Before: CRC errors 0, USB busy drops 1,062,724, accepted 43-byte status frames 32,148, received 29-byte vision commands 1,243. After: CRC errors 0, USB busy drops 1,063,036, accepted status frames 35,502, vision commands 1,545. RX increased by 302, matching 268 aim commands, 32 no-target neutral commands, and two exit neutral commands; CRC errors remained zero.
- During active WSL HIL samples, the USB busy counter stayed flat while accepted status frames advanced by 362 in about 1.8 seconds and 459 in about 2.3 seconds (about 200 Hz). It increased after the process exited and the host stopped reading. An independent 5.027-second Windows COM14 capture received 1,004 CRC-valid 43-byte frames at 199.73 Hz, no bad frames, and no busy-drop increase.
- A later long-lived OpenOCD poll logged CMSIS-DAP USB I/O errors. A short reconnect succeeded and showed the CPU running, but counters had reinitialized (RX=0, status TX=1); the reset cause is unknown. The HIL snapshots above were saved before this, and future counter reads should use short OpenOCD sessions until the reset/probe behavior is investigated.
- To run the OpenCV command-line tools, pass values with --option=value (for example --config-path=configs/demo.yaml); separating option and value with a space is misparsed by this build's cv::CommandLineParser.
