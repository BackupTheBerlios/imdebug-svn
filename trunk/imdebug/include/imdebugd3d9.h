/*============================================================================
 * Copyright (c) 2005 William V. Baxter III
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must
 *    not claim that you wrote the original software. If you use
 *    this software in a product, an acknowledgment in the product
 *    documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must
 *    not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *==========================================================================*/

#ifndef __IMDEBUGD3D_H__
#define __IMDEBUGD3D_H__

#include <imdebug.h>
#include <d3d9.h>
#include <string>

using namespace std;

string format2string(D3DFORMAT format);

inline HRESULT
imdebugRenderTarget(IDirect3DSurface9 *pRenderTarget, IDirect3DDevice9 *pd3dDevice, char *argstring = NULL)
{
    HRESULT hr = S_OK;
    IDirect3DTexture9 *pTempTex;
    IDirect3DSurface9 *pData;
    D3DSURFACE_DESC desc;
    V( pRenderTarget->GetDesc(&desc) );

    V( D3DXCreateTexture(pd3dDevice, desc.Width, desc.Height, 0, 0, desc.Format, D3DPOOL_SYSTEMMEM, &pTempTex) );
    V( pTempTex->GetSurfaceLevel(0, &pData) );
    V( pd3dDevice->GetRenderTargetData(pRenderTarget, pData));

    D3DLOCKED_RECT lockedRect;
    V( pData->LockRect(&lockedRect, NULL, D3DLOCK_READONLY) );

    string s = (NULL == argstring) ? format2string(desc.Format) : argstring;

    imdebug(s.append(" w=%d h=%d %p").c_str(), desc.Width, desc.Height, lockedRect.pBits);

    pData->UnlockRect();

    SAFE_RELEASE(pTempTex);
    SAFE_RELEASE(pData);

    return hr;
}


inline string format2string(D3DFORMAT format)
{
    switch(format) 
    {
    case D3DFMT_R16F:
    case D3DFMT_R32F:
        return "lum b=32f";
        break;
    case D3DFMT_A8:
    case D3DFMT_P8:
    case D3DFMT_L8:
    case D3DFMT_L16:
    case D3DFMT_D16_LOCKABLE:
    case D3DFMT_D32F_LOCKABLE:
        return "lum";
        break;
    case D3DFMT_G16R16F:
        return "luma b=32f";
        break;
    case D3DFMT_V8U8:
    case D3DFMT_V16U16:
    case D3DFMT_G16R16:
    case D3DFMT_A8P8:
    case D3DFMT_A8L8:
    case D3DFMT_A4L4:
        return "luma";
        break;
    case D3DFMT_R3G3B2:
    case D3DFMT_R5G6B5:
    case D3DFMT_R8G8B8:
    case D3DFMT_CxV8U8:
    case D3DFMT_L6V5U5:
        return "rgb";
        break;
    case D3DFMT_X4R4G4B4:
    case D3DFMT_A8R3G3B2:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_X8R8G8B8:
    case D3DFMT_A8R8G8B8:
    case D3DFMT_Q16W16V16U16:
    case D3DFMT_Q8W8V8U8:
    case D3DFMT_A2W10V10U10:
    case D3DFMT_X8L8V8U8:
        return "rgba";
        break;
    case D3DFMT_X8B8G8R8:
    case D3DFMT_A8B8G8R8:
    case D3DFMT_A16B16G16R16:
    case D3DFMT_A2R10G10B10:
    case D3DFMT_A2B10G10R10:
        return "abgr";
        break;
    case D3DFMT_A16B16G16R16F:
    case D3DFMT_A32B32G32R32F:
        return "abgr b=32f";
        break;
    default:
        return "rgba";
        break;
    }
}
#endif
