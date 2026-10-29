# Explicit callable-program drivers

```sh
beryl driver --program CLAMPREC --record-bytes 3 --out .beryl/clamp-driver
cobc -x -free -o ./clamp-test .beryl/clamp-driver/driver.cob tests/cobol/clamp_subprogram.cob
beryl run --exec ./clamp-test --input tests/records/boundary.bin \
  --expected tests/expected/boundary.out --expected-exit 7
```

`driver` generates source and `driver.json`; it does not discover an entry point, modify the supplied subprogram, or invoke a compiler. The caller explicitly chooses the program name and ABI: **one fixed-length record passed BY REFERENCE**. The subprogram must accept that exact layout and return with GOBACK/EXIT PROGRAM. Multiple parameters, BY VALUE, automatic linkage discovery, variable-length records and programs that terminate the whole process with STOP RUN require a custom, statically supplied driver. Such drivers can already be tested using `beryl run`.

The generated driver reads exactly one fixed SEQUENTIAL record, calls the named program, and writes the modified raw bytes. It preserves the subprogram's RETURN-CODE. The provided CLAMPREC fixture deliberately returns 7 to test this behavior, so its run example explicitly expects 7. Input `099` becomes `100`; input `100` remains `100`.

Record size is 1..65536 bytes. Optional `--input-name`/`--output-name` specify distinct safe relative paths of at most 100 bytes each, keeping generated literal lines within the supported free-format width; defaults are INPUT.DAT and OUTPUT.DAT. Literal quotes are escaped using COBOL's doubled-quote convention. The output directory must be new. The metadata records program name, interface, size, names and source hash. Compilation and runtime dependencies remain explicit user choices. The generated source has been compiled and exercised with GnuCOBOL 3.2.0; IBM/z/OS compatibility is not claimed.

Driver operational statuses are 121 input-open failure, 122 missing/short first record, 123 extra input/invalid subsequent record, 124 CALL exception, and 125/126/127 output open/write/close failure. Normal completion returns the called program's status. These codes are driver conventions, not reserved statuses in arbitrary COBOL programs. Declared outputs remain subject to Beryl's usual file oracle, capture bounds and workspace rules.
