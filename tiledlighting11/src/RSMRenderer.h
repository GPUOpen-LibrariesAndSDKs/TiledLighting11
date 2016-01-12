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
// File: RSMRenderer.h
//
// Reflective Shadow Map rendering class
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
    struct Scene;
    class CommonUtil;
    class LightUtil;
}

namespace TiledLighting11
{
    typedef void (*UpdateCameraCallback)( const DirectX::XMMATRIX& mViewProj );

    class RSMRenderer
    {
    public:

        RSMRenderer();
        ~RSMRenderer();

        void SetCallbacks( UpdateCameraCallback cameraCallback );

        void AddShadersToCache( AMD::ShaderCache *pShaderCache );

        // Various hook functions
        void OnCreateDevice( ID3D11Device* pd3dDevice );
        void OnDestroyDevice();
        HRESULT OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
        void OnReleasingSwapChain();

        void RenderSpotRSMs( int NumSpotLights, const GuiState& CurrentGuiState, const Scene& Scene, const CommonUtil& CommonUtil );
        void RenderPointRSMs( int NumPointLights, const GuiState& CurrentGuiState, const Scene& Scene, const CommonUtil& CommonUtil );
        void GenerateVPLs( int NumSpotLights, int NumPointLights, const LightUtil& LightUtil );

        ID3D11ShaderResourceView* GetSpotDepthSRV() { return m_SpotAtlas.m_pDepthSRV; }
        ID3D11ShaderResourceView* GetSpotNormalSRV() { return m_SpotAtlas.m_pNormalSRV; }
        ID3D11ShaderResourceView* GetSpotDiffuseSRV() { return m_SpotAtlas.m_pDiffuseSRV; }

        ID3D11ShaderResourceView* GetPointDepthSRV() { return m_PointAtlas.m_pDepthSRV; }
        ID3D11ShaderResourceView* GetPointNormalSRV() { return m_PointAtlas.m_pNormalSRV; }
        ID3D11ShaderResourceView* GetPointDiffuseSRV() { return m_PointAtlas.m_pDiffuseSRV; }

        ID3D11ShaderResourceView * const * GetVPLBufferCenterAndRadiusSRVParam() const { return &m_pVPLBufferCenterAndRadiusSRV; }
        ID3D11ShaderResourceView * const * GetVPLBufferDataSRVParam() const { return &m_pVPLBufferDataSRV; }

        int ReadbackNumVPLs();

    private:
        void RenderRSMScene( const GuiState& CurrentGuiState, const Scene& Scene, const CommonUtil& CommonUtil );

    private:

        UpdateCameraCallback        m_CameraCallback;

        struct GBufferAtlas
        {
            void Release()
            {
                SAFE_RELEASE( m_pDepthSRV );
                SAFE_RELEASE( m_pDepthDSV );
                SAFE_RELEASE( m_pDepthTexture );

                SAFE_RELEASE( m_pNormalRTV );
                SAFE_RELEASE( m_pNormalSRV );
                SAFE_RELEASE( m_pNormalTexture );

                SAFE_RELEASE( m_pDiffuseRTV );
                SAFE_RELEASE( m_pDiffuseSRV );
                SAFE_RELEASE( m_pDiffuseTexture );
            }

            ID3D11Texture2D*            m_pDepthTexture;
            ID3D11DepthStencilView*     m_pDepthDSV;
            ID3D11ShaderResourceView*   m_pDepthSRV;

            ID3D11Texture2D*            m_pNormalTexture;
            ID3D11RenderTargetView*     m_pNormalRTV;
            ID3D11ShaderResourceView*   m_pNormalSRV;

            ID3D11Texture2D*            m_pDiffuseTexture;
            ID3D11RenderTargetView*     m_pDiffuseRTV;
            ID3D11ShaderResourceView*   m_pDiffuseSRV;
        };

        GBufferAtlas                m_SpotAtlas;
        GBufferAtlas                m_PointAtlas;

        ID3D11Buffer*               m_pVPLBufferCenterAndRadius;
        ID3D11ShaderResourceView*   m_pVPLBufferCenterAndRadiusSRV;
        ID3D11UnorderedAccessView*  m_pVPLBufferCenterAndRadiusUAV;

        ID3D11Buffer*               m_pVPLBufferData;
        ID3D11ShaderResourceView*   m_pVPLBufferDataSRV;
        ID3D11UnorderedAccessView*  m_pVPLBufferDataUAV;

        ID3D11Buffer*               m_pSpotInvViewProjBuffer;
        ID3D11ShaderResourceView*   m_pSpotInvViewProjBufferSRV;

        ID3D11Buffer*               m_pPointInvViewProjBuffer;
        ID3D11ShaderResourceView*   m_pPointInvViewProjBufferSRV;

        ID3D11Buffer*               m_pNumVPLsConstantBuffer;
        ID3D11Buffer*               m_pCPUReadbackConstantBuffer;

        ID3D11VertexShader*         m_pRSMVS;
        ID3D11PixelShader*          m_pRSMPS;
        ID3D11InputLayout*          m_pRSMLayout;

        ID3D11ComputeShader*        m_pGenerateSpotVPLsCS;
        ID3D11ComputeShader*        m_pGeneratePointVPLsCS;
    };
    
} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
