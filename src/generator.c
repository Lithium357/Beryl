#include "beryl/generator.h"
#include "beryl/beryl.h"
#include "beryl/record_codec.h"
#include "beryl/report.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* SplitMix64, fixed unsigned arithmetic. Seed zero is a valid state. */
uint64_t generator_next(uint64_t *state) {
    uint64_t z = (*state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}
static void numeric_value(const Field *f, unsigned mode, uint64_t *state, char value[B_DIGITS + 4]) {
    unsigned char digits[B_DIGITS] = {0}; bool negative = false;
    if (mode == 1 || (mode == 2 && f->signed_value)) {
        memset(digits, 9, f->digits); negative = mode == 2;
    } else if (mode == 3 || (mode == 4 && f->signed_value)) {
        digits[f->digits - 1] = 1; negative = mode == 4;
    } else if (mode == 5) negative = f->signed_value;
    else if (mode >= 6) {
        for (unsigned i = 0; i < f->digits; i++) digits[i] = (unsigned char)(generator_next(state) % 10);
        negative = f->signed_value && (generator_next(state) & 1U);
    }
    size_t n = 0; if (negative) value[n++] = '-';
    for (unsigned i = 0; i < f->digits; i++) {
        if (f->scale && i == f->digits - f->scale) value[n++] = '.';
        value[n++] = (char)('0' + digits[i]);
    }
    value[n] = 0;
}
int generate_record(const Layout *layout, size_t index, uint64_t *state, unsigned char *record, char error[512]) {
    if (!layout->count || layout->count > B_FIELDS || layout->size > B_RECORD_LIMIT) {
        snprintf(error, 512, "invalid generation layout"); return -1;
    }
    for (size_t i = 0; i < layout->count; i++) {
        const Field *f = &layout->fields[i];
        if (f->offset > layout->size || f->size > layout->size - f->offset) { snprintf(error, 512, "generation field outside record"); return -1; }
        unsigned mode = index < 6 ? (unsigned)index : (unsigned)(generator_next(state) % 10);
        if (f->storage == B_TEXT) {
            if (!f->size || f->size > 1024) { snprintf(error, 512, "invalid text generation field"); return -1; }
            char value[1025]; size_t n = mode == 0 ? 0 : mode == 3 ? 1 : mode == 4 ? f->size - 1 : f->size;
            for (size_t j = 0; j < n; j++) value[j] = mode == 1 ? '~' : mode == 2 ? '0' : mode < 6 ? 'A' : (char)(' ' + generator_next(state) % 95);
            value[n] = 0;
            if (field_encode(f, value, record + f->offset, error) < 0) return -1;
        } else {
            if (!f->digits || f->digits > B_DIGITS || f->scale >= f->digits) { snprintf(error, 512, "invalid numeric generation field"); return -1; }
            char value[B_DIGITS + 4]; numeric_value(f, mode, state, value);
            if (field_encode(f, value, record + f->offset, error) < 0) return -1;
        }
    }
    return record_validate(layout, record, layout->size, error);
}
static int parse_uint(const char *s, uint64_t max, uint64_t *out) {
    if (!*s) return -1;
    uint64_t n = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return -1;
        unsigned d = (unsigned)(*s - '0');
        if (n > max / 10 || (n == max / 10 && d > max % 10)) return -1;
        n = n * 10 + d;
    }
    *out = n; return 0;
}
static int new_directory(const char *path) {
    char parent[B_PATH], leaf[B_PATH];
    if (!*path || strlen(path) >= B_PATH) { errno = EINVAL; return -1; }
    strcpy(parent, path); size_t n = strlen(parent);
    while (n > 1 && parent[n - 1] == '/') parent[--n] = 0;
    char *slash = strrchr(parent, '/');
    if (slash) { strcpy(leaf, slash + 1); if (slash == parent) parent[1] = 0; else *slash = 0; }
    else { strcpy(leaf, parent); strcpy(parent, "."); }
    if (!safe_name(leaf)) { errno = EINVAL; return -1; }
    int fd = open_directory(parent, true); if (fd < 0) return -1;
    if (mkdirat(fd, leaf, 0700) < 0) { int e = errno; close(fd); errno = e; return -1; }
    int dir = openat(fd, leaf, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    int e = errno; close(fd); errno = e; return dir;
}
static void generate_help(void) {
    puts("Usage: beryl generate --copybook FILE --count N --seed N --out NEW-DIRECTORY [--malformed]\n"
         "Free-format PIC X/9/S9/V DISPLAY and COMP-3 only; unsupported clauses are rejected.\n"
         "Count: 1..10000; seed: 0..18446744073709551615; corpus cap: 64 MiB.\n"
         "Default: valid records. --malformed deliberately corrupts storage or record length.\n"
         "The output directory must not already exist. generation.json is the completion marker.");
}
int generate_cli(int argc, char **argv) {
    const char *copybook = NULL, *out = NULL; uint64_t count = 0, seed = 0; bool seeded = false, malformed = false;
    for (int i = 2; i < argc; i++) {
        const char *flag = argv[i];
        if (!strcmp(flag, "--help")) { generate_help(); return 0; }
        if (!strcmp(flag, "--malformed")) { malformed = true; continue; }
        if (i + 1 == argc) { fprintf(stderr, "error: %s requires a value; see beryl generate --help\n", flag); return 2; }
        const char *v = argv[++i];
        if (!strcmp(flag, "--copybook")) copybook = v;
        else if (!strcmp(flag, "--out")) out = v;
        else if (!strcmp(flag, "--count")) { if (parse_uint(v, 10000, &count) < 0 || !count) { fputs("error: --count must be 1..10000\n", stderr); return 2; } }
        else if (!strcmp(flag, "--seed")) { if (parse_uint(v, UINT64_MAX, &seed) < 0) { fputs("error: --seed must be an unsigned 64-bit integer\n", stderr); return 2; } seeded = true; }
        else { fprintf(stderr, "error: unknown flag %s; see beryl generate --help\n", flag); return 2; }
    }
    if (!copybook || !out || !count || !seeded) { fputs("error: generate requires --copybook, --count, --seed and --out; see beryl generate --help\n", stderr); return 2; }
    Bytes source = {0}; char error[512]; Layout layout;
    if (read_bytes(copybook, B_ENV_LIMIT, &source) < 0 || source.truncated) { fprintf(stderr, "error: --copybook %s: regular source file up to 1 MiB required\n", copybook); bytes_free(&source); return 2; }
    if (copybook_parse((const char *)source.data, source.len, &layout, error) < 0) { fprintf(stderr, "error: --copybook %s: %s\n", copybook, error); bytes_free(&source); return 2; }
    if ((layout.size + 1) * count > 64U * 1024U * 1024U) { fputs("error: --count and record size exceed 64 MiB corpus budget\n", stderr); bytes_free(&source); return 2; }
    int dir = new_directory(out);
    if (dir < 0) { fprintf(stderr, "error: --out %s: %s; choose a new directory without symlinks\n", out, strerror(errno)); bytes_free(&source); return 2; }
    unsigned char *record = malloc(layout.size + 1); char *manifest = NULL; size_t manifest_len = 0;
    FILE *f = open_memstream(&manifest, &manifest_len); int rc = 2;
    if (!record || !f || copy_to(dir, "copybook.cpy", &source) < 0) goto fail;
    char hash[65]; hash_bytes(source.data, source.len, hash);
    fprintf(f, "{\n\"schema\":\"beryl-generation-1\",\"version\":\"" BERYL_VERSION "\",\"seed\":\"%" PRIu64 "\",\"prng\":\"splitmix64-v1\",\"mode\":\"%s\",\"copybook_sha256\":\"%s\",\"record_bytes\":%zu,\"count\":%" PRIu64 ",\n\"fields\":[",
            seed, malformed ? "malformed" : "valid", hash, layout.size, count);
    for (size_t i = 0; i < layout.count; i++) {
        const Field *field = &layout.fields[i]; if (i) fputc(',', f);
        fputs("{\"name\":", f); json_string(f, field->name);
        fprintf(f, ",\"offset\":%zu,\"bytes\":%zu,\"digits\":%u,\"scale\":%u,\"signed\":%s,\"storage\":\"%s\"}", field->offset, field->size, field->digits, field->scale, field->signed_value ? "true" : "false", field->storage == B_TEXT ? "TEXT" : field->storage == B_DISPLAY ? "DISPLAY" : "COMP-3");
    }
    fputs("],\n\"records\":[\n", f); uint64_t state = seed;
    for (size_t i = 0; i < (size_t)count; i++) {
        if (generate_record(&layout, i, &state, record, error) < 0) { fprintf(stderr, "error: generation: %s\n", error); goto done; }
        size_t len = layout.size; const char *mutation = "none";
        if (malformed) {
            if (i % 3 == 1) { record[len++] = 0; mutation = "append-byte"; }
            else if (i % 3 == 2) {
                size_t j; for (j = 0; j < layout.count && layout.fields[j].storage == B_TEXT; j++) {}
                if (j < layout.count) { record[layout.fields[j].offset] = 0xff; mutation = "invalid-digit"; }
                else { len--; mutation = "truncate-byte"; }
            } else { len--; mutation = "truncate-byte"; }
        }
        char name[32]; snprintf(name, sizeof name, "case-%06zu.bin", i);
        if (write_at(dir, name, record, len) < 0) goto fail;
        hash_bytes(record, len, hash);
        fprintf(f, "%s{\"file\":\"%s\",\"bytes\":%zu,\"sha256\":\"%s\",\"mutation\":\"%s\"}", i ? ",\n" : "", name, len, hash, mutation);
    }
    fputs("\n]}\n", f);
    { int bad = ferror(f); if (fclose(f)) bad = 1; f = NULL; if (bad || write_at(dir, "generation.json", manifest, manifest_len) < 0) goto fail; }
    printf("GENERATED: %" PRIu64 " %s records (%zu-byte layout), seed=%" PRIu64 "\ncorpus: %s\n", count, malformed ? "malformed" : "valid", layout.size, seed, out);
    rc = 0; goto done;
fail:
    fprintf(stderr, "error: cannot save --out %s: %s; partial corpus may remain without generation.json\n", out, strerror(errno));
done:
    if (f) fclose(f);
    free(manifest); free(record); bytes_free(&source); close(dir); return rc;
}
