# Reference execution

```sh
beryl run --exec ./candidate --reference ./baseline --input case.bin --results .beryl/diff
```

Both executables receive the same archived input bytes, literal arguments, environment and resource limits in separate private workspaces. The baseline's own evidence is under the candidate result's `reference/` directory. A difference is `REGRESSION_CANDIDATE` (exit 1), not an automatic bug claim. Matching selected observations are PASS relative to the reference; they do not establish business correctness.

`--compare output` (default) compares exit status/signal and the required output bytes. `--compare status` compares only exit status/signal and allows absent files. `--compare all` also compares stdout/stderr. Matching nonzero statuses are allowed in reference mode; `--expected-exit` is the single-executable exit contract, not an additional reference oracle. Matching crashes remain CRASH. A timeout on either side is TIMEOUT, because equality cannot be established. A selected truncated capture is HARNESS_ERROR rather than a successful comparison. Loader/harness failures are errors, not regressions.

Bytes are exact by default. `--normalize-crlf 1` explicitly compares CRLF as LF for the selected file/streams. Raw archived bytes and their hashes remain unchanged. Policy, normalization, differing observation bitmask, reference path/hash and both executions' evidence are recorded. `--expected` and `--reference` are alternative oracles and cannot be combined.

`beryl replay RESULT` reruns both executables with the same comparison configuration and verifies both executable digests. Explicit `--exec PATH` / `--reference PATH` overrides permit intentional executable changes. The original runtime/external environment must still be available. This is not a security sandbox.
