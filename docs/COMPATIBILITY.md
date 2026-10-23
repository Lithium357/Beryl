# Compatibility

Phase 1 targets Linux/POSIX process APIs and C17. It was built and tested on x86-64 Ubuntu under WSL with GCC 15.2.0 and GnuCOBOL 3.2.0; see VALIDATION.md. Native Windows and macOS runners are not implemented. Linux `/proc/self/fd` is used to anchor private-directory creation to a verified open descriptor.

Targets are precompiled executables. The runner has no compiler adapter yet because it does not compile user targets. GnuCOBOL is used by CMake only to compile integration fixtures. Production runs work without `cobc`. Dynamic executables still require their normal runtime libraries. Statically supplied drivers can be used as executable targets, but automatic callable-subprogram discovery and driver generation are not implemented.

The fixtures use free-format COBOL, literal INPUT.DAT/OUTPUT.DAT assignments, and fixed three-byte numeric DISPLAY records with SEQUENTIAL organization. The raw fixture files contain `100` without a newline. The valid program clamps values at 100. The faulty program mishandles the boundary, producing `101` for `100`. Tests verify actual bytes from the compiled COBOL executables.

Phase 1 treats all record encodings as opaque bytes. No copybook parser, DISPLAY/COMP-3 encoder, EBCDIC translation, variable-length record handling, SQL/CICS/IMS integration, coverage mapping, or IBM mainframe compatibility is claimed. A target with other formats may run, but the caller must supply the appropriate bytes and execution dependencies. Beryl does not claim business correctness without an approved oracle.
