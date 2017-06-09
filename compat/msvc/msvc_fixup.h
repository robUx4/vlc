#ifdef _MSC_VER /* help visual studio compile vlc headers */

#include <xkeycheck.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif /* WIN32_LEAN_AND_MEAN */

#include "ms_fixup.h"

#if 0
#ifndef NOUSER
#define NOUSER /* avoid bogus pragma push/pop */
typedef struct tagMSG MSG, *LPMSG;
#endif /* NOUSER */
#endif

#ifndef WIN32
#define WIN32
#endif /* WIN32 */

#if _MSC_VER >= 1900 && !defined(__STDC_VERSION__)
#define __STDC_VERSION__ 199901L
#endif

# if !defined(snprintf) && _MSC_VER < 1900
#  define HAVE_SNPRINTF /* bogus autoconf detection using a define */
#  define snprintf _snprintf
# endif
# if !defined(strdup) && _MSC_VER < 1900
#  define HAVE_STRDUP /* bogus autoconf detection using a define */
# define strdup _strdup
#endif

//# define N_(x) x
//# define _(x) x

#if _MSC_VER < 1900
# ifndef __cplusplus
#  define inline __inline
# endif
#endif
#define __inline__ __inline

# define alloca _alloca
# define bzero(a,b) memset( (a), 0, (b) )
# define bcopy memcpy
# define bcmp memcmp

#define restrict 

#define __func__ __FUNCDNAME__
# if !defined(vsnprintf) && !defined(__cplusplus) && _MSC_VER < 1900
#  define HAVE_VSNPRINTF /* bogus autoconf detection using a define */
#  define vsnprintf _vsnprintf
# endif

#define NOMINMAX

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif /* _CRT_SECURE_NO_WARNINGS */

#ifndef _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#endif /* _CRT_NONSTDC_NO_WARNINGS */

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif /* _USE_MATH_DEFINES */

#define PATH_MAX MAX_PATH

/* avoid compilation issues in ARM */
#ifndef _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE
#define _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE 1
#endif

#if defined(_DEBUG) && 0
#define _CRTDBG_MAP_ALLOC
#endif
#include <stdlib.h>
#if defined(_DEBUG) && 0
#include <crtdbg.h>
//#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
//#define new DEBUG_NEW
#endif

#if _MSC_VER < 1900
#pragma warning(disable:4201) /* nameless structs */
#endif /* _MSC_VER */

#endif
