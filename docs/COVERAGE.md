# Verified GnuCOBOL source-line coverage

```sh
beryl instrument --source tests/cobol/accounts.cob --out .beryl/instrumented
beryl run --exec .beryl/instrumented/program --input tests/records/boundary.bin \
  --expected tests/expected/boundary.out
beryl coverage --build .beryl/instrumented --out .beryl/coverage
```

The optional adapter targets GnuCOBOL 3.2 with GCC/gcov. It uses GnuCOBOL's `--coverage` and retained generated C, without editing the user's source. Compiler version/output, exact source, generated C, coverage notes, binary, explicit argv and compilation environment are retained. The environment archive may contain secrets; keep the build directory private. `--cobc PATH` and `--gcov PATH` select tools explicitly. Instrumentation supports `--format free|fixed`; source-adjacent COPY lookup is enabled. Other dependency/build systems should retain their explicit build configuration rather than assuming this small adapter can reproduce it.

Collection validates source, generated-C, target and coverage-note hashes before reading counters. Only source lines explicitly attributed to `source.cob` by GnuCOBOL's `#line` mapping **and** present in the selected gcov COBOL section enter the report. Generated C/runtime lines are excluded. A missing counter/mapping, changed artifact, unavailable tool, unsupported compiler format or missing execution data is an error; coverage is not fabricated or substituted with generated-C percentages.

`coverage.json` reports covered/mapped COBOL lines and raw gcov counts. `gcov.txt` retains the exact collection evidence. These are line observations from compiler instrumentation, not branch coverage or proof that every COBOL construct was mapped. gcov's partially executed line marker counts as a visited line; it is not full basic-block coverage. Branch coverage is explicitly null/unavailable. Ordinary run JSON remains coverage-unavailable unless this separate collection is performed; the adapter does not yet guide mutation selection.

Counters accumulate across executions in the instrumentation build directory, including when the executable's cwd is a disposable run directory. This is an intentional target runtime write outside that cwd. Stop executions before collecting, avoid concurrent runs/collections, and use a fresh instrumented build to start a fresh measurement. Retain the original runtime/compiler artifacts when reproducing a measurement. Run limits and OS/container isolation guidance still apply.

The end-to-end test compiles the supplied COBOL fixture, executes a known branch, verifies source line 23 was hit and line 25 was not, then executes the other branch and verifies line 25 is hit. It also rejects collection before execution and after changing source. This validates the source attribution for this adapter; it does not establish IBM/z/OS or arbitrary compiler compatibility.
