#ifndef BERYL_RECORD_CODEC_H
#define BERYL_RECORD_CODEC_H
#include "beryl/copybook.h"
/* Numeric values are exact decimal strings. Output is fixed-scale, with no floating point. */
int field_encode(const Field *field, const char *value, unsigned char *out, char error[512]);
int field_decode(const Field *field, const unsigned char *data, char *value, size_t capacity, char error[512]);
int record_validate(const Layout *layout, const unsigned char *record, size_t len, char error[512]);
/* Select canonical unsigned packed output; NULL layout validates the option. */
int layout_set_unsigned_packed_sign(Layout *layout,const char *sign,char error[512]);
#endif
