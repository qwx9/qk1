#pragma once

#include <assert.h>
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
#include <sys/stat.h>
#include <unistd.h>

typedef unsigned char uchar;
typedef long long vlong;
typedef unsigned long long uvlong;
typedef uint8_t u8int;
typedef int16_t s16int;
typedef uint16_t u16int;
typedef int32_t s32int;
typedef uint32_t u32int;
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

extern char lasterr[256];
#define werrstr(fmt...) do{snprint(lasterr, sizeof(lasterr), fmt); }while(0)

char *seprint(char *, char *, char *, ...);
int nrand(int);
