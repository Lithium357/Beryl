# COBOL files in one page

A batch program often reads records from an assigned filename and writes records to another filename. The `SELECT ... ASSIGN TO "INPUT.DAT"` in the supplied fixtures makes the runtime look for INPUT.DAT relative to the process working directory. Beryl creates that directory and places your supplied bytes there.

**Fixed-record SEQUENTIAL** files consist of records of a declared size. In our fixtures the record is three DISPLAY digits: input `100` occupies three bytes, with no newline. Two such records would be six bytes (`100099`). LINE SEQUENTIAL is a different organization with line separators. Compiler/runtime settings and other organizations may introduce different storage conventions; inspect actual raw bytes instead of assuming every COBOL file is ordinary text. The Phase 1 fixture reads exactly one record.

A **copybook** is reusable COBOL source, often containing record field declarations. For example, a group could hold `05 ACCOUNT-ID PIC X(4).` followed by `05 BALANCE PIC 9(3).`. For this simple DISPLAY layout the group would have four character bytes followed by three digit bytes. Beryl Phase 1 does not parse copybooks or derive field offsets.

**DISPLAY** stores characters. `PIC 9(3)` describes three numeric digits; `PIC X(4)` describes four character positions. `PIC 9(3)V9(2)` has an implied decimal point: under a basic unsigned DISPLAY layout, `12345` represents 123.45 and has no stored dot. Signed storage and encoding conventions depend on the declared clauses and compiler. Do not invent a sign byte for an unfamiliar layout.

**COMP-3** means packed decimal: two decimal digit nibbles per byte, with a final sign nibble. It can contain non-text bytes and NULs. Exact decimal values must not be converted through binary floating point when constructing fixtures. A future record-codec phase will validate explicit GnuCOBOL storage rules against compiled programs; Phase 1 preserves packed fields only as raw bytes.

The supplied boundary test uses raw input `100` and golden output `100`. `accounts` passes; `accounts_bug` writes `101`, and Beryl reports a mismatch at zero-based byte offset 2. The C test fixture separately verifies NUL and non-ASCII bytes, so the harness is not relying on text-string comparison. A golden file is a user-approved expectation, not evidence by itself that an accounting rule is correct.
