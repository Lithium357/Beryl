#include "beryl/report.h"
#include "beryl/fixtures.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

void json_string(FILE *f, const char *bytes) {
    fputc('"', f);
    for (const unsigned char *p = (const unsigned char *)bytes; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', f); fputc(*p, f); }
        else if (*p < 32 || *p >= 127) fprintf(f, "\\u%04x", (unsigned)*p);
        else fputc(*p, f);
    }
    fputc('"', f);
}
static void xml_text(FILE *f, const char *bytes) {
    for (const unsigned char *p = (const unsigned char *)bytes; *p; p++) {
        if (*p == '&') fputs("&amp;", f);
        else if (*p == '<') fputs("&lt;", f);
        else if (*p == '>') fputs("&gt;", f);
        else if (*p == '"') fputs("&quot;", f);
        else if (*p == '\'') fputs("&apos;", f);
        else if (*p < 32 || *p >= 127) fprintf(f, "\\x%02x", (unsigned)*p);
        else fputc(*p, f);
    }
}
static int finish(FILE *f, char **buffer, size_t *len, int directory, const char *name) {
    int bad = ferror(f);
    if (fclose(f)) bad = 1;
    int rc = bad ? -1 : write_at(directory, name, *buffer, *len);
    free(*buffer); return rc;
}
static void stream_json(FILE *f, const char *name, const Bytes *bytes) {
    char hash[65]; hash_bytes(bytes->data, bytes->len, hash);
    fprintf(f, ",\n  \"%s\":{\"captured_bytes\":%zu,\"truncated\":%s,\"sha256\":\"%s\"}",
            name, bytes->len, bytes->truncated ? "true" : "false", hash);
}
int save_reports(const Config *c, const Result *r, int directory) {
    char *buffer = NULL; size_t len = 0;
    FILE *f = open_memstream(&buffer, &len); if (!f) return -1;
    fputs("{\n  \"schema\":\"beryl-result-1\",\n  \"version\":\"" BERYL_VERSION "\",\n  \"classification\":", f);
    json_string(f, outcome_name(r->outcome)); fputs(",\n  \"diagnostic\":", f); json_string(f, r->diagnostic);
    fixtures_json(f,c,r);
    fputs(",\n  \"reference\":", f);
    if(r->reference) {
        fputs("{\"path\":",f);json_string(f,c->reference);fputs(",\"directory\":",f);json_string(f,r->reference->directory);
        fprintf(f,",\"sha256\":\"%s\",\"exit_code\":%d,\"signal\":%d}",r->reference->target_hash,r->reference->exit_code,r->reference->signal_number);
    } else fputs("null",f);
    fprintf(f,",\n  \"comparison\":\"%s\",\n  \"normalize_crlf\":%s,\n  \"differences\":%u,\n  \"coverage\":\"unavailable\"",c->comparison==1?"status":c->comparison==2?"all":"output",c->normalize_crlf?"true":"false",r->differences);
    fputs(",\n  \"target\":{\"path\":", f); json_string(f, c->target);
    fprintf(f, ",\"sha256\":\"%s\",\"compiler\":", r->target_hash); json_string(f, c->compiler); fputc('}', f);
    fputs(",\n  \"argv\":[", f); json_string(f, c->target);
    for (size_t i = 0; i < c->argc; i++) { fputc(',', f); json_string(f, c->args[i]); } fputc(']', f);
    fputs(",\n  \"input\":{\"name\":", f); json_string(f, c->input_name);
    fprintf(f, ",\"sha256\":\"%s\"},\n  \"expected_sha256\":", r->input_hash);
    if (c->has_expected) json_string(f, r->expected_hash); else fputs("null", f);
    fputs(",\n  \"output\":{\"name\":", f); json_string(f, c->output_name);
    fprintf(f, ",\"sha256\":\"%s\",\"hash_scope\":\"%s\",\"captured_bytes\":%zu,\"missing\":%s,\"truncated\":%s}",
            r->output_hash, r->output.truncated ? "captured-prefix" : "complete", r->output.len,
            r->missing_output ? "true" : "false", r->output.truncated ? "true" : "false");
    stream_json(f, "stdout", &r->out); stream_json(f, "stderr", &r->err);
    char env[65]; hash_bytes(c->environment.data, c->environment.len, env);
    fprintf(f, ",\n  \"environment_sha256\":\"%s\",\n  \"seed\":",env);
    if(c->seeded)fprintf(f,"\"%" PRIu64 "\"",c->seed);else fputs("null",f);
    fprintf(f,",\n  \"case_index\":%" PRIu64 ",\n  \"mutation\":",c->case_index);json_string(f,c->mutation);
    fprintf(f,",\n  \"layout_sha256\":\"%s\",\n  \"exit_code\":%d,\n  \"signal\":%d,\n  \"exec_errno\":%d,\n  \"duration_ms\":%" PRIu64,
            c->layout_hash, r->exit_code, r->signal_number, r->exec_errno, r->duration_ms);
    fprintf(f, ",\n  \"settings\":{\"expected_exit\":%d,\"timeout_ms\":%u,\"cpu_seconds\":%u,\"max_input_bytes\":%zu,\"max_capture_bytes\":%zu,\"max_file_bytes\":%zu,\"address_space_bytes\":536870912,\"core_bytes\":0},\n  \"work_directory\":",
            c->expected_exit, c->timeout_ms, c->cpu_seconds, c->max_input, c->max_capture, c->max_file);
    char work[B_PATH];
    if (path_join(work, r->directory, "work") < 0) { fclose(f); free(buffer); return -1; }
    json_string(f, work); fputs("\n}\n", f);
    if (finish(f, &buffer, &len, directory, "result.json") < 0) return -1;
    buffer = NULL; len = 0; f = open_memstream(&buffer, &len); if (!f) return -1;
    bool error = r->outcome == HARNESS_ERROR || r->outcome == EXEC_ERROR;
    bool failure = r->outcome == FAIL_ASSERTION || r->outcome == CRASH || r->outcome == TIMEOUT || r->outcome == REGRESSION_CANDIDATE;
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuites><testsuite name=\"beryl\" tests=\"1\" failures=\"%d\" errors=\"%d\" skipped=\"%d\" time=\"%" PRIu64 ".%03" PRIu64 "\"><testcase classname=\"beryl.batch\" name=\"",
            failure, error, r->outcome == UNVERIFIED, r->duration_ms / 1000, r->duration_ms % 1000);
    xml_text(f, c->target); fprintf(f, "\" time=\"%" PRIu64 ".%03" PRIu64 "\">", r->duration_ms / 1000, r->duration_ms % 1000);
    if (failure || error) {
        const char *tag = error ? "error" : "failure";
        fprintf(f, "<%s type=\"%s\" message=\"", tag, outcome_name(r->outcome)); xml_text(f, r->diagnostic);
        fputs("\">", f); xml_text(f, r->diagnostic); fprintf(f, "</%s>", tag);
    } else if (r->outcome == UNVERIFIED) fputs("<skipped message=\"No golden oracle; observation is unverified\"/>", f);
    fputs("</testcase></testsuite></testsuites>\n", f);
    return finish(f, &buffer, &len, directory, "junit.xml");
}
