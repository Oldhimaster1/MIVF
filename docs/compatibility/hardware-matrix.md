# Hardware Matrix

Model-specific, evidence-gated. Do not infer compatibility across models from a single
tested console, and don't extrapolate from emulator testing to any physical hardware
claim — see [Validation Method](validation-method.md) for the full status vocabulary.

| Console model | Available for testing | Physically tested | Build ID tested | Result | Performance result | Known limitations |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| New Nintendo 3DS | Unknown | *pending detail* | *pending* | *pending* | *pending* | *pending* |
| New Nintendo 3DS XL | Unknown | Not tested | — | — | — | — |
| New Nintendo 2DS XL | Unknown | Not tested | — | — | — | — |
| Nintendo 3DS | Unknown | *pending detail* | *pending* | *pending* | *pending* | *pending* |
| Nintendo 3DS XL | Unknown | Not tested | — | — | — | — |
| Nintendo 2DS | Unknown | Not tested | — | — | — | — |

Two rows above are marked *pending detail* rather than a flat "not tested," because a
preliminary physical-hardware Showcase run was reported successful on **one** real
console — but the exact model (whether New or Old 3DS family), build ID, and artifact
hash have not yet been recorded. See [Project Status](../status.md) for exactly what's
outstanding. Once recorded, exactly one row above will move to a real, dated result; the
rest stay `Not tested` until independently run.

## Rules this table follows

- Do not claim general "3DS compatibility" from emulator testing.
- Do not claim Old-3DS-family compatibility without physical testing of that specific
  family.
- Do not extrapolate from one physical model to every member of its family.
- Every hardware result is tied to an exact MIVF build ID and artifact hash.
- Functional success and acceptable performance are recorded as separate columns, not
  conflated.
- Logs and footage supporting each result are preserved — see
  [Reporting Results](reporting-results.md).
