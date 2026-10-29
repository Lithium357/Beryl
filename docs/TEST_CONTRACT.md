# Execution and replay contract

`beryl run` resolves the executable to an absolute path, reads bounded regular input/golden files, and creates a fresh `case-XXXXXX` directory with `mkdtemp`. It archives the raw fixtures before execution, copies input bytes into `work/<input-name>`, and invokes `execve` with `work/` as cwd. Stdin is `/dev/null`. No COBOL source is changed. Paths and arguments have a 4095-byte limit; there are at most 64 extra arguments.

Relative input/output names can include directories. They cannot be empty, absolute, identical, contain `.`/`..` components, empty components, backslashes, colons, or control bytes. Managed result-root components cannot be symlinks or `..`. Directory descriptors and `openat` with `O_NOFOLLOW`/exclusive creation protect managed writes and output capture. Sources and executable are supplied by the caller; executable symlinks are deliberately resolved. Sources must be regular files (a final input/golden symlink is refused). Output directories are prepared before execution; the required output file is not pre-created.

The expected exit is zero unless `--expected-exit 0..255` is supplied. Outcome precedence is harness error, wall timeout, execution/setup error, terminating signal, exit assertion, missing output, then file comparison. CPU/file resource-limit signals are CRASH, with the actual signal recorded. An OS loader failure is EXEC_ERROR; a program deliberately returning 127 is an ordinary exit assertion. A source/flag validation error before execution exits 2 and might have no archive.

| Outcome | Meaning | CLI status |
| --- | --- | --- |
| PASS | Golden file and expected exit match | 0 |
| UNVERIFIED | Expected exit matches and required output exists, but no golden file | 0 |
| FAIL_ASSERTION | Exit mismatch, missing output, byte/length difference, or truncated golden comparison | 1 |
| CRASH | Child terminated by a signal | 1 |
| TIMEOUT | Wall limit reached; process group killed | 1 |
| EXEC_ERROR | Child setup or exec failed, errno recorded | 2 |
| HARNESS_ERROR | Beryl cannot safely prepare, capture, or save evidence | 2 |

Comparison is binary with a zero-based first differing byte offset, expected/actual hex bytes, and lengths. Length-only differences report the first absent offset. Output contents are not normalized. Empty files are valid. stdout/stderr have independent bounded captures and flags, and are observations rather than comparison oracles. A file longer than `--max-capture-bytes` is archived as a prefix with `output_truncated=1` and `output_hash_scope=captured-prefix`; its hash describes the stored bytes. It cannot PASS a golden comparison. A golden/input file above its configured cap is rejected before execution, without silently using a prefix.

Limits are integers: wall time 1..600000 ms, CPU 1..3600 seconds, and input/capture/per-file sizes 1..67108864 bytes. Beryl uses a monotonic clock with approximately 10 ms polling granularity. Files have a per-file RLIMIT_FSIZE; pipes are continuously drained while retaining only the capture prefix. Limits are not a security boundary. Targets should remain in their inherited process group and close output files before exit. Beryl kills remaining group members after leader exit but cannot guarantee cleanup of descendants that deliberately escape the group. Partial output after crash/timeout is retained when safely readable. Retained work files can be larger than archived captures.

## Manifest and replay

The stable Phase 1 format is LF-separated `key=value` text starting with `format=beryl-1`. Numeric fields are decimal. `target`, `input_name`, `output_name`, `compiler`, `diagnostic`, `argv0`, each `argN`, and `work_directory` are lowercase hex-encoded byte strings; other fields are plain ASCII. The executable path is `argv0`; `argc` is the number of *additional* arguments, numbered `arg0` onward. `work_directory` is the original absolute execution cwd, while replay intentionally creates a new private cwd. Digests are SHA-256 in lowercase hex. `seed=none` records that Phase 1 has no randomized input generation. Version is Beryl's version; `--compiler-version TEXT` records user-supplied target compiler provenance. Beryl cannot infer the original COBOL compiler from a precompiled executable.

The manifest is written after captured artifacts as a completion marker. Filesystem failures may leave an incomplete directory without a manifest; Beryl reports HARNESS_ERROR and exits 2 rather than treating incomplete evidence as a completed case. Results are not a transactional or power-loss-durable database.

`environment.bin` stores the exact environment supplied to `execve`, with NUL-separated entries. Replay validates input, golden, and environment digests, restores arguments, environment and limits, and generates a new result directory. It ignores the current process environment for target execution. It verifies that the executable hash still matches. `beryl replay DIR --exec PATH` explicitly permits another executable and records its new path/hash. `--results DIR` selects the new archive root; other replay overrides are rejected. Malformed/incomplete manifests and corrupted archived bytes are rejected.

Replay is for archives you trust, not an import format for arbitrary third-party commands. The archive does not bundle the executable, loader, shared libraries, external files, OS state or runtime. Reproduce those dependencies separately. The input bytes and configuration are preserved; durations, absolute cwd, and behavior depending on external state need not be identical. Compiler provenance is informational. Manifest/fixture hashes detect accidental changes; they are not authenticated signatures. Do not edit archives concurrently with execution or replay.

## JSON and JUnit reports (version 0.2)

Every completed archived run now also contains `result.json` (`schema=beryl-result-1`) and `junit.xml`. JSON includes classification, diagnostic, hashes, argv, target/compiler provenance, relative input/output names, original cwd, status/signal/duration, limits, and independent stream/output truncation fields. A missing oracle is `expected_sha256:null`; generation is separate, so a run has `seed:null`. Environment contents remain in the private binary archive; JSON contains only their digest. All reports are saved before the line-oriented completion manifest. Early errors that have no archive might only appear on the terminal.

JSON paths/arguments are byte strings: printable ASCII is literal, JSON-sensitive characters are escaped, and other bytes are emitted as U+00XX escapes. Recover non-ASCII POSIX path bytes by mapping those code points back to bytes rather than assuming UTF-8 decoding. XML replaces control/non-ASCII bytes with visible `\\xNN` text and escapes XML entities. Reports do not embed binary captured stream contents.

JUnit contains one suite and one testcase per archived run. PASS has no failure element; UNVERIFIED is skipped with a no-oracle explanation; assertion/crash/timeout outcomes are failures; execution/harness errors are errors. Durations are seconds with millisecond precision, formatted using integer arithmetic. Suites can be collected by CI using the result-directory glob `**/junit.xml`.

The replay format remains `beryl-1`, so version 0.1 archives can still be replayed. JSON/XML summaries are additional outputs; replay reads the validated line-oriented manifest and raw fixtures.

## Declared file sets (version 0.5)

The primary-only format remains `beryl-1`. Archives declaring additional files use `beryl-2`; both are supported by the current reader. Additional inputs and golden outputs are separately snapshotted/hashed, all declared paths are collision-checked, and replay restores the full configuration. See FILE_SETS.md for flags, caps, archive names, reference behavior and reduction semantics. Older clients must reject `beryl-2` rather than produce results with an incomplete fixture set.
