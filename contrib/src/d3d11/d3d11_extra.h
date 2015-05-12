#ifndef __d3d11_extra_h__
#define __d3d11_extra_h__

#include <d3d11.h>

/* Forward declarations */

#ifndef __ID3D11DeviceContext_FWD_DEFINED__
#define __ID3D11DeviceContext_FWD_DEFINED__
typedef interface ID3D11DeviceContext ID3D11DeviceContext;
#endif

#ifndef __ID3D11Device_FWD_DEFINED__
#define __ID3D11Device_FWD_DEFINED__
typedef interface ID3D11Device ID3D11Device;
#endif

#ifndef __ID3D11VideoDecoderOutputView_FWD_DEFINED__
#define __ID3D11VideoDecoderOutputView_FWD_DEFINED__
typedef interface ID3D11VideoDecoderOutputView ID3D11VideoDecoderOutputView;
#endif

#ifndef __ID3D11VideoDecoder_FWD_DEFINED__
#define __ID3D11VideoDecoder_FWD_DEFINED__
typedef interface ID3D11VideoDecoder ID3D11VideoDecoder;
#endif

#ifndef __ID3D11VideoContext_FWD_DEFINED__
#define __ID3D11VideoContext_FWD_DEFINED__
typedef interface ID3D11VideoContext ID3D11VideoContext;
#endif

#ifndef __ID3D11VideoDevice_FWD_DEFINED__
#define __ID3D11VideoDevice_FWD_DEFINED__
typedef interface ID3D11VideoDevice ID3D11VideoDevice;
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef enum D3D11_VDOV_DIMENSION {
    D3D11_VDOV_DIMENSION_UNKNOWN = 0,
    D3D11_VDOV_DIMENSION_TEXTURE2D = 1
} D3D11_VDOV_DIMENSION;
typedef struct D3D11_TEX2D_VDOV {
    UINT ArraySlice;
} D3D11_TEX2D_VDOV;
typedef struct D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC {
    GUID DecodeProfile;
    D3D11_VDOV_DIMENSION ViewDimension;
    __C89_NAMELESS union {
        D3D11_TEX2D_VDOV Texture2D;
    } __C89_NAMELESSUNIONNAME;
} D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC;
/*****************************************************************************
 * ID3D11VideoDecoderOutputView interface
 */
#ifndef __ID3D11VideoDecoderOutputView_INTERFACE_DEFINED__
#define __ID3D11VideoDecoderOutputView_INTERFACE_DEFINED__

DEFINE_GUID(IID_ID3D11VideoDecoderOutputView, 0xc2931aea, 0x2a85, 0x4f20, 0x86,0x0f, 0xfb,0xa1,0xfd,0x25,0x6e,0x18);
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("c2931aea-2a85-4f20-860f-fba1fd256e18")
ID3D11VideoDecoderOutputView : public ID3D11View
{
    virtual void STDMETHODCALLTYPE GetDesc(
        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc) = 0;

};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ID3D11VideoDecoderOutputView, 0xc2931aea, 0x2a85, 0x4f20, 0x86,0x0f, 0xfb,0xa1,0xfd,0x25,0x6e,0x18)
#endif
#else
typedef struct ID3D11VideoDecoderOutputViewVtbl {
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ID3D11VideoDecoderOutputView* This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        ID3D11VideoDecoderOutputView* This);

    ULONG (STDMETHODCALLTYPE *Release)(
        ID3D11VideoDecoderOutputView* This);

    /*** ID3D11DeviceChild methods ***/
    void (STDMETHODCALLTYPE *GetDevice)(
        ID3D11VideoDecoderOutputView* This,
        ID3D11Device **ppDevice);

    HRESULT (STDMETHODCALLTYPE *GetPrivateData)(
        ID3D11VideoDecoderOutputView* This,
        REFGUID guid,
        UINT *pDataSize,
        void *pData);

    HRESULT (STDMETHODCALLTYPE *SetPrivateData)(
        ID3D11VideoDecoderOutputView* This,
        REFGUID guid,
        UINT DataSize,
        const void *pData);

    HRESULT (STDMETHODCALLTYPE *SetPrivateDataInterface)(
        ID3D11VideoDecoderOutputView* This,
        REFGUID guid,
        const IUnknown *pData);

    /*** ID3D11View methods ***/
    void (STDMETHODCALLTYPE *GetResource)(
        ID3D11VideoDecoderOutputView* This,
        ID3D11Resource **ppResource);

    /*** ID3D11VideoDecoderOutputView methods ***/
    void (STDMETHODCALLTYPE *GetDesc)(
        ID3D11VideoDecoderOutputView* This,
        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc);

    END_INTERFACE
} ID3D11VideoDecoderOutputViewVtbl;
interface ID3D11VideoDecoderOutputView {
    CONST_VTBL ID3D11VideoDecoderOutputViewVtbl* lpVtbl;
};

#ifdef COBJMACROS
#ifndef WIDL_C_INLINE_WRAPPERS
/*** IUnknown methods ***/
#define ID3D11VideoDecoderOutputView_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ID3D11VideoDecoderOutputView_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ID3D11VideoDecoderOutputView_Release(This) (This)->lpVtbl->Release(This)
/*** ID3D11DeviceChild methods ***/
#define ID3D11VideoDecoderOutputView_GetDevice(This,ppDevice) (This)->lpVtbl->GetDevice(This,ppDevice)
#define ID3D11VideoDecoderOutputView_GetPrivateData(This,guid,pDataSize,pData) (This)->lpVtbl->GetPrivateData(This,guid,pDataSize,pData)
#define ID3D11VideoDecoderOutputView_SetPrivateData(This,guid,DataSize,pData) (This)->lpVtbl->SetPrivateData(This,guid,DataSize,pData)
#define ID3D11VideoDecoderOutputView_SetPrivateDataInterface(This,guid,pData) (This)->lpVtbl->SetPrivateDataInterface(This,guid,pData)
/*** ID3D11View methods ***/
#define ID3D11VideoDecoderOutputView_GetResource(This,ppResource) (This)->lpVtbl->GetResource(This,ppResource)
/*** ID3D11VideoDecoderOutputView methods ***/
#define ID3D11VideoDecoderOutputView_GetDesc(This,pDesc) (This)->lpVtbl->GetDesc(This,pDesc)
#else
/*** IUnknown methods ***/
static FORCEINLINE HRESULT ID3D11VideoDecoderOutputView_QueryInterface(ID3D11VideoDecoderOutputView* This,REFIID riid,void **ppvObject) {
    return This->lpVtbl->QueryInterface(This,riid,ppvObject);
}
static FORCEINLINE ULONG ID3D11VideoDecoderOutputView_AddRef(ID3D11VideoDecoderOutputView* This) {
    return This->lpVtbl->AddRef(This);
}
static FORCEINLINE ULONG ID3D11VideoDecoderOutputView_Release(ID3D11VideoDecoderOutputView* This) {
    return This->lpVtbl->Release(This);
}
/*** ID3D11DeviceChild methods ***/
static FORCEINLINE void ID3D11VideoDecoderOutputView_GetDevice(ID3D11VideoDecoderOutputView* This,ID3D11Device **ppDevice) {
    This->lpVtbl->GetDevice(This,ppDevice);
}
static FORCEINLINE HRESULT ID3D11VideoDecoderOutputView_GetPrivateData(ID3D11VideoDecoderOutputView* This,REFGUID guid,UINT *pDataSize,void *pData) {
    return This->lpVtbl->GetPrivateData(This,guid,pDataSize,pData);
}
static FORCEINLINE HRESULT ID3D11VideoDecoderOutputView_SetPrivateData(ID3D11VideoDecoderOutputView* This,REFGUID guid,UINT DataSize,const void *pData) {
    return This->lpVtbl->SetPrivateData(This,guid,DataSize,pData);
}
static FORCEINLINE HRESULT ID3D11VideoDecoderOutputView_SetPrivateDataInterface(ID3D11VideoDecoderOutputView* This,REFGUID guid,const IUnknown *pData) {
    return This->lpVtbl->SetPrivateDataInterface(This,guid,pData);
}
/*** ID3D11View methods ***/
static FORCEINLINE void ID3D11VideoDecoderOutputView_GetResource(ID3D11VideoDecoderOutputView* This,ID3D11Resource **ppResource) {
    This->lpVtbl->GetResource(This,ppResource);
}
/*** ID3D11VideoDecoderOutputView methods ***/
static FORCEINLINE void ID3D11VideoDecoderOutputView_GetDesc(ID3D11VideoDecoderOutputView* This,D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc) {
    This->lpVtbl->GetDesc(This,pDesc);
}
#endif
#endif

#endif

void STDMETHODCALLTYPE ID3D11VideoDecoderOutputView_GetDesc_Proxy(
    ID3D11VideoDecoderOutputView* This,
    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc);
void __RPC_STUB ID3D11VideoDecoderOutputView_GetDesc_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);

#endif  /* __ID3D11VideoDecoderOutputView_INTERFACE_DEFINED__ */

/*****************************************************************************
 * ID3D11VideoDecoder interface
 */
#ifndef __ID3D11VideoDecoder_INTERFACE_DEFINED__
#define __ID3D11VideoDecoder_INTERFACE_DEFINED__

DEFINE_GUID(IID_ID3D11VideoDecoder, 0x3c9c5b51, 0x995d, 0x48d1, 0x9b,0x8d, 0xfa,0x5c,0xae,0xde,0xd6,0x5c);
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("3c9c5b51-995d-48d1-9b8d-fa5caeded65c")
ID3D11VideoDecoder : public ID3D11DeviceChild
{
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ID3D11VideoDecoder, 0x3c9c5b51, 0x995d, 0x48d1, 0x9b,0x8d, 0xfa,0x5c,0xae,0xde,0xd6,0x5c)
#endif
#else
typedef struct ID3D11VideoDecoderVtbl {
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ID3D11VideoDecoder* This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        ID3D11VideoDecoder* This);

    ULONG (STDMETHODCALLTYPE *Release)(
        ID3D11VideoDecoder* This);

    /*** ID3D11DeviceChild methods ***/
    void (STDMETHODCALLTYPE *GetDevice)(
        ID3D11VideoDecoder* This,
        ID3D11Device **ppDevice);

    HRESULT (STDMETHODCALLTYPE *GetPrivateData)(
        ID3D11VideoDecoder* This,
        REFGUID guid,
        UINT *pDataSize,
        void *pData);

    HRESULT (STDMETHODCALLTYPE *SetPrivateData)(
        ID3D11VideoDecoder* This,
        REFGUID guid,
        UINT DataSize,
        const void *pData);

    HRESULT (STDMETHODCALLTYPE *SetPrivateDataInterface)(
        ID3D11VideoDecoder* This,
        REFGUID guid,
        const IUnknown *pData);

    END_INTERFACE
} ID3D11VideoDecoderVtbl;
interface ID3D11VideoDecoder {
    CONST_VTBL ID3D11VideoDecoderVtbl* lpVtbl;
};

#ifdef COBJMACROS
#ifndef WIDL_C_INLINE_WRAPPERS
/*** IUnknown methods ***/
#define ID3D11VideoDecoder_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ID3D11VideoDecoder_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ID3D11VideoDecoder_Release(This) (This)->lpVtbl->Release(This)
/*** ID3D11DeviceChild methods ***/
#define ID3D11VideoDecoder_GetDevice(This,ppDevice) (This)->lpVtbl->GetDevice(This,ppDevice)
#define ID3D11VideoDecoder_GetPrivateData(This,guid,pDataSize,pData) (This)->lpVtbl->GetPrivateData(This,guid,pDataSize,pData)
#define ID3D11VideoDecoder_SetPrivateData(This,guid,DataSize,pData) (This)->lpVtbl->SetPrivateData(This,guid,DataSize,pData)
#define ID3D11VideoDecoder_SetPrivateDataInterface(This,guid,pData) (This)->lpVtbl->SetPrivateDataInterface(This,guid,pData)
#else
/*** IUnknown methods ***/
static FORCEINLINE HRESULT ID3D11VideoDecoder_QueryInterface(ID3D11VideoDecoder* This,REFIID riid,void **ppvObject) {
    return This->lpVtbl->QueryInterface(This,riid,ppvObject);
}
static FORCEINLINE ULONG ID3D11VideoDecoder_AddRef(ID3D11VideoDecoder* This) {
    return This->lpVtbl->AddRef(This);
}
static FORCEINLINE ULONG ID3D11VideoDecoder_Release(ID3D11VideoDecoder* This) {
    return This->lpVtbl->Release(This);
}
/*** ID3D11DeviceChild methods ***/
static FORCEINLINE void ID3D11VideoDecoder_GetDevice(ID3D11VideoDecoder* This,ID3D11Device **ppDevice) {
    This->lpVtbl->GetDevice(This,ppDevice);
}
static FORCEINLINE HRESULT ID3D11VideoDecoder_GetPrivateData(ID3D11VideoDecoder* This,REFGUID guid,UINT *pDataSize,void *pData) {
    return This->lpVtbl->GetPrivateData(This,guid,pDataSize,pData);
}
static FORCEINLINE HRESULT ID3D11VideoDecoder_SetPrivateData(ID3D11VideoDecoder* This,REFGUID guid,UINT DataSize,const void *pData) {
    return This->lpVtbl->SetPrivateData(This,guid,DataSize,pData);
}
static FORCEINLINE HRESULT ID3D11VideoDecoder_SetPrivateDataInterface(ID3D11VideoDecoder* This,REFGUID guid,const IUnknown *pData) {
    return This->lpVtbl->SetPrivateDataInterface(This,guid,pData);
}
#endif
#endif

#endif


#endif  /* __ID3D11VideoDecoder_INTERFACE_DEFINED__ */

typedef enum D3D11_VIDEO_DECODER_BUFFER_TYPE {
    D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS = 0,
    D3D11_VIDEO_DECODER_BUFFER_MACROBLOCK_CONTROL = 1,
    D3D11_VIDEO_DECODER_BUFFER_RESIDUAL_DIFFERENCE = 2,
    D3D11_VIDEO_DECODER_BUFFER_DEBLOCKING_CONTROL = 3,
    D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX = 4,
    D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL = 5,
    D3D11_VIDEO_DECODER_BUFFER_BITSTREAM = 6,
    D3D11_VIDEO_DECODER_BUFFER_MOTION_VECTOR = 7,
    D3D11_VIDEO_DECODER_BUFFER_FILM_GRAIN = 8
} D3D11_VIDEO_DECODER_BUFFER_TYPE;
typedef struct D3D11_ENCRYPTED_BLOCK_INFO {
    UINT NumEncryptedBytesAtBeginning;
    UINT NumBytesInSkipPattern;
    UINT NumBytesInEncryptPattern;
} D3D11_ENCRYPTED_BLOCK_INFO;
typedef struct D3D11_VIDEO_DECODER_BUFFER_DESC {
    D3D11_VIDEO_DECODER_BUFFER_TYPE BufferType;
    UINT BufferIndex;
    UINT DataOffset;
    UINT DataSize;
    UINT FirstMBaddress;
    UINT NumMBsInBuffer;
    UINT Width;
    UINT Height;
    UINT Stride;
    UINT ReservedBits;
    void *pIV;
    UINT IVSize;
    WINBOOL PartialEncryption;
    D3D11_ENCRYPTED_BLOCK_INFO EncryptedBlockInfo;
} D3D11_VIDEO_DECODER_BUFFER_DESC;
/*****************************************************************************
 * ID3D11VideoContext interface
 */
#ifndef __ID3D11VideoContext_INTERFACE_DEFINED__
#define __ID3D11VideoContext_INTERFACE_DEFINED__

DEFINE_GUID(IID_ID3D11VideoContext, 0x61f21c45, 0x3c0e, 0x4a74, 0x9c,0xea, 0x67,0x10,0x0d,0x9a,0xd5,0xe4);
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("61f21c45-3c0e-4a74-9cea-67100d9ad5e4")
ID3D11VideoContext : public ID3D11DeviceChild
{
    virtual HRESULT STDMETHODCALLTYPE GetDecoderBuffer(
        ID3D11VideoDecoder *pDecoder,
        D3D11_VIDEO_DECODER_BUFFER_TYPE BufferType,
        UINT *pBufferSize,
        void **ppBuffer) = 0;

    virtual HRESULT STDMETHODCALLTYPE ReleaseDecoderBuffer(
        ID3D11VideoDecoder *pDecoder,
        D3D11_VIDEO_DECODER_BUFFER_TYPE BufferType) = 0;

    virtual HRESULT STDMETHODCALLTYPE DecoderBeginFrame(
        ID3D11VideoDecoder *pDecoder,
        ID3D11VideoDecoderOutputView *pVideoDecoderOutputView,
        UINT ContentKeySize,
        const void *pContentKey) = 0;

    virtual HRESULT STDMETHODCALLTYPE DecoderEndFrame(
        ID3D11VideoDecoder *pDecoder) = 0;

    virtual HRESULT STDMETHODCALLTYPE SubmitDecoderBuffers(
        ID3D11VideoDecoder *pDecoder,
        UINT NumBuffers,
        const D3D11_VIDEO_DECODER_BUFFER_DESC *pBufferDesc) = 0;

};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ID3D11VideoContext, 0x61f21c45, 0x3c0e, 0x4a74, 0x9c,0xea, 0x67,0x10,0x0d,0x9a,0xd5,0xe4)
#endif
#else
typedef struct ID3D11VideoContextVtbl {
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ID3D11VideoContext* This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        ID3D11VideoContext* This);

    ULONG (STDMETHODCALLTYPE *Release)(
        ID3D11VideoContext* This);

    /*** ID3D11DeviceChild methods ***/
    void (STDMETHODCALLTYPE *GetDevice)(
        ID3D11VideoContext* This,
        ID3D11Device **ppDevice);

    HRESULT (STDMETHODCALLTYPE *GetPrivateData)(
        ID3D11VideoContext* This,
        REFGUID guid,
        UINT *pDataSize,
        void *pData);

    HRESULT (STDMETHODCALLTYPE *SetPrivateData)(
        ID3D11VideoContext* This,
        REFGUID guid,
        UINT DataSize,
        const void *pData);

    HRESULT (STDMETHODCALLTYPE *SetPrivateDataInterface)(
        ID3D11VideoContext* This,
        REFGUID guid,
        const IUnknown *pData);

    /*** ID3D11VideoContext methods ***/
    HRESULT (STDMETHODCALLTYPE *GetDecoderBuffer)(
        ID3D11VideoContext* This,
        ID3D11VideoDecoder *pDecoder,
        D3D11_VIDEO_DECODER_BUFFER_TYPE BufferType,
        UINT *pBufferSize,
        void **ppBuffer);

    HRESULT (STDMETHODCALLTYPE *ReleaseDecoderBuffer)(
        ID3D11VideoContext* This,
        ID3D11VideoDecoder *pDecoder,
        D3D11_VIDEO_DECODER_BUFFER_TYPE BufferType);

    HRESULT (STDMETHODCALLTYPE *DecoderBeginFrame)(
        ID3D11VideoContext* This,
        ID3D11VideoDecoder *pDecoder,
        ID3D11VideoDecoderOutputView *pVideoDecoderOutputView,
        UINT ContentKeySize,
        const void *pContentKey);

    HRESULT (STDMETHODCALLTYPE *DecoderEndFrame)(
        ID3D11VideoContext* This,
        ID3D11VideoDecoder *pDecoder);

    HRESULT (STDMETHODCALLTYPE *SubmitDecoderBuffers)(
        ID3D11VideoContext* This,
        ID3D11VideoDecoder *pDecoder,
        UINT NumBuffers,
        const D3D11_VIDEO_DECODER_BUFFER_DESC *pBufferDesc);

    END_INTERFACE
} ID3D11VideoContextVtbl;
interface ID3D11VideoContext {
    CONST_VTBL ID3D11VideoContextVtbl* lpVtbl;
};

#ifdef COBJMACROS
#ifndef WIDL_C_INLINE_WRAPPERS
/*** IUnknown methods ***/
#define ID3D11VideoContext_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ID3D11VideoContext_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ID3D11VideoContext_Release(This) (This)->lpVtbl->Release(This)
/*** ID3D11DeviceChild methods ***/
#define ID3D11VideoContext_GetDevice(This,ppDevice) (This)->lpVtbl->GetDevice(This,ppDevice)
#define ID3D11VideoContext_GetPrivateData(This,guid,pDataSize,pData) (This)->lpVtbl->GetPrivateData(This,guid,pDataSize,pData)
#define ID3D11VideoContext_SetPrivateData(This,guid,DataSize,pData) (This)->lpVtbl->SetPrivateData(This,guid,DataSize,pData)
#define ID3D11VideoContext_SetPrivateDataInterface(This,guid,pData) (This)->lpVtbl->SetPrivateDataInterface(This,guid,pData)
/*** ID3D11VideoContext methods ***/
#define ID3D11VideoContext_GetDecoderBuffer(This,pDecoder,BufferType,pBufferSize,ppBuffer) (This)->lpVtbl->GetDecoderBuffer(This,pDecoder,BufferType,pBufferSize,ppBuffer)
#define ID3D11VideoContext_ReleaseDecoderBuffer(This,pDecoder,BufferType) (This)->lpVtbl->ReleaseDecoderBuffer(This,pDecoder,BufferType)
#define ID3D11VideoContext_DecoderBeginFrame(This,pDecoder,pVideoDecoderOutputView,ContentKeySize,pContentKey) (This)->lpVtbl->DecoderBeginFrame(This,pDecoder,pVideoDecoderOutputView,ContentKeySize,pContentKey)
#define ID3D11VideoContext_DecoderEndFrame(This,pDecoder) (This)->lpVtbl->DecoderEndFrame(This,pDecoder)
#define ID3D11VideoContext_SubmitDecoderBuffers(This,pDecoder,NumBuffers,pBufferDesc) (This)->lpVtbl->SubmitDecoderBuffers(This,pDecoder,NumBuffers,pBufferDesc)
#else
/*** IUnknown methods ***/
static FORCEINLINE HRESULT ID3D11VideoContext_QueryInterface(ID3D11VideoContext* This,REFIID riid,void **ppvObject) {
    return This->lpVtbl->QueryInterface(This,riid,ppvObject);
}
static FORCEINLINE ULONG ID3D11VideoContext_AddRef(ID3D11VideoContext* This) {
    return This->lpVtbl->AddRef(This);
}
static FORCEINLINE ULONG ID3D11VideoContext_Release(ID3D11VideoContext* This) {
    return This->lpVtbl->Release(This);
}
/*** ID3D11DeviceChild methods ***/
static FORCEINLINE void ID3D11VideoContext_GetDevice(ID3D11VideoContext* This,ID3D11Device **ppDevice) {
    This->lpVtbl->GetDevice(This,ppDevice);
}
static FORCEINLINE HRESULT ID3D11VideoContext_GetPrivateData(ID3D11VideoContext* This,REFGUID guid,UINT *pDataSize,void *pData) {
    return This->lpVtbl->GetPrivateData(This,guid,pDataSize,pData);
}
static FORCEINLINE HRESULT ID3D11VideoContext_SetPrivateData(ID3D11VideoContext* This,REFGUID guid,UINT DataSize,const void *pData) {
    return This->lpVtbl->SetPrivateData(This,guid,DataSize,pData);
}
static FORCEINLINE HRESULT ID3D11VideoContext_SetPrivateDataInterface(ID3D11VideoContext* This,REFGUID guid,const IUnknown *pData) {
    return This->lpVtbl->SetPrivateDataInterface(This,guid,pData);
}
/*** ID3D11VideoContext methods ***/
static FORCEINLINE HRESULT ID3D11VideoContext_GetDecoderBuffer(ID3D11VideoContext* This,ID3D11VideoDecoder *pDecoder,D3D11_VIDEO_DECODER_BUFFER_TYPE BufferType,UINT *pBufferSize,void **ppBuffer) {
    return This->lpVtbl->GetDecoderBuffer(This,pDecoder,BufferType,pBufferSize,ppBuffer);
}
static FORCEINLINE HRESULT ID3D11VideoContext_ReleaseDecoderBuffer(ID3D11VideoContext* This,ID3D11VideoDecoder *pDecoder,D3D11_VIDEO_DECODER_BUFFER_TYPE BufferType) {
    return This->lpVtbl->ReleaseDecoderBuffer(This,pDecoder,BufferType);
}
static FORCEINLINE HRESULT ID3D11VideoContext_DecoderBeginFrame(ID3D11VideoContext* This,ID3D11VideoDecoder *pDecoder,ID3D11VideoDecoderOutputView *pVideoDecoderOutputView,UINT ContentKeySize,const void *pContentKey) {
    return This->lpVtbl->DecoderBeginFrame(This,pDecoder,pVideoDecoderOutputView,ContentKeySize,pContentKey);
}
static FORCEINLINE HRESULT ID3D11VideoContext_DecoderEndFrame(ID3D11VideoContext* This,ID3D11VideoDecoder *pDecoder) {
    return This->lpVtbl->DecoderEndFrame(This,pDecoder);
}
static FORCEINLINE HRESULT ID3D11VideoContext_SubmitDecoderBuffers(ID3D11VideoContext* This,ID3D11VideoDecoder *pDecoder,UINT NumBuffers,const D3D11_VIDEO_DECODER_BUFFER_DESC *pBufferDesc) {
    return This->lpVtbl->SubmitDecoderBuffers(This,pDecoder,NumBuffers,pBufferDesc);
}
#endif
#endif

#endif

HRESULT STDMETHODCALLTYPE ID3D11VideoContext_GetDecoderBuffer_Proxy(
    ID3D11VideoContext* This,
    ID3D11VideoDecoder *pDecoder,
    D3D11_VIDEO_DECODER_BUFFER_TYPE BufferType,
    UINT *pBufferSize,
    void **ppBuffer);
void __RPC_STUB ID3D11VideoContext_GetDecoderBuffer_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoContext_ReleaseDecoderBuffer_Proxy(
    ID3D11VideoContext* This,
    ID3D11VideoDecoder *pDecoder,
    D3D11_VIDEO_DECODER_BUFFER_TYPE BufferType);
void __RPC_STUB ID3D11VideoContext_ReleaseDecoderBuffer_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoContext_DecoderBeginFrame_Proxy(
    ID3D11VideoContext* This,
    ID3D11VideoDecoder *pDecoder,
    ID3D11VideoDecoderOutputView *pVideoDecoderOutputView,
    UINT ContentKeySize,
    const void *pContentKey);
void __RPC_STUB ID3D11VideoContext_DecoderBeginFrame_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoContext_DecoderEndFrame_Proxy(
    ID3D11VideoContext* This,
    ID3D11VideoDecoder *pDecoder);
void __RPC_STUB ID3D11VideoContext_DecoderEndFrame_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoContext_SubmitDecoderBuffers_Proxy(
    ID3D11VideoContext* This,
    ID3D11VideoDecoder *pDecoder,
    UINT NumBuffers,
    const D3D11_VIDEO_DECODER_BUFFER_DESC *pBufferDesc);
void __RPC_STUB ID3D11VideoContext_SubmitDecoderBuffers_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);

#endif  /* __ID3D11VideoContext_INTERFACE_DEFINED__ */

typedef struct D3D11_VIDEO_DECODER_DESC {
    GUID Guid;
    UINT SampleWidth;
    UINT SampleHeight;
    DXGI_FORMAT OutputFormat;
} D3D11_VIDEO_DECODER_DESC;
typedef struct D3D11_VIDEO_DECODER_CONFIG {
    GUID guidConfigBitstreamEncryption;
    GUID guidConfigMBcontrolEncryption;
    GUID guidConfigResidDiffEncryption;
    UINT ConfigBitstreamRaw;
    UINT ConfigMBcontrolRasterOrder;
    UINT ConfigResidDiffHost;
    UINT ConfigSpatialResid8;
    UINT ConfigResid8Subtraction;
    UINT ConfigSpatialHost8or9Clipping;
    UINT ConfigSpatialResidInterleaved;
    UINT ConfigIntraResidUnsigned;
    UINT ConfigResidDiffAccelerator;
    UINT ConfigHostInverseScan;
    UINT ConfigSpecificIDCT;
    UINT Config4GroupedCoefs;
    USHORT ConfigMinRenderTargetBuffCount;
    USHORT ConfigDecoderSpecific;
} D3D11_VIDEO_DECODER_CONFIG;
typedef enum D3D11_AUTHENTICATED_CHANNEL_TYPE {
    D3D11_AUTHENTICATED_CHANNEL_D3D11 = 1,
    D3D11_AUTHENTICATED_CHANNEL_DRIVER_SOFTWARE = 2,
    D3D11_AUTHENTICATED_CHANNEL_DRIVER_HARDWARE = 3
} D3D11_AUTHENTICATED_CHANNEL_TYPE;
#ifndef __ID3D11VideoProcessorEnumerator_FWD_DEFINED__
#define __ID3D11VideoProcessorEnumerator_FWD_DEFINED__
typedef interface ID3D11VideoProcessorEnumerator ID3D11VideoProcessorEnumerator;
#endif

#ifndef __ID3D11VideoProcessor_FWD_DEFINED__
#define __ID3D11VideoProcessor_FWD_DEFINED__
typedef interface ID3D11VideoProcessor ID3D11VideoProcessor;
#endif

#ifndef __ID3D11VideoProcessorInputView_FWD_DEFINED__
#define __ID3D11VideoProcessorInputView_FWD_DEFINED__
typedef interface ID3D11VideoProcessorInputView ID3D11VideoProcessorInputView;
#endif

#ifndef __ID3D11VideoProcessorOutputView_FWD_DEFINED__
#define __ID3D11VideoProcessorOutputView_FWD_DEFINED__
typedef interface ID3D11VideoProcessorOutputView ID3D11VideoProcessorOutputView;
#endif

#ifndef __ID3D11AuthenticatedChannel_FWD_DEFINED__
#define __ID3D11AuthenticatedChannel_FWD_DEFINED__
typedef interface ID3D11AuthenticatedChannel ID3D11AuthenticatedChannel;
#endif

#ifndef __ID3D11CryptoSession_FWD_DEFINED__
#define __ID3D11CryptoSession_FWD_DEFINED__
typedef interface ID3D11CryptoSession ID3D11CryptoSession;
#endif

/*****************************************************************************
 * ID3D11VideoDevice interface
 */
#ifndef __ID3D11VideoDevice_INTERFACE_DEFINED__
#define __ID3D11VideoDevice_INTERFACE_DEFINED__

DEFINE_GUID(IID_ID3D11VideoDevice, 0x10ec4d5b, 0x975a, 0x4689, 0xb9,0xe4, 0xd0,0xaa,0xc3,0x0f,0xe3,0x33);
#if defined(__cplusplus) && !defined(CINTERFACE)
MIDL_INTERFACE("10ec4d5b-975a-4689-b9e4-d0aac30fe333")
ID3D11VideoDevice : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE CreateVideoDecoder(
        const D3D11_VIDEO_DECODER_DESC *pVideoDesc,
        const D3D11_VIDEO_DECODER_CONFIG *pConfig,
        ID3D11VideoDecoder **ppVideoDecoder) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessor(
        ID3D11VideoProcessorEnumerator *ppProcEnumerator,
        UINT RateConversionIndex,
        ID3D11VideoProcessor **ppVideoProcessor) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateAuthenticatedChannel(
        D3D11_AUTHENTICATED_CHANNEL_TYPE ChannelType,
        ID3D11AuthenticatedChannel **ppAuthenticatedChannel) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateCryptoSession(
        const GUID *pCryptoType,
        const GUID *pDecoderProfile,
        const GUID *pKeyExchangeType,
        ID3D11CryptoSession **ppCryptoSession) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateVideoDecoderOutputView(
        ID3D11Resource *pResource,
        const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc,
        ID3D11VideoDecoderOutputView **ppVideoDecoderOutputView) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessorInputView(
        ID3D11Resource *pResource,
        ID3D11VideoProcessorEnumerator *ppProcEnumerator,
        const void *pInputDesc,
        ID3D11VideoProcessorInputView **ppVideoProcInputView) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessorOutputView(
        ID3D11Resource *pResource,
        ID3D11VideoProcessorEnumerator *pEnumerator,
        const void *pOutputDesc,
        ID3D11VideoProcessorOutputView **ppVideoProcOutputView) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessorEnumerator(
        const void *pContentDesc,
        ID3D11VideoProcessorEnumerator **ppProcEnumerator) = 0;

    virtual UINT STDMETHODCALLTYPE GetVideoDecoderProfileCount(
        ) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderProfile(
        UINT Index,
        GUID *pDecoderProfile) = 0;

    virtual HRESULT STDMETHODCALLTYPE CheckVideoDecoderFormat(
        const GUID *pDecoderProfile,
        DXGI_FORMAT Format,
        WINBOOL *pSupported) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderConfigCount(
        const D3D11_VIDEO_DECODER_DESC *pDesc,
        UINT *pCount) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderConfig(
        const D3D11_VIDEO_DECODER_DESC *pDesc,
        UINT DecoderIndex,
        D3D11_VIDEO_DECODER_CONFIG *pConfig) = 0;

};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ID3D11VideoDevice, 0x10ec4d5b, 0x975a, 0x4689, 0xb9,0xe4, 0xd0,0xaa,0xc3,0x0f,0xe3,0x33)
#endif
#else
typedef struct ID3D11VideoDeviceVtbl {
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ID3D11VideoDevice* This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        ID3D11VideoDevice* This);

    ULONG (STDMETHODCALLTYPE *Release)(
        ID3D11VideoDevice* This);

    /*** ID3D11VideoDevice methods ***/
    HRESULT (STDMETHODCALLTYPE *CreateVideoDecoder)(
        ID3D11VideoDevice* This,
        const D3D11_VIDEO_DECODER_DESC *pVideoDesc,
        const D3D11_VIDEO_DECODER_CONFIG *pConfig,
        ID3D11VideoDecoder **ppVideoDecoder);

    HRESULT (STDMETHODCALLTYPE *CreateVideoProcessor)(
        ID3D11VideoDevice* This,
        ID3D11VideoProcessorEnumerator *ppProcEnumerator,
        UINT RateConversionIndex,
        ID3D11VideoProcessor **ppVideoProcessor);

    HRESULT (STDMETHODCALLTYPE *CreateAuthenticatedChannel)(
        ID3D11VideoDevice* This,
        D3D11_AUTHENTICATED_CHANNEL_TYPE ChannelType,
        ID3D11AuthenticatedChannel **ppAuthenticatedChannel);

    HRESULT (STDMETHODCALLTYPE *CreateCryptoSession)(
        ID3D11VideoDevice* This,
        const GUID *pCryptoType,
        const GUID *pDecoderProfile,
        const GUID *pKeyExchangeType,
        ID3D11CryptoSession **ppCryptoSession);

    HRESULT (STDMETHODCALLTYPE *CreateVideoDecoderOutputView)(
        ID3D11VideoDevice* This,
        ID3D11Resource *pResource,
        const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc,
        ID3D11VideoDecoderOutputView **ppVideoDecoderOutputView);

    HRESULT (STDMETHODCALLTYPE *CreateVideoProcessorInputView)(
        ID3D11VideoDevice* This,
        ID3D11Resource *pResource,
        ID3D11VideoProcessorEnumerator *ppProcEnumerator,
        const void *pInputDesc,
        ID3D11VideoProcessorInputView **ppVideoProcInputView);

    HRESULT (STDMETHODCALLTYPE *CreateVideoProcessorOutputView)(
        ID3D11VideoDevice* This,
        ID3D11Resource *pResource,
        ID3D11VideoProcessorEnumerator *pEnumerator,
        const void *pOutputDesc,
        ID3D11VideoProcessorOutputView **ppVideoProcOutputView);

    HRESULT (STDMETHODCALLTYPE *CreateVideoProcessorEnumerator)(
        ID3D11VideoDevice* This,
        const void *pContentDesc,
        ID3D11VideoProcessorEnumerator **ppProcEnumerator);

    UINT (STDMETHODCALLTYPE *GetVideoDecoderProfileCount)(
        ID3D11VideoDevice* This);

    HRESULT (STDMETHODCALLTYPE *GetVideoDecoderProfile)(
        ID3D11VideoDevice* This,
        UINT Index,
        GUID *pDecoderProfile);

    HRESULT (STDMETHODCALLTYPE *CheckVideoDecoderFormat)(
        ID3D11VideoDevice* This,
        const GUID *pDecoderProfile,
        DXGI_FORMAT Format,
        WINBOOL *pSupported);

    HRESULT (STDMETHODCALLTYPE *GetVideoDecoderConfigCount)(
        ID3D11VideoDevice* This,
        const D3D11_VIDEO_DECODER_DESC *pDesc,
        UINT *pCount);

    HRESULT (STDMETHODCALLTYPE *GetVideoDecoderConfig)(
        ID3D11VideoDevice* This,
        const D3D11_VIDEO_DECODER_DESC *pDesc,
        UINT DecoderIndex,
        D3D11_VIDEO_DECODER_CONFIG *pConfig);

    END_INTERFACE
} ID3D11VideoDeviceVtbl;
interface ID3D11VideoDevice {
    CONST_VTBL ID3D11VideoDeviceVtbl* lpVtbl;
};

#ifdef COBJMACROS
#ifndef WIDL_C_INLINE_WRAPPERS
/*** IUnknown methods ***/
#define ID3D11VideoDevice_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ID3D11VideoDevice_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ID3D11VideoDevice_Release(This) (This)->lpVtbl->Release(This)
/*** ID3D11VideoDevice methods ***/
#define ID3D11VideoDevice_CreateVideoDecoder(This,pVideoDesc,pConfig,ppVideoDecoder) (This)->lpVtbl->CreateVideoDecoder(This,pVideoDesc,pConfig,ppVideoDecoder)
#define ID3D11VideoDevice_CreateVideoProcessor(This,ppProcEnumerator,RateConversionIndex,ppVideoProcessor) (This)->lpVtbl->CreateVideoProcessor(This,ppProcEnumerator,RateConversionIndex,ppVideoProcessor)
#define ID3D11VideoDevice_CreateAuthenticatedChannel(This,ChannelType,ppAuthenticatedChannel) (This)->lpVtbl->CreateAuthenticatedChannel(This,ChannelType,ppAuthenticatedChannel)
#define ID3D11VideoDevice_CreateCryptoSession(This,pCryptoType,pDecoderProfile,pKeyExchangeType,ppCryptoSession) (This)->lpVtbl->CreateCryptoSession(This,pCryptoType,pDecoderProfile,pKeyExchangeType,ppCryptoSession)
#define ID3D11VideoDevice_CreateVideoDecoderOutputView(This,pResource,pDesc,ppVideoDecoderOutputView) (This)->lpVtbl->CreateVideoDecoderOutputView(This,pResource,pDesc,ppVideoDecoderOutputView)
#define ID3D11VideoDevice_CreateVideoProcessorInputView(This,pResource,ppProcEnumerator,pInputDesc,ppVideoProcInputView) (This)->lpVtbl->CreateVideoProcessorInputView(This,pResource,ppProcEnumerator,pInputDesc,ppVideoProcInputView)
#define ID3D11VideoDevice_CreateVideoProcessorOutputView(This,pResource,pEnumerator,pOutputDesc,ppVideoProcOutputView) (This)->lpVtbl->CreateVideoProcessorOutputView(This,pResource,pEnumerator,pOutputDesc,ppVideoProcOutputView)
#define ID3D11VideoDevice_CreateVideoProcessorEnumerator(This,pContentDesc,ppProcEnumerator) (This)->lpVtbl->CreateVideoProcessorEnumerator(This,pContentDesc,ppProcEnumerator)
#define ID3D11VideoDevice_GetVideoDecoderProfileCount(This) (This)->lpVtbl->GetVideoDecoderProfileCount(This)
#define ID3D11VideoDevice_GetVideoDecoderProfile(This,Index,pDecoderProfile) (This)->lpVtbl->GetVideoDecoderProfile(This,Index,pDecoderProfile)
#define ID3D11VideoDevice_CheckVideoDecoderFormat(This,pDecoderProfile,Format,pSupported) (This)->lpVtbl->CheckVideoDecoderFormat(This,pDecoderProfile,Format,pSupported)
#define ID3D11VideoDevice_GetVideoDecoderConfigCount(This,pDesc,pCount) (This)->lpVtbl->GetVideoDecoderConfigCount(This,pDesc,pCount)
#define ID3D11VideoDevice_GetVideoDecoderConfig(This,pDesc,DecoderIndex,pConfig) (This)->lpVtbl->GetVideoDecoderConfig(This,pDesc,DecoderIndex,pConfig)
#else
/*** IUnknown methods ***/
static FORCEINLINE HRESULT ID3D11VideoDevice_QueryInterface(ID3D11VideoDevice* This,REFIID riid,void **ppvObject) {
    return This->lpVtbl->QueryInterface(This,riid,ppvObject);
}
static FORCEINLINE ULONG ID3D11VideoDevice_AddRef(ID3D11VideoDevice* This) {
    return This->lpVtbl->AddRef(This);
}
static FORCEINLINE ULONG ID3D11VideoDevice_Release(ID3D11VideoDevice* This) {
    return This->lpVtbl->Release(This);
}
/*** ID3D11VideoDevice methods ***/
static FORCEINLINE HRESULT ID3D11VideoDevice_CreateVideoDecoder(ID3D11VideoDevice* This,const D3D11_VIDEO_DECODER_DESC *pVideoDesc,const D3D11_VIDEO_DECODER_CONFIG *pConfig,ID3D11VideoDecoder **ppVideoDecoder) {
    return This->lpVtbl->CreateVideoDecoder(This,pVideoDesc,pConfig,ppVideoDecoder);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_CreateVideoProcessor(ID3D11VideoDevice* This,ID3D11VideoProcessorEnumerator *ppProcEnumerator,UINT RateConversionIndex,ID3D11VideoProcessor **ppVideoProcessor) {
    return This->lpVtbl->CreateVideoProcessor(This,ppProcEnumerator,RateConversionIndex,ppVideoProcessor);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_CreateAuthenticatedChannel(ID3D11VideoDevice* This,D3D11_AUTHENTICATED_CHANNEL_TYPE ChannelType,ID3D11AuthenticatedChannel **ppAuthenticatedChannel) {
    return This->lpVtbl->CreateAuthenticatedChannel(This,ChannelType,ppAuthenticatedChannel);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_CreateCryptoSession(ID3D11VideoDevice* This,const GUID *pCryptoType,const GUID *pDecoderProfile,const GUID *pKeyExchangeType,ID3D11CryptoSession **ppCryptoSession) {
    return This->lpVtbl->CreateCryptoSession(This,pCryptoType,pDecoderProfile,pKeyExchangeType,ppCryptoSession);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_CreateVideoDecoderOutputView(ID3D11VideoDevice* This,ID3D11Resource *pResource,const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc,ID3D11VideoDecoderOutputView **ppVideoDecoderOutputView) {
    return This->lpVtbl->CreateVideoDecoderOutputView(This,pResource,pDesc,ppVideoDecoderOutputView);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_CreateVideoProcessorInputView(ID3D11VideoDevice* This,ID3D11Resource *pResource,ID3D11VideoProcessorEnumerator *ppProcEnumerator,const void *pInputDesc,ID3D11VideoProcessorInputView **ppVideoProcInputView) {
    return This->lpVtbl->CreateVideoProcessorInputView(This,pResource,ppProcEnumerator,pInputDesc,ppVideoProcInputView);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_CreateVideoProcessorOutputView(ID3D11VideoDevice* This,ID3D11Resource *pResource,ID3D11VideoProcessorEnumerator *pEnumerator,const void *pOutputDesc,ID3D11VideoProcessorOutputView **ppVideoProcOutputView) {
    return This->lpVtbl->CreateVideoProcessorOutputView(This,pResource,pEnumerator,pOutputDesc,ppVideoProcOutputView);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_CreateVideoProcessorEnumerator(ID3D11VideoDevice* This,const void *pContentDesc,ID3D11VideoProcessorEnumerator **ppProcEnumerator) {
    return This->lpVtbl->CreateVideoProcessorEnumerator(This,pContentDesc,ppProcEnumerator);
}
static FORCEINLINE UINT ID3D11VideoDevice_GetVideoDecoderProfileCount(ID3D11VideoDevice* This) {
    return This->lpVtbl->GetVideoDecoderProfileCount(This);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_GetVideoDecoderProfile(ID3D11VideoDevice* This,UINT Index,GUID *pDecoderProfile) {
    return This->lpVtbl->GetVideoDecoderProfile(This,Index,pDecoderProfile);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_CheckVideoDecoderFormat(ID3D11VideoDevice* This,const GUID *pDecoderProfile,DXGI_FORMAT Format,WINBOOL *pSupported) {
    return This->lpVtbl->CheckVideoDecoderFormat(This,pDecoderProfile,Format,pSupported);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_GetVideoDecoderConfigCount(ID3D11VideoDevice* This,const D3D11_VIDEO_DECODER_DESC *pDesc,UINT *pCount) {
    return This->lpVtbl->GetVideoDecoderConfigCount(This,pDesc,pCount);
}
static FORCEINLINE HRESULT ID3D11VideoDevice_GetVideoDecoderConfig(ID3D11VideoDevice* This,const D3D11_VIDEO_DECODER_DESC *pDesc,UINT DecoderIndex,D3D11_VIDEO_DECODER_CONFIG *pConfig) {
    return This->lpVtbl->GetVideoDecoderConfig(This,pDesc,DecoderIndex,pConfig);
}
#endif
#endif

#endif

HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_CreateVideoDecoder_Proxy(
    ID3D11VideoDevice* This,
    const D3D11_VIDEO_DECODER_DESC *pVideoDesc,
    const D3D11_VIDEO_DECODER_CONFIG *pConfig,
    ID3D11VideoDecoder **ppVideoDecoder);
void __RPC_STUB ID3D11VideoDevice_CreateVideoDecoder_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_CreateVideoProcessor_Proxy(
    ID3D11VideoDevice* This,
    ID3D11VideoProcessorEnumerator *ppProcEnumerator,
    UINT RateConversionIndex,
    ID3D11VideoProcessor **ppVideoProcessor);
void __RPC_STUB ID3D11VideoDevice_CreateVideoProcessor_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_CreateAuthenticatedChannel_Proxy(
    ID3D11VideoDevice* This,
    D3D11_AUTHENTICATED_CHANNEL_TYPE ChannelType,
    ID3D11AuthenticatedChannel **ppAuthenticatedChannel);
void __RPC_STUB ID3D11VideoDevice_CreateAuthenticatedChannel_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_CreateCryptoSession_Proxy(
    ID3D11VideoDevice* This,
    const GUID *pCryptoType,
    const GUID *pDecoderProfile,
    const GUID *pKeyExchangeType,
    ID3D11CryptoSession **ppCryptoSession);
void __RPC_STUB ID3D11VideoDevice_CreateCryptoSession_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_CreateVideoDecoderOutputView_Proxy(
    ID3D11VideoDevice* This,
    ID3D11Resource *pResource,
    const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc,
    ID3D11VideoDecoderOutputView **ppVideoDecoderOutputView);
void __RPC_STUB ID3D11VideoDevice_CreateVideoDecoderOutputView_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_CreateVideoProcessorInputView_Proxy(
    ID3D11VideoDevice* This,
    ID3D11Resource *pResource,
    ID3D11VideoProcessorEnumerator *ppProcEnumerator,
    const void *pInputDesc,
    ID3D11VideoProcessorInputView **ppVideoProcInputView);
void __RPC_STUB ID3D11VideoDevice_CreateVideoProcessorInputView_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_CreateVideoProcessorOutputView_Proxy(
    ID3D11VideoDevice* This,
    ID3D11Resource *pResource,
    ID3D11VideoProcessorEnumerator *pEnumerator,
    const void *pOutputDesc,
    ID3D11VideoProcessorOutputView **ppVideoProcOutputView);
void __RPC_STUB ID3D11VideoDevice_CreateVideoProcessorOutputView_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_CreateVideoProcessorEnumerator_Proxy(
    ID3D11VideoDevice* This,
    const void *pContentDesc,
    ID3D11VideoProcessorEnumerator **ppProcEnumerator);
void __RPC_STUB ID3D11VideoDevice_CreateVideoProcessorEnumerator_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
UINT STDMETHODCALLTYPE ID3D11VideoDevice_GetVideoDecoderProfileCount_Proxy(
    ID3D11VideoDevice* This);
void __RPC_STUB ID3D11VideoDevice_GetVideoDecoderProfileCount_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_GetVideoDecoderProfile_Proxy(
    ID3D11VideoDevice* This,
    UINT Index,
    GUID *pDecoderProfile);
void __RPC_STUB ID3D11VideoDevice_GetVideoDecoderProfile_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_CheckVideoDecoderFormat_Proxy(
    ID3D11VideoDevice* This,
    const GUID *pDecoderProfile,
    DXGI_FORMAT Format,
    WINBOOL *pSupported);
void __RPC_STUB ID3D11VideoDevice_CheckVideoDecoderFormat_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_GetVideoDecoderConfigCount_Proxy(
    ID3D11VideoDevice* This,
    const D3D11_VIDEO_DECODER_DESC *pDesc,
    UINT *pCount);
void __RPC_STUB ID3D11VideoDevice_GetVideoDecoderConfigCount_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);
HRESULT STDMETHODCALLTYPE ID3D11VideoDevice_GetVideoDecoderConfig_Proxy(
    ID3D11VideoDevice* This,
    const D3D11_VIDEO_DECODER_DESC *pDesc,
    UINT DecoderIndex,
    D3D11_VIDEO_DECODER_CONFIG *pConfig);
void __RPC_STUB ID3D11VideoDevice_GetVideoDecoderConfig_Stub(
    IRpcStubBuffer* This,
    IRpcChannelBuffer* pRpcChannelBuffer,
    PRPC_MESSAGE pRpcMessage,
    DWORD* pdwStubPhase);

#endif  /* __ID3D11VideoDevice_INTERFACE_DEFINED__ */

#ifdef __cplusplus
}
#endif

#endif /* __d3d11_extra_h__ */
