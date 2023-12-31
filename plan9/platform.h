#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <ctype.h>

#define RAND_MAX ((2<<15)-1)

#define acosf acos
#define asinf asin
#define atan2f atan2
#define atanf atan
#define ceilf ceil
#define cosf cos
#define floorf floor
#define sinf sin
#define sqrtf sqrt
#define tanf tan

#define __inline__ inline

#ifdef __mips__
#define QUAKE_BIG_ENDIAN
#else
# ifdef __power__
# define QUAKE_BIG_ENDIAN
# else
#  ifdef __power64__
#  define QUAKE_BIG_ENDIAN
#  else
#   ifdef __sparc__
#   define QUAKE_BIG_ENDIAN
#   else
#    ifdef __sparc64__
#    else
#    define QUAKE_LITTLE_ENDIAN
#    endif
#   endif
#  endif
# endif
#endif

typedef enum {false, true} bool;

static double ln2c;
#define exp2(x) (exp((x) * (ln2c ? ln2c : (ln2c = log(2.0)))))

int qctz(unsigned);
