# Declared file sets

```sh
beryl run --exec ./batch --input cases/transactions.bin --input-name INPUT.DAT \
  --file CONFIG.DAT=cases/config.bin \
  --output-name OUTPUT.DAT --expected golden/transactions.out \
  --output reports/AUDIT.DAT=golden/audit.out --output STATUS.DAT
```

The primary input/output interface remains compatible. Up to eight additional inputs and eight additional required outputs can be declared. `--file NAME=PATH` copies raw bytes from PATH to NAME inside the private execution directory. `--output NAME=GOLDEN` compares another binary golden file, while `--output NAME` captures and requires the file without asserting its contents. Mapping names are at most 511 bytes; primary names retain their original limit. Filenames are literal relative paths. Duplicate names and file/directory prefix collisions are rejected before execution.

Every required output must exist. PASS requires at least one matching golden oracle or a successful explicit reference comparison; all other no-oracle observations remain UNVERIFIED. Every supplied golden is checked. In reference mode, selected output policies compare all declared files, using the same explicit normalization rule, and golden flags cannot be mixed with a reference oracle. A selected truncated capture cannot establish equality. Missing files and unsafe output paths retain their precise diagnostics.

`--max-input-bytes` caps the combined primary/additional input bytes. `--max-capture-bytes` applies independently to each stdout/stderr/output/golden capture. File counts and individual caps bound memory use, but choosing large caps across many files can consume substantial parent memory. This is not an aggregate disk quota or security sandbox.

Archives include `input-NN.bin`, `expected-NN.bin` and `output-NN.bin`, with name/hash/oracle/missing/truncation metadata. File-set archives use replay format `beryl-2`, so older clients reject them instead of silently ignoring additional fixtures. Version 0.5 reads both formats; primary-only archives still use `beryl-1`. Replay validates all additional input/oracle digests and restores the declared names. JSON includes additional-input/output arrays; JUnit covers the whole case contract.

Fuzz campaigns snapshot additional static inputs/oracles once, then mutate the primary record. Minimization retains secondary fixtures while reducing the primary input, and exact signatures include all captured declared outputs. All originals remain available. Reference executions receive the same snapshotted file set in separate directories. No directory trees are imported implicitly.
