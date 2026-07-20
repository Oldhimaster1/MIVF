# Development Preview Status

This repository is an in-development public source preview.

## Validation completed

- Public documentation builds successfully with MkDocs strict mode.
- Public media and benchmark SHA-256 manifests verify.
- Python source syntax validation passes.
- Automated test result: 243 passed, 4 skipped, 1 failed.

## Known test limitations

One encoder test currently expects a missing
`read_mivf_first_page_offset()` helper. The underlying first-page-offset
parsing is present elsewhere in the encoder, but this specific test/API
mismatch remains unresolved.

The offscreen GUI smoke script currently depends on development-only image
fixtures that are not included in this public source snapshot.

## Binary status

No `.3dsx`, `.cia`, or Windows Toolkit binary is asserted to have been built
from this public commit yet. Matching binary artifacts will be published only
after they are rebuilt and validated from the tagged public source state.

## Security scan

Focused disclosure scans passed. An entropy-based secret scan remains pending.
