#include "beryl/beryl.h"
#include "beryl/report.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char root[] = "/tmp/beryl-reports-XXXXXX"; assert(mkdtemp(root));
    Config c; config_defaults(&c); strcpy(c.target, "/target/<test>&\"'\\\n\xff"); strcpy(c.compiler, "test compiler");
    c.args[0] = strdup("literal\targ\"\\"); assert(c.args[0]); c.argc = 1;
    for (int outcome = PASS; outcome <= UNVERIFIED; outcome++) {
        Result r = {0}; r.outcome = (Outcome)outcome; r.duration_ms = 1234;
        strcpy(r.diagnostic, "message <&>\"'\n\t\xff");
        snprintf(r.directory, sizeof r.directory, "%s/%s", root, outcome_name(r.outcome));
        int dir = open_directory(r.directory, true); assert(dir >= 0); assert(!save_reports(&c, &r, dir));
        Bytes json = {0}, xml = {0}; assert(!read_at(dir, "result.json", B_ENV_LIMIT, &json)); assert(!read_at(dir, "junit.xml", B_ENV_LIMIT, &xml));
        json.data[json.len] = 0; xml.data[xml.len] = 0;
        assert(strstr((char *)json.data, "\\u000a") && strstr((char *)json.data, "\\u00ff"));
        assert(strstr((char *)json.data, "\"duration_ms\":1234") && strstr((char *)json.data, "\"expected_sha256\":null"));
        assert(strstr((char *)xml.data, "&lt;test&gt;&amp;&quot;&apos;\\x5c") == NULL); /* backslash is ordinary XML text */
        assert(strstr((char *)xml.data, "&lt;test&gt;&amp;&quot;&apos;\\\\x0a\\xff"));
        assert(strstr((char *)xml.data, "time=\"1.234\""));
        if (r.outcome == UNVERIFIED) assert(strstr((char *)xml.data, "<skipped "));
        else if (r.outcome == EXEC_ERROR || r.outcome == HARNESS_ERROR) assert(strstr((char *)xml.data, "<error "));
        else if (r.outcome != PASS) assert(strstr((char *)xml.data, "<failure "));
        else assert(!strstr((char *)xml.data, "<failure ") && !strstr((char *)xml.data, "<error ") && !strstr((char *)xml.data, "<skipped "));
        bytes_free(&json); bytes_free(&xml); close(dir);
    }
    config_free(&c); printf("reports: JSON/XML escaping and all seven outcome mappings passed\nevidence: %s\n", root); return 0;
}
