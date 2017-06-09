#ifndef __cplusplus

#if defined(__clang__) && __has_include_next(<stdalign.h>)
#  include_next <stdalign.h>
#else
# define memory_order_relaxed   0
# define alignof(x) __alignof(x)
#endif

#endif /* __cplusplus */
