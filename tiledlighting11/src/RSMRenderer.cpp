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
// File: RSMRenderer.cpp
//
// Reflective Shadow Map rendering class
//--------------------------------------------------------------------------------------

#include "..\\..\\DXUT\\Core\\DXUT.h"

#include "..\\..\\AMD_SDK\\inc\\AMD_SDK.h"

#include "RSMRenderer.h"
#include "CommonUtil.h"
#include "LightUtil.h"


#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

using namespace DirectX;

static const int gRSMSpotResolution = 32;
static const int gRSMPointResolution = 32;


namespace TiledLighting11
{

    RSMRenderer::RSMRenderer()
        :m_CameraCallback( 0 ),
        m_pVPLBufferCenterAndRadius( 0 ),
        m_pVPLBufferCenterAndRadiusSRV( 0 ),
        m_pVPLBufferCenterAndRadiusUAV( 0 ),
        m_pVPLBufferData( 0 ),
        m_pVPLBufferDataSRV( 0 ),
        m_pVPLBufferDataUAV( 0 ),
        m_pSpotInvViewProjBuffer( 0 ),
        m_pSpotInvViewProjBufferSRV( 0 ),
        m_pPointInvViewProjBuffer( 0 ),
        m_pPointInvViewProjBufferSRV( 0 ),
        m_pNumVPLsConstantBuffer( 0 ),
        m_pCPUReadbackConstantBuffer( 0 ),
        m_pRSMVS( 0 ),
        m_pRSMPS( 0 ),
        m_pRSMLayout( 0 ),
        m_pGenerateSpotVPLsCS( 0 ),
        m_pGeneratePointVPLsCS( 0 )
    {
        ZeroMemory( &m_SpotAtlas, sizeof( m_SpotAtlas ) );
        ZeroMemory( &m_PointAtlas, sizeof( m_PointAtlas ) );
    }


    RSMRenderer::~RSMRenderer()
    {
    }


    void RSMRenderer::SetCallbacks( UpdateCameraCallback cameraCallback )
    {
        m_CameraCallback = cameraCallback;
    }


    void RSMRenderer::OnCreateDevice( ID3D11Device* pd3dDevice )
    {
        HRESULT hr;

        int width = gRSMSpotResolution * MAX_NUM_SHADOWCASTING_SPOTS;
        int height = gRSMSpotResolution;
        V( AMD::CreateDepthStencilSurface( &m_SpotAtlas.m_pDepthTexture, &m_SpotAtlas.m_pDepthSRV, &m_SpotAtlas.m_pDepthDSV, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_R16_UNORM, width, height, 1 ) );
        V( AMD::CreateSurface( &m_SpotAtlas.m_pNormalTexture, &m_SpotAtlas.m_pNormalSRV, &m_SpotAtlas.m_pNormalRTV, 0, DXGI_FORMAT_R11G11B10_FLOAT, width, height, 1 ) );
        V( AMD::CreateSurface( &m_SpotAtlas.m_pDiffuseTexture, &m_SpotAtlas.m_pDiffuseSRV, &m_SpotAtlas.m_pDiffuseRTV, 0, DXGI_FORMAT_R11G11B10_FLOAT, width, height, 1 ) );
        int maxVPLs = width * height;

        width = gRSMPointResolution * 6;
        height = gRSMPointResolution * MAX_NUM_SHADOWCASTING_POINTS;
        V( AMD::CreateDepthStencilSurface( &m_PointAtlas.m_pDepthTexture, &m_PointAtlas.m_pDepthSRV, &m_PointAtlas.m_pDepthDSV, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_R16_UNORM, width, height, 1 ) );
        V( AMD::CreateSurface( &m_PointAtlas.m_pNormalTexture, &m_PointAtlas.m_pNormalSRV, &m_PointAtlas.m_pNormalRTV, 0, DXGI_FORMAT_R11G11B10_FLOAT, width, height, 1 ) );
        V( AMD::CreateSurface( &m_PointAtlas.m_pDiffuseTexture, &m_PointAtlas.m_pDiffuseSRV, &m_PointAtlas.m_pDiffuseRTV, 0, DXGI_FORMAT_R11G11B10_FLOAT, width, height, 1 ) );
        maxVPLs += width * height;

        D3D11_BUFFER_DESC desc;

        ZeroMemory( &desc, sizeof( desc ) );
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.StructureByteStride = 16;
        desc.ByteWidth = desc.StructureByteStride * maxVPLs;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

        V( pd3dDevice->CreateBuffer( &desc, 0, &m_pVPLBufferCenterAndRadius ) );
        DXUT_SetDebugName( m_pVPLBufferCenterAndRadius, "VPLBufferCenterAndRadius" );

        // see struct VPLData in CommonHeader.h
        desc.StructureByteStride = 48;
        desc.ByteWidth = desc.StructureByteStride * maxVPLs;
        V( pd3dDevice->CreateBuffer( &desc, 0, &m_pVPLBufferData ) );
        DXUT_SetDebugName( m_pVPLBufferData, "VPLBufferData" );

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.ElementOffset = 0;
        SRVDesc.Buffer.ElementWidth = maxVPLs;
        V( pd3dDevice->CreateShaderResourceView( m_pVPLBufferCenterAndRadius, &SRVDesc, &m_pVPLBufferCenterAndRadiusSRV ) );
        V( pd3dDevice->CreateShaderResourceView( m_pVPLBufferData, &SRVDesc, &m_pVPLBufferDataSRV ) );

        D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
        UAVDesc.Format = SRVDesc.Format;
        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = maxVPLs;
        UAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
        V( pd3dDevice->CreateUnorderedAccessView( m_pVPLBufferCenterAndRadius, &UAVDesc, &m_pVPLBufferCenterAndRadiusUAV ) );
        V( pd3dDevice->CreateUnorderedAccessView( m_pVPLBufferData, &UAVDesc, &m_pVPLBufferDataUAV ) );

        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.StructureByteStride = 16*4;
        desc.ByteWidth = desc.StructureByteStride * MAX_NUM_SHADOWCASTING_SPOTS;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        V( pd3dDevice->CreateBuffer( &desc, 0, &m_pSpotInvViewProjBuffer ) );
        DXUT_SetDebugName( m_pSpotInvViewProjBuffer, "SpotInvViewProjBuffer" );

        SRVDesc.Buffer.ElementWidth = MAX_NUM_SHADOWCASTING_SPOTS;
        V( pd3dDevice->CreateShaderResourceView( m_pSpotInvViewProjBuffer, &SRVDesc, &m_pSpotInvViewProjBufferSRV ) );

        desc.ByteWidth = desc.StructureByteStride * MAX_NUM_SHADOWCASTING_POINTS * 6;
        V( pd3dDevice->CreateBuffer( &desc, 0, &m_pPointInvViewProjBuffer ) );
        DXUT_SetDebugName( m_pPointInvViewProjBuffer, "PointInvViewProjBuffer" );

        SRVDesc.Buffer.ElementWidth = 6*MAX_NUM_SHADOWCASTING_POINTS;
        V( pd3dDevice->CreateShaderResourceView( m_pPointInvViewProjBuffer, &SRVDesc, &m_pPointInvViewProjBufferSRV ) );


        ZeroMemory( &desc, sizeof( desc ) );
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = 16;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        V( pd3dDevice->CreateBuffer( &desc, 0, &m_pNumVPLsConstantBuffer ) );
        DXUT_SetDebugName( m_pNumVPLsConstantBuffer, "NumVPLsConstantBuffer" );

        desc.BindFlags = 0;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        V( pd3dDevice->CreateBuffer( &desc, 0, &m_pCPUReadbackConstantBuffer ) );
        DXUT_SetDebugName( m_pCPUReadbackConstantBuffer, "CPUReadbackConstantBuffer" );
    }


    void RSMRenderer::OnDestroyDevice()
    {
        SAFE_RELEASE( m_pGeneratePointVPLsCS );
        SAFE_RELEASE( m_pGenerateSpotVPLsCS );

        SAFE_RELEASE( m_pRSMLayout );
        SAFE_RELEASE( m_pRSMPS );
        SAFE_RELEASE( m_pRSMVS );

        SAFE_RELEASE( m_pCPUReadbackConstantBuffer );
        SAFE_RELEASE( m_pNumVPLsConstantBuffer );

        SAFE_RELEASE( m_pPointInvViewProjBufferSRV );
        SAFE_RELEASE( m_pPointInvViewProjBuffer );

        SAFE_RELEASE( m_pSpotInvViewProjBufferSRV );
        SAFE_RELEASE( m_pSpotInvViewProjBuffer );

        SAFE_RELEASE( m_pVPLBufferCenterAndRadiusSRV );
        SAFE_RELEASE( m_pVPLBufferCenterAndRadiusUAV );
        SAFE_RELEASE( m_pVPLBufferCenterAndRadius );
        SAFE_RELEASE( m_pVPLBufferDataSRV );
        SAFE_RELEASE( m_pVPLBufferDataUAV );
        SAFE_RELEASE( m_pVPLBufferData );

        m_SpotAtlas.Release();
        m_PointAtlas.Release();
    }


    HRESULT RSMRenderer::OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
    {
        return S_OK;
    }

    void RSMRenderer::OnReleasingSwapChain()
    {
    }


    void RSMRenderer::RenderSpotRSMs( int NumSpotLights, const GuiState& CurrentGuiState, const Scene& Scene, const CommonUtil& CommonUtil )
    {
        AMDProfileEvent( AMD_PROFILE_RED, L"SpotRSM" ); 

        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        D3D11_VIEWPORT oldVp[ 8 ];
        UINT numVPs = 1;
        pd3dImmediateContext->RSGetViewports( &numVPs, oldVp );

        ID3D11RenderTargetView* pRTVs[] = { m_SpotAtlas.m_pNormalRTV, m_SpotAtlas.m_pDiffuseRTV };
        const int NumTargets = ARRAYSIZE( pRTVs );

        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        pd3dImmediateContext->ClearDepthStencilView( m_SpotAtlas.m_pDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0 );
        pd3dImmediateContext->ClearRenderTargetView( m_SpotAtlas.m_pNormalRTV, clearColor );
        pd3dImmediateContext->ClearRenderTargetView( m_SpotAtlas.m_pDiffuseRTV, clearColor );

        pd3dImmediateContext->OMSetRenderTargets( NumTargets, pRTVs, m_SpotAtlas.m_pDepthDSV );

        D3D11_VIEWPORT vps[ 8 ];
        for ( int i = 0; i < NumTargets; i++ )
        {
            vps[ i ].Width = gRSMSpotResolution;
            vps[ i ].Height = gRSMSpotResolution;
            vps[ i ].MinDepth = 0.0f;
            vps[ i ].MaxDepth = 1.0f;
            vps[ i ].TopLeftY = 0.0f;
            vps[ i ].TopLeftX = 0.0f;
        }

        HRESULT hr;
        D3D11_MAPPED_SUBRESOURCE MappedSubresource;
        V( pd3dImmediateContext->Map( m_pSpotInvViewProjBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource ) );
        memcpy(MappedSubresource.pData, LightUtil::GetShadowCastingSpotLightViewProjInvTransposedArray(), sizeof(XMMATRIX)*MAX_NUM_SHADOWCASTING_SPOTS);
        pd3dImmediateContext->Unmap( m_pSpotInvViewProjBuffer, 0 );

        const XMMATRIX* SpotLightViewProjArray = LightUtil::GetShadowCastingSpotLightViewProjTransposedArray();

        for ( int i = 0; i < NumSpotLights; i++ )
        {
            for ( int j = 0; j < NumTargets; j++ )
            {
                vps[ j ].TopLeftX = (float)i * gRSMSpotResolution;
            }

            pd3dImmediateContext->RSSetViewports( NumTargets, vps );
            m_CameraCallback( SpotLightViewProjArray[i] );

            RenderRSMScene( CurrentGuiState, Scene, CommonUtil );
        }

        pd3dImmediateContext->RSSetViewports( 1, oldVp );

        ID3D11RenderTargetView* pNullRTVs[] = { 0, 0, 0 };

        pd3dImmediateContext->OMSetRenderTargets( ARRAYSIZE( pNullRTVs ), pNullRTVs, 0 );
    }


    void RSMRenderer::RenderPointRSMs( int NumPointLights, const GuiState& CurrentGuiState, const Scene& Scene, const CommonUtil& CommonUtil )
    {
        AMDProfileEvent( AMD_PROFILE_RED, L"PointRSM" ); 

        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        D3D11_VIEWPORT oldVp[ 8 ];
        UINT numVPs = 1;
        pd3dImmediateContext->RSGetViewports( &numVPs, oldVp );

        ID3D11RenderTargetView* pRTVs[] = { m_PointAtlas.m_pNormalRTV, m_PointAtlas.m_pDiffuseRTV };
        const int NumTargets = ARRAYSIZE( pRTVs );

        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        pd3dImmediateContext->ClearDepthStencilView( m_PointAtlas.m_pDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0 );
        pd3dImmediateContext->ClearRenderTargetView( m_PointAtlas.m_pNormalRTV, clearColor );
        pd3dImmediateContext->ClearRenderTargetView( m_PointAtlas.m_pDiffuseRTV, clearColor );

        pd3dImmediateContext->OMSetRenderTargets( NumTargets, pRTVs, m_PointAtlas.m_pDepthDSV );

        D3D11_VIEWPORT vps[ 8 ];
        for ( int i = 0; i < NumTargets; i++ )
        {
            vps[ i ].Width = gRSMPointResolution;
            vps[ i ].Height = gRSMPointResolution;
            vps[ i ].MinDepth = 0.0f;
            vps[ i ].MaxDepth = 1.0f;
            vps[ i ].TopLeftY = 0.0f;
            vps[ i ].TopLeftX = 0.0f;
        }

        HRESULT hr;
        D3D11_MAPPED_SUBRESOURCE MappedSubresource;
        V( pd3dImmediateContext->Map( m_pPointInvViewProjBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource ) );
        memcpy(MappedSubresource.pData, LightUtil::GetShadowCastingPointLightViewProjInvTransposedArray(), sizeof(XMMATRIX)*MAX_NUM_SHADOWCASTING_POINTS*6);
        pd3dImmediateContext->Unmap( m_pPointInvViewProjBuffer, 0 );

        const XMMATRIX (*PointLightViewProjArray)[6] = LightUtil::GetShadowCastingPointLightViewProjTransposedArray();

        for ( int i = 0; i < NumPointLights; i++ )
        {
            for ( int j = 0; j < NumTargets; j++ )
            {
                vps[ j ].TopLeftY = (float)i * gRSMPointResolution;
            }

            for ( int f = 0; f < 6; f++ )
            {
                m_CameraCallback( PointLightViewProjArray[i][f] );

                for ( int j = 0; j < NumTargets; j++ )
                {
                    vps[ j ].TopLeftX = (float)f * gRSMPointResolution;
                }

                pd3dImmediateContext->RSSetViewports( NumTargets, vps );

                RenderRSMScene( CurrentGuiState, Scene, CommonUtil );
            }
        }

        pd3dImmediateContext->RSSetViewports( 1, oldVp );

        ID3D11RenderTargetView* pNullRTVs[] = { 0, 0, 0 };

        pd3dImmediateContext->OMSetRenderTargets( ARRAYSIZE( pNullRTVs ), pNullRTVs, 0 );
    }


    void RSMRenderer::GenerateVPLs( int NumSpotLights, int NumPointLights, const LightUtil& LightUtil )
    {
        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        ID3D11UnorderedAccessView* pUAVs[] = { m_pVPLBufferCenterAndRadiusUAV, m_pVPLBufferDataUAV };

        // Clear the VPL counter
        UINT InitialCounts[ 2 ] = { 0, 0 };
        pd3dImmediateContext->CSSetUnorderedAccessViews( 0, ARRAYSIZE( pUAVs ), pUAVs, InitialCounts );

        const int numThreadsX = 16;
        const int numThreadsY = 16;

        const int sampleKernel = 2;

        if ( NumSpotLights > 0 )
        {
            ID3D11ShaderResourceView* pSRVs[] = 
			{ 
				m_SpotAtlas.m_pDepthSRV, 
				m_SpotAtlas.m_pNormalSRV, 
				m_SpotAtlas.m_pDiffuseSRV, 
				m_pSpotInvViewProjBufferSRV, 
				*LightUtil.GetSpotLightBufferCenterAndRadiusSRVParam(LIGHTING_SHADOWS),  
				*LightUtil.GetSpotLightBufferColorSRVParam(LIGHTING_SHADOWS),
				*LightUtil.GetSpotLightBufferSpotParamsSRVParam(LIGHTING_SHADOWS)
			};
            pd3dImmediateContext->CSSetShaderResources( 0, ARRAYSIZE( pSRVs ), pSRVs );
            pd3dImmediateContext->CSSetShader( m_pGenerateSpotVPLsCS, 0, 0 );

            int dispatchCountX = (NumSpotLights * gRSMSpotResolution/sampleKernel) / numThreadsX;
            int dispatchCountY = (gRSMSpotResolution/sampleKernel) / numThreadsY;

            pd3dImmediateContext->Dispatch( dispatchCountX, dispatchCountY, 1 );

            ZeroMemory( pSRVs, sizeof( pSRVs ) );
            pd3dImmediateContext->CSSetShaderResources( 0, ARRAYSIZE( pSRVs ), pSRVs );
        }

        if ( NumPointLights > 0  )
        {
            ID3D11ShaderResourceView* pSRVs[] = 
			{ 
				m_PointAtlas.m_pDepthSRV, 
				m_PointAtlas.m_pNormalSRV, 
				m_PointAtlas.m_pDiffuseSRV, 
				m_pPointInvViewProjBufferSRV, 
				*LightUtil.GetPointLightBufferCenterAndRadiusSRVParam(LIGHTING_SHADOWS),
				*LightUtil.GetPointLightBufferColorSRVParam(LIGHTING_SHADOWS)
			};
            pd3dImmediateContext->CSSetShaderResources( 0, ARRAYSIZE( pSRVs ), pSRVs );
            pd3dImmediateContext->CSSetShader( m_pGeneratePointVPLsCS, 0, 0 );

            int dispatchCountX = (6 * gRSMPointResolution/sampleKernel) / numThreadsX;
            int dispatchCountY = (NumPointLights * gRSMPointResolution/sampleKernel) / numThreadsY;

            pd3dImmediateContext->Dispatch( dispatchCountX, dispatchCountY, 1 );

			ZeroMemory( pSRVs, sizeof( pSRVs ) );
            pd3dImmediateContext->CSSetShaderResources( 0, ARRAYSIZE( pSRVs ), pSRVs );
        }

        ID3D11UnorderedAccessView* pNullUAVs[] = { NULL, NULL };
        pd3dImmediateContext->CSSetUnorderedAccessViews( 0, ARRAYSIZE( pNullUAVs ), pNullUAVs, NULL );

        // Copy the number of items counter from the UAV into a constant buffer for reading in the forward pass
        pd3dImmediateContext->CopyStructureCount( m_pNumVPLsConstantBuffer, 0, m_pVPLBufferCenterAndRadiusUAV );

        pd3dImmediateContext->CSSetConstantBuffers( 4, 1, &m_pNumVPLsConstantBuffer );
        pd3dImmediateContext->PSSetConstantBuffers( 4, 1, &m_pNumVPLsConstantBuffer );
    }


    int RSMRenderer::ReadbackNumVPLs()
    {
        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
        int NumVPLs = 0;

        pd3dImmediateContext->CopyStructureCount( m_pCPUReadbackConstantBuffer, 0, m_pVPLBufferCenterAndRadiusUAV );

        D3D11_MAPPED_SUBRESOURCE resource;
        if ( pd3dImmediateContext->Map( m_pCPUReadbackConstantBuffer, 0, D3D11_MAP_READ, 0, &resource ) == S_OK )
        {
            NumVPLs = *( (int*)resource.pData );

            pd3dImmediateContext->Unmap( m_pCPUReadbackConstantBuffer, 0 );
        }

        return NumVPLs;
    }

    void RSMRenderer::RenderRSMScene( const GuiState& CurrentGuiState, const Scene& Scene, const CommonUtil& CommonUtil )
    {
        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_LESS), 0x00 ); 
        pd3dImmediateContext->IASetInputLayout( m_pRSMLayout );
        pd3dImmediateContext->VSSetShader( m_pRSMVS, NULL, 0 );
        pd3dImmediateContext->PSSetShader( m_pRSMPS, NULL, 0 );
        pd3dImmediateContext->PSSetSamplers( 0, 1, CommonUtil.GetSamplerStateParam(SAMPLER_STATE_POINT) );

        // Draw the main scene
        Scene.m_pSceneMesh->Render( pd3dImmediateContext, 0, 1 );

        // Draw the grid objects (i.e. the "lots of triangles" system)
        for( int i = 0; i < CurrentGuiState.m_nNumGridObjects; i++ )
        {
            CommonUtil.DrawGrid(i, CurrentGuiState.m_nGridObjectTriangleDensity);
        }

        // Skip the alpha-test geometry
    }

    //--------------------------------------------------------------------------------------
    // Add shaders to the shader cache
    //--------------------------------------------------------------------------------------
    void RSMRenderer::AddShadersToCache( AMD::ShaderCache *pShaderCache )
    {
        // Ensure all shaders (and input layouts) are released
        SAFE_RELEASE( m_pRSMVS );
        SAFE_RELEASE( m_pRSMPS );
        SAFE_RELEASE( m_pRSMLayout );
        SAFE_RELEASE( m_pGenerateSpotVPLsCS );
        SAFE_RELEASE( m_pGeneratePointVPLsCS );

        const D3D11_INPUT_ELEMENT_DESC Layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pRSMVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RSMVS", 
            L"RSM.hlsl", 0, NULL, &m_pRSMLayout, Layout, ARRAYSIZE( Layout ) );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pRSMPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RSMPS", 
            L"RSM.hlsl", 0, NULL, NULL, NULL, 0 );

        AMD::ShaderCache::Macro GenerateVPLMacros[ 1 ];
        wcscpy_s( GenerateVPLMacros[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"SPOT_LIGHTS" );

        GenerateVPLMacros[ 0 ].m_iValue = 1;
        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pGenerateSpotVPLsCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"GenerateVPLsCS",
            L"GenerateVPLs.hlsl", ARRAYSIZE( GenerateVPLMacros ), GenerateVPLMacros, NULL, NULL, 0 );

        GenerateVPLMacros[ 0 ].m_iValue = 0;
        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pGeneratePointVPLsCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"GenerateVPLsCS",
            L"GenerateVPLs.hlsl", ARRAYSIZE( GenerateVPLMacros ), GenerateVPLMacros, NULL, NULL, 0 );
    }

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
