# AGENTS.md - MIVF Maintainer Rules

You are working on MIVF.

Before patching:
1. Run git status -sb.
2. Run git log --oneline --decorate -8.
3. Run git diff --stat.
4. Read docs/AI_CONTEXT.md.
5. Identify active runtime path.
6. Create backups.

Never patch inactive code.
Never assume old branch/stash context is current.

Do not modify these subsystems unless the task explicitly requires it:
- audio startup and audio.ready
- NDSP setup
- IA4M/PC16 runtime decode
- MoFlex playback
- video decode
- mivf_io_ring.c
- mivf_stream.c and mivf_stream.h
- encoder logic
- seek-index logic
- input remapping

Patch one feature at a time.

After patching:
1. Run safety grep.
2. Build or syntax-check.
3. Report exact code changes.
4. Do not commit automatically.

Problem-solving style:
- Find active path first.
- Preserve known-good behavior.
- Prefer smallest safe patch.
- Stop and report if a change becomes risky.
