#ifdef _MSC_VER /* help visual studio compile vlc headers */
/* common header for cl.exe and clang.exe */

#include <basetsd.h>
typedef SSIZE_T ssize_t;

typedef unsigned int mode_t;

// Stats part

#define	_S_IFBLK	0x3000	/* Block: Is this ever set under w32? */

#define _S_IFMT 0xF000
#define _S_IFDIR 0x4000
#define _S_IFCHR 0x2000
#define _S_IFIFO 0x1000
#define _S_IFREG 0x8000
#define _S_IREAD 0x0100
#define _S_IWRITE 0x0080
#define _S_IEXEC 0x0040

#define S_IFMT _S_IFMT
#define S_IFDIR _S_IFDIR
#define S_IFCHR _S_IFCHR
#define S_IFREG _S_IFREG
#define S_IREAD _S_IREAD
#define S_IWRITE _S_IWRITE
#define S_IEXEC _S_IEXEC
#define	S_IFIFO		_S_IFIFO
#define	S_IFBLK		_S_IFBLK

#define	_S_IRWXU	(_S_IREAD | _S_IWRITE | _S_IEXEC)
#define	_S_IXUSR	_S_IEXEC
#define	_S_IWUSR	_S_IWRITE

#define	S_IRWXU		_S_IRWXU
#define	S_IXUSR		_S_IXUSR
#define	S_IWUSR		_S_IWUSR
#define	S_IRUSR		_S_IRUSR
#define	_S_IRUSR	_S_IREAD

#define SHUT_RD      SD_RECEIVE
#define SHUT_WR      SD_SEND
#define SHUT_RDWR    SD_BOTH

#define O_RDONLY  _O_RDONLY
#define O_WRONLY  _O_WRONLY
#define O_CREAT   _O_CREAT
#define O_TRUNC   _O_TRUNC
#define O_BINARY  _O_BINARY
#define O_APPEND  _O_APPEND
#define O_EXCL    _O_EXCL
#define O_RDWR    _O_RDWR
#define O_TEXT    _O_TEXT
#define O_NOINHERIT _O_NOINHERIT

#define  S_ISFIFO(m)  (((m) & S_IFMT) == S_IFIFO)
#define  S_ISCHR(m)   (((m) & S_IFMT) == S_IFCHR)
#define  S_ISBLK(m)   (((m) & S_IFMT) == S_IFBLK)

# ifndef S_ISREG
#  define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
# endif

#ifndef S_ISDIR
# define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef strcasecmp
# define HAVE_STRCASECMP  /* bogus autoconf detection using a define */
# define strcasecmp _stricmp
#endif
#ifndef strncasecmp
# define HAVE_STRNCASECMP /* bogus autoconf detection using a define */
# define strncasecmp _strnicmp
#endif
#ifndef snwprintf
# define HAVE_SNWPRINTF   /* bogus autoconf detection using a define */
# define snwprintf _snwprintf
#endif

#ifndef SSIZE_MAX
#ifdef _WIN64
#define SSIZE_MAX _I64_MAX
#else
#define SSIZE_MAX INT_MAX
#endif
#endif

/* make sure math.h defines the values we want */
#define _USE_MATH_DEFINES

#endif /* _MSC_VER */
