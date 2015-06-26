/*****************************************************************************
 * d3d11_surface.c : D3D11 GPU surface conversion module for vlc
 *****************************************************************************
 * Copyright © 2015 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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

#include "dxgi.h"

typedef struct
{
    const char   *name;
    DXGI_FORMAT  format;
} dxgi_format_t;

static const dxgi_format_t dxgi_formats[] = {
    { "NV12",        DXGI_FORMAT_NV12                },
    { "I420_OPAQUE", DXGI_FORMAT_420_OPAQUE          },
    { "RGBA",        DXGI_FORMAT_R8G8B8A8_UNORM      },
    { "RGBA_SRGB",   DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
    { "BGRX",        DXGI_FORMAT_B8G8R8X8_UNORM      },
    { "BGRA",        DXGI_FORMAT_B8G8R8A8_UNORM      },
    { "BGRA_SRGB",   DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
    { "AYUV",        DXGI_FORMAT_AYUV                },
    { "YUY2",        DXGI_FORMAT_YUY2                },
    { "AI44",        DXGI_FORMAT_AI44                },
    { "P8",          DXGI_FORMAT_P8                  },
    { "A8P8",        DXGI_FORMAT_A8P8                },
    { "UNKNOWN",     DXGI_FORMAT_UNKNOWN             },

    { NULL, 0,}
};

const char *DxgiFormatToStr(DXGI_FORMAT format)
{
    for (const dxgi_format_t *f = dxgi_formats; f->name != NULL; ++f)
    {
        if (f->format == format)
            return f->name;
    }
    return NULL;
}
