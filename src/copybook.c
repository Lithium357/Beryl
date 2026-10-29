#include "beryl/copybook.h"
#include "beryl/beryl.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char text[128]; unsigned line; } Token;
typedef struct { Token *tokens; size_t count, index; char *error; } Parser;
typedef struct { unsigned level; char path[256]; bool elementary, child; } Group;

static int fail(Parser *p, const char *format, ...) {
    unsigned line = p->index < p->count ? p->tokens[p->index].line :
                    p->count ? p->tokens[p->count - 1].line : 1;
    int n = snprintf(p->error, 512, "line %u: ", line);
    va_list args;
    va_start(args, format);
    vsnprintf(p->error + n, 512 - (size_t)n, format, args);
    va_end(args);
    return -1;
}

static int lex(const char *s, size_t len, Parser *p) {
    size_t capacity = 128;
    p->tokens = malloc(capacity * sizeof *p->tokens);
    if (!p->tokens) return fail(p, "out of memory reading copybook");
    unsigned line = 1;
    for (size_t i = 0; i < len;) {
        unsigned char c = (unsigned char)s[i];
        if (!c || c >= 128) return fail(p, "only ASCII free-format copybook source is supported");
        if (isspace(c)) { if (c == '\n') line++; i++; continue; }
        if (c == '*' && i + 1 < len && s[i + 1] == '>') {
            while (i < len && s[i] != '\n') i++;
            continue;
        }
        size_t start = i++;
        if (isalnum(c) || c == '-') {
            while (i < len && (isalnum((unsigned char)s[i]) || s[i] == '-')) i++;
        } else if (c != '(' && c != ')' && c != '.') {
            p->index = p->count;
            return fail(p, "unexpected source byte 0x%02x on line %u; use free-format declarations", (unsigned)c, line);
        }
        if (i - start >= sizeof p->tokens[0].text) return fail(p, "token on line %u exceeds 127 bytes", line);
        if (p->count == capacity) {
            if (capacity == 32768) return fail(p, "copybook exceeds 32768 tokens");
            capacity *= 2;
            Token *more = realloc(p->tokens, capacity * sizeof *more);
            if (!more) return fail(p, "out of memory reading copybook");
            p->tokens = more;
        }
        Token *t = &p->tokens[p->count++]; t->line = line;
        for (size_t j = 0; j < i - start; j++) t->text[j] = (char)toupper((unsigned char)s[start + j]);
        t->text[i - start] = 0;
    }
    return 0;
}

static const char *peek(Parser *p) { return p->index < p->count ? p->tokens[p->index].text : ""; }
static bool take(Parser *p, const char *s) {
    if (strcmp(peek(p), s)) return false;
    p->index++; return true;
}
static int unsigned_token(const char *s, unsigned max, unsigned *out) {
    if (!*s) return -1;
    unsigned n = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9' || n > max / 10) return -1;
        n = n * 10 + (unsigned)(*s - '0');
        if (n > max) return -1;
    }
    *out = n; return 0;
}
static bool level_token(const char *s) {
    unsigned n;
    return strlen(s) == 2 && !unsigned_token(s, 99, &n);
}
static int repeat(const char **cursor, char symbol, unsigned max, unsigned *count) {
    const char *p = *cursor;
    unsigned n = 0;
    while (*p == symbol) { n++; p++; }
    if (!n) return -1;
    if (*p == '(') {
        if (n != 1) return -1;
        const char *start = ++p;
        while (*p >= '0' && *p <= '9') p++;
        if (*p != ')' || p == start || (size_t)(p - start) >= 12) return -1;
        char number[12]; memcpy(number, start, (size_t)(p - start)); number[p - start] = 0;
        if (unsigned_token(number, max, &n) < 0) return -1;
        p++;
    }
    if (!n || n > max) return -1;
    *cursor = p; *count = n; return 0;
}
static int picture(Parser *p, Field *f) {
    char pic[256] = ""; size_t used = 0; int depth = 0;
    while (*peek(p)) {
        const char *s = peek(p);
        if (used && !depth && (level_token(s) || !strcmp(s, ".") ||
            !strcmp(s, "USAGE") || !strcmp(s, "DISPLAY") || !strcmp(s, "COMP-3") ||
            !strcmp(s, "PACKED-DECIMAL"))) break;
        if (strspn(s, "S9VX012345678()") != strlen(s)) break;
        if (used + strlen(s) >= sizeof pic) return fail(p, "field %s: PIC exceeds 255 characters", f->name);
        if (!strcmp(s, "(")) depth++;
        if (!strcmp(s, ")")) depth--;
        if (depth < 0) return fail(p, "field %s: unbalanced PIC", f->name);
        strcpy(pic + used, s); used += strlen(s); p->index++;
    }
    const char *s = pic;
    if (*s == 'X') {
        unsigned width;
        if (repeat(&s, 'X', 1024, &width) < 0 || *s) return fail(p, "field %s: unsupported PIC %s", f->name, pic);
        f->storage = B_TEXT; f->size = width; return 0;
    }
    if (*s == 'S') { f->signed_value = true; s++; }
    unsigned whole = 0, scale = 0;
    if (repeat(&s, '9', B_DIGITS, &whole) < 0) return fail(p, "field %s: unsupported PIC %s", f->name, pic);
    if (*s == 'V') {
        s++;
        if (repeat(&s, '9', B_DIGITS, &scale) < 0) return fail(p, "field %s: invalid implied-decimal PIC %s", f->name, pic);
    }
    if (*s || whole + scale > B_DIGITS || depth) return fail(p, "field %s: unsupported PIC %s (maximum 38 digits)", f->name, pic);
    f->storage = B_DISPLAY; f->digits = whole + scale; f->scale = scale; f->size = f->digits;
    return 0;
}

int copybook_parse(const char *source, size_t len, Layout *layout, char error[512]) {
    memset(layout, 0, sizeof *layout); error[0] = 0;
    Parser p = {NULL, 0, 0, error}; Group groups[49]; size_t depth = 0; int rc = -1;
    if (len > B_ENV_LIMIT) { snprintf(error, 512, "copybook exceeds 1 MiB"); return -1; }
    if (lex(source, len, &p) < 0) goto done;
    while (*peek(&p)) {
        unsigned level;
        if (strlen(peek(&p)) != 2 || unsigned_token(peek(&p), 49, &level) < 0 || !level)
            { fail(&p, "unsupported level or feature '%s'; use one level 01 group with levels 02..49", peek(&p)); goto done; }
        p.index++;
        if (!depth && level != 1) { fail(&p, "record must start with a level 01 group"); goto done; }
        if (depth && level == 1) { fail(&p, "multiple level 01 records are unsupported"); goto done; }
        const char *name = peek(&p);
        if (!isalpha((unsigned char)*name) || strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != strlen(name) || strlen(name) > 63 || name[strlen(name) - 1] == '-')
            { fail(&p, "invalid field name '%s' (maximum 63 characters)", name); goto done; }
        char path[256];
        while (depth && level <= groups[depth - 1].level) {
            if (!groups[depth - 1].elementary && !groups[depth - 1].child)
                { fail(&p, "group %s is empty", groups[depth - 1].path); goto done; }
            depth--;
        }
        if (depth && groups[depth - 1].elementary) { fail(&p, "elementary field %s cannot contain children", groups[depth - 1].path); goto done; }
        int n = depth ? snprintf(path, sizeof path, "%s.%s", groups[depth - 1].path, name) : snprintf(path, sizeof path, "%s", name);
        if (n < 0 || n >= (int)sizeof path) { fail(&p, "qualified field path exceeds 255 bytes"); goto done; }
        if (depth) groups[depth - 1].child = true;
        p.index++;
        Field f = {0}; strcpy(f.name, path); f.level = level;
        bool elementary = take(&p, "PIC") || take(&p, "PICTURE");
        if (elementary) {
            if (level == 1) { fail(&p, "level 01 must be a group"); goto done; }
            (void)take(&p, "IS");
            if (picture(&p, &f) < 0) goto done;
            bool usage = take(&p, "USAGE");
            if (usage) (void)take(&p, "IS");
            if (take(&p, "COMP-3") || take(&p, "PACKED-DECIMAL")) {
                if (f.storage == B_TEXT) { fail(&p, "field %s: COMP-3 requires a numeric PIC", f.name); goto done; }
                f.storage = B_PACKED; f.size = (f.digits + 2U) / 2U;
            } else if (!take(&p, "DISPLAY") && usage) { fail(&p, "field %s: unsupported USAGE '%s'", f.name, peek(&p)); goto done; }
            if (layout->count == B_FIELDS || f.size > B_RECORD_LIMIT - layout->size)
                { fail(&p, "record exceeds 256 elementary fields or 65536 bytes"); goto done; }
            f.offset = layout->size; layout->size += f.size; layout->fields[layout->count++] = f;
        }
        if (!take(&p, ".") && *peek(&p) && !level_token(peek(&p))) {
            fail(&p, "field %s: unsupported clause '%s'; REDEFINES, OCCURS, SIGN, VALUE, COPY and edited/binary PICs are unsupported", path, peek(&p)); goto done;
        }
        if (depth == 49) { fail(&p, "group nesting exceeds 49 levels"); goto done; }
        groups[depth].level = level; strcpy(groups[depth].path, path);
        groups[depth].elementary = elementary; groups[depth].child = false; depth++;
    }
    for (size_t i = 0; i < depth; i++) if (!groups[i].elementary && !groups[i].child)
        { fail(&p, "group %s is empty", groups[i].path); goto done; }
    if (!layout->count) { fail(&p, "copybook has no elementary fields"); goto done; }
    rc = 0;
done:
    free(p.tokens); if (rc < 0) memset(layout, 0, sizeof *layout); return rc;
}

int copybook_load(const char *path, Layout *layout, char error[512]) {
    Bytes source = {0};
    if (read_bytes(path, B_ENV_LIMIT, &source) < 0 || source.truncated) {
        snprintf(error, 512, "cannot read copybook %.350s (regular file, maximum 1 MiB required)", path);
        bytes_free(&source); return -1;
    }
    int rc = copybook_parse((const char *)source.data, source.len, layout, error);
    bytes_free(&source); return rc;
}
int copybook_parse_format(const char *source,size_t len,const char *format,Layout *layout,char error[512]) {
    if(!strcmp(format,"free"))return copybook_parse(source,len,layout,error);
    memset(layout,0,sizeof *layout);
    if(strcmp(format,"fixed")){strcpy(error,"--format must be free or fixed");return -1;}
    if(len>B_ENV_LIMIT){strcpy(error,"copybook exceeds 1 MiB");return -1;}
    char *normalized=malloc(len+1);if(!normalized){strcpy(error,"out of memory normalizing fixed-format copybook");return -1;}
    size_t used=0;unsigned line=1;int rc=-1;
    for(size_t at=0;at<len;line++) {
        size_t end=at;while(end<len&&source[end]!='\n')end++;size_t width=end-at;if(width&&source[at+width-1]=='\r')width--;
        bool blank=true;for(size_t i=0;i<width;i++){unsigned char c=(unsigned char)source[at+i];if(c!=' ')blank=false;if(c<32||c>=127){snprintf(error,512,"line %u: fixed format requires ASCII without tabs/control bytes",line);goto done;}}
        if(!blank) {
            if(width<7){snprintf(error,512,"line %u: fixed-format indicator column 7 is missing",line);goto done;}
            for(size_t i=0;i<6;i++)if(source[at+i]!=' '&&(source[at+i]<'0'||source[at+i]>'9')){snprintf(error,512,"line %u: invalid sequence area in columns 1..6",line);goto done;}
            char indicator=source[at+6];
            if(indicator!='*'&&indicator!='/') {
                if(indicator!=' '){snprintf(error,512,"line %u: unsupported fixed-format indicator '%c' (continuation/debug lines require compiler preprocessing)",line,indicator);goto done;}
                size_t stop=width<72?width:72;
                for(size_t i=7;i<stop;i++)normalized[used++]=source[at+i];
            }
        }
        normalized[used++]='\n';at=end<len?end+1:end;
    }
    rc=copybook_parse(normalized,used,layout,error);
done:free(normalized);return rc;
}
