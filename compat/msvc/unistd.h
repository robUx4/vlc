#ifndef UNISTD_H__
# define UNISTD_H__

#if defined(__clang__) && __has_include_next(<unistd.h>)
# include_next <unistd.h>
#else
# include <io.h>
#endif
#include <direct.h>

#endif
