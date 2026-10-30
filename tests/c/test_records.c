#include "beryl/beryl.h"
#include "beryl/copybook.h"
#include "beryl/record_codec.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *schema =
    "01 rec.\n"
    " 05 label-field PIC X(4).\n"
    " 05 numbers.\n"
    "  10 unsigned-display PIC 9(3).\n"
    "  10 signed-display PIC S9(3)V9(2) DISPLAY.\n"
    "  10 signed-odd PIC S9(3) COMP-3.\n"
    "  10 signed-even PIC S9(4) USAGE IS PACKED-DECIMAL.\n"
    "  10 unsigned-odd PIC 9(3) COMP-3.\n"
    "  10 unsigned-even PIC 9(4) COMP-3.\n"
    "  10 scaled-packed PIC S9(3)V9(2) COMP-3.\n";
static const char *values[3][8] = {
    {"AB", "123", "-12.34", "-123", "1234", "123", "1234", "-123.45"},
    {"", "0", "0.00", "0", "0", "0", "0", "0.00"},
    {"~~~~", "999", "-999.99", "999", "-9999", "999", "9999", "999.99"}
};
static void vector(const char *pic, const char *value, const unsigned char *expected, size_t len, const char *decoded) {
    char source[256], error[512], text[100]; Layout layout;
    snprintf(source, sizeof source, "01 r. 05 f PIC %s.", pic);
    assert(!copybook_parse(source, strlen(source), &layout, error)); assert(layout.size == len);
    unsigned char data[100]; assert(!field_encode(&layout.fields[0], value, data, error)); assert(!memcmp(data, expected, len));
    assert(!field_decode(&layout.fields[0], data, text, sizeof text, error)); assert(!strcmp(text, decoded));
}
static void reject(const char *source) {
    Layout layout; char error[512];
    if (!copybook_parse(source, strlen(source), &layout, error)) { fprintf(stderr, "unexpected parser success: %s\n", source); abort(); }
    assert(*error && !layout.count);
}
static void exhaustive_storage(void) {
    const char *pictures[] = {"S9(3) COMP-3", "9(3) COMP-3", "S9(2) COMP-3"};
    char source[100], error[512], text[80], expected[80];
    for (size_t profile = 0; profile < 3; profile++) {
        Layout layout;
        snprintf(source, sizeof source, "01 r. 05 f pic %s.", pictures[profile]);
        assert(!copybook_parse(source, strlen(source), &layout, error));
        Field *field = &layout.fields[0];
        assert(field->size == 2);
        for (unsigned bits = 0; bits < 65536; bits++) {
            unsigned a = bits >> 12, b = (bits >> 8) & 15U;
            unsigned c = (bits >> 4) & 15U, sign = bits & 15U;
            bool accepted = b < 10 && c < 10 &&
                (profile == 2 ? a == 0 : a < 10) &&
                (profile == 1 ? (sign == 12 || sign == 15) : (sign == 12 || sign == 13));
            unsigned char raw[2] = {(unsigned char)(bits >> 8), (unsigned char)bits};
            int status = field_decode(field, raw, text, sizeof text, error);
            assert((status == 0) == accepted);
            if (!accepted) continue;
            unsigned magnitude = a * 100 + b * 10 + c;
            snprintf(expected, sizeof expected, "%s%u", sign == 13 ? "-" : "", magnitude);
            assert(!strcmp(text, expected));
            /* Every accepted representation must encode back exactly, including
               negative zero and the explicitly selected unsigned C/F sign. */
            field->unsigned_packed_c = sign == 12;
            unsigned char encoded[2];
            assert(!field_encode(field, expected, encoded, error));
            assert(!memcmp(raw, encoded, sizeof raw));
            size_t required = strlen(expected) + 1;
            memset(text, '#', sizeof text);
            assert(!field_decode(field, raw, text, required, error));
            assert(text[required] == '#');
            assert(field_decode(field, raw, text, required - 1, error) < 0);
        }
    }
    for (unsigned signed_field = 0; signed_field < 2; signed_field++) {
        Layout layout;
        snprintf(source, sizeof source, "01 r. 05 f pic %s9.", signed_field ? "S" : "");
        assert(!copybook_parse(source, strlen(source), &layout, error));
        for (unsigned byte = 0; byte < 256; byte++) {
            bool negative = signed_field && byte >= 'p' && byte <= 'y';
            bool accepted = (byte >= '0' && byte <= '9') || negative;
            unsigned char raw = (unsigned char)byte;
            int status = field_decode(&layout.fields[0], &raw, text, sizeof text, error);
            assert((status == 0) == accepted);
            if (!accepted) continue;
            snprintf(expected, sizeof expected, "%s%u", negative ? "-" : "",
                byte - (negative ? (unsigned)'p' : (unsigned)'0'));
            assert(!strcmp(text, expected));
        }
    }
    puts("records: 196608 packed storage patterns and 512 DISPLAY bytes checked exhaustively; exact decimal values, re-encoding and decode capacity verified");
}
static void unit(void) {
    exhaustive_storage();
    Layout layout; char error[512]; assert(!copybook_parse(schema, strlen(schema), &layout, error));
    assert(layout.count == 8 && layout.size == 25); assert(layout.fields[7].offset == 22);
    assert(!strcmp(layout.fields[1].name, "REC.NUMBERS.UNSIGNED-DISPLAY"));
    const char *good = "*> free-format comment\n01 r\n05 g\n10 f picture is s999v99 usage display.\n05 x pic x ( 3 ).";
    assert(!copybook_parse(good, strlen(good), &layout, error)); assert(layout.size == 8);
    const char *bad[] = {
        "", "05 x pic 9.", "01 x pic 9.", "01 x.", "01 x. 05 g.", "01 x. 01 y. 05 f pic 9.",
        "01 x. 05 f pic 9. 10 child pic 9.", "01 x. 05 f pic X(0).", "01 x. 05 f pic 9(39).",
        "01 x. 05 f pic S9(38)V9.", "01 x. 05 f pic X(1025).", "01 x. 05 f pic X(3) COMP-3.",
        "01 x. 05 f pic 9 SIGN LEADING SEPARATE.", "01 x. 05 f pic 9(3) COMP-5.",
        "01 x. 05 f pic 9(3) USAGE BINARY.", "01 x. 05 f pic 9 OCCURS 3.",
        "01 x. 05 f REDEFINES y PIC 9.", "01 x. 05 f pic 9 VALUE 0.",
        "01 x. 05 f pic 9. 88 valid VALUE 1.", "01 x. COPY foo.", "000100 01 x. 05 f pic 9.",
        "01 x. 05 f pic 9.9.", "01 x. 05 f pic ZZ9.", "01 x. 05 f pic +999.", "01 x. 77 f pic 9.", "01 x. 05 bad- pic 9."
    };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) reject(bad[i]);
    unsigned char odd[] = {0x12, 0x3d}, even[] = {0x01, 0x23, 0x4c}, unsigned_even[] = {0x01, 0x23, 0x4f};
    vector("S9(3) COMP-3", "-123", odd, sizeof odd, "-123");
    vector("S9(4) COMP-3", "+1234", even, sizeof even, "1234");
    vector("9(4) COMP-3", "1234", unsigned_even, sizeof unsigned_even, "1234");
    Layout ibm;const char *unsigned_schema="01 r. 05 f pic 9(4) comp-3.";
    assert(!copybook_parse(unsigned_schema,strlen(unsigned_schema),&ibm,error));
    assert(!layout_set_unsigned_packed_sign(&ibm,"c",error));
    unsigned char ibm_bytes[3];char ibm_text[80];
    assert(!field_encode(&ibm.fields[0],"1234",ibm_bytes,error));assert(!memcmp(ibm_bytes,even,3));
    assert(!field_decode(&ibm.fields[0],even,ibm_text,sizeof ibm_text,error));assert(!strcmp(ibm_text,"1234"));
    assert(!field_decode(&ibm.fields[0],unsigned_even,ibm_text,sizeof ibm_text,error));
    assert(layout_set_unsigned_packed_sign(&ibm,"d",error)<0&&ibm.fields[0].unsigned_packed_c);
    ibm_bytes[2]=0x4d;assert(field_decode(&ibm.fields[0],ibm_bytes,ibm_text,sizeof ibm_text,error)<0);
    assert(!layout_set_unsigned_packed_sign(&ibm,"f",error));assert(!field_encode(&ibm.fields[0],"1234",ibm_bytes,error));assert(!memcmp(ibm_bytes,unsigned_even,3));
    unsigned char fractional[] = {0x00, 0x12, 0x3d}, zero[] = {0x00, 0x0c}, negzero[] = {0x00, 0x0d};
    vector("S9(3)V9(2) COMP-3", "-1.23", fractional, sizeof fractional, "-1.23");
    vector("S9(3) COMP-3", "0", zero, sizeof zero, "0");
    vector("S9(3) COMP-3", "-0", negzero, sizeof negzero, "-0");
    vector("S9(3)V9(2)", "-12.34", (const unsigned char *)"0123t", 5, "-12.34");
    vector("9(3)V9(2)", "1.2", (const unsigned char *)"00120", 5, "1.20");
    vector("X(4)", "AB", (const unsigned char *)"AB  ", 4, "AB  ");
    vector("9(38)", "99999999999999999999999999999999999999", (const unsigned char *)"99999999999999999999999999999999999999", 38, "99999999999999999999999999999999999999");
    const unsigned char packed38[] = {0x09,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x9c};
    vector("S9(38) COMP-3", "99999999999999999999999999999999999999", packed38, sizeof packed38, "99999999999999999999999999999999999999");
    assert(!copybook_parse(schema, strlen(schema), &layout, error));
    unsigned char record[25];
    for (size_t row = 0; row < 3; row++) {
        for (size_t i = 0; i < layout.count; i++) assert(!field_encode(&layout.fields[i], values[row][i], record + layout.fields[i].offset, error));
        assert(!record_validate(&layout, record, sizeof record, error));
    }
    assert(record_validate(&layout, record, 24, error) < 0);
    record[layout.fields[3].offset] = 0xfa; assert(record_validate(&layout, record, 25, error) < 0);
    Field *f = &layout.fields[1]; unsigned char out[50];
    const char *invalid[] = {"-1", "1000", "1.0", "1x", "", "+", ".5", "1e2", " 1"};
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; i++) assert(field_encode(f, invalid[i], out, error) < 0);
    f = &layout.fields[2]; assert(field_encode(f, "1.234", out, error) < 0);
    f = &layout.fields[4]; unsigned char corrupt[] = {0x11, 0x23, 0x4c}; char text[80];
    assert(field_decode(f, corrupt, text, sizeof text, error) < 0); corrupt[0] = 0; corrupt[2] = 0x4f;
    assert(field_decode(f, corrupt, text, sizeof text, error) < 0);
    assert(field_encode(&layout.fields[0], "too long", out, error) < 0);
    assert(field_decode(&layout.fields[0], (const unsigned char *)"ABCD", text, 4, error) < 0);
    assert(field_decode(&layout.fields[1], (const unsigned char *)"999", text, 1, error) < 0);
    puts("records: parser rejection cases, layout offsets, exact decimal vectors, signs/zeros/scales, round trips and invalid storage passed");
}
static void cobol(const char *cli, const char *fixture,const char *packed_sign) {
    char dir[] = "/tmp/beryl-codec-XXXXXX"; assert(mkdtemp(dir));
    char input[B_PATH], results[B_PATH], error[512]; assert(!path_join(input, dir, "vectors.bin")); assert(!path_join(results, dir, "results"));
    Layout layout; assert(!copybook_parse(schema, strlen(schema), &layout, error));
    assert(!layout_set_unsigned_packed_sign(&layout,packed_sign,error));
    FILE *f = fopen(input, "wb"); assert(f); unsigned char record[25];
    for (size_t row = 0; row < 3; row++) {
        for (size_t i = 0; i < layout.count; i++) assert(!field_encode(&layout.fields[i], values[row][i], record + layout.fields[i].offset, error));
        assert(fwrite(record, 1, sizeof record, f) == sizeof record);
    }
    for (int number = -999; number <= 999; number++) {
        int magnitude = number < 0 ? -number : number;
        char whole[16], scaled[16], tens[16], positive[16], positive_tens[16];
        snprintf(whole, sizeof whole, "%d", number);
        snprintf(scaled, sizeof scaled, "%s%d.%02d", number < 0 ? "-" : "", magnitude / 100, magnitude % 100);
        snprintf(tens, sizeof tens, "%d", number * 10);
        snprintf(positive, sizeof positive, "%d", magnitude);
        snprintf(positive_tens, sizeof positive_tens, "%d", magnitude * 10);
        const char *sweep[] = {"SWP", positive, scaled, whole, tens, positive, positive_tens, scaled};
        for (size_t i = 0; i < layout.count; i++)
            assert(!field_encode(&layout.fields[i], sweep[i], record + layout.fields[i].offset, error));
        assert(fwrite(record, 1, sizeof record, f) == sizeof record);
    }
    assert(!fclose(f));
    pid_t pid = fork(); assert(pid >= 0);
    if (!pid) { execl(cli, cli, "run", "--exec", fixture, "--input", input, "--expected", input, "--results", results, (char *)NULL); _exit(99); }
    int status; while (waitpid(pid, &status, 0) < 0) assert(errno == EINTR);
    assert(WIFEXITED(status) && !WEXITSTATUS(status));
    puts("COBOL: 2002 independent 25-byte storage vectors agree with C encoder, including the complete -999..999 sweep, scaled DISPLAY and odd/even packed digits");
}
int main(int argc, char **argv) {
    if ((argc == 4 || argc == 5) && !strcmp(argv[1], "--cobol")) cobol(argv[2], argv[3],argc==5?argv[4]:"f");
    else { assert(argc == 1); unit(); }
    return 0;
}
