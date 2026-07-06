# MIVF Maintainer Prompt

Use the saved MIVF maintainer workflow.

Before patching:
1. Read AGENTS.md.
2. Read docs/AI_CONTEXT.md.
3. Inspect git status and current diff.
4. Identify active path.
5. Create backups.

Constraints:
- One feature only.
- Active path only.
- Build after patch.
- Do not commit automatically.

Required output:
1. Active path and current behavior.
2. Proposed smallest patch and risk level.
3. Exact files/functions changed.
4. Safety grep result.
5. Build result.
6. Runtime checklist.
7. Commit guidance.
