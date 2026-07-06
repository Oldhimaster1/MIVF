# MIVF Encoder Tuning Prompt

Use the saved MIVF maintainer workflow.

Scope:
Encoder-side tuning only.

Do not modify player runtime path unless explicitly requested.

Process:
1. Inspect current encoder arguments/profile.
2. Propose smallest tuning slice.
3. Apply minimal change.
4. Run python -m py_compile encode_mivf.py.
5. Report exact flags/behavior changed.
6. Do not commit automatically.
