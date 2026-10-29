#include "beryl/beryl.h"
#include "beryl/copybook.h"
#include "beryl/generator.h"
#include "beryl/record_codec.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *schema = "01 r. 05 name pic X(4). 05 amount pic S9(3)V9(2) COMP-3. 05 count pic 9(4) DISPLAY. 05 signed-count pic S9(2) DISPLAY.";
static char cli[B_PATH], base[B_PATH];
static int invoke(char *const argv[]) {
    pid_t pid = fork(); assert(pid >= 0);
    if (!pid) { char log[B_PATH]; assert(!path_join(log, base, "cli.log")); int fd = open(log, O_WRONLY|O_CREAT|O_TRUNC, 0600); assert(fd >= 0); assert(dup2(fd, 1) >= 0 && dup2(fd, 2) >= 0); close(fd); execv(cli, argv); _exit(99); }
    int status; while (waitpid(pid, &status, 0) < 0) assert(errno == EINTR);
    assert(WIFEXITED(status)); return WEXITSTATUS(status);
}
static void bytes(const char *root, const char *file, Bytes *b) {
    char path[B_PATH]; assert(!path_join(path, root, file)); assert(!read_bytes(path, B_ENV_LIMIT, b)); assert(!b->truncated);
}
int main(int argc, char **argv) {
    assert((argc == 2 || argc == 3) && realpath(argv[1], cli)); strcpy(base, "/tmp/beryl-generation-XXXXXX"); assert(mkdtemp(base));
    Layout layout; char error[512]; assert(!copybook_parse(schema, strlen(schema), &layout, error));
    uint64_t state = 0; assert(generator_next(&state) == UINT64_C(0xe220a8397b1dcdaf)); assert(generator_next(&state) == UINT64_C(0x6e789e6aa1b965f4));
    unsigned char record[100]; state = 12345;
    const char *boundaries[] = {"0.00", "999.99", "-999.99", "0.01", "-0.01", "-0.00"};
    for (size_t i = 0; i < 512; i++) {
        assert(!generate_record(&layout, i, &state, record, error)); assert(!record_validate(&layout, record, layout.size, error));
        if (i < 6) { char text[80]; const Field *f = &layout.fields[1]; assert(!field_decode(f, record + f->offset, text, sizeof text, error)); assert(!strcmp(text, boundaries[i])); }
    }
    char source[B_PATH], one[B_PATH], two[B_PATH], three[B_PATH], malformed[B_PATH];
    assert(!path_join(source, base, "record.cpy")); assert(!path_join(one, base, "one")); assert(!path_join(two, base, "two")); assert(!path_join(three, base, "three")); assert(!path_join(malformed, base, "malformed"));
    FILE *f = fopen(source, "wb"); assert(f); assert(fwrite(schema, 1, strlen(schema), f) == strlen(schema)); assert(!fclose(f));
    char *args[] = {cli, "generate", "--copybook", source, "--count", "100", "--seed", "12345", "--out", one, NULL, NULL};
    assert(!invoke(args)); args[9] = two; assert(!invoke(args)); args[7] = "12346"; args[9] = three; assert(!invoke(args));
    bool different = false;
    for (size_t i = 0; i < 100; i++) {
        char name[32]; snprintf(name, sizeof name, "case-%06zu.bin", i); Bytes a = {0}, b = {0}, c = {0}; bytes(one, name, &a); bytes(two, name, &b); bytes(three, name, &c);
        assert(a.len == layout.size && b.len == a.len && c.len == a.len); assert(!memcmp(a.data, b.data, a.len));
        assert(!record_validate(&layout, a.data, a.len, error)); if (memcmp(a.data, c.data, a.len)) different = true;
        bytes_free(&a); bytes_free(&b); bytes_free(&c);
    }
    assert(different); Bytes a = {0}, b = {0}; bytes(one, "generation.json", &a); bytes(two, "generation.json", &b);
    assert(a.len == b.len && !memcmp(a.data, b.data, a.len)); bytes_free(&a); bytes_free(&b);
    bytes(one, "copybook.cpy", &a); assert(a.len == strlen(schema) && !memcmp(a.data, schema, a.len)); bytes_free(&a);
    args[9] = one; assert(invoke(args) == 2); /* no overwrite */
    args[9] = malformed; args[10] = "--malformed"; assert(!invoke(args));
    for (size_t i = 0; i < 100; i++) {
        char name[32]; snprintf(name, sizeof name, "case-%06zu.bin", i); bytes(malformed, name, &a); assert(record_validate(&layout, a.data, a.len, error) < 0); bytes_free(&a);
    }
    args[10] = NULL; args[7] = "-1"; assert(invoke(args) == 2); args[7] = "18446744073709551616"; assert(invoke(args) == 2);
    char maximum[B_PATH]; assert(!path_join(maximum, base, "maximum-seed")); args[9] = maximum; args[7] = "18446744073709551615"; args[5] = "6"; assert(!invoke(args));
    args[5] = "0"; assert(invoke(args) == 2); args[5] = "10001"; assert(invoke(args) == 2);
    char link[B_PATH]; assert(!path_join(link, base, "link")); assert(!symlink("/tmp", link)); char unsafe[B_PATH]; assert(!path_join(unsafe, link, "beryl-must-not-write")); args[9] = unsafe; args[5] = "6"; assert(invoke(args) == 2);
    char text_source[B_PATH], text_out[B_PATH]; assert(!path_join(text_source, base, "text.cpy")); assert(!path_join(text_out, base, "text-malformed"));
    f = fopen(text_source, "wb"); assert(f); assert(fputs("01 r. 05 x pic X.", f) >= 0); assert(!fclose(f));
    args[3] = text_source; args[5] = "3"; args[7] = "0"; args[9] = text_out; args[10] = "--malformed"; assert(!invoke(args));
    Layout text_layout; assert(!copybook_load(text_source, &text_layout, error));
    for (size_t i = 0; i < 3; i++) { char name[32]; snprintf(name, sizeof name, "case-%06zu.bin", i); bytes(text_out, name, &a); assert(record_validate(&text_layout, a.data, a.len, error) < 0); bytes_free(&a); }
    char budget_source[B_PATH], budget_out[B_PATH]; assert(!path_join(budget_source, base, "budget.cpy")); assert(!path_join(budget_out, base, "budget"));
    f = fopen(budget_source, "wb"); assert(f); assert(fputs("01 r.\n", f) >= 0);
    for (size_t i = 0; i < 64; i++) assert(fputs("05 FILLER PIC X(1024).\n", f) >= 0);
    assert(!fclose(f)); args[3] = budget_source; args[5] = "1025"; args[9] = budget_out; args[10] = NULL; assert(invoke(args) == 2); assert(access(budget_out, F_OK) < 0 && errno == ENOENT);
    if (argc == 3) {
        char input[B_PATH], results[B_PATH]; assert(!path_join(input, base, "combined.bin")); assert(!path_join(results, base, "cobol-results"));
        FILE *combined = fopen(input, "wb"); assert(combined);
        for (size_t i = 0; i < 100; i++) {
            char name[32]; snprintf(name, sizeof name, "case-%06zu.bin", i); bytes(one, name, &a);
            assert(fwrite(a.data, 1, a.len, combined) == a.len); bytes_free(&a);
        }
        assert(!fclose(combined));
        char *run[] = {cli, "run", "--exec", argv[2], "--input", input, "--expected", input, "--results", results, NULL}; assert(!invoke(run));
        puts("COBOL: all 100 generated records pass NUMERIC checks and echo byte-for-byte, including negative zero");
    }
    puts("generation: 512 API records validated; 100-record corpora deterministic; seed divergence, six boundaries, malformed rejection, seed/count limits, overwrite and symlink refusal passed");
    printf("evidence: %s\n", base); return 0;
}
