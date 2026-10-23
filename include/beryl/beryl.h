#ifndef BERYL_H
#define BERYL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#define BERYL_VERSION "0.2.0"
#define B_PATH 4096
#define B_ARGS 64
#define B_ENV_LIMIT (1024U * 1024U)
typedef struct { unsigned char *data; size_t len; bool truncated; } Bytes;
typedef struct {
    char target[B_PATH], input[B_PATH], expected[B_PATH], results[B_PATH];
    char input_name[B_PATH], output_name[B_PATH], compiler[256];
    char *args[B_ARGS + 2]; size_t argc;
    unsigned timeout_ms, cpu_seconds; size_t max_input, max_capture, max_file;
    int expected_exit; bool has_expected;
    Bytes environment;
} Config;
typedef enum { PASS, FAIL_ASSERTION, CRASH, TIMEOUT, EXEC_ERROR, HARNESS_ERROR, UNVERIFIED } Outcome;
typedef struct {
    Outcome outcome; char diagnostic[512], directory[B_PATH];
    char target_hash[65], input_hash[65], expected_hash[65], output_hash[65];
    int exit_code, signal_number, exec_errno;
    uint64_t duration_ms; bool missing_output;
    Bytes out, err, output;
} Result;
int read_bytes(const char *path, size_t limit, Bytes *b);
int read_at(int root, const char *path, size_t limit, Bytes *b);
int write_at(int root, const char *name, const void *data, size_t len);
int copy_to(int root, const char *name, const Bytes *b);
void bytes_free(Bytes *b);
int safe_name(const char *name);
int open_directory(const char *path, bool create);
int open_parent(int root, const char *name, bool create, char leaf[B_PATH]);
int path_join(char out[B_PATH], const char *a, const char *b);
int hash_file(const char *path, char hex[65]);
void hash_bytes(const void *data, size_t len, char hex[65]);
int workspace_create(const Config *c, Result *r, int *archive, int *work);
int run_child(const Config *c, Result *r, int work);
void compare_result(const Config *c, Result *r, const Bytes *expected);
const char *outcome_name(Outcome o);
int save_result(const Config *c, const Result *r, int archive);
int execute_case(Config *c, Result *r);
void config_defaults(Config *c);
void config_free(Config *c);
int replay_load(const char *directory, Config *c, char saved_target_hash[65]);
int cli_main(int argc, char **argv);
#endif
