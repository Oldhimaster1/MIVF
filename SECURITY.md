# Security

## Reporting a vulnerability

Report a security concern by opening a GitHub issue **without exploit details**, or
contact the maintainer directly through the repository's contact information. MIVF does
not yet have a dedicated private disclosure mailbox or bug-bounty program — for anything
sensitive, say so in the issue title and wait for a maintainer to open a private channel
before sharing specifics.

See docs/project/third-party-notices.md for bundled/derived components (FFmpeg,
devkitPro/libctru) that carry their own separate security-disclosure processes upstream.

## Scope

MIVF is homebrew software for a personal, offline media workflow: a desktop
encoder/toolkit that converts user-supplied media into a `.mivf` file, and a 3DS
application that plays it back from local SD storage. There is no server component, no
account system, and no network service exposed by either half. The realistic security
surface is: (1) parsing of untrusted/malformed `.mivf`, sidecar, and project files, and
(2) the desktop toolkit's own local file handling.

## Release integrity

Every published release artifact ships with a `SHA256SUMS.txt` covering the exact
`.3dsx`/`.cia` binaries. Verify a download against that manifest before installing.
Source and binaries are built from the same tagged commit — see the release notes for the
exact commit and build ID.
