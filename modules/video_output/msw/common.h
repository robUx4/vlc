/*****************************************************************************
 * common.h: Windows video output header file
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Damien Fouilleul <damienf@videolan.org>
 *          Martell Malone <martellmalone@gmail.com>
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

#ifdef MODULE_NAME_IS_directdraw
# include <ddraw.h>
#endif
#ifdef MODULE_NAME_IS_direct3d11
# include <d3d11.h>
# if VLC_WINSTORE_APP
#  include <dxgi1_2.h>
# else
#  include <dxgi.h>
#endif
# include <d3dcompiler.h>
#endif
#ifdef MODULE_NAME_IS_direct3d9
# include <d3d9.h>
# include <d3dx9effect.h>
#endif
#ifdef MODULE_NAME_IS_glwin32
# include "../opengl.h"
#endif
#ifdef MODULE_NAME_IS_direct2d
# include <d2d1.h>
#endif

/*****************************************************************************
 * event_thread_t: event thread
 *****************************************************************************/
#include "events.h"

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the module specific properties of an output thread.
 *****************************************************************************/
struct vout_display_sys_t
{
    /* */
    event_thread_t *event;

    /* */
    HWND                 hwnd;                  /* Handle of the main window */
    HWND                 hvideownd;        /* Handle of the video sub-window */
    struct vout_window_t *parent_window;         /* Parent window VLC object */
    HWND                 hparent;             /* Handle of the parent window */
    HWND                 hfswnd;          /* Handle of the fullscreen window */

    /* size of the display */
    RECT         rect_display;

    /* size of the overall window (including black bands) */
    RECT         rect_parent;

    unsigned changes;        /* changes made to the video display */

    /* Misc */
    bool is_first_display;
    bool is_on_top;

    /* screensaver system settings to be restored when vout is closed */
    UINT i_spi_screensaveactive;

    /* Coordinates of src and dest images (used when blitting to display) */
    RECT         rect_src;
    RECT         rect_src_clipped;
    RECT         rect_dest;
    RECT         rect_dest_clipped;

    picture_pool_t *pool;

#ifdef MODULE_NAME_IS_directdraw
    /* Multi-monitor support */
    HMONITOR             hmonitor;          /* handle of the current monitor */
    GUID                 *display_driver;

    /* Overlay alignment restrictions */
    int          i_align_src_boundary;
    int          i_align_src_size;
    int          i_align_dest_boundary;
    int          i_align_dest_size;

    bool   use_wallpaper;   /* show as desktop wallpaper ? */

    bool   use_overlay;     /* Are we using an overlay surface */
    bool   restore_overlay;

    /* DDraw capabilities */
    bool            can_blit_fourcc;

    uint32_t        i_rgb_colorkey;      /* colorkey in RGB used by the overlay */
    uint32_t        i_colorkey;                 /* colorkey used by the overlay */

    COLORREF        color_bkg;
    COLORREF        color_bkgtxt;

    LPDIRECTDRAW2        ddobject;                    /* DirectDraw object */
    LPDIRECTDRAWSURFACE2 display;                        /* Display device */
    LPDIRECTDRAWCLIPPER  clipper;             /* clipper used for blitting */
    HINSTANCE            hddraw_dll;       /* handle of the opened ddraw dll */

    picture_sys_t        *picsys;

    /* It protects the following variables */
    vlc_mutex_t    lock;
    bool           ch_wallpaper;
    bool           wallpaper_requested;
#endif

#ifdef MODULE_NAME_IS_glwin32
    HDC                   hGLDC;
    HGLRC                 hGLRC;
    vlc_gl_t              gl;
    vout_display_opengl_t *vgl;
    HDC                   affinityHDC; // DC for the selected GPU
#endif

#ifdef MODULE_NAME_IS_direct2d
    HINSTANCE              d2_dll;            /* handle of the opened d2d1 dll */
    ID2D1Factory           *d2_factory;                         /* D2D factory */
    ID2D1HwndRenderTarget  *d2_render_target;          /* D2D rendering target */
    ID2D1Bitmap            *d2_bitmap;                            /* D2 bitmap */
#endif

#ifdef MODULE_NAME_IS_direct3d11
#if !VLC_WINSTORE_APP
    HINSTANCE                hdxgi_dll;        /* handle of the opened dxgi dll */
    HINSTANCE                hd3d11_dll;       /* handle of the opened d3d11 dll */
    HINSTANCE                hd3dcompiler_dll; /* handle of the opened d3dcompiler dll */
    IDXGIAdapter             *dxgiadapter;     /* DXGI adapter */
    IDXGIFactory             *dxgifactory;     /* DXGI factory */
    IDXGISwapChain           *dxgiswapChain;   /* DXGI 1.0 swap chain */
    /* We should find a better way to store this or atleast a shorter name */
    PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN OurD3D11CreateDeviceAndSwapChain;
    PFN_D3D11_CREATE_DEVICE                OurD3D11CreateDevice;
    pD3DCompile                            OurD3DCompile;
#else
    IDXGISwapChain1          *dxgiswapChain;   /* DXGI 1.1 swap chain */
#endif
    ID3D11Device             *d3ddevice;       /* D3D device */
    ID3D11DeviceContext      *d3dcontext;      /* D3D context */
    ID3D11RenderTargetView   *d3drenderTargetView[2];
    ID3D11DepthStencilView   *d3ddepthStencilView;
    ID3D11VertexShader       *d3dvertexShader;
    ID3D11PixelShader        *d3dpixelShader;
    ID3D11InputLayout        *d3dvertexLayout;
    ID3D11SamplerState       *d3dsampState;
    picture_sys_t            *picsys;
    D3D_FEATURE_LEVEL        d3dfeaturelevel;

    /* Y/RGB picture */
    ID3D11Buffer             *pVertexBuffer;
    ID3D11Buffer             *pIndexBuffer;
    ID3D11Texture2D          *d3dtexture;
    ID3D11ShaderResourceView *d3dresViewY;
    ID3D11ShaderResourceView *d3dresViewUV;

    DXGI_FORMAT              d3dFormatTex;
    DXGI_FORMAT              d3dFormatY;
    DXGI_FORMAT              d3dFormatUV;
    vlc_fourcc_t             vlcFormat;
    const char               *d3dPxShader;

    ID3D11DepthStencilState  *pDepthStencilState;

    // SPU
    vlc_fourcc_t             pSubpictureChromas[2];
    ID3D11Texture2D          *spuStagingTexture;
    DXGI_FORMAT              d3dregion_format;
    int                      d3dregion_count;
    struct d3d_picture_t     *d3dregions;
#endif

#ifdef MODULE_NAME_IS_direct3d9
    bool allow_hw_yuv;    /* Should we use hardware YUV->RGB conversions */
    /* show video on desktop window ? */
    bool use_desktop;
    struct {
        bool is_fullscreen;
        bool is_on_top;
        RECT win;
    } desktop_save;
    vout_display_cfg_t cfg_saved; /* configuration used before going into desktop mode */

    // core objects
    HINSTANCE               hd3d9_dll;       /* handle of the opened d3d9 dll */
    HINSTANCE               hd3d9x_dll;      /* handle of the opened d3d9x dll */
    IDirect3DPixelShader9*  d3dx_shader;
    LPDIRECT3D9             d3dobj;
    D3DCAPS9                d3dcaps;
    LPDIRECT3DDEVICE9       d3ddev;
    D3DPRESENT_PARAMETERS   d3dpp;

    // scene objects
    LPDIRECT3DTEXTURE9      d3dtex;
    LPDIRECT3DVERTEXBUFFER9 d3dvtc;
    D3DFORMAT               d3dregion_format;
    int                     d3dregion_count;
    struct d3d_region_t     *d3dregion;

    picture_sys_t           *picsys;

    /* */
    bool                    reset_device;
    bool                    reopen_device;
    bool                    lost_not_ready;
    bool                    clear_scene;

    /* It protects the following variables */
    vlc_mutex_t    lock;
    bool           ch_desktop;
    bool           desktop_requested;
#endif

#if defined(MODULE_NAME_IS_wingdi)
    int  i_depth;

    /* Our offscreen bitmap and its framebuffer */
    HDC        off_dc;
    HBITMAP    off_bitmap;

    struct
    {
        BITMAPINFO bitmapinfo;
        RGBQUAD    red;
        RGBQUAD    green;
        RGBQUAD    blue;
    };
#endif
};

/*****************************************************************************
 * Prototypes from common.c
 *****************************************************************************/
int  CommonInit(vout_display_t *);
void CommonClean(vout_display_t *);
void CommonManage(vout_display_t *);
int  CommonControl(vout_display_t *, int , va_list );
void CommonDisplay(vout_display_t *);
int  CommonUpdatePicture(picture_t *, picture_t **, uint8_t *, unsigned);

void UpdateRects (vout_display_t *,
                  const vout_display_cfg_t *,
                  const video_format_t *,
                  bool is_forced);
void AlignRect(RECT *, int align_boundary, int align_size);

picture_pool_t *CommonPool(vout_display_t *, unsigned);

/*****************************************************************************
 * Constants
 *****************************************************************************/
#define IDM_TOGGLE_ON_TOP WM_USER + 1
#define DX_POSITION_CHANGE 0x1000
#define DX_WALLPAPER_CHANGE 0x2000
#define DX_DESKTOP_CHANGE 0x4000
