# Vision work queue

Task state in this file is authoritative. Detailed designs and evidence may live under docs/workflow but must link back here.

Central specification: [MC-02 and vision engineering/learning workflow](../mc-02/docs/workflow/specs/engineering-learning-workflow.md).

## Communication and safety

- [x] **VIS25-0001** [bug] Replace the SP frame upper-bound checks with exact 43-byte state and 29-byte command assertions. 验收：both compile-time checks fail if either packed wire structure changes size. 验证：CPU build compiled both static assertions.
- [x] **VIS25-0002** [bug] Make Gimbal sends report full-frame write success and count every HIL transmission, including stale-link neutral frames. 验收：short writes, exceptions, and every neutral or aim send are reflected in the final summary; wire bytes and CRC remain unchanged. 验证：all HIL send paths use counted wrappers; protocol tests pass.
- [ ] **VIS25-0003** [bug] [ready-for-agent] Bound startup waiting for the first valid MC-02 status frame and exit cleanly if the link never comes online. 验收：missing, corrupt, and valid first frames have bounded, tested outcomes with no blocked process.
- [ ] **VIS25-0004** [bug] [ready-for-agent] Validate mode, finite aim values, fresh MC-02 state, and control bounds before emitting an aim command. 验收：invalid or stale inputs produce a neutral frame with fire disabled.
- [ ] **VIS25-0005** [bug] [ready-for-agent] Verify neutral control on no target, camera timeout for every enabled camera backend, serial loss, signal, and normal exit. 验收：each host-simulatable scenario produces a no-fire neutral command; serial loss is reflected in TX failure counts; MC-02 timeout clears stale commands if transmission is impossible.

## Build and tests

- [x] **VIS25-0006** [enhancement] Add a CPU-only build configuration that does not require industrial-camera SDKs while preserving the WSL/HikRobot preset. 验收：CI can build required executables with both camera SDK backends disabled. 验证：WSL Release CPU preset configured and built without either SDK.
- [x] **VIS25-0007** [enhancement] Add CTest coverage for SP structure sizes, CRC, aim/neutral command construction, malformed frames, and revoked control. 验收：tests run in a Linux CPU environment without hardware. 验证：Release-mode protocol/control CTest passed, including no-target, stale-link, invalid-plan, and non-finite inputs.
- [x] **VIS25-0008** [enhancement] Add a headless demo-video replay check that reports target frames and finite aim outputs. 验收：CI processes a bounded demo segment with at least one target and finite aim output and opens no GUI. 验证：151 frames, 122 target frames and 122 finite aim frames; no GUI.
- [ ] **VIS25-0009** [enhancement] [ready-for-human] Add host CI on main and pull requests for the CPU build, protocol tests, and offline replay. 验收：the Actions workflow is green on a pull request and fails on any build, CTest, or replay failure.

## Hardware integration

- [ ] **VIS25-0010** [enhancement] [ready-for-human] Automate manually dispatched bare-board HIL using exact firmware and vision commit SHAs on a restricted Windows runner. 验收：report records both SHAs, builds, frames, aim/neutral counts, RX and CRC counter deltas, and process cleanup.
- [ ] **VIS25-0011** [enhancement] [ready-for-human] Verify Windows MVS camera and virtual COM-to-WSL PTY bridges end-to-end. 验收：at least 100 camera frames reach the recognizer, and bidirectional SP status/command frames are verified through the WSL serial bridge. 验证：camera bridge evidence is recorded in the [local WSL deployment record](../mc-02/docs/本机WSL自瞄联调部署记录.md); the serial bridge still needs end-to-end WSL evidence.
- [ ] **VIS25-0012** [bug] [needs-info] Diagnose HIL-10 CMSIS-DAP I/O errors and distinguish probe failure from MCU reset and counter reinitialization. 验收：short SWD snapshots do not reset or halt the MCU and before/after counters prove that the target stayed up. 继续前需补充原始长会话命令和错误日志；当前通用 attach-only 脚本会停核。

## Engineering workflow

- [x] **VIS25-0013** [enhancement] Connect the vision task queue, CPU CI, and shared Chinese learning workspace to the MC-02 workflow. 验收：the central spec and teaching index are linked, CPU CI covers protocol and headless replay, and the shared C++ exercises pass; firmware-side counterpart is **MC02-0030** ([MC-02 TODO](../mc-02/.Doc/TODO.md)). 验证：双向任务链接、中央规格/学习索引链接、CPU CTest 与四组共享练习通过。
- [ ] **VIS25-0014** [enhancement] [ready-for-human] Create la-defense's public vision fork and private HIL repository, then configure a restricted Windows runner for manual exact-SHA tests. 验收：only trusted in-repository workflows execute on the runner; reports record both SHAs, no-fire frames, RX/CRC deltas, build/flash status, and reset evidence; firmware-side counterpart is **MC02-0031** ([MC-02 TODO](../mc-02/.Doc/TODO.md)). 已创建 [视觉 fork](https://github.com/la-defense/sp_vision_25) 和 [私有 HIL 仓库](https://github.com/la-defense/mc-02-hil)，runner 尚未配置。

## Startup timeouts

- [ ] **VIS25-0015** [bug] [ready-for-human] Verify silent-drop TCP connect timeout for the Windows MVS frame bridge and confirm HIL revokes control. 验收：fault-injected silent drop returns within the shared two-second connect deadline, emits/retries neutral control as required, and is recorded in the HIL log. 验证：CPU build and refused-local-port regression test pass; the silent-drop path still needs deterministic host or HIL evidence.
