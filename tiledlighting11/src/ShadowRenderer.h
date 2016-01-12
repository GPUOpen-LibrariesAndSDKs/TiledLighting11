//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//--------------------------------------------------------------------------------------
// File: ShadowRenderer.h
//
// Shadow rendering class
//--------------------------------------------------------------------------------------

#pragma once

#include "..\\..\\DXUT\\Core\\DXUT.h"
#include "CommonConstants.h"


namespace TiledLighting11
{
    typedef void (*UpdateCameraCallback)( const DirectX::XMMATRIX& mViewProj );
    typedef void (*RenderSceneCallback)();

    class ShadowRenderer
    {
    public:

        ShadowRenderer();
        ~ShadowRenderer();

        void SetCallbacks( UpdateCameraCallback cameraCallback, RenderSceneCallback renderCallback );

        // Various hook functions
        HRESULT OnCreateDevice( ID3D11Device* pd3dDevice );
        void OnDestroyDevice();
        HRESULT OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
        void OnReleasingSwapChain();

        void RenderPointMap( int numShadowCastingPointLights );
        void RenderSpotMap( int numShadowCastingSpotLights );

        ID3D11ShaderResourceView * const * GetPointAtlasSRVParam() const { return &m_pPointAtlasSRV; }
        ID3D11ShaderResourceView * const * GetSpotAtlasSRVParam() const { return &m_pSpotAtlasSRV; }

    private:

        UpdateCameraCallback        m_CameraCallback;
        RenderSceneCallback         m_RenderCallback;

        ID3D11Texture2D*            m_pPointAtlasTexture;
        ID3D11DepthStencilView*     m_pPointAtlasView;
        ID3D11ShaderResourceView*   m_pPointAtlasSRV;

        ID3D11Texture2D*            m_pSpotAtlasTexture;
        ID3D11DepthStencilView*     m_pSpotAtlasView;
        ID3D11ShaderResourceView*   m_pSpotAtlasSRV;
    };

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
