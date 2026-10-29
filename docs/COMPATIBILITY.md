# Compatibility

Beryl targets Linux/POSIX process APIs and C17. It was built and tested on x86-64 Ubuntu under WSL with GCC 15.2.0 and GnuCOBOL 3.2.0; see VALIDATION.md. Native Windows and macOS runners are not implemented. Linux `/proc/self/fd` is used to anchor private-directory creation to a verified open descriptor.

Targets are precompiled executables. Production runs work without `cobc`. Dynamic executables still require their normal runtime libraries. Explicit single-record BY REFERENCE drivers can be generated with `beryl driver`; users compile them with their selected subprogram. Statically supplied drivers support other declared linkage interfaces. Automatic callable-subprogram discovery is not implemented. See CALLABLE.md.

The fixtures use free-format COBOL, literal INPUT.DAT/OUTPUT.DAT assignments, and fixed three-byte numeric DISPLAY records with SEQUENTIAL organization. The raw fixture files contain `100` without a newline. The valid program clamps values at 100. The faulty program mishandles the boundary, producing `101` for `100`. Tests verify actual bytes from the compiled COBOL executables.

Run/replay treat record encodings as opaque bytes. Phase 2 adds a bounded free-format declaration parser and ASCII DISPLAY/canonical COMP-3 codecs for generation. Three independent 25-byte vectors from a compiled COBOL program agree with the encoder. A second COBOL fixture checks 100 generated records for numeric validity and echoes them unchanged. These tests validate GnuCOBOL 3.2.0's default storage conventions, including signed DISPLAY and odd/even packed digits; they do not establish compatibility with arbitrary compiler options or dialects. See RECORD_LAYOUTS.md for exact assumptions.

The optional compiler matrix also validates selected GCC COBOL and GnuCOBOL dialect configurations; see COMPILER_MATRIX.md for exact fixtures, limits and actual results. Compiler-mapped line coverage is available through the GnuCOBOL 3.2 adapter described in COVERAGE.md.

No EBCDIC translation, variable-length record handling, SQL/CICS/IMS integration, or IBM mainframe compatibility is claimed. A target with other formats may run, but the caller must supply the appropriate bytes and execution dependencies. Beryl does not claim business correctness without an approved oracle.
