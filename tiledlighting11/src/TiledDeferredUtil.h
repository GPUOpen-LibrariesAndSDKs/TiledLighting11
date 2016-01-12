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
// File: TiledDeferredUtil.h
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
    static const int MAX_NUM_GBUFFER_RENDER_TARGETS = 5;

    class TiledDeferredUtil
    {
    public:
        // Constructor / destructor
        TiledDeferredUtil();
        ~TiledDeferredUtil();

        void AddShadersToCache( AMD::ShaderCache *pShaderCache );

        // Various hook functions
        HRESULT OnCreateDevice( ID3D11Device* pd3dDevice );
        void OnDestroyDevice();
        HRESULT OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
        void OnReleasingSwapChain();
        void OnRender( float fElapsedTime, const GuiState& CurrentGuiState, const DepthStencilBuffer& DepthStencilBufferForOpaque, const DepthStencilBuffer& DepthStencilBufferForTransparency, const Scene& Scene, const CommonUtil& CommonUtil, const LightUtil& LightUtil, const ShadowRenderer& ShadowRenderer, const RSMRenderer& RSMRenderer );

    private:
        ID3D11ComputeShader * GetLightCullAndShadeCS( unsigned uMSAASampleCount, int nNumGBufferRenderTargets, bool bShadowsEnabled, bool bVPLsEnabled ) const;
        ID3D11ComputeShader * GetDebugDrawNumLightsPerTileCS( unsigned uMSAASampleCount, int nDebugDrawType, bool bVPLsEnabled ) const;

    private:
        // G-Buffer
        ID3D11Texture2D* m_pGBuffer[MAX_NUM_GBUFFER_RENDER_TARGETS];
        ID3D11ShaderResourceView* m_pGBufferSRV[MAX_NUM_GBUFFER_RENDER_TARGETS];
        ID3D11RenderTargetView* m_pGBufferRTV[MAX_NUM_GBUFFER_RENDER_TARGETS];

        // off-screen buffer for shading (the compute shader writes to this)
        ID3D11Texture2D*            m_pOffScreenBuffer;
        ID3D11ShaderResourceView*   m_pOffScreenBufferSRV;
        ID3D11RenderTargetView*     m_pOffScreenBufferRTV;
        ID3D11UnorderedAccessView*  m_pOffScreenBufferUAV;

        // shaders for Tiled Deferred (G-Buffer)
        static const int NUM_GBUFFER_PIXEL_SHADERS = 2*(MAX_NUM_GBUFFER_RENDER_TARGETS-1);
        ID3D11VertexShader*         m_pSceneDeferredBuildGBufferVS;
        ID3D11PixelShader*          m_pSceneDeferredBuildGBufferPS[NUM_GBUFFER_PIXEL_SHADERS];
        ID3D11InputLayout*          m_pLayoutDeferredBuildGBuffer11;

        // compute shaders for tiled culling and shading
        static const int NUM_DEFERRED_LIGHTING_COMPUTE_SHADERS = 2*2*NUM_MSAA_SETTINGS*(MAX_NUM_GBUFFER_RENDER_TARGETS-1);
        ID3D11ComputeShader*        m_pLightCullAndShadeCS[NUM_DEFERRED_LIGHTING_COMPUTE_SHADERS];

        // debug draw shaders for the lights-per-tile visualization modes
        static const int NUM_DEBUG_DRAW_COMPUTE_SHADERS = 2*2*NUM_MSAA_SETTINGS;  // one for each MSAA setting,
                                                                                  // times 2 for VPL on/off,
                                                                                  // times 2 for radar vs. grayscale
        ID3D11ComputeShader*        m_pDebugDrawNumLightsPerTileCS[NUM_DEBUG_DRAW_COMPUTE_SHADERS];

        // state for Tiled Deferred
        ID3D11BlendState*           m_pBlendStateOpaque;
        ID3D11BlendState*           m_pBlendStateAlphaToCoverage;
        ID3D11BlendState*           m_pBlendStateAlpha;
    };

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
