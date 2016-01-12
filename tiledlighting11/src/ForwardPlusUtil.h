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
// File: ForwardPlusUtil.h
//
// Helper functions for the TiledLighting11 sample.
//--------------------------------------------------------------------------------------

#pragma once

#include "..\\..\\DXUT\\Core\\DXUT.h"
#include "CommonConstants.h"

// Forward declarations
namespace AMD
{
    class ShaderCache;
}
namespace TiledLighting11
{
    struct GuiState;
    struct DepthStencilBuffer;
    struct Scene;
    class CommonUtil;
    class LightUtil;
    class ShadowRenderer;
    class RSMRenderer;
}

namespace TiledLighting11
{
    class ForwardPlusUtil
    {
    public:
        // Constructor / destructor
        ForwardPlusUtil();
        ~ForwardPlusUtil();

        void AddShadersToCache( AMD::ShaderCache *pShaderCache );
        void RenderSceneForShadowMaps( const GuiState& CurrentGuiState, const Scene& Scene, const CommonUtil& CommonUtil );

        // Various hook functions
        HRESULT OnCreateDevice( ID3D11Device* pd3dDevice );
        void OnDestroyDevice();
        HRESULT OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
        void OnReleasingSwapChain();
        void OnRender( float fElapsedTime, const GuiState& CurrentGuiState, const DepthStencilBuffer& DepthStencilBufferForOpaque, const DepthStencilBuffer& DepthStencilBufferForTransparency, const Scene& Scene, const CommonUtil& CommonUtil, const LightUtil& LightUtil, const ShadowRenderer& ShadowRenderer, const RSMRenderer& RSMRenderer );

    private:
        ID3D11PixelShader * GetScenePS( bool bAlphaTestEnabled, bool bShadowsEnabled, bool bVPLsEnabled ) const;
        ID3D11ComputeShader * GetLightCullCS( unsigned uMSAASampleCount, bool bVPLsEnabled ) const;

    private:
        // shaders for Forward+
        ID3D11VertexShader*         m_pScenePositionOnlyVS;
        ID3D11VertexShader*         m_pScenePositionAndTexVS;
        ID3D11VertexShader*         m_pSceneForwardVS;
        ID3D11PixelShader*          m_pSceneAlphaTestOnlyPS;
        ID3D11InputLayout*          m_pLayoutPositionOnly11;
        ID3D11InputLayout*          m_pLayoutPositionAndTex11;
        ID3D11InputLayout*          m_pLayoutForward11;

        static const int NUM_FORWARD_PIXEL_SHADERS = 2*2*2;  // alpha test on/off, shadows on/off, VPLs on/off
        ID3D11PixelShader*          m_pSceneForwardPS[NUM_FORWARD_PIXEL_SHADERS];

        // compute shaders for tiled culling
        static const int NUM_LIGHT_CULLING_COMPUTE_SHADERS = 2*NUM_MSAA_SETTINGS;  // one for each MSAA setting,
                                                                                   // times two for VPLs enabled/disabled
        ID3D11ComputeShader*        m_pLightCullCS[NUM_LIGHT_CULLING_COMPUTE_SHADERS];

        // state for Forward+
        ID3D11BlendState*           m_pBlendStateOpaque;
        ID3D11BlendState*           m_pBlendStateOpaqueDepthOnly;
        ID3D11BlendState*           m_pBlendStateAlphaToCoverageDepthOnly;
        ID3D11BlendState*           m_pBlendStateAlpha;
    };

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
