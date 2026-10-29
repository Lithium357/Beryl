# Copybook source formats and COPY resolution

`generate`, `fuzz` and copybook-aware `minimize` accept `--format free|fixed` (free by default). Fixed-format parsing validates columns 1..6 as sequence digits/spaces, recognizes space in indicator column 7, ignores `*`/`/` comment lines, parses columns 8..72, and ignores identification text after column 72. CRLF is supported; tabs, debugging indicators and continuation indicators are rejected explicitly. This deliberately limited mode never guesses continuation semantics.

Use the GnuCOBOL preprocessor when source needs COPY resolution or compiler-level continuation handling:

```sh
beryl preprocess --copybook account.cpy --format fixed \
  --include ./copybooks --out .beryl/resolved
beryl generate --copybook .beryl/resolved/resolved.cpy \
  --count 100 --seed 12345 --out .beryl/corpus
```

`--cobc PATH` selects a compiler explicitly; otherwise `cobc` is found on PATH. Up to 16 `--include DIR` flags are accepted. Beryl invokes `cobc -E` using explicit argument vectors, with the declared source format and source/include directories. It saves exact original source, raw compiler output/stderr, compiler version, compiler executable hash, resolved-source digest and argv. Only compiler `#line` markers are removed. GnuCOBOL 3.2.0 emits free-format expanded declarations for either input format; `resolved.cpy` and the metadata explicitly identify that free-format output. Unsupported declarations still fail Beryl's strict parser after expansion.

The compiler adapter has a 30-second wall/CPU limit, 512 MiB address-space limit and 8 MiB bounded stdout/stderr capture. Copybook source and final expanded declarations are limited to 1 MiB. New output directories are required. Failure preserves available diagnostics and does not publish `preprocess.json` as a successful completion marker. Compiler/runtime availability and settings remain the user's responsibility; no compiler is downloaded at runtime. Compiler preprocessing is optional and does not establish cross-compiler or IBM compatibility.
