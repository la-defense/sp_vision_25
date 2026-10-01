# Task tracker: local TODO

TODO.md is the only authoritative task state. Do not add a second status field to specs, ticket details, maps, PRs, or reports.

Every open checklist item has one VIS25 ID, one category, one state, and a Chinese acceptance condition:

    - [ ] **VIS25-####** [bug|enhancement] [needs-triage|needs-info|ready-for-agent|ready-for-human] Title. 验收：observable completion condition.

Specs live in docs/workflow/specs/<feature>.md; ticket details live in docs/workflow/tickets/<ID>.md; decision maps live in docs/workflow/maps/. These files may describe status history but do not own current status.

Cross-repository tasks have one linked TODO item in each repository. Firmware status remains in MC-02/.Doc/TODO.md; vision status remains here. Triage, task planning, and review follow the MC-02 shared skill instructions.
