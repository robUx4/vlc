#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H
#ifdef _MSC_VER
#pragma once

#define _LITTLE_ENDIAN  1234    // least-significant byte first (vax)
#define _BIG_ENDIAN      4321    // most-significant byte first (IBM, net)
#define _PDP_ENDIAN      3412    // LSB first in word, MSW first in long (pdp)

#if defined(vax) || defined(ns32000) || defined(sun386) || defined(MIPSEL) || \
    defined(BIT_ZERO_ON_RIGHT)
#define BYTE_ORDER _LITTLE_ENDIAN
#endif

#if defined(sel) || defined(pyr) || defined(mc68000) || defined(sparc) || \
    defined(is68k) || defined(tahoe) || defined(ibm032) || defined(ibm370) || \
    defined(MIPSEB) || defined (BIT_ZERO_ON_LEFT)
#define BYTE_ORDER _BIG_ENDIAN
#endif

#ifndef BYTE_ORDER      // still not defined
#if defined(u3b2) || defined(m68k)
#define BYTE_ORDER _BIG_ENDIAN
#endif

#if defined(i286) || defined(i386) || defined(_AMD64_) || defined(_IA64_) || defined(_ARM_) || defined(_ARM64_)
#define BYTE_ORDER _LITTLE_ENDIAN
#endif

#endif

#define MAXPATHLEN PATH_MAX

#endif /* _MSC_VER */
#endif /* _SYS_PARAM_H */
