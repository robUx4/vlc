#ifndef UNISTD_H__
# define UNISTD_H__

#if defined(__clang__)
# if __has_include_next(<unistd.h>)
#  include_next <unistd.h>
# else
#  include <io.h>
# endif
#else
# include <io.h>
#endif

#endif
