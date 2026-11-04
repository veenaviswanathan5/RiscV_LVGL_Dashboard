#ifndef RV_INTTYPES_H
#define RV_INTTYPES_H
#include <stdint.h>
#define PRId32 "d"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"
#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIx64 "llx"
#if __SIZEOF_POINTER__ == 8
#define PRIxPTR "lx"
#define PRIuPTR "lu"
#else
#define PRIxPTR "x"
#define PRIuPTR "u"
#endif
#endif
