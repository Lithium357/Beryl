# Changelog

## Unreleased

- Expand real-compiler numeric storage checks from three to 2,002 records per profile, including every integer from -999 through +999; exhaustively validate 196,608 packed patterns and 512 DISPLAY bytes against the documented codec conventions.
- Add an optional Rocket Visual COBOL 11.0 native compiler profile, checking golden output/replay, exact numeric storage, generated records and callable-driver status handling.
- Validate the actual IBM COBOL for Linux 1.2.0.9 evaluation with source-format conversion, explicit QSAM fixed-record runtime selection and portable callable-driver status storage.
- Add `--unsigned-packed-sign f|c` to generation, campaigns and structured minimization; decode C/F unsigned packed fields and record the selected output convention in JSON. Default output remains F.
- Correct the initial IBM trial-expiration finding: normal evaluation-package initialization resolves the incomplete-extraction diagnostic.

- Add an optional real-compiler CTest matrix for GCC COBOL and GnuCOBOL vendor/standards dialects, covering binary output, replay, storage codecs, generated records and explicit callable drivers.
- Extend the matrix to isolated GnuCOBOL 3.1.2, opensource COBOL 1.5.2J and Java-targeting COBOL 4J, with per-profile runtime environments and explicit Java program-status handling.
- Enable GnuCOBOL dialect checks in the existing CI workflow; document actual proprietary compiler access blockers separately from validated open-source results.

## 0.5.0

- Reduce reproducible archived cases with bounded trials, preserved failure categories/signatures, optional copybook-aware simplification and retained originals.
- Generate explicit single-record BY REFERENCE COBOL drivers; preserve called return codes.
- Parse a deliberately limited fixed-format layout subset and optionally resolve COPY through the GnuCOBOL preprocessor.
- Build/collect GnuCOBOL 3.2/GCC coverage only for verified mapped COBOL source lines.
- Support declared additional binary input/output files across execution, golden/reference comparison, archives, replay, campaigns and reduction.
- Keep IBM/z/OS compatibility explicitly unvalidated pending platform access.

## 0.4.0

- Seeded valid/invalid/raw mutation campaigns, confirmation and observed failure signature groups.

## 0.3.0

- Replayable reference-executable comparison with explicit status/output/stream and CRLF policies.

## 0.2.0

- Strict free-format record layouts, exact DISPLAY/COMP-3 codecs, seeded boundary generation and JSON/JUnit reports.

## 0.1.0

- Linux C17 batch runner, bounded binary evidence, golden comparisons and replay.
