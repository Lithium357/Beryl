# Beryl

Beryl runs existing Linux batch executables against binary fixtures, checks an explicit exit status and optional golden output, and archives enough evidence to replay the case. It also generates seeded boundary records from a strict subset of free-format copybooks. The tool and its tests are C17; COBOL is used only for integration fixtures. No network, accounts, telemetry, or source modifications are involved in running it.

On Linux, install a C compiler, CMake 3.16+, and optionally GnuCOBOL. For Ubuntu, the packages are `build-essential cmake gnucobol`. GnuCOBOL is needed to compile the supplied integration fixtures, but is not needed to build Beryl or run precompiled targets. On Windows, execute these commands inside WSL:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=-Werror
cmake --build build --parallel
ctest --test-dir build --output-on-failure -V

# Optional memory and undefined-behavior checks
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DBERYL_SANITIZERS=ON -DCMAKE_C_FLAGS=-Werror
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure -V
```

CMake compiles the COBOL fixtures with `cobc -x -free -o <output> <source>`. Without `cobc`, CTest reports the three COBOL suites as **Skipped**, and still runs the C tests. Assertions remain enabled in test executables in Release builds too. [Actual validation results](docs/VALIDATION.md) describe the tested environment.

```sh
./build/beryl run --exec ./build/accounts \
  --input tests/records/boundary.bin --input-name INPUT.DAT \
  --output-name OUTPUT.DAT --expected tests/expected/boundary.out \
  --compiler-version "GnuCOBOL 3.2.0" --results .beryl/results

# Use the result directory printed by the previous command:
./build/beryl replay .beryl/results/case-XXXXXX

# Deliberately buggy fixture: exits 1, reports 100 versus 101.
./build/beryl run --exec ./build/accounts_bug \
  --input tests/records/boundary.bin --expected tests/expected/boundary.out
```

Example terminal output (directory suffix and duration vary):

```text
PASS: exit status and required output match golden bytes
results: /.../.beryl/results/case-XXXXXX
exit=0 signal=0 duration_ms=10 stdout_truncated=0 stderr_truncated=0 output_truncated=0

FAIL_ASSERTION: binary difference at offset 2: expected 0x30, actual 0x31 (lengths 3/3)
results: /.../.beryl/results/case-YYYYYY
exit=0 signal=0 duration_ms=10 stdout_truncated=0 stderr_truncated=0 output_truncated=0
```

Each result contains `manifest.txt`, `result.json`, `junit.xml`, `input.bin`, optional `expected.bin`, `environment.bin`, `stdout.bin`, `stderr.bin`, and, when present, `output.bin`. `work/` is the private execution directory, retained for inspection. The manifest records SHA-256 digests, argv, resource limits, compiler provenance (or `unknown`), tool version, statuses, duration, and truncation flags. Text fields in the replay manifest are hex encoded; see the [test contract](docs/TEST_CONTRACT.md) for the exact format and replay behavior. **Environment archives can contain secrets**: keep result directories private and review them before sharing.

Generate a corpus without running a target:

```sh
./build/beryl generate --copybook tests/cobol/account.cpy \
  --count 100 --seed 12345 --out .beryl/corpus

# Explicitly opt into invalid storage or lengths in a separate corpus:
./build/beryl generate --copybook tests/cobol/account.cpy \
  --count 100 --seed 12345 --out .beryl/malformed --malformed
```

The output directory must be new. Each `case-000000.bin` file contains one raw fixed-length record. `generation.json` records the source digest, layout, PRNG version, seed, mutation (if any), and each file's digest. Generation supports PIC X, 9, S9 and implied decimal V with DISPLAY or COMP-3; numeric precision is bounded at 38 digits. It rejects unsupported clauses instead of guessing storage. [Record layout documentation](docs/RECORD_LAYOUTS.md) defines the exact subset, encoding assumptions, boundaries and limits. Generated records are structurally valid by default, without making business-validity claims.

Use `beryl --help`, `beryl --version`, or `beryl run --help` for all options. `--arg VALUE` passes a literal argument without a shell. `--expected-exit N` permits an intentional nonzero exit. Without `--expected`, a successful observation is `UNVERIFIED`, never `PASS`. Exit codes are **0** for PASS/UNVERIFIED, **1** for assertion/crash/timeout, and **2** for usage, execution, or harness errors. A missing declared output is an assertion failure, even without a golden file.

The default wall timeout is 2 seconds, CPU cap 2 seconds, input and capture caps 1 MiB each, and per-file cap 16 MiB. The child also has a 512 MiB address-space cap and no core dumps. Output truncation can never establish golden equality; stdout/stderr truncation is reported but does not invalidate a file oracle. Process groups are killed on timeout and after the leader exits.

**Safety:** targets run as your user. Private directories, safe relative paths, process groups, and resource limits are **not a security sandbox**. A malicious target can access other files, the network, or escape its process group. Use appropriate OS/container isolation for untrusted programs. Beryl refuses symlink components on managed write paths and declared output capture; do not concurrently modify archives or result roots. Workspaces are retained, so disk use across repeated invocations and target-created additional files requires external management. The limits do not bound aggregate file count, total disk use, or fork count.

The implemented scope is the Phase 1 runner plus Phase 2 record parsing, codecs, generation and JSON/JUnit reports. Target execution still supports one input, one required output, a golden-byte oracle, and replay. Reference execution, fuzz campaigns, minimization, coverage, and IBM/z/OS support are future work. See [compatibility](docs/COMPATIBILITY.md) and the [one-page COBOL primer](docs/COBOL_PRIMER.md). A difference indicates a contract mismatch; it does not prove the golden data represents correct business behavior.

The repository's existing `LICENSE` has been preserved without selecting or replacing a license.
