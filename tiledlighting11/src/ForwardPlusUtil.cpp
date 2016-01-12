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
// File: ForwardPlusUtil.cpp
//
// Helper functions for the TiledLighting11 sample.
//--------------------------------------------------------------------------------------

#include "..\\..\\DXUT\\Core\\DXUT.h"
#include "..\\..\\DXUT\\Optional\\SDKmisc.h"
#include "..\\..\\DXUT\\Optional\\DXUTCamera.h"

#include "..\\..\\AMD_SDK\\inc\\AMD_SDK.h"

#include "ForwardPlusUtil.h"
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
    ForwardPlusUtil::ForwardPlusUtil()
        :m_pScenePositionOnlyVS(NULL)
        ,m_pScenePositionAndTexVS(NULL)
        ,m_pSceneForwardVS(NULL)
        ,m_pSceneAlphaTestOnlyPS(NULL)
        ,m_pLayoutPositionOnly11(NULL)
        ,m_pLayoutPositionAndTex11(NULL)
        ,m_pLayoutForward11(NULL)
        ,m_pBlendStateOpaque(NULL)
        ,m_pBlendStateOpaqueDepthOnly(NULL)
        ,m_pBlendStateAlphaToCoverageDepthOnly(NULL)
        ,m_pBlendStateAlpha(NULL)
    {
        for( int i = 0; i < NUM_FORWARD_PIXEL_SHADERS; i++ )
        {
            m_pSceneForwardPS[i] = NULL;
        }

        for( int i = 0; i < NUM_LIGHT_CULLING_COMPUTE_SHADERS; i++ )
        {
            m_pLightCullCS[i] = NULL;
        }
    }


    //--------------------------------------------------------------------------------------
    // Destructor
    //--------------------------------------------------------------------------------------
    ForwardPlusUtil::~ForwardPlusUtil()
    {
        SAFE_RELEASE(m_pScenePositionOnlyVS);
        SAFE_RELEASE(m_pScenePositionAndTexVS);
        SAFE_RELEASE(m_pSceneForwardVS);
        SAFE_RELEASE(m_pSceneAlphaTestOnlyPS);
        SAFE_RELEASE(m_pLayoutPositionOnly11);
        SAFE_RELEASE(m_pLayoutPositionAndTex11);
        SAFE_RELEASE(m_pLayoutForward11);

        for( int i = 0; i < NUM_FORWARD_PIXEL_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pSceneForwardPS[i]);
        }

        for( int i = 0; i < NUM_LIGHT_CULLING_COMPUTE_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pLightCullCS[i]);
        }

        SAFE_RELEASE(m_pBlendStateOpaque);
        SAFE_RELEASE(m_pBlendStateOpaqueDepthOnly);
        SAFE_RELEASE(m_pBlendStateAlphaToCoverageDepthOnly);
        SAFE_RELEASE(m_pBlendStateAlpha);
    }

    //--------------------------------------------------------------------------------------
    // Device creation hook function
    //--------------------------------------------------------------------------------------
    HRESULT ForwardPlusUtil::OnCreateDevice( ID3D11Device* pd3dDevice )
    {
        HRESULT hr;

        // Create blend states 
        D3D11_BLEND_DESC BlendStateDesc;
        ZeroMemory( &BlendStateDesc, sizeof( D3D11_BLEND_DESC ) );
        BlendStateDesc.AlphaToCoverageEnable = FALSE;
        BlendStateDesc.IndependentBlendEnable = FALSE;
        BlendStateDesc.RenderTarget[0].BlendEnable = FALSE;
        BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; 
        BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO; 
        BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; 
        BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; 
        BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        V_RETURN( pd3dDevice->CreateBlendState( &BlendStateDesc, &m_pBlendStateOpaque ) );
        BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0;
        V_RETURN( pd3dDevice->CreateBlendState( &BlendStateDesc, &m_pBlendStateOpaqueDepthOnly ) );
        BlendStateDesc.AlphaToCoverageEnable = TRUE;
        V_RETURN( pd3dDevice->CreateBlendState( &BlendStateDesc, &m_pBlendStateAlphaToCoverageDepthOnly ) );
        BlendStateDesc.AlphaToCoverageEnable = FALSE;
        BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
        BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
        BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        V_RETURN( pd3dDevice->CreateBlendState( &BlendStateDesc, &m_pBlendStateAlpha ) );

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Device destruction hook function
    //--------------------------------------------------------------------------------------
    void ForwardPlusUtil::OnDestroyDevice()
    {
        SAFE_RELEASE(m_pScenePositionOnlyVS);
        SAFE_RELEASE(m_pScenePositionAndTexVS);
        SAFE_RELEASE(m_pSceneForwardVS);
        SAFE_RELEASE(m_pSceneAlphaTestOnlyPS);
        SAFE_RELEASE(m_pLayoutPositionOnly11);
        SAFE_RELEASE(m_pLayoutPositionAndTex11);
        SAFE_RELEASE(m_pLayoutForward11);

        for( int i = 0; i < NUM_FORWARD_PIXEL_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pSceneForwardPS[i]);
        }

        for( int i = 0; i < NUM_LIGHT_CULLING_COMPUTE_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pLightCullCS[i]);
        }

        SAFE_RELEASE(m_pBlendStateOpaque);
        SAFE_RELEASE(m_pBlendStateOpaqueDepthOnly);
        SAFE_RELEASE(m_pBlendStateAlphaToCoverageDepthOnly);
        SAFE_RELEASE(m_pBlendStateAlpha);
    }


    //--------------------------------------------------------------------------------------
    // Resized swap chain hook function
    //--------------------------------------------------------------------------------------
    HRESULT ForwardPlusUtil::OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
    {
        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Releasing swap chain hook function
    //--------------------------------------------------------------------------------------
    void ForwardPlusUtil::OnReleasingSwapChain()
    {
    }

    //--------------------------------------------------------------------------------------
    // Render hook function, to draw the lights (as instanced quads)
    //--------------------------------------------------------------------------------------
    void ForwardPlusUtil::OnRender( float fElapsedTime, const GuiState& CurrentGuiState, const DepthStencilBuffer& DepthStencilBufferForOpaque, const DepthStencilBuffer& DepthStencilBufferForTransparency, const Scene& Scene, const CommonUtil& CommonUtil, const LightUtil& LightUtil, const ShadowRenderer& ShadowRenderer, const RSMRenderer& RSMRenderer )
    {
        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();

        // Clear the backbuffer and depth stencil
        float ClearColor[4] = { 0.0013f, 0.0015f, 0.0050f, 0.0f };
        pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
        pd3dImmediateContext->ClearDepthStencilView( DepthStencilBufferForOpaque.m_pDepthStencilView, D3D11_CLEAR_DEPTH, 0.0f, 0 );  // we are using inverted depth, so clear to zero

        if( CurrentGuiState.m_bTransparentObjectsEnabled )
        {
            pd3dImmediateContext->ClearDepthStencilView( DepthStencilBufferForTransparency.m_pDepthStencilView, D3D11_CLEAR_DEPTH, 0.0f, 0 );  // we are using inverted depth, so clear to zero
            CommonUtil.SortTransparentObjects(Scene.m_pCamera->GetEyePt());
        }

        bool bMSAAEnabled = ( CurrentGuiState.m_uMSAASampleCount > 1 );
        bool bShadowsEnabled = ( CurrentGuiState.m_nLightingMode == LIGHTING_SHADOWS ) && CurrentGuiState.m_bShadowsEnabled;
        bool bVPLsEnabled = ( CurrentGuiState.m_nLightingMode == LIGHTING_SHADOWS ) && CurrentGuiState.m_bVPLsEnabled;

        // Default pixel shader
        ID3D11PixelShader* pScenePS = GetScenePS(false, bShadowsEnabled, bVPLsEnabled);
        ID3D11PixelShader* pScenePSAlphaTest = GetScenePS(true, bShadowsEnabled, bVPLsEnabled);

        // See if we need to use one of the debug drawing shaders instead
        bool bDebugDrawingEnabled = ( CurrentGuiState.m_nDebugDrawType == DEBUG_DRAW_RADAR_COLORS ) || ( CurrentGuiState.m_nDebugDrawType == DEBUG_DRAW_GRAYSCALE );
        if( bDebugDrawingEnabled )
        {
            pScenePS = CommonUtil.GetDebugDrawNumLightsPerTilePS(CurrentGuiState.m_nDebugDrawType, bVPLsEnabled, false);
            pScenePSAlphaTest = CommonUtil.GetDebugDrawNumLightsPerTilePS(CurrentGuiState.m_nDebugDrawType, bVPLsEnabled, false);
        }

        // Light culling compute shader
        ID3D11ComputeShader* pLightCullCS = GetLightCullCS(CurrentGuiState.m_uMSAASampleCount, bVPLsEnabled);
        ID3D11ShaderResourceView* pDepthSRV = DepthStencilBufferForOpaque.m_pDepthStencilSRV;

        // Light culling compute shader for transparent objects
        ID3D11ComputeShader* pLightCullCSForTransparency = CommonUtil.GetLightCullCSForBlendedObjects(CurrentGuiState.m_uMSAASampleCount);
        ID3D11ShaderResourceView* pDepthSRVForTransparency = DepthStencilBufferForTransparency.m_pDepthStencilSRV;

        // Switch off alpha blending
        float BlendFactor[1] = { 0.0f };
        pd3dImmediateContext->OMSetBlendState( m_pBlendStateOpaque, BlendFactor, 0xffffffff );

        // Render objects here...
        {
            ID3D11RenderTargetView* pNULLRTV = NULL;
            ID3D11DepthStencilView* pNULLDSV = NULL;
            ID3D11ShaderResourceView* pNULLSRV = NULL;
            ID3D11UnorderedAccessView* pNULLUAV = NULL;
            ID3D11SamplerState* pNULLSampler = NULL;

            TIMER_Begin( 0, L"Core algorithm" );

            TIMER_Begin( 0, L"Depth pre-pass" );
            {
                // Depth pre-pass (to eliminate pixel overdraw during forward rendering)
                pd3dImmediateContext->OMSetRenderTargets( 1, &pNULLRTV, DepthStencilBufferForOpaque.m_pDepthStencilView );  // null color buffer
                pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_GREATER), 0x00 );  // we are using inverted 32-bit float depth for better precision
                pd3dImmediateContext->IASetInputLayout( m_pLayoutPositionOnly11 );
                pd3dImmediateContext->VSSetShader( m_pScenePositionOnlyVS, NULL, 0 );
                pd3dImmediateContext->PSSetShader( NULL, NULL, 0 );  // null pixel shader
                pd3dImmediateContext->PSSetShaderResources( 0, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 1, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetSamplers( 0, 1, &pNULLSampler );

                // Draw the grid objects (i.e. the "lots of triangles" system)
                for( int i = 0; i < CurrentGuiState.m_nNumGridObjects; i++ )
                {
                    CommonUtil.DrawGrid(i, CurrentGuiState.m_nGridObjectTriangleDensity, false);
                }

                // Draw the main scene
                Scene.m_pSceneMesh->Render( pd3dImmediateContext );

                // Draw the alpha test geometry
                ID3D11BlendState* pBlendStateForAlphaTest = bMSAAEnabled ? m_pBlendStateAlphaToCoverageDepthOnly : m_pBlendStateOpaqueDepthOnly;
                pd3dImmediateContext->RSSetState( CommonUtil.GetRasterizerState(RASTERIZER_STATE_DISABLE_CULLING) );
                pd3dImmediateContext->OMSetBlendState( pBlendStateForAlphaTest, BlendFactor, 0xffffffff );
                pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, DepthStencilBufferForOpaque.m_pDepthStencilView );  // bind color buffer to prevent D3D warning
                pd3dImmediateContext->IASetInputLayout( m_pLayoutPositionAndTex11 );
                pd3dImmediateContext->VSSetShader( m_pScenePositionAndTexVS, NULL, 0 );
                pd3dImmediateContext->PSSetShader( m_pSceneAlphaTestOnlyPS, NULL, 0 );
                pd3dImmediateContext->PSSetSamplers( 0, 1, CommonUtil.GetSamplerStateParam(SAMPLER_STATE_ANISO) );
                Scene.m_pAlphaMesh->Render( pd3dImmediateContext, 0 );

                // Restore to default
                pd3dImmediateContext->RSSetState( NULL );
                pd3dImmediateContext->OMSetBlendState( m_pBlendStateOpaque, BlendFactor, 0xffffffff );

                // Draw the transparent objects
                if( CurrentGuiState.m_bTransparentObjectsEnabled )
                {
                    pd3dImmediateContext->OMSetRenderTargets( 1, &pNULLRTV, DepthStencilBufferForTransparency.m_pDepthStencilView );  // depth buffer for blended objects
                    // depth-only rendering of the transparent objects, 
                    // render them as if they were opaque, to fill the second depth buffer
                    CommonUtil.RenderTransparentObjects(CurrentGuiState.m_nDebugDrawType, false, false, true);
                }
            }
            TIMER_End(); // Depth pre-pass

            TIMER_Begin( 0, L"Light culling" );
            {
                // Cull lights on the GPU, using a Compute Shader
                pd3dImmediateContext->OMSetRenderTargets( 1, &pNULLRTV, pNULLDSV );  // null color buffer and depth-stencil
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
                pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1,  CommonUtil.GetLightIndexBufferUAVParam(), NULL );
                pd3dImmediateContext->CSSetUnorderedAccessViews( 1, 1,  CommonUtil.GetSpotIndexBufferUAVParam(), NULL );
                pd3dImmediateContext->CSSetUnorderedAccessViews( 2, 1,  CommonUtil.GetVPLIndexBufferUAVParam(), NULL );
                pd3dImmediateContext->Dispatch(CommonUtil.GetNumTilesX(),CommonUtil.GetNumTilesY(),1);

                if( CurrentGuiState.m_bTransparentObjectsEnabled )
                {
                    pd3dImmediateContext->CSSetShader( pLightCullCSForTransparency, NULL, 0 );
                    pd3dImmediateContext->CSSetShaderResources( 4, 1, &pDepthSRVForTransparency );
                    pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1,  CommonUtil.GetLightIndexBufferForBlendedObjectsUAVParam(), NULL );
                    pd3dImmediateContext->CSSetUnorderedAccessViews( 1, 1,  CommonUtil.GetSpotIndexBufferForBlendedObjectsUAVParam(), NULL );
                    pd3dImmediateContext->Dispatch(CommonUtil.GetNumTilesX(),CommonUtil.GetNumTilesY(),1);
                    pd3dImmediateContext->CSSetShaderResources( 4, 1, &pNULLSRV );
                }

                pd3dImmediateContext->CSSetShader( NULL, NULL, 0 );
                pd3dImmediateContext->CSSetShaderResources( 0, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetShaderResources( 1, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetShaderResources( 2, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetShaderResources( 3, 1, &pNULLSRV );
                pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &pNULLUAV, NULL );
                pd3dImmediateContext->CSSetUnorderedAccessViews( 1, 1, &pNULLUAV, NULL );
                pd3dImmediateContext->CSSetUnorderedAccessViews( 2, 1, &pNULLUAV, NULL );
            }
            TIMER_End(); // Light culling

            TIMER_Begin( 0, L"Forward rendering" );
            {
                // Forward rendering
                pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, DepthStencilBufferForOpaque.m_pDepthStencilView );
                pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_EQUAL_AND_DISABLE_DEPTH_WRITE), 0x00 );
                pd3dImmediateContext->IASetInputLayout( m_pLayoutForward11 );
                pd3dImmediateContext->VSSetShader( m_pSceneForwardVS, NULL, 0 );
                pd3dImmediateContext->PSSetShader( pScenePS, NULL, 0 );
                pd3dImmediateContext->PSSetSamplers( 0, 1, CommonUtil.GetSamplerStateParam(SAMPLER_STATE_ANISO) );
                pd3dImmediateContext->PSSetShaderResources( 2, 1, LightUtil.GetPointLightBufferCenterAndRadiusSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->PSSetShaderResources( 3, 1, LightUtil.GetPointLightBufferColorSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->PSSetShaderResources( 4, 1, CommonUtil.GetLightIndexBufferSRVParam() );
                pd3dImmediateContext->PSSetShaderResources( 5, 1, LightUtil.GetSpotLightBufferCenterAndRadiusSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->PSSetShaderResources( 6, 1, LightUtil.GetSpotLightBufferColorSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->PSSetShaderResources( 7, 1, LightUtil.GetSpotLightBufferSpotParamsSRVParam(CurrentGuiState.m_nLightingMode) );
                pd3dImmediateContext->PSSetShaderResources( 8, 1, CommonUtil.GetSpotIndexBufferSRVParam() );
                pd3dImmediateContext->PSSetShaderResources( 9, 1, RSMRenderer.GetVPLBufferCenterAndRadiusSRVParam() );
                pd3dImmediateContext->PSSetShaderResources( 10, 1, RSMRenderer.GetVPLBufferDataSRVParam() );
                pd3dImmediateContext->PSSetShaderResources( 11, 1, CommonUtil.GetVPLIndexBufferSRVParam() );

                if( bShadowsEnabled )
                {
                    pd3dImmediateContext->PSSetSamplers( 1, 1, CommonUtil.GetSamplerStateParam(SAMPLER_STATE_SHADOW) );
                    pd3dImmediateContext->PSSetShaderResources( 13, 1, ShadowRenderer.GetPointAtlasSRVParam() );
                    pd3dImmediateContext->PSSetShaderResources( 14, 1, ShadowRenderer.GetSpotAtlasSRVParam() );
                }

                // Draw the grid objects (i.e. the "lots of triangles" system)
                for( int i = 0; i < CurrentGuiState.m_nNumGridObjects; i++ )
                {
                    // uncomment these RSSetState calls to see the grid objects in wireframe (to see the triangle density)
                    //pd3dImmediateContext->RSSetState( CommonUtil.GetRasterizerState(RASTERIZER_STATE_WIREFRAME) );
                    CommonUtil.DrawGrid(i, CurrentGuiState.m_nGridObjectTriangleDensity);
                    //pd3dImmediateContext->RSSetState( NULL );
                }

                // Draw the main scene
                Scene.m_pSceneMesh->Render( pd3dImmediateContext, 0, 1 );

                // Draw the alpha test geometry
                pd3dImmediateContext->RSSetState( CommonUtil.GetRasterizerState(RASTERIZER_STATE_DISABLE_CULLING) );
                pd3dImmediateContext->PSSetShader( pScenePSAlphaTest, NULL, 0 );
                Scene.m_pAlphaMesh->Render( pd3dImmediateContext, 0, 1 );
                pd3dImmediateContext->RSSetState( NULL );

                if( CurrentGuiState.m_bTransparentObjectsEnabled )
                {
                    if( !bDebugDrawingEnabled )
                    {
                        pd3dImmediateContext->RSSetState( CommonUtil.GetRasterizerState(RASTERIZER_STATE_DISABLE_CULLING) );
                        pd3dImmediateContext->OMSetBlendState( m_pBlendStateAlpha, BlendFactor, 0xffffffff );
                    }

                    pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_GREATER_AND_DISABLE_DEPTH_WRITE), 0x00 );  // we are using inverted 32-bit float depth for better precision
                    pd3dImmediateContext->PSSetShaderResources( 4, 1, CommonUtil.GetLightIndexBufferForBlendedObjectsSRVParam() );
                    pd3dImmediateContext->PSSetShaderResources( 8, 1, CommonUtil.GetSpotIndexBufferForBlendedObjectsSRVParam() );
                    CommonUtil.RenderTransparentObjects(CurrentGuiState.m_nDebugDrawType, bShadowsEnabled, bVPLsEnabled, false);

                    if( !bDebugDrawingEnabled )
                    {
                        pd3dImmediateContext->RSSetState( NULL );
                        pd3dImmediateContext->OMSetBlendState( m_pBlendStateOpaque, BlendFactor, 0xffffffff );
                    }
                }

                // restore to default
                pd3dImmediateContext->PSSetShaderResources( 2, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 3, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 4, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 5, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 6, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 7, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 8, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 9, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 10, 1, &pNULLSRV );
                pd3dImmediateContext->PSSetShaderResources( 11, 1, &pNULLSRV );
                pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_GREATER), 0x00 );  // we are using inverted 32-bit float depth for better precision

                if( bShadowsEnabled )
                {
                    pd3dImmediateContext->PSSetSamplers( 1, 1, &pNULLSampler );
                    pd3dImmediateContext->PSSetShaderResources( 13, 1, &pNULLSRV );
                    pd3dImmediateContext->PSSetShaderResources( 14, 1, &pNULLSRV );
                }
            }
            TIMER_End(); // Forward rendering

            TIMER_End(); // Core algorithm

            TIMER_Begin( 0, L"Light debug drawing" );
            {
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
    void ForwardPlusUtil::AddShadersToCache( AMD::ShaderCache *pShaderCache )
    {
        // Ensure all shaders (and input layouts) are released
        SAFE_RELEASE(m_pScenePositionOnlyVS);
        SAFE_RELEASE(m_pScenePositionAndTexVS);
        SAFE_RELEASE(m_pSceneForwardVS);
        SAFE_RELEASE(m_pSceneAlphaTestOnlyPS);
        SAFE_RELEASE(m_pLayoutPositionOnly11);
        SAFE_RELEASE(m_pLayoutPositionAndTex11);
        SAFE_RELEASE(m_pLayoutForward11);

        for( int i = 0; i < NUM_FORWARD_PIXEL_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pSceneForwardPS[i]);
        }

        for( int i = 0; i < NUM_LIGHT_CULLING_COMPUTE_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pLightCullCS[i]);
        }

        AMD::ShaderCache::Macro ShaderMacroSceneForwardPS[3];
        wcscpy_s( ShaderMacroSceneForwardPS[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"USE_ALPHA_TEST" );
        wcscpy_s( ShaderMacroSceneForwardPS[1].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"SHADOWS_ENABLED" );
        wcscpy_s( ShaderMacroSceneForwardPS[2].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"VPLS_ENABLED" );

        const D3D11_INPUT_ELEMENT_DESC Layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pScenePositionOnlyVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderScenePositionOnlyVS",
            L"Forward.hlsl", 0, NULL, &m_pLayoutPositionOnly11, Layout, ARRAYSIZE( Layout ) );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pScenePositionAndTexVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderScenePositionAndTexVS",
            L"Forward.hlsl", 0, NULL, &m_pLayoutPositionAndTex11, Layout, ARRAYSIZE( Layout ) );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pSceneForwardVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderSceneForwardVS",
            L"Forward.hlsl", 0, NULL, &m_pLayoutForward11, Layout, ARRAYSIZE( Layout ) );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pSceneAlphaTestOnlyPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderSceneAlphaTestOnlyPS",
            L"Forward.hlsl", 0, NULL, NULL, NULL, 0 );

        for( int i = 0; i < 2; i++ )
        {
            // USE_ALPHA_TEST 0 first time through (false), then 1 (true)
            ShaderMacroSceneForwardPS[0].m_iValue = i;

            for( int j = 0; j < 2; j++ )
            {
                // SHADOWS_ENABLED 0 first time through (false), then 1 (true)
                ShaderMacroSceneForwardPS[1].m_iValue = j;

                for( int k = 0; k < 2; k++ )
                {
                    // VPLS_ENABLED 0 first time through (false), then 1 (true)
                    ShaderMacroSceneForwardPS[2].m_iValue = k;

                    pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pSceneForwardPS[2*2*i + 2*j + k], AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderSceneForwardPS",
                        L"Forward.hlsl", 3, ShaderMacroSceneForwardPS, NULL, NULL, 0 );
                }
            }
        }

        AMD::ShaderCache::Macro ShaderMacroLightCullCS[2];
        wcscpy_s( ShaderMacroLightCullCS[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"TILED_CULLING_COMPUTE_SHADER_MODE" );
        wcscpy_s( ShaderMacroLightCullCS[1].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"NUM_MSAA_SAMPLES" );

        // sanity check
        assert(NUM_LIGHT_CULLING_COMPUTE_SHADERS == 2*NUM_MSAA_SETTINGS);

        for( int i = 0; i < 2; i++ )
        {
            // TILED_CULLING_COMPUTE_SHADER_MODE 0 first time through (Forward+, VPLs disabled),
            // then 1 (Forward+, VPLs enabled)
            ShaderMacroLightCullCS[0].m_iValue = i;

            for( int j = 0; j < NUM_MSAA_SETTINGS; j++ )
            {
                // set NUM_MSAA_SAMPLES
                ShaderMacroLightCullCS[1].m_iValue = g_nMSAASampleCount[j];
                pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pLightCullCS[NUM_MSAA_SETTINGS*i + j], AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CullLightsCS",
                    L"TilingForward.hlsl", 2, ShaderMacroLightCullCS, NULL, NULL, 0 );
            }
        }
    }

    //--------------------------------------------------------------------------------------
    // Depth-only rendering for shadow maps
    //--------------------------------------------------------------------------------------
    void ForwardPlusUtil::RenderSceneForShadowMaps( const GuiState& CurrentGuiState, const Scene& Scene, const CommonUtil& CommonUtil )
    {
        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        ID3D11ShaderResourceView* pNULLSRV = NULL;
        ID3D11SamplerState* pNULLSampler = NULL;

        pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_LESS), 0x00 ); 
        pd3dImmediateContext->IASetInputLayout( m_pLayoutPositionOnly11 );
        pd3dImmediateContext->VSSetShader( m_pScenePositionOnlyVS, NULL, 0 );
        pd3dImmediateContext->PSSetShader( NULL, NULL, 0 );
        pd3dImmediateContext->PSSetShaderResources( 0, 1, &pNULLSRV );
        pd3dImmediateContext->PSSetShaderResources( 1, 1, &pNULLSRV );
        pd3dImmediateContext->PSSetSamplers( 0, 1, &pNULLSampler );

        // Draw the main scene
        Scene.m_pSceneMesh->Render( pd3dImmediateContext );

        // Draw the grid objects (i.e. the "lots of triangles" system)
        for( int i = 0; i < CurrentGuiState.m_nNumGridObjects; i++ )
        {
            CommonUtil.DrawGrid(i, CurrentGuiState.m_nGridObjectTriangleDensity, false);
        }

        // Draw the alpha-test geometry
        pd3dImmediateContext->RSSetState( CommonUtil.GetRasterizerState(RASTERIZER_STATE_DISABLE_CULLING) );
        pd3dImmediateContext->IASetInputLayout( m_pLayoutPositionAndTex11 );
        pd3dImmediateContext->VSSetShader( m_pScenePositionAndTexVS, NULL, 0 );
        pd3dImmediateContext->PSSetShader( m_pSceneAlphaTestOnlyPS, NULL, 0 );
        pd3dImmediateContext->PSSetSamplers( 0, 1, CommonUtil.GetSamplerStateParam(SAMPLER_STATE_ANISO) );
        Scene.m_pAlphaMesh->Render( pd3dImmediateContext, 0 );
        pd3dImmediateContext->RSSetState( NULL );
    }

    //--------------------------------------------------------------------------------------
    // Return one of the forward pixel shaders, based on settings for alpha test, shadows, and VPLs
    //--------------------------------------------------------------------------------------
    ID3D11PixelShader * ForwardPlusUtil::GetScenePS( bool bAlphaTestEnabled, bool bShadowsEnabled, bool bVPLsEnabled ) const
    {
        const int nIndexMultiplierAlphaTest = bAlphaTestEnabled ? 1 : 0;
        const int nIndexMultiplierShadows = bShadowsEnabled ? 1 : 0;
        const int nIndexVPLs = bVPLsEnabled ? 1 : 0;

        return m_pSceneForwardPS[2*2*nIndexMultiplierAlphaTest + 2*nIndexMultiplierShadows + nIndexVPLs];
    }

    //--------------------------------------------------------------------------------------
    // Return one of the light culling compute shaders, based on MSAA settings
    //--------------------------------------------------------------------------------------
    ID3D11ComputeShader * ForwardPlusUtil::GetLightCullCS( unsigned uMSAASampleCount, bool bVPLsEnabled ) const
    {
        // sanity check
        assert(NUM_LIGHT_CULLING_COMPUTE_SHADERS == 2*NUM_MSAA_SETTINGS);

        const int nIndexMultiplier = bVPLsEnabled ? 1 : 0;

        switch( uMSAASampleCount )
        {
        case 1: return m_pLightCullCS[NUM_MSAA_SETTINGS*nIndexMultiplier + MSAA_SETTING_NO_MSAA]; break;
        case 2: return m_pLightCullCS[NUM_MSAA_SETTINGS*nIndexMultiplier + MSAA_SETTING_2X_MSAA]; break;
        case 4: return m_pLightCullCS[NUM_MSAA_SETTINGS*nIndexMultiplier + MSAA_SETTING_4X_MSAA]; break;
        default: assert(false); break;
        }

        return NULL;
    }

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
