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
// File: TiledDeferredUtil.cpp
//
// Helper functions for the TiledLighting11 sample.
//--------------------------------------------------------------------------------------

#include "..\\..\\DXUT\\Core\\DXUT.h"
#include "..\\..\\DXUT\\Optional\\SDKmisc.h"
#include "..\\..\\DXUT\\Optional\\DXUTCamera.h"

#include "..\\..\\AMD_SDK\\inc\\AMD_SDK.h"

#include "TiledDeferredUtil.h"
#include "CommonUtil.h"
#include "LightUtil.h"
#include "ShadowRenderer.h"
#include "RSMRenderer.h"

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

namespace TiledLighting11
{

    //--------------------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------------------
    TiledDeferredUtil::TiledDeferredUtil()
        :m_pOffScreenBuffer(NULL)
        ,m_pOffScreenBufferSRV(NULL)
        ,m_pOffScreenBufferRTV(NULL)
        ,m_pOffScreenBufferUAV(NULL)
        ,m_pSceneDeferredBuildGBufferVS(NULL)
        ,m_pLayoutDeferredBuildGBuffer11(NULL)
        ,m_pBlendStateOpaque(NULL)
        ,m_pBlendStateAlphaToCoverage(NULL)
        ,m_pBlendStateAlpha(NULL)
    {
        for( int i = 0; i < MAX_NUM_GBUFFER_RENDER_TARGETS; i++ )
        {
            m_pGBuffer[i] = NULL;
            m_pGBufferSRV[i] = NULL;
            m_pGBufferRTV[i] = NULL;
        }

        for( int i = 0; i < NUM_GBUFFER_PIXEL_SHADERS; i++ )
        {
            m_pSceneDeferredBuildGBufferPS[i] = NULL;
        }

        for( int i = 0; i < NUM_DEFERRED_LIGHTING_COMPUTE_SHADERS; i++ )
        {
            m_pLightCullAndShadeCS[i] = NULL;
        }

        for( int i = 0; i < NUM_DEBUG_DRAW_COMPUTE_SHADERS; i++ )
        {
            m_pDebugDrawNumLightsPerTileCS[i] = NULL;
        }
    }


    //--------------------------------------------------------------------------------------
    // Destructor
    //--------------------------------------------------------------------------------------
    TiledDeferredUtil::~TiledDeferredUtil()
    {
        for( int i = 0; i < MAX_NUM_GBUFFER_RENDER_TARGETS; i++ )
        {
            SAFE_RELEASE(m_pGBuffer[i]);
            SAFE_RELEASE(m_pGBufferSRV[i]);
            SAFE_RELEASE(m_pGBufferRTV[i]);
        }

        SAFE_RELEASE(m_pOffScreenBuffer);
        SAFE_RELEASE(m_pOffScreenBufferSRV);
        SAFE_RELEASE(m_pOffScreenBufferRTV);
        SAFE_RELEASE(m_pOffScreenBufferUAV);

        SAFE_RELEASE(m_pSceneDeferredBuildGBufferVS);
        SAFE_RELEASE(m_pLayoutDeferredBuildGBuffer11);

        for( int i = 0; i < NUM_GBUFFER_PIXEL_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pSceneDeferredBuildGBufferPS[i]);
        }

        for( int i = 0; i < NUM_DEFERRED_LIGHTING_COMPUTE_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pLightCullAndShadeCS[i]);
        }

        for( int i = 0; i < NUM_DEBUG_DRAW_COMPUTE_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pDebugDrawNumLightsPerTileCS[i]);
        }

        SAFE_RELEASE(m_pBlendStateOpaque);
        SAFE_RELEASE(m_pBlendStateAlphaToCoverage);
        SAFE_RELEASE(m_pBlendStateAlpha);
    }

    //--------------------------------------------------------------------------------------
    // Device creation hook function
    //--------------------------------------------------------------------------------------
    HRESULT TiledDeferredUtil::OnCreateDevice( ID3D11Device* pd3dDevice )
    {
        HRESULT hr;

        // Create blend states 
        D3D11_BLEND_DESC BlendStateDesc;
        ZeroMemory( &BlendStateDesc, sizeof( D3D11_BLEND_DESC ) );
        BlendStateDesc.AlphaToCoverageEnable = FALSE;
        BlendStateDesc.IndependentBlendEnable = FALSE;
        D3D11_RENDER_TARGET_BLEND_DESC RTBlendDesc;
        RTBlendDesc.BlendEnable = FALSE;
        RTBlendDesc.SrcBlend = D3D11_BLEND_ONE; 
        RTBlendDesc.DestBlend = D3D11_BLEND_ZERO; 
        RTBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
        RTBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE; 
        RTBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO; 
        RTBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        RTBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        BlendStateDesc.RenderTarget[0] = RTBlendDesc;
        BlendStateDesc.RenderTarget[1] = RTBlendDesc;
        V_RETURN( pd3dDevice->CreateBlendState( &BlendStateDesc, &m_pBlendStateOpaque ) );
        BlendStateDesc.AlphaToCoverageEnable = TRUE;
        V_RETURN( pd3dDevice->CreateBlendState( &BlendStateDesc, &m_pBlendStateAlphaToCoverage ) );
        BlendStateDesc.AlphaToCoverageEnable = FALSE;
        RTBlendDesc.BlendEnable = TRUE;
        RTBlendDesc.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        RTBlendDesc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        RTBlendDesc.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
        RTBlendDesc.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        BlendStateDesc.RenderTarget[0] = RTBlendDesc;
        BlendStateDesc.RenderTarget[1] = RTBlendDesc;
        V_RETURN( pd3dDevice->CreateBlendState( &BlendStateDesc, &m_pBlendStateAlpha ) );

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Device destruction hook function
    //--------------------------------------------------------------------------------------
    void TiledDeferredUtil::OnDestroyDevice()
    {
        for( int i = 0; i < MAX_NUM_GBUFFER_RENDER_TARGETS; i++ )
        {
            SAFE_RELEASE(m_pGBuffer[i]);
            SAFE_RELEASE(m_pGBufferSRV[i]);
            SAFE_RELEASE(m_pGBufferRTV[i]);
        }

        SAFE_RELEASE(m_pOffScreenBuffer);
        SAFE_RELEASE(m_pOffScreenBufferSRV);
        SAFE_RELEASE(m_pOffScreenBufferRTV);
        SAFE_RELEASE(m_pOffScreenBufferUAV);

        SAFE_RELEASE(m_pSceneDeferredBuildGBufferVS);
        SAFE_RELEASE(m_pLayoutDeferredBuildGBuffer11);

        for( int i = 0; i < NUM_GBUFFER_PIXEL_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pSceneDeferredBuildGBufferPS[i]);
        }

        for( int i = 0; i < NUM_DEFERRED_LIGHTING_COMPUTE_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pLightCullAndShadeCS[i]);
        }

        for( int i = 0; i < NUM_DEBUG_DRAW_COMPUTE_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pDebugDrawNumLightsPerTileCS[i]);
        }

        SAFE_RELEASE(m_pBlendStateOpaque);
        SAFE_RELEASE(m_pBlendStateAlphaToCoverage);
        SAFE_RELEASE(m_pBlendStateAlpha)
    }


    //--------------------------------------------------------------------------------------
    // Resized swap chain hook function
    //--------------------------------------------------------------------------------------
    HRESULT TiledDeferredUtil::OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
    {
        HRESULT hr;

        // create the G-Buffer
        V_RETURN( AMD::CreateSurface( &m_pGBuffer[0], &m_pGBufferSRV[0], &m_pGBufferRTV[0], NULL, DXGI_FORMAT_R8G8B8A8_UNORM, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count ) );
        V_RETURN( AMD::CreateSurface( &m_pGBuffer[1], &m_pGBufferSRV[1], &m_pGBufferRTV[1], NULL, DXGI_FORMAT_R8G8B8A8_UNORM, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count ) );

        // create extra dummy G-Buffer render targets to test performance 
        // as the G-Buffer gets fatter
        for( int i = 2; i < MAX_NUM_GBUFFER_RENDER_TARGETS; i++ )
        {
            V_RETURN( AMD::CreateSurface( &m_pGBuffer[i], &m_pGBufferSRV[i], &m_pGBufferRTV[i], NULL, DXGI_FORMAT_R8G8B8A8_UNORM, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count ) );
        }

        // create the offscreen buffer for shading
        // (note, multisampling is not supported with UAVs, 
        // so scale the resolution instead)
        unsigned uCorrectedWidth = (pBackBufferSurfaceDesc->SampleDesc.Count > 1)  ? 2*pBackBufferSurfaceDesc->Width : pBackBufferSurfaceDesc->Width;
        unsigned uCorrectedHeight = (pBackBufferSurfaceDesc->SampleDesc.Count == 4) ? 2*pBackBufferSurfaceDesc->Height : pBackBufferSurfaceDesc->Height;
        V_RETURN( AMD::CreateSurface( &m_pOffScreenBuffer, &m_pOffScreenBufferSRV, &m_pOffScreenBufferRTV, &m_pOffScreenBufferUAV, DXGI_FORMAT_R16G16B16A16_FLOAT, uCorrectedWidth, uCorrectedHeight, 1 ) );

        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Releasing swap chain hook function
    //--------------------------------------------------------------------------------------
    void TiledDeferredUtil::OnReleasingSwapChain()
    {
        for( int i = 0; i < MAX_NUM_GBUFFER_RENDER_TARGETS; i++ )
        {
            SAFE_RELEASE(m_pGBuffer[i]);
            SAFE_RELEASE(m_pGBufferSRV[i]);
            SAFE_RELEASE(m_pGBufferRTV[i]);
        }

        SAFE_RELEASE(m_pOffScreenBuffer);
        SAFE_RELEASE(m_pOffScreenBufferSRV);
        SAFE_RELEASE(m_pOffScreenBufferRTV);
        SAFE_RELEASE(m_pOffScreenBufferUAV);
    }

    //--------------------------------------------------------------------------------------
    // Render hook function, to draw the lights (as instanced quads)
    //--------------------------------------------------------------------------------------
    void TiledDeferredUtil::OnRender( float fElapsedTime, const GuiState& CurrentGuiState, const DepthStencilBuffer& DepthStencilBufferForOpaque, const DepthStencilBuffer& DepthStencilBufferForTransparency, const Scene& Scene, const CommonUtil& CommonUtil, const LightUtil& LightUtil, const ShadowRenderer& ShadowRenderer, const RSMRenderer& RSMRenderer )
    {
        assert(CurrentGuiState.m_nNumGBufferRenderTargets >=2 && CurrentGuiState.m_nNumGBufferRenderTargets <= MAX_NUM_GBUFFER_RENDER_TARGETS);

        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        // no need to clear DXUT's main RT, because we do a full-screen blit to it later

        float ClearColorGBuffer[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        for( int i = 0; i < CurrentGuiState.m_nNumGBufferRenderTargets; i++ )
        {
            pd3dImmediateContext->ClearRenderTargetView( m_pGBufferRTV[i], ClearColorGBuffer );
        }
        pd3dImmediateContext->ClearDepthStencilView( DepthStencilBufferForOpaque.m_pDepthStencilView, D3D11_CLEAR_DEPTH, 0.0f, 0 );  // we are using inverted depth, so clear to zero

        if( CurrentGuiState.m_bTransparentObjectsEnabled )
        {
            pd3dImmediateContext->ClearDepthStencilView( DepthStencilBufferForTransparency.m_pDepthStencilView, D3D11_CLEAR_DEPTH, 0.0f, 0 );  // we are using inverted depth, so clear to zero
            CommonUtil.SortTransparentObjects(Scene.m_pCamera->GetEyePt());
        }

        bool bMSAAEnabled = ( CurrentGuiState.m_uMSAASampleCount > 1 );
        bool bShadowsEnabled = ( CurrentGuiState.m_nLightingMode == LIGHTING_SHADOWS ) && CurrentGuiState.m_bShadowsEnabled;
        bool bVPLsEnabled = ( CurrentGuiState.m_nLightingMode == LIGHTING_SHADOWS ) && CurrentGuiState.m_bVPLsEnabled;
        bool bDebugDrawingEnabled = ( CurrentGuiState.m_nDebugDrawType == DEBUG_DRAW_RADAR_COLORS ) || ( CurrentGuiState.m_nDebugDrawType == DEBUG_DRAW_GRAYSCALE );

        // Light culling compute shader
        ID3D11ComputeShader* pLightCullCS = bDebugDrawingEnabled ? GetDebugDrawNumLightsPerTileCS( CurrentGuiState.m_uMSAASampleCount, CurrentGuiState.m_nDebugDrawType, bVPLsEnabled ) : GetLightCullAndShadeCS( CurrentGuiState.m_uMSAASampleCount, CurrentGuiState.m_nNumGBufferRenderTargets, bShadowsEnabled, bVPLsEnabled );
        ID3D11ShaderResourceView* pDepthSRV = DepthStencilBufferForOpaque.m_pDepthStencilSRV;

        // Light culling compute shader for transparent objects
        ID3D11ComputeShader* pLightCullCSForTransparency = CommonUtil.GetLightCullCSForBlendedObjects(CurrentGuiState.m_uMSAASampleCount);
        ID3D11ShaderResourceView* pDepthSRVForTransparency = DepthStencilBufferForTransparency.m_pDepthStencilSRV;

        // Switch off alpha blending
        float BlendFactor[1] = { 0.0f };
        pd3dImmediateContext->OMSetBlendState( m_pBlendStateOpaque, BlendFactor, 0xffffffff );

        // Render objects here...
        {
            ID3D11RenderTargetView* pNULLRTVs[MAX_NUM_GBUFFER_RENDER_TARGETS] = { NULL, NULL, NULL, NULL, NULL };
            ID3D11DepthStencilView* pNULLDSV = NULL;
            ID3D11ShaderResourceView* pNULLSRV = NULL;
            ID3D11ShaderResourceView* pNULLSRVs[MAX_NUM_GBUFFER_RENDER_TARGETS] = { NULL, NULL, NULL, NULL, NULL };
            ID3D11UnorderedAccessView* pNULLUAV = NULL;
            ID3D11SamplerState* pNULLSampler = NULL;

            TIMER_Begin( 0, L"Core algorithm" );

            TIMER_Begin( 0, L"G-Buffer" );
            {
                // Set render targets to GBuffer RTs
                ID3D11RenderTargetView* pRTViews[MAX_NUM_GBUFFER_RENDER_TARGETS] = { NULL, NULL, NULL, NULL, NULL };
                for( int i = 0; i < CurrentGuiState.m_nNumGBufferRenderTargets; i++ )
                {
                    pRTViews[i] = m_pGBufferRTV[i];
                }
                pd3dImmediateContext->OMSetRenderTargets( (unsigned)CurrentGuiState.m_nNumGBufferRenderTargets, pRTViews, DepthStencilBufferForOpaque.m_pDepthStencilView );
                pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_GREATER), 0x00 );  // we are using inverted 32-bit float depth for better precision
                pd3dImmediateContext->IASetInputLayout( m_pLayoutDeferredBuildGBuffer11 );
                pd3dImmediateContext->VSSetShader( m_pSceneDeferredBuildGBufferVS, NULL, 0 );
                pd3dImmediateContext->PSSetShader( m_pSceneDeferredBuildGBufferPS[CurrentGuiState.m_nNumGBufferRenderTargets-2], NULL, 0 );
                pd3dImmediateContext->PSSetSamplers( 0, 1, CommonUtil.GetSamplerStateParam(SAMPLER_STATE_ANISO) );

                // Draw the grid objects (i.e. the "lots of triangles" system)
                for( int i = 0; i < CurrentGuiState.m_nNumGridObjects; i++ )
                {
                    CommonUtil.DrawGrid(i, CurrentGuiState.m_nGridObjectTriangleDensity);
                }

                // Draw the main scene
                Scene.m_pSceneMesh->Render( pd3dImmediateContext, 0, 1 );

                // Draw the alpha test geometry
                if( bMSAAEnabled )
                {
                    pd3dImmediateContext->OMSetBlendState( m_pBlendStateAlphaToCoverage, BlendFactor, 0xffffffff );
                }
                pd3dImmediateContext->RSSetState( CommonUtil.GetRasterizerState(RASTERIZER_STATE_DISABLE_CULLING) );
                pd3dImmediateContext->PSSetShader( m_pSceneDeferredBuildGBufferPS[(MAX_NUM_GBUFFER_RENDER_TARGETS-1) + (CurrentGuiState.m_nNumGBufferRenderTargets-2)], NULL, 0 );
                Scene.m_pAlphaMesh->Render( pd3dImmediateContext, 0, 1 );
                pd3dImmediateContext->RSSetState( NULL );
                if( bMSAAEnabled )
                {
                    pd3dImmediateContext->OMSetBlendState( m_pBlendStateOpaque, BlendFactor, 0xffffffff );
                }

                if( CurrentGuiState.m_bTransparentObjectsEnabled )
                {
                    ID3D11RenderTargetView* pNULLRTV = NULL;
                    pd3dImmediateContext->OMSetRenderTargets( 1, &pNULLRTV, DepthStencilBufferForTransparency.m_pDepthStencilView );  // depth buffer for blended objects
                    // depth-only rendering of the transparent objects, 
                    // render them as if they were opaque, to fill the second depth buffer
                    CommonUtil.RenderTransparentObjects(CurrentGuiState.m_nDebugDrawType, false, false, true);
                }
            }
            TIMER_End(); // G-Buffer

            TIMER_Begin( 0, L"Cull and light" );
            {
                // Cull lights and do lighting on the GPU, using a single Compute Shader
                pd3dImmediateContext->OMSetRenderTargets( (unsigned)CurrentGuiState.m_nNumGBufferRenderTargets, pNULLRTVs, pNULLDSV );  // null color buffers and depth-stencil
                pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DISABLE_DEPTH_TEST), 0x00 );
                pd3dImmediateContext->VSSetShader( NULL, NULL, 0 );  // null vertex shader
                pd3dImmediateContext->PSSetShader( NULL, NULL, 0 );  // null pixel shader
                pd3dImmediateContext->PSSetShaderResources( 0, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 1, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetSamplers( 0, 1, &pNULLSampler );
                pd3dImmediateContext->CSSetShader( pLightCullCS, NULL, 0 );
                pd3dImmediateContext->CSSetShaderResources( 0, 1, LightUtil.GetPointLightBufferCenterAndRadiusSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->CSSetShaderResources( 1, 1, LightUtil.GetSpotLightBufferCenterAndRadiusSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->CSSetShaderResources( 2, 1, RSMRenderer.GetVPLBufferCenterAndRadiusSRVParam() );
                pd3dImmediateContext->CSSetShaderResources( 3, 1, &pDepthSRV );
                pd3dImmediateContext->CSSetShaderResources( 4, 1, LightUtil.GetPointLightBufferColorSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->CSSetShaderResources( 5, 1, LightUtil.GetSpotLightBufferColorSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->CSSetShaderResources( 6, 1, LightUtil.GetSpotLightBufferSpotParamsSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->CSSetShaderResources( 7, 1, RSMRenderer.GetVPLBufferDataSRVParam() );
                pd3dImmediateContext->CSSetShaderResources( 8, (unsigned)CurrentGuiState.m_nNumGBufferRenderTargets, &m_pGBufferSRV[0] );
                pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1,  &m_pOffScreenBufferUAV, NULL );

                if( bShadowsEnabled )
                {
                    pd3dImmediateContext->CSSetSamplers( 1, 1, CommonUtil.GetSamplerStateParam(SAMPLER_STATE_SHADOW) );
                    pd3dImmediateContext->CSSetShaderResources( 13, 1, ShadowRenderer.GetPointAtlasSRVParam() );
                    pd3dImmediateContext->CSSetShaderResources( 14, 1, ShadowRenderer.GetSpotAtlasSRVParam() );
                }

                pd3dImmediateContext->Dispatch(CommonUtil.GetNumTilesX(),CommonUtil.GetNumTilesY(),1);

                if( bShadowsEnabled )
                {
                    pd3dImmediateContext->CSSetSamplers( 1, 1, &pNULLSampler );
                    pd3dImmediateContext->CSSetShaderResources( 13, 1, &pNULLSRV );
                    pd3dImmediateContext->CSSetShaderResources( 14, 1, &pNULLSRV );
                }

                pd3dImmediateContext->CSSetShaderResources( 4, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetShaderResources( 5, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetShaderResources( 6, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetShaderResources( 7, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetShaderResources( 8, (unsigned)CurrentGuiState.m_nNumGBufferRenderTargets, &pNULLSRVs[0] );

                if( CurrentGuiState.m_bTransparentObjectsEnabled )
                {
                    pd3dImmediateContext->CSSetShader( pLightCullCSForTransparency, NULL, 0 );
                    pd3dImmediateContext->CSSetShaderResources( 4, 1, &pDepthSRVForTransparency );
                    pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1,  CommonUtil.GetLightIndexBufferForBlendedObjectsUAVParam(), NULL );
                    pd3dImmediateContext->CSSetUnorderedAccessViews( 1, 1,  CommonUtil.GetSpotIndexBufferForBlendedObjectsUAVParam(), NULL );
                    pd3dImmediateContext->Dispatch(CommonUtil.GetNumTilesX(),CommonUtil.GetNumTilesY(),1);
                    pd3dImmediateContext->CSSetShaderResources( 4, 1, &pNULLSRV );
                    pd3dImmediateContext->CSSetUnorderedAccessViews( 1, 1, &pNULLUAV, NULL );
                }

                pd3dImmediateContext->CSSetShader( NULL, NULL, 0 );
                pd3dImmediateContext->CSSetShaderResources( 0, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetShaderResources( 1, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetShaderResources( 2, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetShaderResources( 3, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &pNULLUAV, NULL );
            }
            TIMER_End(); // Cull and light

            TIMER_End(); // Core algorithm

            TIMER_Begin( 0, L"Blit to main RT" );
            {
                ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
                pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, pNULLDSV );
                pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DISABLE_DEPTH_TEST), 0x00 );

                // Set the input layout
                pd3dImmediateContext->IASetInputLayout( NULL );

                // Set vertex buffer
                UINT stride = 0;
                UINT offset = 0;
                ID3D11Buffer* pBuffer[1] = { NULL };
                pd3dImmediateContext->IASetVertexBuffers( 0, 1, pBuffer, &stride, &offset );

                // Set primitive topology
                pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

                pd3dImmediateContext->VSSetShader( CommonUtil.GetFullScreenVS(), NULL, 0 );
                pd3dImmediateContext->PSSetShader( CommonUtil.GetFullScreenPS(1), NULL, 0 );
                pd3dImmediateContext->PSSetSamplers( 0, 1, CommonUtil.GetSamplerStateParam(SAMPLER_STATE_LINEAR) );
                pd3dImmediateContext->PSSetShaderResources( 0, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 1, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 2, 1, &m_pOffScreenBufferSRV );

                // Draw fullscreen quad
                pd3dImmediateContext->Draw(3,0);

                // restore to default
                pd3dImmediateContext->PSSetShaderResources( 2, 1, &pNULLSRV );
            }
            TIMER_End(); // Blit to main RT

            TIMER_Begin( 0, L"Forward transparency" );
            if( CurrentGuiState.m_bTransparentObjectsEnabled )
            {
                if( !bDebugDrawingEnabled )
                {
                    pd3dImmediateContext->RSSetState( CommonUtil.GetRasterizerState(RASTERIZER_STATE_DISABLE_CULLING) );
                    pd3dImmediateContext->OMSetBlendState( m_pBlendStateAlpha, BlendFactor, 0xffffffff );
                }

                ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
                pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, DepthStencilBufferForOpaque.m_pDepthStencilView );
                pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_GREATER_AND_DISABLE_DEPTH_WRITE), 0x00 );  // we are using inverted 32-bit float depth for better precision
                pd3dImmediateContext->PSSetShaderResources( 2, 1, LightUtil.GetPointLightBufferCenterAndRadiusSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->PSSetShaderResources( 3, 1, LightUtil.GetPointLightBufferColorSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->PSSetShaderResources( 4, 1, CommonUtil.GetLightIndexBufferForBlendedObjectsSRVParam() );
                pd3dImmediateContext->PSSetShaderResources( 5, 1, LightUtil.GetSpotLightBufferCenterAndRadiusSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->PSSetShaderResources( 6, 1, LightUtil.GetSpotLightBufferColorSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->PSSetShaderResources( 7, 1, LightUtil.GetSpotLightBufferSpotParamsSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->PSSetShaderResources( 8, 1, CommonUtil.GetSpotIndexBufferForBlendedObjectsSRVParam() );

                if( bShadowsEnabled )
                {
                    pd3dImmediateContext->PSSetSamplers( 1, 1, CommonUtil.GetSamplerStateParam(SAMPLER_STATE_SHADOW) );
                    pd3dImmediateContext->PSSetShaderResources( 13, 1, ShadowRenderer.GetPointAtlasSRVParam() );
                    pd3dImmediateContext->PSSetShaderResources( 14, 1, ShadowRenderer.GetSpotAtlasSRVParam() );
                }

                CommonUtil.RenderTransparentObjects(CurrentGuiState.m_nDebugDrawType, bShadowsEnabled, bVPLsEnabled, false);

                if( !bDebugDrawingEnabled )
                {
                    pd3dImmediateContext->RSSetState( NULL );
                    pd3dImmediateContext->OMSetBlendState( m_pBlendStateOpaque, BlendFactor, 0xffffffff );
                }

                // restore to default
                pd3dImmediateContext->PSSetShaderResources( 2, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 3, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 4, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 5, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 6, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 7, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 8, 1, &pNULLSRV );
                pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_GREATER), 0x00 );  // we are using inverted 32-bit float depth for better precision

                if( bShadowsEnabled )
                {
                    pd3dImmediateContext->PSSetSamplers( 1, 1, &pNULLSampler );
                    pd3dImmediateContext->PSSetShaderResources( 13, 1, &pNULLSRV );
                    pd3dImmediateContext->PSSetShaderResources( 14, 1, &pNULLSRV );
                }
            }
            TIMER_End(); // Forward transparency

            TIMER_Begin( 0, L"Light debug drawing" );
            {
                ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
                pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, DepthStencilBufferForOpaque.m_pDepthStencilView );

                // Light debug drawing
                if( CurrentGuiState.m_bLightDrawingEnabled )
                {
                    LightUtil.RenderLights( fElapsedTime, CurrentGuiState.m_uNumPointLights, CurrentGuiState.m_uNumSpotLights, CurrentGuiState.m_nLightingMode, CommonUtil );
                }
            }
            TIMER_End(); // Light debug drawing
        }
    }

    //--------------------------------------------------------------------------------------
    // Add shaders to the shader cache
    //--------------------------------------------------------------------------------------
    void TiledDeferredUtil::AddShadersToCache( AMD::ShaderCache *pShaderCache )
    {
        // Ensure all shaders (and input layouts) are released
        SAFE_RELEASE(m_pSceneDeferredBuildGBufferVS);
        SAFE_RELEASE(m_pLayoutDeferredBuildGBuffer11);

        for( int i = 0; i < NUM_GBUFFER_PIXEL_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pSceneDeferredBuildGBufferPS[i]);
        }

        for( int i = 0; i < NUM_DEFERRED_LIGHTING_COMPUTE_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pLightCullAndShadeCS[i]);
        }

        for( int i = 0; i < NUM_DEBUG_DRAW_COMPUTE_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pDebugDrawNumLightsPerTileCS[i]);
        }

        AMD::ShaderCache::Macro ShaderMacroBuildGBufferPS[2];
        wcscpy_s( ShaderMacroBuildGBufferPS[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"USE_ALPHA_TEST" );
        wcscpy_s( ShaderMacroBuildGBufferPS[1].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"NUM_GBUFFER_RTS" );

        const D3D11_INPUT_ELEMENT_DESC Layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pSceneDeferredBuildGBufferVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderSceneToGBufferVS",
            L"Deferred.hlsl", 0, NULL, &m_pLayoutDeferredBuildGBuffer11, Layout, ARRAYSIZE( Layout ) );

        for( int i = 0; i < 2; i++ )
        {
            // USE_ALPHA_TEST false first time through, then true
            ShaderMacroBuildGBufferPS[0].m_iValue = i;

            for( int j = 2; j <= MAX_NUM_GBUFFER_RENDER_TARGETS; j++ )
            {
                // set NUM_GBUFFER_RTS
                ShaderMacroBuildGBufferPS[1].m_iValue = j;

                pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pSceneDeferredBuildGBufferPS[(MAX_NUM_GBUFFER_RENDER_TARGETS-1)*i+j-2], AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderSceneToGBufferPS",
                    L"Deferred.hlsl", 2, ShaderMacroBuildGBufferPS, NULL, NULL, 0 );
            }
        }

        AMD::ShaderCache::Macro ShaderMacroLightCullCS[5];
        wcscpy_s( ShaderMacroLightCullCS[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"TILED_CULLING_COMPUTE_SHADER_MODE" );
        wcscpy_s( ShaderMacroLightCullCS[1].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"SHADOWS_ENABLED" );
        wcscpy_s( ShaderMacroLightCullCS[2].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"NUM_MSAA_SAMPLES" );
        wcscpy_s( ShaderMacroLightCullCS[3].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"NUM_GBUFFER_RTS" );
        wcscpy_s( ShaderMacroLightCullCS[4].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"LIGHTS_PER_TILE_MODE" );

        // Set LIGHTS_PER_TILE_MODE to 0 (lights per tile visualization disabled)
        ShaderMacroLightCullCS[4].m_iValue = 0;

        for( int i = 0; i < 2; i++ )
        {
            // TILED_CULLING_COMPUTE_SHADER_MODE 2 first time through (Tiled Deferred, VPLs disabled),
            // then 3 (Tiled Deferred, VPLs enabled)
            ShaderMacroLightCullCS[0].m_iValue = i+2;

            for( int j = 0; j < 2; j++ )
            {
                // SHADOWS_ENABLED false first time through, then true
                ShaderMacroLightCullCS[1].m_iValue = j;

                for( int k = 0; k < NUM_MSAA_SETTINGS; k++ )
                {
                    // set NUM_MSAA_SAMPLES
                    ShaderMacroLightCullCS[2].m_iValue = g_nMSAASampleCount[k];

                    for( int m = 2; m <= MAX_NUM_GBUFFER_RENDER_TARGETS; m++ )
                    {
                        // set NUM_GBUFFER_RTS
                        ShaderMacroLightCullCS[3].m_iValue = m;

                        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pLightCullAndShadeCS[2*NUM_MSAA_SETTINGS*(MAX_NUM_GBUFFER_RENDER_TARGETS-1)*i + NUM_MSAA_SETTINGS*(MAX_NUM_GBUFFER_RENDER_TARGETS-1)*j + (MAX_NUM_GBUFFER_RENDER_TARGETS-1)*k + m-2], AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CullLightsAndDoLightingCS",
                            L"TilingDeferred.hlsl", 5, ShaderMacroLightCullCS, NULL, NULL, 0 );
                    }
                }
            }
        }

        // Set SHADOWS_ENABLED to 0 (false)
        ShaderMacroLightCullCS[1].m_iValue = 0;

        // Set NUM_GBUFFER_RTS to 2
        ShaderMacroLightCullCS[3].m_iValue = 2;

        for( int i = 0; i < 2; i++ )
        {
            // TILED_CULLING_COMPUTE_SHADER_MODE 2 first time through (Tiled Deferred, VPLs disabled),
            // then 3 (Tiled Deferred, VPLs enabled)
            ShaderMacroLightCullCS[0].m_iValue = i+2;

            for( int j = 0; j < 2; j++ )
            {
                // LIGHTS_PER_TILE_MODE 1 first time through (grayscale), then 2 (radar colors)
                ShaderMacroLightCullCS[4].m_iValue = j+1;

                for( int k = 0; k < NUM_MSAA_SETTINGS; k++ )
                {
                    // set NUM_MSAA_SAMPLES
                    ShaderMacroLightCullCS[2].m_iValue = g_nMSAASampleCount[k];

                    pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawNumLightsPerTileCS[2*NUM_MSAA_SETTINGS*i + NUM_MSAA_SETTINGS*j + k], AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CullLightsAndDoLightingCS",
                        L"TilingDeferred.hlsl", 5, ShaderMacroLightCullCS, NULL, NULL, 0 );
                }
            }
        }
    }

    //--------------------------------------------------------------------------------------
    // Return one of the light culling and shading compute shaders, based on MSAA settings
    //--------------------------------------------------------------------------------------
    ID3D11ComputeShader * TiledDeferredUtil::GetLightCullAndShadeCS( unsigned uMSAASampleCount, int nNumGBufferRenderTargets, bool bShadowsEnabled, bool bVPLsEnabled ) const
    {
        const int nIndexMultiplierShadows = bShadowsEnabled ? 1 : 0;
        const int nIndexMultiplierVPLs = bVPLsEnabled ? 1 : 0;

        int nMSAAMode = 0;
        switch( uMSAASampleCount )
        {
        case 1: nMSAAMode = 0; break;
        case 2: nMSAAMode = 1; break;
        case 4: nMSAAMode = 2; break;
        default: assert(false); break;
        }

        return m_pLightCullAndShadeCS[(2*NUM_MSAA_SETTINGS*(MAX_NUM_GBUFFER_RENDER_TARGETS-1)*nIndexMultiplierVPLs) + (NUM_MSAA_SETTINGS*(MAX_NUM_GBUFFER_RENDER_TARGETS-1)*nIndexMultiplierShadows) + ((MAX_NUM_GBUFFER_RENDER_TARGETS-1)*nMSAAMode) + (nNumGBufferRenderTargets-2)];
    }

    //--------------------------------------------------------------------------------------
    // Return one of the lights-per-tile visualization compute shaders, based on MSAA settings
    //--------------------------------------------------------------------------------------
    ID3D11ComputeShader * TiledDeferredUtil::GetDebugDrawNumLightsPerTileCS( unsigned uMSAASampleCount, int nDebugDrawType, bool bVPLsEnabled ) const
    {
        if ( ( nDebugDrawType != DEBUG_DRAW_RADAR_COLORS ) && ( nDebugDrawType != DEBUG_DRAW_GRAYSCALE ) )
        {
            return NULL;
        }

        const int nIndexMultiplierDebugDrawType = ( nDebugDrawType == DEBUG_DRAW_RADAR_COLORS ) ? 1 : 0;
        const int nIndexMultiplierVPLs = bVPLsEnabled ? 1 : 0;

        int nMSAAMode = 0;
        switch( uMSAASampleCount )
        {
        case 1: nMSAAMode = 0; break;
        case 2: nMSAAMode = 1; break;
        case 4: nMSAAMode = 2; break;
        default: assert(false); break;
        }

        return m_pDebugDrawNumLightsPerTileCS[(2*NUM_MSAA_SETTINGS*nIndexMultiplierVPLs) + (NUM_MSAA_SETTINGS*nIndexMultiplierDebugDrawType) + nMSAAMode];
    }

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
