#define _NO_CRT_STDIO_INLINE
#include <cstdio>

int __CRTDECL sscanf(
    _In_z_                       char const* const _Buffer,
    _In_z_ _Scanf_format_string_ char const* const _Format,
    ...)
{
    int _Result;
    va_list _ArgList;
    __crt_va_start(_ArgList, _Format);
    _Result = _vsscanf_l(_Buffer, _Format, NULL, _ArgList);
    __crt_va_end(_ArgList);
    return _Result;
}

int __CRTDECL _vsscanf_l(
    _In_z_                        char const* const _Buffer,
    _In_z_ _Printf_format_string_ char const* const _Format,
    _In_opt_                      _locale_t   const _Locale,
    va_list           _ArgList
)
{
    return __stdio_common_vsscanf(
        _CRT_INTERNAL_LOCAL_SCANF_OPTIONS,
        _Buffer, (size_t)-1, _Format, _Locale, _ArgList);
}
