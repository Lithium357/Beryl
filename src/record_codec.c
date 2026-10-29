#include "beryl/record_codec.h"
#include <stdio.h>
#include <string.h>

static int bad(const Field *f, char error[512], const char *why) {
    snprintf(error, 512, "field %.255s: %s", f->name, why); return -1;
}
static bool valid(const Field *f) {
    if (f->storage == B_TEXT) return f->size > 0 && f->size <= 1024;
    return (f->storage == B_DISPLAY || f->storage == B_PACKED) &&
        f->digits > 0 && f->digits <= B_DIGITS && f->scale < f->digits &&
        f->size == (f->storage == B_PACKED ? (f->digits + 2U) / 2U : f->digits);
}
int layout_set_unsigned_packed_sign(Layout *layout,const char *sign,char error[512]) {
    if(strcmp(sign,"c")&&strcmp(sign,"f")){strcpy(error,"--unsigned-packed-sign must be c or f");return -1;}
    if(layout){
        if(layout->count>B_FIELDS){strcpy(error,"invalid field count");return -1;}
        for(size_t i=0;i<layout->count;i++)layout->fields[i].unsigned_packed_c=!strcmp(sign,"c");
    }
    return 0;
}
static int digits(const Field *f, const char *value, unsigned char out[B_DIGITS], bool *negative) {
    const char *p = value; *negative = false;
    if (*p == '-' || *p == '+') { *negative = *p == '-'; p++; }
    if (*negative && !f->signed_value) return -1;
    const char *start = p;
    while (*p >= '0' && *p <= '9') p++;
    if (p == start) return -1;
    const char *end = p;
    while (start + 1 < end && *start == '0') start++;
    size_t whole = (size_t)(end - start);
    if (whole > f->digits - f->scale) return -1;
    memset(out, 0, f->digits);
    size_t offset = f->digits - f->scale - whole;
    for (size_t i = 0; i < whole; i++) out[offset + i] = (unsigned char)(start[i] - '0');
    if (*p == '.') {
        if (!f->scale) return -1;
        const char *fraction = ++p;
        while (*p >= '0' && *p <= '9') p++;
        size_t n = (size_t)(p - fraction);
        if (!n || n > f->scale) return -1;
        for (size_t i = 0; i < n; i++) out[f->digits - f->scale + i] = (unsigned char)(fraction[i] - '0');
    }
    return *p ? -1 : 0;
}
int field_encode(const Field *f, const char *value, unsigned char *out, char error[512]) {
    if (!valid(f)) return bad(f, error, "invalid field descriptor");
    if (f->storage == B_TEXT) {
        size_t n = strlen(value);
        if (n > f->size) return bad(f, error, "text exceeds PIC X width");
        memset(out, ' ', f->size); memcpy(out, value, n); return 0;
    }
    unsigned char d[B_DIGITS]; bool negative;
    if (digits(f, value, d, &negative) < 0) return bad(f, error, "invalid exact decimal, sign, precision or scale");
    if (f->storage == B_DISPLAY) {
        for (unsigned i = 0; i < f->digits; i++) out[i] = (unsigned char)('0' + d[i]);
        if (negative) out[f->digits - 1] = (unsigned char)('p' + d[f->digits - 1]);
    } else {
        memset(out, 0, f->size);
        size_t pad = f->size * 2 - f->digits - 1;
        for (unsigned i = 0; i < f->digits; i++) {
            size_t nibble = i + pad;
            out[nibble / 2] |= (unsigned char)(d[i] << (nibble % 2 ? 0 : 4));
        }
        out[f->size - 1] |= (unsigned char)(f->signed_value ? (negative ? 0xd : 0xc) : f->unsigned_packed_c ? 0xc : 0xf);
    }
    return 0;
}
int field_decode(const Field *f, const unsigned char *data, char *value, size_t capacity, char error[512]) {
    if (!valid(f)) return bad(f, error, "invalid field descriptor");
    if (f->storage == B_TEXT) {
        if (capacity <= f->size) return bad(f, error, "text decode buffer too small");
        memcpy(value, data, f->size); value[f->size] = 0; return 0;
    }
    unsigned char d[B_DIGITS]; bool negative = false;
    if (f->storage == B_DISPLAY) {
        for (unsigned i = 0; i < f->digits; i++) {
            unsigned char c = data[i];
            if (i == f->digits - 1 && f->signed_value && c >= 'p' && c <= 'y') { c = (unsigned char)('0' + c - 'p'); negative = true; }
            if (c < '0' || c > '9') return bad(f, error, "invalid ASCII DISPLAY digit/sign");
            d[i] = (unsigned char)(c - '0');
        }
    } else {
        unsigned sign = data[f->size - 1] & 15U;
        if (f->signed_value ? (sign != 0xc && sign != 0xd) : (sign != 0xf && sign != 0xc))
            return bad(f, error, "unsupported COMP-3 sign nibble (canonical C/D signed or C/F unsigned required)");
        negative = sign == 0xd;
        size_t pad = f->size * 2 - f->digits - 1;
        if (pad && (data[0] >> 4)) return bad(f, error, "nonzero COMP-3 padding nibble");
        for (unsigned i = 0; i < f->digits; i++) {
            size_t nibble = i + pad;
            d[i] = (unsigned char)((data[nibble / 2] >> (nibble % 2 ? 0 : 4)) & 15);
            if (d[i] > 9) return bad(f, error, "invalid COMP-3 digit nibble");
        }
    }
    size_t first = 0, whole = f->digits - f->scale;
    while (first + 1 < whole && !d[first]) first++;
    size_t required = (negative ? 1U : 0U) + whole - first + (f->scale ? 1U + f->scale : 0U) + 1;
    if (capacity < required) return bad(f, error, "numeric decode buffer too small");
    size_t n = 0;
    if (negative) value[n++] = '-';
    for (size_t i = first; i < whole; i++) value[n++] = (char)('0' + d[i]);
    if (f->scale) { value[n++] = '.'; for (size_t i = whole; i < f->digits; i++) value[n++] = (char)('0' + d[i]); }
    value[n] = 0; return 0;
}
int record_validate(const Layout *layout, const unsigned char *record, size_t len, char error[512]) {
    if (!layout->count || layout->count > B_FIELDS || len != layout->size) { snprintf(error, 512, "record length %zu does not match layout %zu", len, layout->size); return -1; }
    for (size_t i = 0; i < layout->count; i++) {
        const Field *f = &layout->fields[i];
        if (f->offset > len || f->size > len - f->offset) return bad(f, error, "field outside record");
        if (f->storage == B_TEXT && valid(f)) continue;
        char value[B_DIGITS + 4];
        if (field_decode(f, record + f->offset, value, sizeof value, error) < 0) return -1;
    }
    return 0;
}
