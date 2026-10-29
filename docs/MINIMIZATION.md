# Reproducible test-case reduction

```sh
beryl minimize .beryl/results/case-XXXXXX --out .beryl/reduced
beryl minimize .beryl/results/case-XXXXXX --copybook record.cpy \
  --max-tries 200 --budget-ms 30000 --out .beryl/reduced-record
```

The input must be an archived assertion, regression candidate, crash or timeout. Input, oracle, environment and executable hashes are checked, and two fresh baseline executions must agree with the archived failure signature. A changed/nonreproducible failure is refused. `--exec` and `--reference` deliberately select replacement executables, but the recorded failure still has to reproduce. PASS/UNVERIFIED and harness/loader errors are not reduction candidates.

Without a copybook, the reducer removes byte chunks and then attempts zero-byte simplifications. With a copybook, it simplifies text fields to spaces and numeric fields toward zero/±1/±the smallest unit while preserving record layout. `--byte-fallback` explicitly permits raw deletion after field simplification. Every accepted change must decrease length or a deterministic byte-complexity score; it must also preserve the configured failure predicate and reproduce on a second execution. Original and reduced raw bytes are saved separately, with SHA-256 hashes, every trial's execution evidence, and `minimization.json` pointing to a confirmed result directory.

`--predicate classification` (default) retains the outcome, assertion category, signal and differing comparison channels; status differences retain the relevant exit values. `--predicate signature` additionally retains exact observed output/stream signatures. Thus an output mismatch cannot silently turn into a missing-file assertion, loader failure or different signal. Signature mode can prevent shrinking when output legitimately depends on removed input. Equality of signatures is reproducibility evidence, not proof of a unique root cause.

`--max-tries` is 1..10000 (default 200), counting candidate executions and confirmations; two initial baseline executions are separate. `--budget-ms` is 1..3600000 (default 30000) and must initially allow two complete baseline timeout windows. The original per-execution limits are retained. Work is not started if its full timeout window cannot fit in the remaining budget. Scheduling/filesystem overhead is outside that best-effort bound. The stop reason distinguishes completed search, try limit and time budget. Exit 0 means the retained case has confirmed evidence under the chosen predicate, not that it is globally minimal; unsupported input, changed evidence or operational failure exits 2. A bounded search may leave the input unchanged.
