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
// File: LightUtil.h
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
    class CommonUtil;
}

namespace TiledLighting11
{
    enum LightingMode
    {
        LIGHTING_SHADOWS = 0,
        LIGHTING_RANDOM,
        LIGHTING_NUM_MODES
    };

    class LightUtil
    {
    public:
        // Constructor / destructor
        LightUtil();
        ~LightUtil();

        static void InitLights( const DirectX::XMVECTOR &BBoxMin, const DirectX::XMVECTOR &BBoxMax );

        // returning a 2D array as XMMATRIX*[6], please forgive this ugly syntax
        static const DirectX::XMMATRIX (*GetShadowCastingPointLightViewProjTransposedArray())[6];
        static const DirectX::XMMATRIX (*GetShadowCastingPointLightViewProjInvTransposedArray())[6];

        static const DirectX::XMMATRIX* GetShadowCastingSpotLightViewProjTransposedArray();
        static const DirectX::XMMATRIX* GetShadowCastingSpotLightViewProjInvTransposedArray();

        void AddShadersToCache( AMD::ShaderCache *pShaderCache );

        void RenderLights( float fElapsedTime, unsigned uNumPointLights, unsigned uNumSpotLights, int nLightingMode, const CommonUtil& CommonUtil ) const;

        // Various hook functions
        HRESULT OnCreateDevice( ID3D11Device* pd3dDevice );
        void OnDestroyDevice();
        HRESULT OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
        void OnReleasingSwapChain();

        ID3D11ShaderResourceView * const * GetPointLightBufferCenterAndRadiusSRVParam( int nLightingMode ) const { return (nLightingMode == LIGHTING_SHADOWS) ? &m_pShadowCastingPointLightBufferCenterAndRadiusSRV : &m_pPointLightBufferCenterAndRadiusSRV; }
        ID3D11ShaderResourceView * const * GetPointLightBufferColorSRVParam( int nLightingMode ) const { return (nLightingMode == LIGHTING_SHADOWS) ? &m_pShadowCastingPointLightBufferColorSRV : &m_pPointLightBufferColorSRV; }

        ID3D11ShaderResourceView * const * GetSpotLightBufferCenterAndRadiusSRVParam( int nLightingMode ) const { return (nLightingMode == LIGHTING_SHADOWS) ? &m_pShadowCastingSpotLightBufferCenterAndRadiusSRV : &m_pSpotLightBufferCenterAndRadiusSRV; }
        ID3D11ShaderResourceView * const * GetSpotLightBufferColorSRVParam( int nLightingMode ) const { return (nLightingMode == LIGHTING_SHADOWS) ? &m_pShadowCastingSpotLightBufferColorSRV : &m_pSpotLightBufferColorSRV; }
        ID3D11ShaderResourceView * const * GetSpotLightBufferSpotParamsSRVParam( int nLightingMode ) const { return (nLightingMode == LIGHTING_SHADOWS) ? &m_pShadowCastingSpotLightBufferSpotParamsSRV : &m_pSpotLightBufferSpotParamsSRV; }
        ID3D11ShaderResourceView * const * GetSpotLightBufferSpotMatricesSRVParam( int nLightingMode ) const { return (nLightingMode == LIGHTING_SHADOWS) ? &m_pShadowCastingSpotLightBufferSpotMatricesSRV : &m_pSpotLightBufferSpotMatricesSRV; }

    private:

        // point lights
        ID3D11Buffer*               m_pPointLightBufferCenterAndRadius;
        ID3D11ShaderResourceView*   m_pPointLightBufferCenterAndRadiusSRV;
        ID3D11Buffer*               m_pPointLightBufferColor;
        ID3D11ShaderResourceView*   m_pPointLightBufferColorSRV;

        // shadow casting point lights
        ID3D11Buffer*               m_pShadowCastingPointLightBufferCenterAndRadius;
        ID3D11ShaderResourceView*   m_pShadowCastingPointLightBufferCenterAndRadiusSRV;
        ID3D11Buffer*               m_pShadowCastingPointLightBufferColor;
        ID3D11ShaderResourceView*   m_pShadowCastingPointLightBufferColorSRV;

        // spot lights
        ID3D11Buffer*               m_pSpotLightBufferCenterAndRadius;
        ID3D11ShaderResourceView*   m_pSpotLightBufferCenterAndRadiusSRV;
        ID3D11Buffer*               m_pSpotLightBufferColor;
        ID3D11ShaderResourceView*   m_pSpotLightBufferColorSRV;
        ID3D11Buffer*               m_pSpotLightBufferSpotParams;
        ID3D11ShaderResourceView*   m_pSpotLightBufferSpotParamsSRV;

        // these are only used for debug drawing the spot lights
        ID3D11Buffer*               m_pSpotLightBufferSpotMatrices;
        ID3D11ShaderResourceView*   m_pSpotLightBufferSpotMatricesSRV;

        // spot lights
        ID3D11Buffer*               m_pShadowCastingSpotLightBufferCenterAndRadius;
        ID3D11ShaderResourceView*   m_pShadowCastingSpotLightBufferCenterAndRadiusSRV;
        ID3D11Buffer*               m_pShadowCastingSpotLightBufferColor;
        ID3D11ShaderResourceView*   m_pShadowCastingSpotLightBufferColorSRV;
        ID3D11Buffer*               m_pShadowCastingSpotLightBufferSpotParams;
        ID3D11ShaderResourceView*   m_pShadowCastingSpotLightBufferSpotParamsSRV;

        // these are only used for debug drawing the spot lights
        ID3D11Buffer*               m_pShadowCastingSpotLightBufferSpotMatrices;
        ID3D11ShaderResourceView*   m_pShadowCastingSpotLightBufferSpotMatricesSRV;

        // sprite quad VB (for debug drawing the point lights)
        ID3D11Buffer*               m_pQuadForLightsVB;

        // cone VB and IB (for debug drawing the spot lights)
        ID3D11Buffer*               m_pConeForSpotLightsVB;
        ID3D11Buffer*               m_pConeForSpotLightsIB;

        // debug draw shaders for the point lights
        ID3D11VertexShader*         m_pDebugDrawPointLightsVS;
        ID3D11PixelShader*          m_pDebugDrawPointLightsPS;
        ID3D11InputLayout*          m_pDebugDrawPointLightsLayout11;

        // debug draw shaders for the spot lights
        ID3D11VertexShader*         m_pDebugDrawSpotLightsVS;
        ID3D11PixelShader*          m_pDebugDrawSpotLightsPS;
        ID3D11InputLayout*          m_pDebugDrawSpotLightsLayout11;

        // state
        ID3D11BlendState*           m_pBlendStateAdditive;
    };

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
