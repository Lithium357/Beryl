#ifndef BERYL_COPYBOOK_H
#define BERYL_COPYBOOK_H
#include <stdbool.h>
#include <stddef.h>
#define B_FIELDS 256
#define B_DIGITS 38
#define B_RECORD_LIMIT 65536U
typedef enum { B_TEXT, B_DISPLAY, B_PACKED } FieldStorage;
typedef struct {
    char name[256];
    unsigned level, digits, scale;
    bool signed_value;
    FieldStorage storage;
    size_t offset, size;
} Field;
typedef struct { Field fields[B_FIELDS]; size_t count, size; } Layout;
/* Free-format declarations only. Errors include the source line and field. */
int copybook_parse(const char *source, size_t len, Layout *layout, char error[512]);
int copybook_load(const char *path, Layout *layout, char error[512]);
int copybook_parse_format(const char *source,size_t len,const char *format,Layout *layout,char error[512]);
#endif
