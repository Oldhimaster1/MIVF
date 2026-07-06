# MIVF UI Feature Prompt

Use the saved MIVF maintainer workflow.

Task:
Implement exactly one UI feature slice.

Allowed:
- source/main.c active draw functions
- color, spacing, text, separators, badges
- read-only display of existing state

Forbidden unless explicitly approved:
- input handling changes
- settings semantics changes
- browser scanning or preview loading logic
- audio/NDSP/IA4M/PC16
- MoFlex/video decode
- stream/ring/seek-index logic
- encoder changes

Process:
1. Inspect active path and report plan.
2. Create backups.
3. Patch one small feature.
4. Safety grep.
5. Build.
6. Stop.
7. Do not commit automatically.
