#ifdef _MSC_VER /* help visual studio compile vlc headers */

#ifndef __clang__
#error Clang compatibility header
#endif

#include "mscver_fixup.h"

#define _CRT_DECLARE_NONSTDC_NAMES 1
#include <corecrt.h>

#define STATIC_ASSERT_CONCAT_(a, b) a##b
#define STATIC_ASSERT_CONCAT(a, b) STATIC_ASSERT_CONCAT_(a, b)
#define _Static_assert(x, s) extern char STATIC_ASSERT_CONCAT(static_assert_, __LINE__)[sizeof(struct { unsigned:-!(x); })]
#define static_assert _Static_assert

#if 0 /* TODO */

//#define stat _stat64
//#define fstat(fd,out) _fstat64(fd,out)
/* NO extern int stricmp(char const * s1, char const * s2); */
//extern void* fdopen(int fd, char const* fmt);
extern int read(int fd, void * buf, unsigned int bufcount);
extern int write(int fd, void const * buf, unsigned int bufcount);
extern int close(int fd);
extern int setmode(int fd, int mode);
extern int dup(int fd);
/* NO extern void swab(char * b1, char * b2, int size); */
extern long lseek(int fd, long offset, int origin);
//extern int fileno(void* FILE);
#endif /* TODO */

#endif /* _MSC_VER */
