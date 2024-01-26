#include <assert.h>
#include <ctype.h>
#include <endian.h>
#include <fcntl.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

typedef unsigned char uchar;
typedef long long vlong;
typedef unsigned long long uvlong;
typedef int8_t s8int;
typedef uint8_t u8int;
typedef int16_t s16int;
typedef uint16_t u16int;
typedef int32_t s32int;
typedef uint32_t u32int;
typedef int64_t s64int;
typedef uint64_t u64int;
typedef intptr_t intptr;
typedef uintptr_t uintptr;

#define nil NULL
#define USED(x) (void)(x)
#define nelem(x) (int)(sizeof(x)/sizeof((x)[0]))

#define sprint sprintf
#define snprint snprintf
#define vsnprint vsnprintf
#define cistrcmp strcasecmp
#define cistrncmp strncasecmp
#define getcallerpc(x) nil
#define getmalloctag(p) (USED(p), 0)
#define setmalloctag(p, t) do{USED(p); USED(t);}while(0)
#define setrealloctag(p, t) do{USED(p); USED(t);}while(0)

#define qctz(x) __builtin_ctz(x)

#ifndef BYTE_ORDER
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#define BYTE_ORDER __BYTE_ORDER
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define QUAKE_LITTLE_ENDIAN
#else
#define QUAKE_BIG_ENDIAN
#endif

extern char lasterr[256];
#define werrstr(fmt...) do{snprint(lasterr, sizeof(lasterr), fmt); }while(0)

char *seprint(char *, char *, char *, ...);

#define DotProduct(x,y) DotProduct_((x),(y))
