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
// File: ShadowRenderer.cpp
//
// Shadow rendering class
//--------------------------------------------------------------------------------------

#include "..\\..\\DXUT\\Core\\DXUT.h"

#include "..\\..\\AMD_SDK\\inc\\AMD_SDK.h"

#include "ShadowRenderer.h"
#include "LightUtil.h"


#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

using namespace DirectX;

static const int gPointShadowResolution = 256;
static const int gSpotShadowResolution = 256;


namespace TiledLighting11
{

    ShadowRenderer::ShadowRenderer()
        :m_CameraCallback( 0 ),
        m_RenderCallback( 0 ),
        m_pPointAtlasTexture( 0 ),
        m_pPointAtlasView( 0 ),
        m_pPointAtlasSRV( 0 ),
        m_pSpotAtlasTexture( 0 ),
        m_pSpotAtlasView( 0 ),
        m_pSpotAtlasSRV( 0 )
    {
    }


    ShadowRenderer::~ShadowRenderer()
    {
    }


    void ShadowRenderer::SetCallbacks( UpdateCameraCallback cameraCallback, RenderSceneCallback renderCallback )
    {
        m_CameraCallback = cameraCallback;
        m_RenderCallback = renderCallback;
    }


    HRESULT ShadowRenderer::OnCreateDevice( ID3D11Device* pd3dDevice )
    {
        HRESULT hr;
        V_RETURN( AMD::CreateDepthStencilSurface( &m_pPointAtlasTexture, &m_pPointAtlasSRV, &m_pPointAtlasView, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_R16_UNORM, 6 * gPointShadowResolution, MAX_NUM_SHADOWCASTING_POINTS * gPointShadowResolution, 1 ) );
        V_RETURN( AMD::CreateDepthStencilSurface( &m_pSpotAtlasTexture, &m_pSpotAtlasSRV, &m_pSpotAtlasView, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_R16_UNORM, MAX_NUM_SHADOWCASTING_SPOTS * gSpotShadowResolution, gSpotShadowResolution, 1 ) );
        return S_OK;
    }


    void ShadowRenderer::OnDestroyDevice()
    {
        SAFE_RELEASE( m_pSpotAtlasSRV );
        SAFE_RELEASE( m_pSpotAtlasView );
        SAFE_RELEASE( m_pSpotAtlasTexture );

        SAFE_RELEASE( m_pPointAtlasSRV );
        SAFE_RELEASE( m_pPointAtlasView );
        SAFE_RELEASE( m_pPointAtlasTexture );
    }

    HRESULT ShadowRenderer::OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
    {
        return S_OK;
    }

    void ShadowRenderer::OnReleasingSwapChain()
    {
    }

    void ShadowRenderer::RenderPointMap( int numShadowCastingPointLights )
    {
        AMDProfileEvent( AMD_PROFILE_RED, L"PointShadows" ); 

        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        D3D11_VIEWPORT oldVp[ 8 ];
        UINT numVPs = 1;
        pd3dImmediateContext->RSGetViewports( &numVPs, oldVp );

        D3D11_VIEWPORT vp;
        vp.Width = gPointShadowResolution;
        vp.Height = gPointShadowResolution;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;

        pd3dImmediateContext->ClearDepthStencilView( m_pPointAtlasView, D3D11_CLEAR_DEPTH, 1.0f, 0 );

        ID3D11RenderTargetView* pNULLRTV = NULL;
        pd3dImmediateContext->OMSetRenderTargets( 1, &pNULLRTV, m_pPointAtlasView );

        const XMMATRIX (*PointLightViewProjArray)[6] = LightUtil::GetShadowCastingPointLightViewProjTransposedArray();

        for ( int p = 0; p < numShadowCastingPointLights; p++ )
        {
            vp.TopLeftY = (float)p * gPointShadowResolution;

            for ( int i = 0; i < 6; i++ )
            {
                m_CameraCallback( PointLightViewProjArray[p][i] );

                vp.TopLeftX = (float)i * gPointShadowResolution;
                pd3dImmediateContext->RSSetViewports( 1, &vp );

                m_RenderCallback();
            }
        }

        pd3dImmediateContext->RSSetViewports( 1, oldVp );
    }


    void ShadowRenderer::RenderSpotMap( int numShadowCastingSpotLights )
    {
        AMDProfileEvent( AMD_PROFILE_RED, L"SpotShadows" ); 

        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        D3D11_VIEWPORT oldVp[ 8 ];
        UINT numVPs = 1;
        pd3dImmediateContext->RSGetViewports( &numVPs, oldVp );

        D3D11_VIEWPORT vp;
        vp.Width = gSpotShadowResolution;
        vp.Height = gSpotShadowResolution;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftY = 0.0f;

        pd3dImmediateContext->ClearDepthStencilView( m_pSpotAtlasView, D3D11_CLEAR_DEPTH, 1.0f, 0 );

        ID3D11RenderTargetView* pNULLRTV = NULL;
        pd3dImmediateContext->OMSetRenderTargets( 1, &pNULLRTV, m_pSpotAtlasView );

        const XMMATRIX* SpotLightViewProjArray = LightUtil::GetShadowCastingSpotLightViewProjTransposedArray();

        for ( int i = 0; i < numShadowCastingSpotLights; i++ )
        {
            vp.TopLeftX = (float)i * gSpotShadowResolution;

            m_CameraCallback( SpotLightViewProjArray[i] );

            pd3dImmediateContext->RSSetViewports( 1, &vp );

            m_RenderCallback();
        }

        pd3dImmediateContext->RSSetViewports( 1, oldVp );
    }

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
