# Vision repository guide

This repository is the sp_vision_25 upper computer. Follow its task and design references before changing runtime behavior.

- Task state lives only in [TODO.md](TODO.md). The shared task contract is in [docs/agents/issue-tracker.md](docs/agents/issue-tracker.md).
- Vision terminology and decisions live in [GLOSSARY.md](GLOSSARY.md) and [docs/adr/](docs/adr/).
- The Chinese learning-note index for both repositories is maintained in the MC-02 repository at https://github.com/la-defense/mc-02_framework/blob/main/docs/%E5%AD%A6%E4%B9%A0%E7%AC%94%E8%AE%B0/README.md.
- Agent skills live only in the MC-02 repository. Start agent work from that repository and switch the code worktree here.
- Bare-board HIL runs only with actuators disconnected. All HIL commands must force fire off and send neutral control on loss of target, link, or process exit.
- Keep the existing SP wire format: MC-02 state frames are 43 bytes and vision commands are 29 bytes, both CRC16/X25.

Use one task branch and PR. Commit completed work before review.
