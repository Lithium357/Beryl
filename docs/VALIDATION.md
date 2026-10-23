# Actual Phase 1 validation

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

Known limits: Linux only, one input/output pair, golden-byte comparisons only, no strong sandbox or total disk/process cap, and replay depends on an unchanged external runtime. Scope stops at Phase 1. The next smallest requested step would be a strict PIC X/PIC 9 DISPLAY parser with independently checked encoding vectors; no Phase 2 code has been started.
