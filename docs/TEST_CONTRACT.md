# Phase 1 test contract

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
