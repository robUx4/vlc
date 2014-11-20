/*****************************************************************************
 * rand.c : non-predictible random bytes generator
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_rand.h>

#if VLC_WINSTORE_APP
# define COBJMACROS
# define INITGUID
# include <winstring.h>
# include <windows.security.cryptography.h>
# ifndef _MSC_VER /* roapi.h is a C++ include file that doesn't work with C files */
#  include <roapi.h>
# else /* _MSC_VER */
DECLSPEC_IMPORT HRESULT WINAPI RoGetActivationFactory(_In_ HSTRING activatableClassId, _In_ REFIID iid,  _COM_Outptr_ void ** factory);
typedef __x_ABI_CWindows_CSecurity_CCryptography_CICryptographicBufferStatics ICryptographicBufferStatics;
typedef __x_ABI_CWindows_CStorage_CStreams_CIBuffer IBuffer;
#   define ICryptographicBufferStatics_Release __x_ABI_CWindows_CSecurity_CCryptography_CICryptographicBufferStatics_Release
#   define ICryptographicBufferStatics_CopyToByteArray __x_ABI_CWindows_CSecurity_CCryptography_CICryptographicBufferStatics_CopyToByteArray
#   define IBuffer_Release __x_ABI_CWindows_CStorage_CStreams_CIBuffer_Release
#   define ICryptographicBufferStatics_GenerateRandom __x_ABI_CWindows_CSecurity_CCryptography_CICryptographicBufferStatics_GenerateRandom
DEFINE_GUID(IID_ICryptographicBufferStatics, 0x320b7e22, 0x3cb0, 0x4cdf, 0x86,0x63, 0x1d,0x28,0x91,0x00,0x65,0xeb);
# endif /* _MSC_VER */
#else
# include <wincrypt.h>
#endif

void vlc_rand_bytes (void *buf, size_t len)
{
    size_t count = len;
    uint8_t *p_buf = (uint8_t *)buf;

    /* fill buffer with pseudo-random data */
    while (count > 0)
    {
        unsigned int val;
        val = rand();
        if (count < sizeof (val))
        {
            memcpy (p_buf, &val, count);
            break;
        }

        memcpy (p_buf, &val, sizeof (val));
        count -= sizeof (val);
        p_buf += sizeof (val);
    }

#if VLC_WINSTORE_APP
    static const WCHAR *className = L"Windows.Security.Cryptography.CryptographicBuffer";
    const UINT32 clen = wcslen(className);

    HSTRING hClassName = NULL;
    HSTRING_HEADER header;
    HRESULT hr = WindowsCreateStringReference(className, clen, &header, &hClassName);
    if (hr) {
        WindowsDeleteString(hClassName);
        return;
    }

    ICryptographicBufferStatics *cryptoStatics = NULL;
    hr = RoGetActivationFactory(hClassName, &IID_ICryptographicBufferStatics, (void**)&cryptoStatics);
    WindowsDeleteString(hClassName);

    if (hr)
        return;

    IBuffer *buffer = NULL;
    hr = ICryptographicBufferStatics_GenerateRandom(cryptoStatics, len, &buffer);
    if (hr) {
        ICryptographicBufferStatics_Release(cryptoStatics);
        return;
    }

    UINT32 olength;
    unsigned char *rnd = NULL;
    hr = ICryptographicBufferStatics_CopyToByteArray(cryptoStatics, buffer, &olength, (BYTE**)&rnd);
    memcpy(buf, rnd, len);

    IBuffer_Release(buffer);
    ICryptographicBufferStatics_Release(cryptoStatics);
#else
    HCRYPTPROV hProv;
    /* acquire default encryption context */
    if( CryptAcquireContext(
        &hProv,                 // Variable to hold returned handle.
        NULL,                   // Use default key container.
        MS_DEF_PROV,            // Use default CSP.
        PROV_RSA_FULL,          // Type of provider to acquire.
        CRYPT_VERIFYCONTEXT) )  // Flag values
    {
        /* fill buffer with pseudo-random data, initial buffer content
           is used as auxiliary random seed */
        CryptGenRandom(hProv, len, buf);
        CryptReleaseContext(hProv, 0);
    }
#endif /* VLC_WINSTORE_APP */
}
