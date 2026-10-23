# Actual validation

## Phase 2, Beryl 0.2.0 (2026-10-23)

The same WSL/GCC 15.2.0/CMake 4.2.3/GnuCOBOL 3.2.0 environment and local compiler extraction described below were used. Phase 1 was committed as `74f9b74` before Phase 2 began. Commands were the same Debug, sanitizer and Release build/test commands below, with the expanded CMake suite. All builds used warnings as errors.

| Configuration | Final actual result |
| --- | --- |
| Debug with GnuCOBOL | 8/8 CTest suites passed, 8.61 s |
| Debug with ASan/UBSan and GnuCOBOL | 8/8 suites passed, no sanitizer findings, 5.77 s |
| Release without cobc on PATH | All five C suites passed; all three COBOL suites explicitly SKIPPED, 2.78 s |

The eight suites are `records`, `generation`, `reports`, `core`, `acceptance`, `cobol_acceptance`, `cobol_codec`, and `cobol_generation`.

- Existing runner acceptance: all 27 C checks and three COBOL run/replay/boundary checks still pass.
- Parser/codec: supported nested groups/offsets and 26 malformed or unsupported declaration examples; independent DISPLAY/COMP-3 vectors for odd/even digits, both signs, zero/negative zero, scales, and 38-digit precision; round trips and invalid digits/signs/padding/short buffers rejected.
- Independent compiler vectors: a GnuCOBOL executable constructs three different 25-byte records using numeric literals; its bytes match the C encoder, and it accepts the encoded inputs.
- Generation: 512 API records validate; two independent 100-record CLI corpora and their metadata are identical for the same seed; another seed changes later records. Directed zero/min/max/±smallest-unit/negative-zero values are checked. All 100 malformed records are rejected by the codec; text-only malformed cases are rejected by length. Seed/count limits, the 64 MiB budget, existing output directories and symlink parents are tested.
- Generated data through COBOL: all 100 generated mixed DISPLAY/COMP-3 records pass GnuCOBOL NUMERIC checks and are echoed byte-for-byte, including negative zero.
- Reporting: all seven classifications have tested JSON/XML escaping and correct JUnit failure/error/skipped mappings. Independently, PowerShell's JSON parser and .NET XML parser accepted seven result JSONs, seven JUnit documents, and a 100-record generation manifest. Preserved format-check artifacts are in ignored `.beryl/validation-ovMtlp/`. These parser checks are additional local validation, not runtime or CTest dependencies.

Manual smoke commands also succeeded:

```sh
./build/beryl generate --copybook tests/cobol/account.cpy --count 100 --seed 12345 --out .beryl/phase2-corpus
./build/beryl generate --copybook tests/cobol/account.cpy --count 100 --seed 12345 --out .beryl/phase2-malformed --malformed
./build/beryl replay .beryl/smoke-pass/case-u2DxAt --results .beryl/phase2-replay
```

The first two commands report 100 valid/malformed records, respectively, with a 13-byte layout. Replay of the version 0.1 archive reports PASS and now emits version 0.2 JSON/JUnit reports. Generated corpora and replay evidence remain in the ignored `.beryl/` directories shown above. Raw CTest logs remain under each build's `Testing/Temporary/LastTest.log`. `/tmp` evidence can disappear when WSL shuts down.

Scope: strict declaration parsing, exact codecs, seeded generation, malformed corpus mode, and reports are implemented. COPY preprocessing, fixed-format parsing, alternative sign conventions, fuzz campaigns, differential execution and minimization remain unsupported. The next incremental feature would be an explicit reference-executable oracle, followed by bounded fuzz campaigns. The resource limits remain process controls rather than a security sandbox. Remote GitHub Actions execution is not claimed.

## Phase 1 baseline, Beryl 0.1.0

Executed on 2026-10-23 UTC (2026-10-22 America/Denver), on Ubuntu x86-64 in WSL. Toolchain: GCC 15.2.0, CMake 4.2.3, GnuCOBOL 3.2.0. All compilation used C17 and `-Werror`, in addition to the warning flags in CMakeLists.txt.

The host had no installed GnuCOBOL and passwordless sudo was unavailable. Actual COBOL tests used Ubuntu packages `gnucobol3`, `libcob4t64`, `libcob4-dev`, and `libgmp-dev`, downloaded with `apt-get download` and extracted with `dpkg-deb -x` into ignored `.tools/gnucobol`. No system installation was claimed or required. The local environment set PATH, LD_LIBRARY_PATH, COB_CONFIG_DIR, COB_COPY_DIR, COB_CFLAGS and COB_LDFLAGS to that extraction. `.tools/environment.sh` remains available in this workspace; source it inside WSL to rebuild the COBOL fixtures here. Standard installations do not need those overrides. Package retrieval was setup work; Beryl itself has no network dependency.

Commands actually executed, after sourcing that local environment for the first two builds:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=-Werror
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure -V

cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DBERYL_SANITIZERS=ON -DCMAKE_C_FLAGS=-Werror
cmake --build build-asan --parallel 4
ctest --test-dir build-asan --output-on-failure -V

# Separate shell without the locally added cobc on PATH:
cmake -S . -B build-no-cobol -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-Werror
cmake --build build-no-cobol --parallel 4
ctest --test-dir build-no-cobol --output-on-failure -V
```

| Configuration | Actual final result |
| --- | --- |
| Debug, warnings as errors | 3/3 CTest suites passed; 27 C acceptance checks and 3 COBOL checks, plus core checks; 5.72 s |
| Debug, ASan + UBSan, warnings as errors | 3/3 suites passed; same checks; no sanitizer findings; 4.31 s |
| Release, no cobc available | core and all 27 C acceptance checks passed; COBOL suite explicitly SKIPPED; 2.69 s |

The crashing target fixture is deliberately built without sanitizers so its actual OS signal is observable; the harness, CLI, and test drivers are instrumented. Raw CTest logs remain in each build directory under `Testing/Temporary/LastTest.log`. Those logs are generated artifacts, excluded from Git. The GitHub Actions workflow has been added but has not been run remotely.

## Required acceptance criteria

| Requirement | Executed check and observed outcome |
| --- | --- |
| 1. Valid COBOL golden output | `accounts`, fixed SEQUENTIAL input `100`: PASS, exactly three output bytes `100` |
| 2. Boundary bug | `accounts_bug`, input `100`: FAIL_ASSERTION, output `101`, first differing offset 2 |
| 3. Missing output | C fixture `M`: FAIL_ASSERTION; diagnostic names OUTPUT.DAT; `missing_output=1` |
| 4. Nonzero exits | C fixture returning 7: assertion by default, PASS with `--expected-exit 7` |
| 5. Crashing target | SIGSEGV and abort fixtures: CRASH; Beryl returns normally and continues subsequent tests |
| 6. Timeout and descendants | Busy loop: TIMEOUT; forked delayed writer: TIMEOUT and no descendant marker after waiting 1.2 s |
| 7. Binary preservation | Input/output including NUL, 0xff and 0x80: archived output is byte-exact |
| 8. Replay | Identical input digest and binary result; COBOL replay passes; archived environment and literal argv restored even after current environment changes |
| 9. Truncation | Flooding fixture: stdout/stderr/output truncation flags set; captured prefix matches golden prefix but remains FAIL_ASSERTION |
| 10. Controlled harness writes | Traversal and absolute names refused; symlink result root refused; output leaf/parent symlinks refused; exclusive writes tested |

Additional checks cover independent SHA-256 vectors (empty, `abc`, a million `a` bytes), streamed file hashing, CPU and file resource limits, oversized input rejection, UNVERIFIED without a golden oracle, exec failure, cleanup after an early leader exit, literal shell metacharacters, literal `--help` as a target argument, archive digest tampering, target hash changes, and deliberate target override.

## Manual end-to-end smoke checks

The following commands were also executed. Compiler provenance was explicitly supplied on the run commands:

```sh
./build/beryl --version
./build/beryl run --exec ./build/accounts --input tests/records/boundary.bin \
  --expected tests/expected/boundary.out --compiler-version GnuCOBOL-3.2.0 --results .beryl/smoke-pass
./build/beryl run --exec ./build/accounts_bug --input tests/records/boundary.bin \
  --expected tests/expected/boundary.out --compiler-version GnuCOBOL-3.2.0 --results .beryl/smoke-fail
./build/beryl replay .beryl/smoke-pass/case-u2DxAt --results .beryl/smoke-replay
```

Observed normal output:

```text
beryl 0.1.0
PASS: exit status and required output match golden bytes
exit=0 signal=0 duration_ms=25 stdout_truncated=0 stderr_truncated=0 output_truncated=0
FAIL_ASSERTION: binary difference at offset 2: expected 0x30, actual 0x31 (lengths 3/3)
exit=0 signal=0 duration_ms=26 stdout_truncated=0 stderr_truncated=0 output_truncated=0
```

The good run returned 0; the faulty run returned 1. Inspectable evidence remains under `.beryl/smoke-pass/case-u2DxAt` and `.beryl/smoke-fail/case-9MnjD9`; replay also passed. Archives contain the full target environment and are intentionally ignored by Git.

At the Phase 1 checkpoint, limits were Linux only, one input/output pair, golden-byte comparisons only, no strong sandbox or total disk/process cap, and replay depending on an unchanged external runtime. No Phase 2 code had been started at that checkpoint; subsequent implementation and validation are recorded above.
