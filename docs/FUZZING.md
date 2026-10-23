# Seeded campaigns

```sh
beryl fuzz --exec ./candidate --reference ./baseline --copybook record.cpy \
  --runs 1000 --seed 12345 --results .beryl/campaign
beryl fuzz --exec ./target --input seed.bin --mode raw \
  --runs 100 --seed 42 --results .beryl/raw
```

`--mode valid` (default) generates representable boundary/random records from the copybook. Optional `--input` starts from a valid same-layout record and replaces one selected field at a time with a generated field. `--mode invalid` corrupts a numeric digit or record length deliberately. `--mode raw` requires an input seed and flips a bit, deletes a byte or inserts a byte without layout promises; its first case executes the seed unchanged. All modes use the explicit uint64 seed and versioned SplitMix64 PRNG. The same version/configuration produces the same input sequence; timings, cwd names and external target behavior can vary.

Run options such as literal arguments, expected exit, golden file, reference executable, comparison policy, capture/file/CPU limits, and declared names are accepted. A no-oracle successful execution remains UNVERIFIED, not PASS. The golden/seed/copybook are snapshotted into the new campaign directory. Every case includes its seed, case index, layout digest and exact operation in the replay manifest and JSON. `beryl replay <case-results-directory>` reruns the exact bytes and configuration; no regeneration is needed.

Failing cases are executed again before signature grouping. A signature includes the outcome, assertion category, signal/status, comparison channels, raw output and stream hashes, and truncation/missing flags. Matching signatures are evidence groups, **not unique root causes**. Cases that change signature on confirmation are reported as unstable; cases without enough budget to confirm are separately unconfirmed. All original and confirmation evidence remains available under `run-NNNNNN/results/`. `campaign.json` records each case and a representative directory/count for each reproducible group.

The count is 1..10000. `--budget-ms` is 1..600000 (default 60000); per-execution timeouts are reduced to leave room for reference/confirmation runs. `--max-total-bytes` is 1..1073741824 (default 64 MiB) and bounds observed archived raw fixtures/captures/environment between cases; a final in-flight case/confirmation can overshoot the threshold. Metadata, retained work files, compiler artifacts and target-created additional files are not counted. Resource budgets are best-effort process/evidence controls with filesystem/scheduling overhead, not an OS security sandbox or an aggregate filesystem quota. Use external isolation/quotas for hostile targets.

Summary exit codes: 0 completed without failures (possibly unverified), 1 completed with observed failures, 2 incomplete budget/execution/harness/usage error. Budget exhaustion writes a summary with the actual completed count and stop reason, without claiming all requested runs completed. Loader/harness errors stop the campaign rather than being treated as fuzz discoveries.
