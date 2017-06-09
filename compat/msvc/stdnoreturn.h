#ifndef __cplusplus

#if defined(__clang__) && __has_include_next(<stdnoreturn.h>)
#  include_next <stdnoreturn.h>
#else
# include <process.h>
# define noreturn __declspec(noreturn)
#endif

#endif /* __cplusplus */
