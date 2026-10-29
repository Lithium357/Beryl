# Record layouts and generation (Phase 2)

`beryl generate --copybook FILE --count N --seed N --out NEW-DIRECTORY` creates one raw fixed-length record per file. It does not execute a target or assert business correctness. Add `--malformed` to create deliberately invalid lengths/storage instead of valid records. `beryl generate --help` lists the CLI limits.

## Accepted declaration subset

ASCII source is case insensitive. Free format with `*>` comments is the default; `--format fixed` enables the limited fixed-column rules described in PREPROCESSING.md. Start with one level 01 group. Groups can contain nested groups and elementary fields at levels 02..49; level numbers have two digits. Group/elementary names start with a letter, contain letters/digits/hyphens and have at most 63 characters. Names cannot end with a hyphen. FILLER is permitted. Source order determines offsets; group paths are retained in field diagnostics and metadata. Periods between declarations are optional. An elementary field cannot contain children, and groups cannot be empty.

Supported pictures are `X(n)`, `9(n)`, `S9(n)`, `9(n)V9(m)`, and `S9(n)V9(m)`. Literal repeated X/9 characters are accepted too, as are single X/9 and whitespace around repetition parentheses. `PIC` or `PICTURE`, optional `IS`, and an optional trailing `DISPLAY`, `COMP-3`, `PACKED-DECIMAL`, or `USAGE [IS] ...` clause are accepted. Default storage is DISPLAY. Usage clauses belong to elementary fields, after the picture. This is a declaration parser, not a full COBOL grammar or compiler; the caller supplies the actual target-compatible record definition.

The parser rejects REDEFINES, OCCURS (including depending-on), levels 66/77/88, edited pictures, binary COMP/COMP-4/COMP-5, SIGN clauses (including separate/leading variants), VALUE, unresolved COPY, compiler directives, group usage clauses, and other unrecognized syntax. It never guesses offsets for unsupported declarations. `beryl preprocess` optionally resolves COPY through GnuCOBOL; generated declarations still have to satisfy this strict subset. There is no encoding conversion or EBCDIC mode.

Limits: source 1 MiB / 32768 tokens, 256 elementary fields, 65536 record bytes, 38 numeric digits, 1024 bytes per text field, 49 nesting entries, and 255 characters per qualified field path. Invalid source errors name the line and field/feature where available; the CLI includes the source filename. Check the layout metadata rather than assuming your original copybook is fully supported.

## Storage conventions

All decimal operations use strings/digit arrays with explicit scale; no floating point is used. PIC V consumes no byte. The encoder accepts a decimal string with optional sign and at least one integer digit. It pads missing fractional digits to the declared scale and rejects excess precision/scale, exponent notation, whitespace, and negative values for unsigned fields. It never rounds. Decode produces a normalized integer part and the declared number of fractional digits. Negative zero is preserved for signed fields.

| Storage | Exact convention |
| --- | --- |
| PIC X | Raw bytes, one byte per character position; string encoding right-pads with ASCII spaces |
| Unsigned DISPLAY | One ASCII digit byte per digit; no stored dot or sign |
| Signed DISPLAY | Default GnuCOBOL ASCII trailing embedded sign: positive final digit `0`..`9`, negative final digit `p`..`y` |
| COMP-3 | `floor((digits + 2) / 2)` bytes; zero leading pad nibble for even digit counts; sign C positive/D negative for signed, F for unsigned |

The decoder deliberately accepts canonical C/D/F packed signs only, not other sign-nibble variants a runtime might tolerate. Alternate overpunch conventions are unsupported. These assumptions are verified against GnuCOBOL 3.2.0's default configuration and must not be generalized to IBM or differently configured compilers. Text raw bytes, including NULs, remain valid; the text-string encoder takes a C string, while raw record APIs retain explicit lengths.

Examples: `PIC 9(3)V9(2)` value 1.20 stores ASCII `00120`. Signed DISPLAY value -12.34 with `S9(3)V9(2)` stores ASCII `0123t`. Packed `S9(3)` -123 is hex `12 3d`; packed `S9(4)` +1234 is `01 23 4c`; unsigned `9(4)` 1234 is `01 23 4f`. Tests cover these independently specified vectors, 38-digit precision, invalid digits/signs/padding, buffers, zeros, and codec round trips.

## Deterministic corpus policy

Seed is required and accepts the entire uint64 range, including zero. The PRNG is versioned `splitmix64-v1`, using fixed unsigned 64-bit arithmetic. The same version, source, count, seed and mode produce identical raw files and generation metadata. Output paths and timestamps are excluded from that metadata. This deterministic test-data PRNG is not for cryptographic use.

The first six records use directed patterns for every field:

| Index | Numeric pattern | Text pattern |
| --- | --- | --- |
| 0 | zero | empty string padded to spaces |
| 1 | maximum positive | full-width `~` |
| 2 | most negative signed / zero unsigned | full-width `0` |
| 3 | +1 in the smallest unit | one `A`, padded |
| 4 | -1 in the smallest unit signed / zero unsigned | width-minus-one `A`, padded |
| 5 | negative zero signed / zero unsigned | full-width `A` |

Counts below six produce only the corresponding prefix of this table. Further records choose among these boundaries and seeded random representable digits/printable ASCII fields. Thus a count of six or fewer intentionally does not vary with the seed. Text patterns explore lengths within the declared width, not every possible byte or an application-specific alphabet. There is no assertion that a negative balance, zero term, or unusual character is invalid for the application.

Malformed mode first creates the same kind of structurally valid record, then cycles through truncating one byte, appending one NUL byte, and replacing the first numeric field's first byte with 0xff. For text-only layouts the third operation uses truncation because all text byte values can be valid. Every resulting malformed record violates the declared length or supported numeric encoding. The operation is recorded per file. These are explicit test inputs, not discovered defects. Structure-aware fuzz campaigns and failure minimization are not implemented yet.

Count is 1..10000 and the conservative raw payload budget is `(record_size + 1) * count <= 64 MiB`. Output directory creation is exclusive; existing directories/files and symlink components are refused. A failed write may leave a partial corpus without `generation.json`. The completion manifest includes tool/schema/PRNG versions, exact seed as a decimal **string** (to preserve uint64 precision in JSON clients), source hash, mode, field offsets/storage/scale, and per-record filenames/lengths/SHA-256/mutation. `copybook.cpy` archives the exact source bytes. Records do not contain newlines unless their raw field bytes do.
