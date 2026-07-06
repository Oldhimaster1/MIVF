# MIVF UI Slice Prompt

Use the saved MIVF maintainer workflow.

Task:
Implement exactly one UI-only feature slice:
{{TASK}}

Allowed:
- source/main.c visual draw functions
- color and spacing adjustments
- labels and separators
- read-only status display

Forbidden:
- input logic changes
- settings semantics changes
- browser scanning/loading logic changes
- audio/NDSP/IA4M/PC16
- MoFlex/video decode
- stream/ring/seek logic
- encoder changes

Hard stop:
If patch touches more than one file or exceeds about 80 changed lines, stop and ask.
