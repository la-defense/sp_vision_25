#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/Linux-filesystem/sp_vision_25" >&2
  exit 2
fi

target=$(realpath "$1")
stage=$(realpath "$(dirname "$0")/../..")
expected_base=bd9f5e798fa3c6dd3b483ae6627796afb41c608d
actual_base=$(git -C "$target" rev-parse HEAD)

if [[ "$target" == /mnt/* ]]; then
  echo "Target must be in the WSL Linux filesystem, not under /mnt." >&2
  exit 2
fi
if [[ "$actual_base" != "$expected_base" ]]; then
  echo "Unexpected target commit: $actual_base (expected $expected_base). Recheck upstream and review the overlay before applying." >&2
  exit 2
fi

files=(
  .gitignore
  CMakeLists.txt
  CMakePresets.json
  io/CMakeLists.txt
  io/camera.cpp
  io/camera.hpp
  io/gimbal/gimbal.cpp
  io/gimbal/gimbal.hpp
  io/hikrobot/hikrobot.cpp
  io/hikrobot/hikrobot.hpp
  io/tcp_frame_camera.cpp
  io/tcp_frame_camera.hpp
  tasks/auto_aim/CMakeLists.txt
  tasks/auto_buff/CMakeLists.txt
  tasks/omniperception/CMakeLists.txt
  tools/CMakeLists.txt
  tools/thread_safe_queue.hpp
  src/bareboard_hil.cpp
  tools/wsl_bridge/apply_wsl_hil_overlay.sh
  tools/wsl_bridge/mvs_camera_bridge.py
  tools/wsl_bridge/serial_to_tcp.py
  docs/wsl_mc02_hil.md
)

for rel in "${files[@]}"; do
  mkdir -p "$target/$(dirname "$rel")"
  cp -p "$stage/$rel" "$target/$rel"
done

echo "Applied WSL HIL overlay to $target"
git -C "$target" status --short
