# MIVF One Feature Safe Patch

Use the saved MIVF maintainer workflow.

Task:
{{TASK}}

Rules:
- One feature only.
- Inspect first.
- Back up before patching.
- Patch active path only.
- Build after patch.
- Do not commit automatically.

Before patching, report:
1. Active path.
2. Existing state.
3. Inactive functions to avoid.
4. Proposed patch.
5. Risk level.

After patching, report:
1. Files and functions changed.
2. Behavior added/removed/preserved.
3. Safety grep result.
4. Build result.
5. Test checklist.
6. Commit guidance.
