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
// File: CommonUtil.cpp
//
// Helper functions for the TiledLighting11 sample.
//--------------------------------------------------------------------------------------

#include "..\\..\\DXUT\\Core\\DXUT.h"
#include "..\\..\\DXUT\\Core\\DDSTextureLoader.h"
#include "..\\..\\DXUT\\Optional\\SDKmisc.h"

#include "..\\..\\AMD_SDK\\inc\\AMD_SDK.h"

#include "CommonUtil.h"

using namespace DirectX;

struct CommonUtilGridVertex
{
    XMFLOAT3 v3Pos;
    XMFLOAT3 v3Norm;
    XMFLOAT2 v2TexCoord;
    XMFLOAT3 v3Tangent;
};

// Grid data (for throwing a lot of tris at the GPU)
// 30x30 cells, times 2 tris per cell, times 2 for front and back, 
// that's 3600 tris per grid (half front facing and half back facing), 
// times 280 grid objects equals 1,008,000 triangles (half front facing and half back facing)
static const int            g_nNumGridCells1DHigh = 30;
static const int            g_nNumGridVerticesHigh = 2 * (g_nNumGridCells1DHigh + 1) * (g_nNumGridCells1DHigh + 1);
static const int            g_nNumGridIndicesHigh = 2 * 6 * g_nNumGridCells1DHigh * g_nNumGridCells1DHigh;
static CommonUtilGridVertex g_GridVertexDataHigh[TiledLighting11::MAX_NUM_GRID_OBJECTS][g_nNumGridVerticesHigh];
static unsigned short       g_GridIndexDataHigh[g_nNumGridIndicesHigh];

// Grid data (for throwing a lot of tris at the GPU)
// 21x21 cells, times 2 tris per cell, times 2 for front and back, 
// that's 1764 tris per grid (half front facing and half back facing), 
// times 280 grid objects equals 493,920 triangles (half front facing and half back facing)
static const int            g_nNumGridCells1DMed = 21;
static const int            g_nNumGridVerticesMed = 2 * (g_nNumGridCells1DMed + 1) * (g_nNumGridCells1DMed + 1);
static const int            g_nNumGridIndicesMed = 2 * 6 * g_nNumGridCells1DMed * g_nNumGridCells1DMed;
static CommonUtilGridVertex g_GridVertexDataMed[TiledLighting11::MAX_NUM_GRID_OBJECTS][g_nNumGridVerticesMed];
static unsigned short       g_GridIndexDataMed[g_nNumGridIndicesMed];

// Grid data (for throwing a lot of tris at the GPU)
// 11x11 cells, times 2 tris per cell, times 2 for front and back, 
// that's 484 tris per grid (half front facing and half back facing), 
// times 280 grid objects equals 135,520 triangles (half front facing and half back facing)
static const int            g_nNumGridCells1DLow = 11;
static const int            g_nNumGridVerticesLow = 2 * (g_nNumGridCells1DLow + 1) * (g_nNumGridCells1DLow + 1);
static const int            g_nNumGridIndicesLow = 2 * 6 * g_nNumGridCells1DLow * g_nNumGridCells1DLow;
static CommonUtilGridVertex g_GridVertexDataLow[TiledLighting11::MAX_NUM_GRID_OBJECTS][g_nNumGridVerticesLow];
static unsigned short       g_GridIndexDataLow[g_nNumGridIndicesLow];

static const int            g_nNumGridIndices[TiledLighting11::TRIANGLE_DENSITY_NUM_TYPES] = { g_nNumGridIndicesLow, g_nNumGridIndicesMed, g_nNumGridIndicesHigh };

struct CommonUtilSpriteVertex
{
    XMFLOAT3 v3Pos;
    XMFLOAT2 v2TexCoord;
};

// static array for sprite quad vertex data
static CommonUtilSpriteVertex         g_QuadForLegendVertexData[6];

// constants for the legend for the lights-per-tile visualization
static const int g_nLegendNumLines = 17;
static const int g_nLegendTextureWidth = 32;
static const int g_nLegendPaddingLeft = 5;
static const int g_nLegendPaddingBottom = 2*AMD::HUD::iElementDelta;

struct BlendedObjectIndex
{
    int   nIndex;
    float fDistance;
};

static const int          g_NumBlendedObjects = 2*20;
static XMMATRIX           g_BlendedObjectInstanceTransform[ g_NumBlendedObjects ];
static BlendedObjectIndex g_BlendedObjectIndices[ g_NumBlendedObjects ];

// there should only be one CommonUtil object
static int CommonUtilObjectCounter = 0;

static int BlendedObjectsSortFunc( const void* p1, const void* p2 )
{
    const BlendedObjectIndex* obj1 = (const BlendedObjectIndex*)p1;
    const BlendedObjectIndex* obj2 = (const BlendedObjectIndex*)p2;

    return obj1->fDistance > obj2->fDistance ? -1 : 1;
}

static void InitBlendedObjectData()
{
    XMFLOAT4X4 mWorld;
    XMStoreFloat4x4( &mWorld, XMMatrixIdentity() );
    for ( int i = 0; i < g_NumBlendedObjects; i++ )
    {
        int nRowLength = 10;
        int col = i % nRowLength;
        int row = (i % (g_NumBlendedObjects/2)) / nRowLength;
        mWorld._41 = ( col * 200.0f ) - 800.0f;
        mWorld._42 = i < g_NumBlendedObjects / 2 ? 80.0f : 220.0f;
        mWorld._43 = ( row * 200.0f ) - 80.0f;

        g_BlendedObjectInstanceTransform[ i ] = XMLoadFloat4x4( &mWorld );
    }
}

static void UpdateBlendedObjectData(const XMVECTOR vEyePt)
{
    for ( int i = 0; i < g_NumBlendedObjects; i++ )
    {
        XMFLOAT4X4 mWorld;
        XMStoreFloat4x4( &mWorld, g_BlendedObjectInstanceTransform[ i ] );
        XMVECTOR vObj = XMVectorSet( mWorld._41, mWorld._42, mWorld._43, 1.0f );
        XMVECTOR vResult = vEyePt - vObj;

        g_BlendedObjectIndices[ i ].nIndex = i;
        g_BlendedObjectIndices[ i ].fDistance = XMVectorGetX(XMVector3LengthSq( vResult ));
    }

    qsort( g_BlendedObjectIndices, g_NumBlendedObjects, sizeof( g_BlendedObjectIndices[ 0 ] ), &BlendedObjectsSortFunc );
}

template <size_t nNumGridVertices, size_t nNumGridIndices>
static void InitGridObjectData(int nNumGridCells1D, CommonUtilGridVertex GridVertexData[TiledLighting11::MAX_NUM_GRID_OBJECTS][nNumGridVertices], unsigned short GridIndexData[nNumGridIndices])
{
    const float fGridSizeWorldSpace = 100.0f;
    const float fGridSizeWorldSpaceHalf = 0.5f * fGridSizeWorldSpace;

    const float fPosStep = fGridSizeWorldSpace / (float)(nNumGridCells1D);
    const float fTexStep = 1.0f / (float)(nNumGridCells1D);

    const float fPosX = 725.0f;
    const float fPosYStart = 1000.0f;
    const float fStepY = 1.05f * fGridSizeWorldSpace;
    const float fPosZStart = 1467.0f;
    const float fStepZ = 1.05f * fGridSizeWorldSpace;

    for( int nGrid = 0; nGrid < TiledLighting11::MAX_NUM_GRID_OBJECTS; nGrid++ )
    {
        const float fCurrentPosYOffset = fPosYStart - (float)((nGrid/28)%10)*fStepY;
        const float fCurrentPosZOffset = fPosZStart - (float)(nGrid%28)*fStepZ;

        // front side verts
        for( int i = 0; i < nNumGridCells1D+1; i++ )
        {
            const float fPosY = fCurrentPosYOffset + fGridSizeWorldSpaceHalf - (float)i*fPosStep;
            const float fV = (float)i*fTexStep;
            for( int j = 0; j < nNumGridCells1D+1; j++ )
            {
                const float fPosZ = fCurrentPosZOffset - fGridSizeWorldSpaceHalf + (float)j*fPosStep;
                const float fU = (float)j*fTexStep;
                const int idx = (nNumGridCells1D+1) * i + j;
                GridVertexData[nGrid][idx].v3Pos = XMFLOAT3(fPosX, fPosY, fPosZ);
                GridVertexData[nGrid][idx].v3Norm = XMFLOAT3(1,0,0);
                GridVertexData[nGrid][idx].v2TexCoord = XMFLOAT2(fU,fV);
                GridVertexData[nGrid][idx].v3Tangent = XMFLOAT3(0,0,-1);
            }
        }

        // back side verts
        for( int i = 0; i < nNumGridCells1D+1; i++ )
        {
            const float fPosY = fCurrentPosYOffset + fGridSizeWorldSpaceHalf - (float)i*fPosStep;
            const float fV = (float)i*fTexStep;
            for( int j = 0; j < nNumGridCells1D+1; j++ )
            {
                const float fPosZ = fCurrentPosZOffset + fGridSizeWorldSpaceHalf - (float)j*fPosStep;
                const float fU = (float)j*fTexStep;
                const int idx = (nNumGridCells1D+1) * (nNumGridCells1D+1) + (nNumGridCells1D+1) * i + j;
                GridVertexData[nGrid][idx].v3Pos = XMFLOAT3(fPosX, fPosY, fPosZ);
                GridVertexData[nGrid][idx].v3Norm = XMFLOAT3(-1,0,0);
                GridVertexData[nGrid][idx].v2TexCoord = XMFLOAT2(fU,fV);
                GridVertexData[nGrid][idx].v3Tangent = XMFLOAT3(0,0,1);
            }
        }
    }

    // front side tris
    for( int i = 0; i < nNumGridCells1D; i++ )
    {
        for( int j = 0; j < nNumGridCells1D; j++ )
        {
            const int vertexStartIndexThisRow = (nNumGridCells1D+1) * i + j;
            const int vertexStartIndexNextRow = (nNumGridCells1D+1) * (i+1) + j;
            const int idx = (6 * nNumGridCells1D * i) + (6*j);
            GridIndexData[idx+0] = (unsigned short)(vertexStartIndexThisRow);
            GridIndexData[idx+1] = (unsigned short)(vertexStartIndexThisRow+1);
            GridIndexData[idx+2] = (unsigned short)(vertexStartIndexNextRow);
            GridIndexData[idx+3] = (unsigned short)(vertexStartIndexThisRow+1);
            GridIndexData[idx+4] = (unsigned short)(vertexStartIndexNextRow+1);
            GridIndexData[idx+5] = (unsigned short)(vertexStartIndexNextRow);
        }
    }

    // back side tris
    for( int i = 0; i < nNumGridCells1D; i++ )
    {
        for( int j = 0; j < nNumGridCells1D; j++ )
        {
            const int vertexStartIndexThisRow = (nNumGridCells1D+1) * (nNumGridCells1D+1) + (nNumGridCells1D+1) * i + j;
            const int vertexStartIndexNextRow = (nNumGridCells1D+1) * (nNumGridCells1D+1) + (nNumGridCells1D+1) * (i+1) + j;
            const int idx = (6 * nNumGridCells1D * nNumGridCells1D) + (6 * nNumGridCells1D * i) + (6*j);
            GridIndexData[idx+0] = (unsigned short)(vertexStartIndexThisRow);
            GridIndexData[idx+1] = (unsigned short)(vertexStartIndexThisRow+1);
            GridIndexData[idx+2] = (unsigned short)(vertexStartIndexNextRow);
            GridIndexData[idx+3] = (unsigned short)(vertexStartIndexThisRow+1);
            GridIndexData[idx+4] = (unsigned short)(vertexStartIndexNextRow+1);
            GridIndexData[idx+5] = (unsigned short)(vertexStartIndexNextRow);
        }
    }
}

namespace TiledLighting11
{

    //--------------------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------------------
    CommonUtil::CommonUtil()
        :m_uWidth(0)
        ,m_uHeight(0)
        ,m_pLightIndexBuffer(NULL)
        ,m_pLightIndexBufferSRV(NULL)
        ,m_pLightIndexBufferUAV(NULL)
        ,m_pLightIndexBufferForBlendedObjects(NULL)
        ,m_pLightIndexBufferForBlendedObjectsSRV(NULL)
        ,m_pLightIndexBufferForBlendedObjectsUAV(NULL)
        ,m_pSpotIndexBuffer(NULL)
        ,m_pSpotIndexBufferSRV(NULL)
        ,m_pSpotIndexBufferUAV(NULL)
        ,m_pSpotIndexBufferForBlendedObjects(NULL)
        ,m_pSpotIndexBufferForBlendedObjectsSRV(NULL)
        ,m_pSpotIndexBufferForBlendedObjectsUAV(NULL)
        ,m_pVPLIndexBuffer(NULL)
        ,m_pVPLIndexBufferSRV(NULL)
        ,m_pVPLIndexBufferUAV(NULL)
        ,m_pBlendedVB(NULL)
        ,m_pBlendedIB(NULL)
        ,m_pBlendedTransform(NULL)
        ,m_pBlendedTransformSRV(NULL)
        ,m_pGridDiffuseTextureSRV(NULL)
        ,m_pGridNormalMapSRV(NULL)
        ,m_pQuadForLegendVB(NULL)
        ,m_pSceneBlendedVS(NULL)
        ,m_pSceneBlendedDepthVS(NULL)
        ,m_pSceneBlendedPS(NULL)
        ,m_pSceneBlendedPSShadows(NULL)
        ,m_pSceneBlendedLayout(NULL)
        ,m_pSceneBlendedDepthLayout(NULL)
        ,m_pDebugDrawNumLightsPerTileRadarColorsPS(NULL)
        ,m_pDebugDrawNumLightsPerTileGrayscalePS(NULL)
        ,m_pDebugDrawNumLightsAndVPLsPerTileRadarColorsPS(NULL)
        ,m_pDebugDrawNumLightsAndVPLsPerTileGrayscalePS(NULL)
        ,m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledRadarColorsPS(NULL)
        ,m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledGrayscalePS(NULL)
        ,m_pDebugDrawLegendForNumLightsPerTileVS(NULL)
        ,m_pDebugDrawLegendForNumLightsPerTileRadarColorsPS(NULL)
        ,m_pDebugDrawLegendForNumLightsPerTileGrayscalePS(NULL)
        ,m_pDebugDrawLegendForNumLightsLayout11(NULL)
        ,m_pFullScreenVS(NULL)
    {
        assert( CommonUtilObjectCounter == 0 );
        CommonUtilObjectCounter++;

        for( int i = 0; i < TRIANGLE_DENSITY_NUM_TYPES; i++ )
        {
            for( int j = 0; j < MAX_NUM_GRID_OBJECTS; j++ )
            {
                m_pGridVB[i][j] = NULL;
            }

            m_pGridIB[i] = NULL;
        }

        for( int i = 0; i < NUM_LIGHT_CULLING_COMPUTE_SHADERS_FOR_BLENDED_OBJECTS; i++ )
        {
            m_pLightCullCSForBlendedObjects[i] = NULL;
        }

        for( int i = 0; i < NUM_FULL_SCREEN_PIXEL_SHADERS; i++ )
        {
            m_pFullScreenPS[i] = NULL;
        }

        for( int i = 0; i < DEPTH_STENCIL_STATE_NUM_TYPES; i++ )
        {
            m_pDepthStencilState[i] = NULL;
        }

        for( int i = 0; i < RASTERIZER_STATE_NUM_TYPES; i++ )
        {
            m_pRasterizerState[i] = NULL;
        }

        for( int i = 0; i < SAMPLER_STATE_NUM_TYPES; i++ )
        {
            m_pSamplerState[i] = NULL;
        }
    }


    //--------------------------------------------------------------------------------------
    // Destructor
    //--------------------------------------------------------------------------------------
    CommonUtil::~CommonUtil()
    {
        assert( CommonUtilObjectCounter == 1 );
        CommonUtilObjectCounter--;

        SAFE_RELEASE(m_pLightIndexBuffer);
        SAFE_RELEASE(m_pLightIndexBufferSRV);
        SAFE_RELEASE(m_pLightIndexBufferUAV);
        SAFE_RELEASE(m_pLightIndexBufferForBlendedObjects);
        SAFE_RELEASE(m_pLightIndexBufferForBlendedObjectsSRV);
        SAFE_RELEASE(m_pLightIndexBufferForBlendedObjectsUAV);
        SAFE_RELEASE(m_pSpotIndexBuffer);
        SAFE_RELEASE(m_pSpotIndexBufferSRV);
        SAFE_RELEASE(m_pSpotIndexBufferUAV);
        SAFE_RELEASE(m_pSpotIndexBufferForBlendedObjects);
        SAFE_RELEASE(m_pSpotIndexBufferForBlendedObjectsSRV);
        SAFE_RELEASE(m_pSpotIndexBufferForBlendedObjectsUAV);
        SAFE_RELEASE(m_pVPLIndexBuffer);
        SAFE_RELEASE(m_pVPLIndexBufferSRV);
        SAFE_RELEASE(m_pVPLIndexBufferUAV);
        SAFE_RELEASE(m_pBlendedVB);
        SAFE_RELEASE(m_pBlendedIB);
        SAFE_RELEASE(m_pBlendedTransform);
        SAFE_RELEASE(m_pBlendedTransformSRV);
        SAFE_RELEASE(m_pGridDiffuseTextureSRV);
        SAFE_RELEASE(m_pGridNormalMapSRV);
        SAFE_RELEASE(m_pQuadForLegendVB);
        SAFE_RELEASE(m_pSceneBlendedVS);
        SAFE_RELEASE(m_pSceneBlendedDepthVS);
        SAFE_RELEASE(m_pSceneBlendedPS);
        SAFE_RELEASE(m_pSceneBlendedPSShadows);
        SAFE_RELEASE(m_pSceneBlendedLayout);
        SAFE_RELEASE(m_pSceneBlendedDepthLayout);
        SAFE_RELEASE(m_pDebugDrawNumLightsPerTileRadarColorsPS);
        SAFE_RELEASE(m_pDebugDrawNumLightsPerTileGrayscalePS);
        SAFE_RELEASE(m_pDebugDrawNumLightsAndVPLsPerTileRadarColorsPS);
        SAFE_RELEASE(m_pDebugDrawNumLightsAndVPLsPerTileGrayscalePS);
        SAFE_RELEASE(m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledRadarColorsPS);
        SAFE_RELEASE(m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledGrayscalePS);
        SAFE_RELEASE(m_pDebugDrawLegendForNumLightsPerTileVS);
        SAFE_RELEASE(m_pDebugDrawLegendForNumLightsPerTileRadarColorsPS);
        SAFE_RELEASE(m_pDebugDrawLegendForNumLightsPerTileGrayscalePS);
        SAFE_RELEASE(m_pDebugDrawLegendForNumLightsLayout11);
        SAFE_RELEASE(m_pFullScreenVS);

        for( int i = 0; i < TRIANGLE_DENSITY_NUM_TYPES; i++ )
        {
            for( int j = 0; j < MAX_NUM_GRID_OBJECTS; j++ )
            {
                SAFE_RELEASE(m_pGridVB[i][j]);
            }

            SAFE_RELEASE(m_pGridIB[i]);
        }

        for( int i = 0; i < NUM_LIGHT_CULLING_COMPUTE_SHADERS_FOR_BLENDED_OBJECTS; i++ )
        {
            SAFE_RELEASE(m_pLightCullCSForBlendedObjects[i]);
        }

        for( int i = 0; i < NUM_FULL_SCREEN_PIXEL_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pFullScreenPS[i]);
        }

        for( int i = 0; i < DEPTH_STENCIL_STATE_NUM_TYPES; i++ )
        {
            SAFE_RELEASE(m_pDepthStencilState[i]);
        }

        for( int i = 0; i < RASTERIZER_STATE_NUM_TYPES; i++ )
        {
            SAFE_RELEASE(m_pRasterizerState[i]);
        }

        for( int i = 0; i < SAMPLER_STATE_NUM_TYPES; i++ )
        {
            SAFE_RELEASE(m_pSamplerState[i]);
        }
    }

    //--------------------------------------------------------------------------------------
    // Device creation hook function
    //--------------------------------------------------------------------------------------
    HRESULT CommonUtil::OnCreateDevice( ID3D11Device* pd3dDevice )
    {
        HRESULT hr;

        D3D11_SUBRESOURCE_DATA InitData;

        // Create the alpha blended cube geometry
        AMD::CreateCube( 40.0f, &m_pBlendedVB, &m_pBlendedIB );

        // Create the buffer for the instance data
        D3D11_BUFFER_DESC BlendedObjectTransformBufferDesc;
        ZeroMemory( &BlendedObjectTransformBufferDesc, sizeof(BlendedObjectTransformBufferDesc) );
        BlendedObjectTransformBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        BlendedObjectTransformBufferDesc.ByteWidth = sizeof( g_BlendedObjectInstanceTransform );
        BlendedObjectTransformBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        BlendedObjectTransformBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        BlendedObjectTransformBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        BlendedObjectTransformBufferDesc.StructureByteStride = sizeof( XMMATRIX );
        V_RETURN( pd3dDevice->CreateBuffer( &BlendedObjectTransformBufferDesc, 0, &m_pBlendedTransform ) );
        DXUT_SetDebugName( m_pBlendedTransform, "BlendedTransform" );

        D3D11_SHADER_RESOURCE_VIEW_DESC BlendedObjectTransformBufferSRVDesc;
        ZeroMemory( &BlendedObjectTransformBufferSRVDesc, sizeof( D3D11_SHADER_RESOURCE_VIEW_DESC ) );
        BlendedObjectTransformBufferSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        BlendedObjectTransformBufferSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        BlendedObjectTransformBufferSRVDesc.Buffer.ElementOffset = 0;
        BlendedObjectTransformBufferSRVDesc.Buffer.ElementWidth = g_NumBlendedObjects;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pBlendedTransform, &BlendedObjectTransformBufferSRVDesc, &m_pBlendedTransformSRV ) );

        // Create the vertex buffer for the grid objects
        D3D11_BUFFER_DESC VBDesc;
        ZeroMemory( &VBDesc, sizeof(VBDesc) );
        VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
        VBDesc.ByteWidth = sizeof( g_GridVertexDataHigh[0] );
        VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        for( int i = 0; i < MAX_NUM_GRID_OBJECTS; i++ )
        {
            InitData.pSysMem = g_GridVertexDataHigh[i];
            V_RETURN( pd3dDevice->CreateBuffer( &VBDesc, &InitData, &m_pGridVB[TRIANGLE_DENSITY_HIGH][i] ) );
        }

        VBDesc.ByteWidth = sizeof( g_GridVertexDataMed[0] );
        for( int i = 0; i < MAX_NUM_GRID_OBJECTS; i++ )
        {
            InitData.pSysMem = g_GridVertexDataMed[i];
            V_RETURN( pd3dDevice->CreateBuffer( &VBDesc, &InitData, &m_pGridVB[TRIANGLE_DENSITY_MEDIUM][i] ) );
        }

        VBDesc.ByteWidth = sizeof( g_GridVertexDataLow[0] );
        for( int i = 0; i < MAX_NUM_GRID_OBJECTS; i++ )
        {
            InitData.pSysMem = g_GridVertexDataLow[i];
            V_RETURN( pd3dDevice->CreateBuffer( &VBDesc, &InitData, &m_pGridVB[TRIANGLE_DENSITY_LOW][i] ) );
        }

        // Create the index buffer for the grid objects
        D3D11_BUFFER_DESC IBDesc;
        ZeroMemory( &IBDesc, sizeof(IBDesc) );
        IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
        IBDesc.ByteWidth = sizeof( g_GridIndexDataHigh );
        IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        InitData.pSysMem = g_GridIndexDataHigh;
        V_RETURN( pd3dDevice->CreateBuffer( &IBDesc, &InitData, &m_pGridIB[TRIANGLE_DENSITY_HIGH] ) );

        IBDesc.ByteWidth = sizeof( g_GridIndexDataMed );
        InitData.pSysMem = g_GridIndexDataMed;
        V_RETURN( pd3dDevice->CreateBuffer( &IBDesc, &InitData, &m_pGridIB[TRIANGLE_DENSITY_MEDIUM] ) );

        IBDesc.ByteWidth = sizeof( g_GridIndexDataLow );
        InitData.pSysMem = g_GridIndexDataLow;
        V_RETURN( pd3dDevice->CreateBuffer( &IBDesc, &InitData, &m_pGridIB[TRIANGLE_DENSITY_LOW] ) );

        // Load the diffuse and normal map for the grid
        {
            WCHAR path[MAX_PATH];
            DXUTFindDXSDKMediaFileCch( path, MAX_PATH, L"misc\\default_diff.dds" );

            // Create the shader resource view.
            CreateDDSTextureFromFile( pd3dDevice, path, NULL, &m_pGridDiffuseTextureSRV );

            DXUTFindDXSDKMediaFileCch( path, MAX_PATH, L"misc\\default_norm.dds" );

            // Create the shader resource view.
            CreateDDSTextureFromFile( pd3dDevice, path, NULL, &m_pGridNormalMapSRV );
        }

        // Default depth-stencil state, except with inverted DepthFunc 
        // (because we are using inverted 32-bit float depth for better precision)
        D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
        ZeroMemory( &DepthStencilDesc, sizeof( D3D11_DEPTH_STENCIL_DESC ) );
        DepthStencilDesc.DepthEnable = TRUE; 
        DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; 
        DepthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER;  // we are using inverted 32-bit float depth for better precision
        DepthStencilDesc.StencilEnable = FALSE; 
        DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK; 
        DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK; 
        V_RETURN( pd3dDevice->CreateDepthStencilState( &DepthStencilDesc, &m_pDepthStencilState[DEPTH_STENCIL_STATE_DEPTH_GREATER] ) );

        // Disable depth test write
        DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; 
        V_RETURN( pd3dDevice->CreateDepthStencilState( &DepthStencilDesc, &m_pDepthStencilState[DEPTH_STENCIL_STATE_DISABLE_DEPTH_WRITE] ) );

        // Disable depth test
        DepthStencilDesc.DepthEnable = FALSE; 
        V_RETURN( pd3dDevice->CreateDepthStencilState( &DepthStencilDesc, &m_pDepthStencilState[DEPTH_STENCIL_STATE_DISABLE_DEPTH_TEST] ) );

        // Comparison greater with depth writes disabled
        DepthStencilDesc.DepthEnable = TRUE; 
        DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; 
        DepthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER;  // we are using inverted 32-bit float depth for better precision
        V_RETURN( pd3dDevice->CreateDepthStencilState( &DepthStencilDesc, &m_pDepthStencilState[DEPTH_STENCIL_STATE_DEPTH_GREATER_AND_DISABLE_DEPTH_WRITE] ) );

        // Comparison equal with depth writes disabled
        DepthStencilDesc.DepthFunc = D3D11_COMPARISON_EQUAL; 
        V_RETURN( pd3dDevice->CreateDepthStencilState( &DepthStencilDesc, &m_pDepthStencilState[DEPTH_STENCIL_STATE_DEPTH_EQUAL_AND_DISABLE_DEPTH_WRITE] ) );

        // Comparison less, for shadow maps
        DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; 
        DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS; 
        V_RETURN( pd3dDevice->CreateDepthStencilState( &DepthStencilDesc, &m_pDepthStencilState[DEPTH_STENCIL_STATE_DEPTH_LESS] ) );

        // Disable culling
        D3D11_RASTERIZER_DESC RasterizerDesc;
        RasterizerDesc.FillMode = D3D11_FILL_SOLID;
        RasterizerDesc.CullMode = D3D11_CULL_NONE;       // disable culling
        RasterizerDesc.FrontCounterClockwise = FALSE;
        RasterizerDesc.DepthBias = 0;
        RasterizerDesc.DepthBiasClamp = 0.0f;
        RasterizerDesc.SlopeScaledDepthBias = 0.0f;
        RasterizerDesc.DepthClipEnable = TRUE;
        RasterizerDesc.ScissorEnable = FALSE;
        RasterizerDesc.MultisampleEnable = FALSE;
        RasterizerDesc.AntialiasedLineEnable = FALSE;
        V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDesc, &m_pRasterizerState[RASTERIZER_STATE_DISABLE_CULLING] ) );

        RasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;  // wireframe
        RasterizerDesc.CullMode = D3D11_CULL_BACK;
        V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDesc, &m_pRasterizerState[RASTERIZER_STATE_WIREFRAME] ) );

        RasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;  // wireframe and ...
        RasterizerDesc.CullMode = D3D11_CULL_NONE;       // disable culling
        V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDesc, &m_pRasterizerState[RASTERIZER_STATE_WIREFRAME_DISABLE_CULLING] ) );

        // Create state objects
        D3D11_SAMPLER_DESC SamplerDesc;
        ZeroMemory( &SamplerDesc, sizeof(SamplerDesc) );
        SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        SamplerDesc.MaxAnisotropy = 16;
        SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        SamplerDesc.MinLOD = -D3D11_FLOAT32_MAX;
        SamplerDesc.MaxLOD =  D3D11_FLOAT32_MAX;
        V_RETURN( pd3dDevice->CreateSamplerState( &SamplerDesc, &m_pSamplerState[SAMPLER_STATE_POINT] ) );
        SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        V_RETURN( pd3dDevice->CreateSamplerState( &SamplerDesc, &m_pSamplerState[SAMPLER_STATE_LINEAR] ) );
        SamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
        V_RETURN( pd3dDevice->CreateSamplerState( &SamplerDesc, &m_pSamplerState[SAMPLER_STATE_ANISO] ) );

        // One more for shadows
        SamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        SamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        SamplerDesc.MaxAnisotropy = 1;
        V_RETURN( pd3dDevice->CreateSamplerState( &SamplerDesc, &m_pSamplerState[SAMPLER_STATE_SHADOW] ) );

        return hr;
    }


    //--------------------------------------------------------------------------------------
    // Device destruction hook function
    //--------------------------------------------------------------------------------------
    void CommonUtil::OnDestroyDevice()
    {
        SAFE_RELEASE(m_pBlendedVB);
        SAFE_RELEASE(m_pBlendedIB);
        SAFE_RELEASE(m_pBlendedTransform);
        SAFE_RELEASE(m_pBlendedTransformSRV);

        for( int i = 0; i < TRIANGLE_DENSITY_NUM_TYPES; i++ )
        {
            for( int j = 0; j < MAX_NUM_GRID_OBJECTS; j++ )
            {
                SAFE_RELEASE(m_pGridVB[i][j]);
            }

            SAFE_RELEASE(m_pGridIB[i]);
        }

        SAFE_RELEASE(m_pGridDiffuseTextureSRV);
        SAFE_RELEASE(m_pGridNormalMapSRV);

        SAFE_RELEASE(m_pSceneBlendedVS);
        SAFE_RELEASE(m_pSceneBlendedDepthVS);
        SAFE_RELEASE(m_pSceneBlendedPS);
        SAFE_RELEASE(m_pSceneBlendedPSShadows);
        SAFE_RELEASE(m_pSceneBlendedLayout);
        SAFE_RELEASE(m_pSceneBlendedDepthLayout);

        for( int i = 0; i < NUM_LIGHT_CULLING_COMPUTE_SHADERS_FOR_BLENDED_OBJECTS; i++ )
        {
            SAFE_RELEASE(m_pLightCullCSForBlendedObjects[i]);
        }

        SAFE_RELEASE( m_pDebugDrawNumLightsPerTileRadarColorsPS );
        SAFE_RELEASE( m_pDebugDrawNumLightsPerTileGrayscalePS );
        SAFE_RELEASE( m_pDebugDrawNumLightsAndVPLsPerTileRadarColorsPS );
        SAFE_RELEASE( m_pDebugDrawNumLightsAndVPLsPerTileGrayscalePS );
        SAFE_RELEASE( m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledRadarColorsPS );
        SAFE_RELEASE( m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledGrayscalePS );

        SAFE_RELEASE( m_pDebugDrawLegendForNumLightsPerTileVS );
        SAFE_RELEASE( m_pDebugDrawLegendForNumLightsPerTileRadarColorsPS );
        SAFE_RELEASE( m_pDebugDrawLegendForNumLightsPerTileGrayscalePS );
        SAFE_RELEASE( m_pDebugDrawLegendForNumLightsLayout11 );

        SAFE_RELEASE(m_pFullScreenVS);

        for( int i = 0; i < NUM_FULL_SCREEN_PIXEL_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pFullScreenPS[i]);
        }

        for( int i = 0; i < DEPTH_STENCIL_STATE_NUM_TYPES; i++ )
        {
            SAFE_RELEASE(m_pDepthStencilState[i]);
        }

        for( int i = 0; i < RASTERIZER_STATE_NUM_TYPES; i++ )
        {
            SAFE_RELEASE(m_pRasterizerState[i]);
        }

        for( int i = 0; i < SAMPLER_STATE_NUM_TYPES; i++ )
        {
            SAFE_RELEASE(m_pSamplerState[i]);
        }
    }


    //--------------------------------------------------------------------------------------
    // Resized swap chain hook function
    //--------------------------------------------------------------------------------------
    HRESULT CommonUtil::OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, int nLineHeight )
    {
        HRESULT hr;

        m_uWidth = pBackBufferSurfaceDesc->Width;
        m_uHeight = pBackBufferSurfaceDesc->Height;

        // depends on m_uWidth and m_uHeight, so don't do this 
        // until you have updated them (see above)
        unsigned uNumTiles = GetNumTilesX()*GetNumTilesY();
        unsigned uMaxNumElementsPerTile = GetMaxNumElementsPerTile();
        unsigned uMaxNumVPLElementsPerTile = GetMaxNumVPLElementsPerTile();

        D3D11_BUFFER_DESC BufferDesc;
        ZeroMemory( &BufferDesc, sizeof(BufferDesc) );
        BufferDesc.Usage = D3D11_USAGE_DEFAULT;
        BufferDesc.ByteWidth = 2 * uMaxNumElementsPerTile * uNumTiles;
        BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        V_RETURN( pd3dDevice->CreateBuffer( &BufferDesc, NULL, &m_pLightIndexBuffer ) );
        DXUT_SetDebugName( m_pLightIndexBuffer, "LightIndexBuffer" );

        V_RETURN( pd3dDevice->CreateBuffer( &BufferDesc, NULL, &m_pLightIndexBufferForBlendedObjects ) );
        DXUT_SetDebugName( m_pLightIndexBufferForBlendedObjects, "LightIndexBufferForBlendedObjects" );

        V_RETURN( pd3dDevice->CreateBuffer( &BufferDesc, NULL, &m_pSpotIndexBuffer ) );
        DXUT_SetDebugName( m_pSpotIndexBuffer, "SpotIndexBuffer" );

        V_RETURN( pd3dDevice->CreateBuffer( &BufferDesc, NULL, &m_pSpotIndexBufferForBlendedObjects ) );
        DXUT_SetDebugName( m_pSpotIndexBufferForBlendedObjects, "SpotIndexBufferForBlendedObjects" );

        BufferDesc.ByteWidth = 2 * uMaxNumVPLElementsPerTile * uNumTiles;
        V_RETURN( pd3dDevice->CreateBuffer( &BufferDesc, NULL, &m_pVPLIndexBuffer ) );
        DXUT_SetDebugName( m_pVPLIndexBuffer, "VPLIndexBuffer" );

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        ZeroMemory( &SRVDesc, sizeof( D3D11_SHADER_RESOURCE_VIEW_DESC ) );
        SRVDesc.Format = DXGI_FORMAT_R16_UINT;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.ElementOffset = 0;
        SRVDesc.Buffer.ElementWidth = uMaxNumElementsPerTile * uNumTiles;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pLightIndexBuffer, &SRVDesc, &m_pLightIndexBufferSRV ) );
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pLightIndexBufferForBlendedObjects, &SRVDesc, &m_pLightIndexBufferForBlendedObjectsSRV ) );

        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pSpotIndexBuffer, &SRVDesc, &m_pSpotIndexBufferSRV ) );
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pSpotIndexBufferForBlendedObjects, &SRVDesc, &m_pSpotIndexBufferForBlendedObjectsSRV ) );

        SRVDesc.Buffer.ElementWidth = uMaxNumVPLElementsPerTile * uNumTiles;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pVPLIndexBuffer, &SRVDesc, &m_pVPLIndexBufferSRV ) );

        D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
        ZeroMemory( &UAVDesc, sizeof( D3D11_UNORDERED_ACCESS_VIEW_DESC ) );
        UAVDesc.Format = DXGI_FORMAT_R16_UINT;
        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = uMaxNumElementsPerTile * uNumTiles;
        V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pLightIndexBuffer, &UAVDesc, &m_pLightIndexBufferUAV ) );
        V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pLightIndexBufferForBlendedObjects, &UAVDesc, &m_pLightIndexBufferForBlendedObjectsUAV ) );

        V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pSpotIndexBuffer, &UAVDesc, &m_pSpotIndexBufferUAV ) );
        V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pSpotIndexBufferForBlendedObjects, &UAVDesc, &m_pSpotIndexBufferForBlendedObjectsUAV ) );

        UAVDesc.Buffer.NumElements = uMaxNumVPLElementsPerTile * uNumTiles;
        V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pVPLIndexBuffer, &UAVDesc, &m_pVPLIndexBufferUAV ) );

        // initialize the vertex buffer data for a quad (for drawing the lights-per-tile legend)
        const float kTextureHeight = (float)g_nLegendNumLines * (float)nLineHeight;
        const float kTextureWidth = (float)g_nLegendTextureWidth;
        const float kPaddingLeft = (float)g_nLegendPaddingLeft;
        const float kPaddingBottom = (float)g_nLegendPaddingBottom;
        float fLeft = kPaddingLeft;
        float fRight = kPaddingLeft + kTextureWidth;
        float fTop = (float)m_uHeight - kPaddingBottom - kTextureHeight;
        float fBottom =(float)m_uHeight - kPaddingBottom;
        g_QuadForLegendVertexData[0].v3Pos = XMFLOAT3( fLeft,  fBottom, 0.0f );
        g_QuadForLegendVertexData[0].v2TexCoord = XMFLOAT2( 0.0f, 0.0f );
        g_QuadForLegendVertexData[1].v3Pos = XMFLOAT3( fLeft,  fTop, 0.0f );
        g_QuadForLegendVertexData[1].v2TexCoord = XMFLOAT2( 0.0f, 1.0f );
        g_QuadForLegendVertexData[2].v3Pos = XMFLOAT3( fRight, fBottom, 0.0f );
        g_QuadForLegendVertexData[2].v2TexCoord = XMFLOAT2( 1.0f, 0.0f );
        g_QuadForLegendVertexData[3].v3Pos = XMFLOAT3( fLeft,  fTop, 0.0f );
        g_QuadForLegendVertexData[3].v2TexCoord = XMFLOAT2( 0.0f, 1.0f );
        g_QuadForLegendVertexData[4].v3Pos = XMFLOAT3( fRight,  fTop, 0.0f );
        g_QuadForLegendVertexData[4].v2TexCoord = XMFLOAT2( 1.0f, 1.0f );
        g_QuadForLegendVertexData[5].v3Pos = XMFLOAT3( fRight, fBottom, 0.0f );
        g_QuadForLegendVertexData[5].v2TexCoord = XMFLOAT2( 1.0f, 0.0f );

        // Create the vertex buffer for the sprite (a single quad)
        D3D11_SUBRESOURCE_DATA InitData;
        D3D11_BUFFER_DESC VBDesc;
        ZeroMemory( &VBDesc, sizeof(VBDesc) );
        VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
        VBDesc.ByteWidth = sizeof( g_QuadForLegendVertexData );
        VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        InitData.pSysMem = g_QuadForLegendVertexData;
        V_RETURN( pd3dDevice->CreateBuffer( &VBDesc, &InitData, &m_pQuadForLegendVB ) );
        DXUT_SetDebugName( m_pQuadForLegendVB, "QuadForLegendVB" );

        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Releasing swap chain hook function
    //--------------------------------------------------------------------------------------
    void CommonUtil::OnReleasingSwapChain()
    {
        SAFE_RELEASE(m_pLightIndexBuffer);
        SAFE_RELEASE(m_pLightIndexBufferSRV);
        SAFE_RELEASE(m_pLightIndexBufferUAV);
        SAFE_RELEASE(m_pLightIndexBufferForBlendedObjects);
        SAFE_RELEASE(m_pLightIndexBufferForBlendedObjectsSRV);
        SAFE_RELEASE(m_pLightIndexBufferForBlendedObjectsUAV);
        SAFE_RELEASE(m_pSpotIndexBuffer);
        SAFE_RELEASE(m_pSpotIndexBufferSRV);
        SAFE_RELEASE(m_pSpotIndexBufferUAV);
        SAFE_RELEASE(m_pSpotIndexBufferForBlendedObjects);
        SAFE_RELEASE(m_pSpotIndexBufferForBlendedObjectsSRV);
        SAFE_RELEASE(m_pSpotIndexBufferForBlendedObjectsUAV);
        SAFE_RELEASE(m_pVPLIndexBuffer);
        SAFE_RELEASE(m_pVPLIndexBufferSRV);
        SAFE_RELEASE(m_pVPLIndexBufferUAV);
        SAFE_RELEASE(m_pQuadForLegendVB);
    }

    void CommonUtil::InitStaticData()
    {
        // make sure our indices will actually fit in a R16_UINT
        assert( g_nNumGridVerticesHigh <= 65536 );
        assert( g_nNumGridVerticesMed  <= 65536 );
        assert( g_nNumGridVerticesLow  <= 65536 );

        InitGridObjectData<g_nNumGridVerticesHigh,g_nNumGridIndicesHigh>( g_nNumGridCells1DHigh, g_GridVertexDataHigh, g_GridIndexDataHigh );
        InitGridObjectData<g_nNumGridVerticesMed, g_nNumGridIndicesMed >( g_nNumGridCells1DMed,  g_GridVertexDataMed,  g_GridIndexDataMed  );
        InitGridObjectData<g_nNumGridVerticesLow, g_nNumGridIndicesLow >( g_nNumGridCells1DLow,  g_GridVertexDataLow,  g_GridIndexDataLow  );

        InitBlendedObjectData();
    }

    //--------------------------------------------------------------------------------------
    // Calculate AABB around all meshes in the scene
    //--------------------------------------------------------------------------------------
    void CommonUtil::CalculateSceneMinMax( CDXUTSDKMesh &Mesh, XMVECTOR *pBBoxMinOut, XMVECTOR *pBBoxMaxOut )
    {
        *pBBoxMaxOut = Mesh.GetMeshBBoxCenter( 0 ) + Mesh.GetMeshBBoxExtents( 0 );
        *pBBoxMinOut = Mesh.GetMeshBBoxCenter( 0 ) - Mesh.GetMeshBBoxExtents( 0 );

        for( unsigned i = 1; i < Mesh.GetNumMeshes(); i++ )
        {
            XMVECTOR vNewMax = Mesh.GetMeshBBoxCenter( i ) + Mesh.GetMeshBBoxExtents( i );
            XMVECTOR vNewMin = Mesh.GetMeshBBoxCenter( i ) - Mesh.GetMeshBBoxExtents( i );

            *pBBoxMaxOut = XMVectorMax(*pBBoxMaxOut, vNewMax);
            *pBBoxMinOut = XMVectorMin(*pBBoxMinOut, vNewMin);
        }

    }

    //--------------------------------------------------------------------------------------
    // Add shaders to the shader cache
    //--------------------------------------------------------------------------------------
    void CommonUtil::AddShadersToCache( AMD::ShaderCache *pShaderCache )
    {
        // Ensure all shaders (and input layouts) are released
        SAFE_RELEASE( m_pSceneBlendedVS );
        SAFE_RELEASE( m_pSceneBlendedDepthVS );
        SAFE_RELEASE( m_pSceneBlendedPS );
        SAFE_RELEASE( m_pSceneBlendedPSShadows );
        SAFE_RELEASE( m_pSceneBlendedLayout );
        SAFE_RELEASE( m_pSceneBlendedDepthLayout );

        for( int i = 0; i < NUM_LIGHT_CULLING_COMPUTE_SHADERS_FOR_BLENDED_OBJECTS; i++ )
        {
            SAFE_RELEASE(m_pLightCullCSForBlendedObjects[i]);
        }

        SAFE_RELEASE( m_pDebugDrawNumLightsPerTileRadarColorsPS );
        SAFE_RELEASE( m_pDebugDrawNumLightsPerTileGrayscalePS );
        SAFE_RELEASE( m_pDebugDrawNumLightsAndVPLsPerTileRadarColorsPS );
        SAFE_RELEASE( m_pDebugDrawNumLightsAndVPLsPerTileGrayscalePS );
        SAFE_RELEASE( m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledRadarColorsPS );
        SAFE_RELEASE( m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledGrayscalePS );
        SAFE_RELEASE( m_pDebugDrawLegendForNumLightsPerTileVS );
        SAFE_RELEASE( m_pDebugDrawLegendForNumLightsPerTileRadarColorsPS );
        SAFE_RELEASE( m_pDebugDrawLegendForNumLightsPerTileGrayscalePS );
        SAFE_RELEASE( m_pDebugDrawLegendForNumLightsLayout11 );

        SAFE_RELEASE(m_pFullScreenVS);

        for( int i = 0; i < NUM_FULL_SCREEN_PIXEL_SHADERS; i++ )
        {
            SAFE_RELEASE(m_pFullScreenPS[i]);
        }

        AMD::ShaderCache::Macro ShaderMacroFullScreenPS;
        wcscpy_s( ShaderMacroFullScreenPS.m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"NUM_MSAA_SAMPLES" );

        AMD::ShaderCache::Macro ShaderMacroBlendedPS;
        wcscpy_s( ShaderMacroBlendedPS.m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"SHADOWS_ENABLED" );

        AMD::ShaderCache::Macro ShaderMacroDebugDrawNumLightsPerTilePS[2];
        wcscpy_s( ShaderMacroDebugDrawNumLightsPerTilePS[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"VPLS_ENABLED" );
        wcscpy_s( ShaderMacroDebugDrawNumLightsPerTilePS[1].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"BLENDED_PASS" );

        const D3D11_INPUT_ELEMENT_DESC LayoutForBlendedObjects[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        const D3D11_INPUT_ELEMENT_DESC LayoutForSprites[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pSceneBlendedVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderBlendedVS",
            L"Transparency.hlsl", 0, NULL, &m_pSceneBlendedLayout, LayoutForBlendedObjects, ARRAYSIZE( LayoutForBlendedObjects ) );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pSceneBlendedDepthVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderBlendedDepthVS",
            L"Transparency.hlsl", 0, NULL, &m_pSceneBlendedDepthLayout, LayoutForBlendedObjects, ARRAYSIZE( LayoutForBlendedObjects ) );

        // SHADOWS_ENABLED = 0 (false)
        ShaderMacroBlendedPS.m_iValue = 0;
        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pSceneBlendedPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderBlendedPS",
            L"Transparency.hlsl", 1, &ShaderMacroBlendedPS, NULL, NULL, 0 );

        // SHADOWS_ENABLED = 1 (true)
        ShaderMacroBlendedPS.m_iValue = 1;
        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pSceneBlendedPSShadows, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderBlendedPS",
            L"Transparency.hlsl", 1, &ShaderMacroBlendedPS, NULL, NULL, 0 );

        // BLENDED_PASS = 0 (false)
        ShaderMacroDebugDrawNumLightsPerTilePS[1].m_iValue = 0;

        // VPLS_ENABLED = 0 (false)
        ShaderMacroDebugDrawNumLightsPerTilePS[0].m_iValue = 0;
        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawNumLightsPerTileRadarColorsPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawNumLightsPerTileRadarColorsPS",
            L"DebugDraw.hlsl", 2, ShaderMacroDebugDrawNumLightsPerTilePS, NULL, NULL, 0 );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawNumLightsPerTileGrayscalePS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawNumLightsPerTileGrayscalePS",
            L"DebugDraw.hlsl", 2, ShaderMacroDebugDrawNumLightsPerTilePS, NULL, NULL, 0 );

        // VPLS_ENABLED = 1 (true)
        ShaderMacroDebugDrawNumLightsPerTilePS[0].m_iValue = 1;
        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawNumLightsAndVPLsPerTileRadarColorsPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawNumLightsPerTileRadarColorsPS",
            L"DebugDraw.hlsl", 2, ShaderMacroDebugDrawNumLightsPerTilePS, NULL, NULL, 0 );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawNumLightsAndVPLsPerTileGrayscalePS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawNumLightsPerTileGrayscalePS",
            L"DebugDraw.hlsl", 2, ShaderMacroDebugDrawNumLightsPerTilePS, NULL, NULL, 0 );

        // BLENDED_PASS = 1 (true), still with VPLS_ENABLED = 1 (true)
        ShaderMacroDebugDrawNumLightsPerTilePS[1].m_iValue = 1;
        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledRadarColorsPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawNumLightsPerTileRadarColorsPS",
            L"DebugDraw.hlsl", 2, ShaderMacroDebugDrawNumLightsPerTilePS, NULL, NULL, 0 );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledGrayscalePS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawNumLightsPerTileGrayscalePS",
            L"DebugDraw.hlsl", 2, ShaderMacroDebugDrawNumLightsPerTilePS, NULL, NULL, 0 );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawLegendForNumLightsPerTileVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"DebugDrawLegendForNumLightsPerTileVS",
            L"DebugDraw.hlsl", 0, NULL, &m_pDebugDrawLegendForNumLightsLayout11, LayoutForSprites, ARRAYSIZE( LayoutForSprites ) );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawLegendForNumLightsPerTileRadarColorsPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawLegendForNumLightsPerTileRadarColorsPS",
            L"DebugDraw.hlsl", 0, NULL, NULL, NULL, 0 );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawLegendForNumLightsPerTileGrayscalePS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawLegendForNumLightsPerTileGrayscalePS",
            L"DebugDraw.hlsl", 0, NULL, NULL, NULL, 0 );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pFullScreenVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"FullScreenQuadVS",
            L"Common.hlsl", 0, NULL, NULL, NULL, 0 );

        // sanity check
        assert(NUM_FULL_SCREEN_PIXEL_SHADERS == NUM_MSAA_SETTINGS);

        for( int i = 0; i < NUM_FULL_SCREEN_PIXEL_SHADERS; i++ )
        {
            // set NUM_MSAA_SAMPLES
            ShaderMacroFullScreenPS.m_iValue = g_nMSAASampleCount[i];
            pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pFullScreenPS[i], AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"FullScreenBlitPS",
                L"Common.hlsl", 1, &ShaderMacroFullScreenPS, NULL, NULL, 0 );
        }

        AMD::ShaderCache::Macro ShaderMacroLightCullCS[2];
        wcscpy_s( ShaderMacroLightCullCS[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"TILED_CULLING_COMPUTE_SHADER_MODE" );
        wcscpy_s( ShaderMacroLightCullCS[1].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"NUM_MSAA_SAMPLES" );

        // Set TILED_CULLING_COMPUTE_SHADER_MODE to 4 (blended geometry mode)
        ShaderMacroLightCullCS[0].m_iValue = 4;

        // sanity check
        assert(NUM_LIGHT_CULLING_COMPUTE_SHADERS_FOR_BLENDED_OBJECTS == NUM_MSAA_SETTINGS);

        for( int i = 0; i < NUM_MSAA_SETTINGS; i++ )
        {
            // set NUM_MSAA_SAMPLES
            ShaderMacroLightCullCS[1].m_iValue = g_nMSAASampleCount[i];
            pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pLightCullCSForBlendedObjects[i], AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CullLightsCS",
                L"TilingForward.hlsl", 2, ShaderMacroLightCullCS, NULL, NULL, 0 );
        }
    }

    //--------------------------------------------------------------------------------------
    // Sort the alpha-blended objects
    //--------------------------------------------------------------------------------------
    void CommonUtil::SortTransparentObjects(const XMVECTOR& vEyePt) const
    {
        UpdateBlendedObjectData( vEyePt );
    }

    //--------------------------------------------------------------------------------------
    // Draw the alpha-blended objects
    //--------------------------------------------------------------------------------------
    void CommonUtil::RenderTransparentObjects(int nDebugDrawType, bool bShadowsEnabled, bool bVPLsEnabled, bool bDepthOnlyRendering) const
    {
        HRESULT hr;
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        // update the buffer containing the per-instance transforms
        V( pd3dImmediateContext->Map( m_pBlendedTransform, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        XMMATRIX* BlendedTransformArray = (XMMATRIX*)MappedResource.pData;
        for ( int i = 0; i < g_NumBlendedObjects; i++ )
        {
            BlendedTransformArray[i] = XMMatrixTranspose(g_BlendedObjectInstanceTransform[ g_BlendedObjectIndices[ i ].nIndex ]);
        }
        pd3dImmediateContext->Unmap( m_pBlendedTransform, 0 );

        if( bDepthOnlyRendering )
        {
            pd3dImmediateContext->IASetInputLayout( m_pSceneBlendedDepthLayout );
        }
        else
        {
            pd3dImmediateContext->IASetInputLayout( m_pSceneBlendedLayout );
        }

        UINT strides[] = { 28 };
        UINT offsets[] = { 0 };
        pd3dImmediateContext->IASetIndexBuffer( m_pBlendedIB, DXGI_FORMAT_R16_UINT, 0 );
        pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pBlendedVB, strides, offsets );
        pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        ID3D11ShaderResourceView* pNULLSRV = NULL;
        ID3D11SamplerState* pNULLSampler = NULL;

        if( bDepthOnlyRendering )
        {
            pd3dImmediateContext->VSSetShader( m_pSceneBlendedDepthVS, NULL, 0 );
            pd3dImmediateContext->PSSetShader( NULL, NULL, 0 );  // null pixel shader
        }
        else
        {
            pd3dImmediateContext->VSSetShader( m_pSceneBlendedVS, NULL, 0 );

            // See if we need to use one of the debug drawing shaders instead of the default
            bool bDebugDrawingEnabled = ( nDebugDrawType == DEBUG_DRAW_RADAR_COLORS ) || ( nDebugDrawType == DEBUG_DRAW_GRAYSCALE );
            if( bDebugDrawingEnabled )
            {
                // although transparency does not use VPLs, we still need to use bVPLsEnabled to get the correct lights-per-tile
                // visualization shader, because bVPLsEnabled = true increases the max num lights per tile
                pd3dImmediateContext->PSSetShader( GetDebugDrawNumLightsPerTilePS(nDebugDrawType, bVPLsEnabled, true), NULL, 0 );
            }
            else
            {
                ID3D11PixelShader* pSceneBlendedPS = bShadowsEnabled ? m_pSceneBlendedPSShadows : m_pSceneBlendedPS;
                pd3dImmediateContext->PSSetShader( pSceneBlendedPS, NULL, 0 );
            }
        }

        pd3dImmediateContext->VSSetShaderResources( 0, 1, &m_pBlendedTransformSRV );
        pd3dImmediateContext->PSSetShaderResources( 0, 1, &pNULLSRV );
        pd3dImmediateContext->PSSetShaderResources( 1, 1, &pNULLSRV );
        pd3dImmediateContext->PSSetSamplers( 0, 1, &pNULLSampler );

        pd3dImmediateContext->DrawIndexedInstanced( 36, g_NumBlendedObjects, 0, 0, 0 );
        pd3dImmediateContext->VSSetShaderResources( 0, 1, &pNULLSRV );
    }

    //--------------------------------------------------------------------------------------
    // Draw the legend for the lights-per-tile visualization
    //--------------------------------------------------------------------------------------
    void CommonUtil::RenderLegend( CDXUTTextHelper *pTxtHelper, int nLineHeight, XMFLOAT4 Color, int nDebugDrawType, bool bVPLsEnabled ) const
    {
        // draw the legend texture for the lights-per-tile visualization
        {
            ID3D11ShaderResourceView* pNULLSRV = NULL;
            ID3D11SamplerState* pNULLSampler = NULL;

            // choose pixel shader based on radar vs. grayscale
            ID3D11PixelShader* pPixelShader = ( nDebugDrawType == DEBUG_DRAW_GRAYSCALE ) ? m_pDebugDrawLegendForNumLightsPerTileGrayscalePS : m_pDebugDrawLegendForNumLightsPerTileRadarColorsPS;

            ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

            // save depth state (for later restore)
            ID3D11DepthStencilState* pDepthStencilStateStored11 = NULL;
            UINT uStencilRefStored11;
            pd3dImmediateContext->OMGetDepthStencilState( &pDepthStencilStateStored11, &uStencilRefStored11 );

            // disable depth test
            pd3dImmediateContext->OMSetDepthStencilState( GetDepthStencilState(DEPTH_STENCIL_STATE_DISABLE_DEPTH_TEST), 0x00 );

            // Set the input layout
            pd3dImmediateContext->IASetInputLayout( m_pDebugDrawLegendForNumLightsLayout11 );

            // Set vertex buffer
            UINT uStride = sizeof( CommonUtilSpriteVertex );
            UINT uOffset = 0;
            pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pQuadForLegendVB, &uStride, &uOffset );

            // Set primitive topology
            pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

            pd3dImmediateContext->VSSetShader( m_pDebugDrawLegendForNumLightsPerTileVS, NULL, 0 );
            pd3dImmediateContext->PSSetShader( pPixelShader, NULL, 0 );
            pd3dImmediateContext->PSSetShaderResources( 0, 1, &pNULLSRV );
            pd3dImmediateContext->PSSetShaderResources( 1, 1, &pNULLSRV );
            pd3dImmediateContext->PSSetSamplers( 0, 1, &pNULLSampler );

            pd3dImmediateContext->Draw(6,0);

            // restore to previous
            pd3dImmediateContext->OMSetDepthStencilState( pDepthStencilStateStored11, uStencilRefStored11 );
            SAFE_RELEASE( pDepthStencilStateStored11 );
        }

        // draw the legend text for the lights-per-tile visualization
        // (twice, to get a drop shadow)
        XMFLOAT4 Colors[2] = { XMFLOAT4(0,0,0,Color.w), Color };
        for( int loopCount = 0; loopCount < 2; loopCount++ )
        {
            // 17 lines times line height
            int nTextureHeight = g_nLegendNumLines*nLineHeight;

            // times 2 below, one for point lights and one for spot lights
            int nMaxNumLightsPerTile = (int)(2*GetMaxNumLightsPerTile());
            if( bVPLsEnabled ) nMaxNumLightsPerTile += GetMaxNumVPLsPerTile();
            WCHAR szBuf[16];

            pTxtHelper->Begin();
            pTxtHelper->SetForegroundColor( Colors[loopCount] );
            pTxtHelper->SetInsertionPos( g_nLegendPaddingLeft+g_nLegendTextureWidth+4-loopCount, (int)m_uHeight-g_nLegendPaddingBottom-nTextureHeight-nLineHeight-3-loopCount );
            pTxtHelper->DrawTextLine( L"Num Lights" );

            int nStartVal, nEndVal;

            if( nDebugDrawType == DEBUG_DRAW_GRAYSCALE )
            {
                float fLightsPerBand = (float)(nMaxNumLightsPerTile) / (float)g_nLegendNumLines;

                for( int i = 0; i < g_nLegendNumLines; i++ )
                {
                    nStartVal = (int)((g_nLegendNumLines-1-i)*fLightsPerBand) + 1;
                    nEndVal = (int)((g_nLegendNumLines-i)*fLightsPerBand);
                    swprintf_s( szBuf, 16, L"[%d,%d]", nStartVal, nEndVal );
                    pTxtHelper->DrawTextLine( szBuf );
                }
            }
            else
            {
                // use a log scale to provide more detail when the number of lights is smaller

                // want to find the base b such that the logb of nMaxNumLightsPerTile is 14
                // (because we have 14 radar colors)
                float fLogBase = exp(0.07142857f*log((float)nMaxNumLightsPerTile));

                swprintf_s( szBuf, 16, L"> %d", nMaxNumLightsPerTile );
                pTxtHelper->DrawTextLine( szBuf );

                swprintf_s( szBuf, 16, L"%d", nMaxNumLightsPerTile );
                pTxtHelper->DrawTextLine( szBuf );

                nStartVal = (int)pow(fLogBase,g_nLegendNumLines-4) + 1;
                nEndVal = nMaxNumLightsPerTile-1;
                swprintf_s( szBuf, 16, L"[%d,%d]", nStartVal, nEndVal );
                pTxtHelper->DrawTextLine( szBuf );

                for( int i = 0; i < g_nLegendNumLines-5; i++ )
                {
                    nStartVal = (int)pow(fLogBase,g_nLegendNumLines-5-i) + 1;
                    nEndVal = (int)pow(fLogBase,g_nLegendNumLines-4-i);
                    if( nStartVal == nEndVal )
                    {
                        swprintf_s( szBuf, 16, L"%d", nStartVal );
                    }
                    else
                    {
                        swprintf_s( szBuf, 16, L"[%d,%d]", nStartVal, nEndVal );
                    }
                    pTxtHelper->DrawTextLine( szBuf );
                }

                pTxtHelper->DrawTextLine( L"1" );
                pTxtHelper->DrawTextLine( L"0" );
            }

            pTxtHelper->End();
        }
    }

    //--------------------------------------------------------------------------------------
    // Calculate the number of tiles in the horizontal direction
    //--------------------------------------------------------------------------------------
    unsigned CommonUtil::GetNumTilesX() const
    {
        return (unsigned)( ( m_uWidth + TILE_RES - 1 ) / (float)TILE_RES );
    }

    //--------------------------------------------------------------------------------------
    // Calculate the number of tiles in the vertical direction
    //--------------------------------------------------------------------------------------
    unsigned CommonUtil::GetNumTilesY() const
    {
        return (unsigned)( ( m_uHeight + TILE_RES - 1 ) / (float)TILE_RES );
    }

    //--------------------------------------------------------------------------------------
    // Adjust max number of lights per tile based on screen height.
    // This assumes that the demo has a constant vertical field of view (fovy).
    //
    // Note that the light culling tile size stays fixed as screen size changes.
    // With a constant fovy, reducing the screen height shrinks the projected 
    // view of the scene, and so more lights can fall into our fixed tile size.
    //
    // This function reduces the max lights per tile as screen height increases, 
    // to save memory. It was tuned for this particular demo and is not intended 
    // as a general solution for all scenes.
    //--------------------------------------------------------------------------------------
    unsigned CommonUtil::GetMaxNumLightsPerTile() const
    {
        const unsigned kAdjustmentMultipier = 16;

        // I haven't tested at greater than 1080p, so cap it
        unsigned uHeight = (m_uHeight > 1080) ? 1080 : m_uHeight;

        // adjust max lights per tile down as height increases
        return ( MAX_NUM_LIGHTS_PER_TILE - ( kAdjustmentMultipier * ( uHeight / 120 ) ) );
    }

    unsigned CommonUtil::GetMaxNumElementsPerTile() const
    {
        // max num lights times 2 (because the halfZ method has two lists per tile, list A and B),
        // plus two more to store the 32-bit halfZ, plus one more for the light count of list A,
        // plus one more for the light count of list B
        return (2*GetMaxNumLightsPerTile() + 4);
    }

    //--------------------------------------------------------------------------------------
    // Adjust max number of lights per tile based on screen height.
    // This assumes that the demo has a constant vertical field of view (fovy).
    //
    // Note that the light culling tile size stays fixed as screen size changes.
    // With a constant fovy, reducing the screen height shrinks the projected 
    // view of the scene, and so more lights can fall into our fixed tile size.
    //
    // This function reduces the max lights per tile as screen height increases, 
    // to save memory. It was tuned for this particular demo and is not intended 
    // as a general solution for all scenes.
    //--------------------------------------------------------------------------------------
    unsigned CommonUtil::GetMaxNumVPLsPerTile() const
    {
        const unsigned kAdjustmentMultipier = 8;

        // I haven't tested at greater than 1080p, so cap it
        unsigned uHeight = (m_uHeight > 1080) ? 1080 : m_uHeight;

        // adjust max lights per tile down as height increases
        return ( MAX_NUM_VPLS_PER_TILE - ( kAdjustmentMultipier * ( uHeight / 120 ) ) );
    }

    unsigned CommonUtil::GetMaxNumVPLElementsPerTile() const
    {
        // max num lights times 2 (because the halfZ method has two lists per tile, list A and B),
        // plus two more to store the 32-bit halfZ, plus one more for the light count of list A,
        // plus one more for the light count of list B
        return (2*GetMaxNumVPLsPerTile() + 4);
    }

    void CommonUtil::DrawGrid(int nGridNumber, int nTriangleDensity, bool bWithTextures) const
    {
        // clamp nGridNumber
        nGridNumber = (nGridNumber < 0) ? 0 : nGridNumber;
        nGridNumber = (nGridNumber > MAX_NUM_GRID_OBJECTS-1) ? MAX_NUM_GRID_OBJECTS-1 : nGridNumber;

        // clamp nTriangleDensity
        nTriangleDensity = (nTriangleDensity < 0) ? 0 : nTriangleDensity;
        nTriangleDensity = (nTriangleDensity > TRIANGLE_DENSITY_NUM_TYPES-1) ? TRIANGLE_DENSITY_NUM_TYPES-1 : nTriangleDensity;

        ID3D11Buffer* const * pGridVB = m_pGridVB[nTriangleDensity];
        ID3D11Buffer* pGridIB = m_pGridIB[nTriangleDensity];

        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        // Set vertex buffer
        UINT uStride = sizeof( CommonUtilGridVertex );
        UINT uOffset = 0;
        pd3dImmediateContext->IASetVertexBuffers( 0, 1, &pGridVB[nGridNumber], &uStride, &uOffset );
        pd3dImmediateContext->IASetIndexBuffer( pGridIB, DXGI_FORMAT_R16_UINT, 0 );

        // Set primitive topology
        pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        if( bWithTextures )
        {
            pd3dImmediateContext->PSSetShaderResources( 0, 1, &m_pGridDiffuseTextureSRV );
            pd3dImmediateContext->PSSetShaderResources( 1, 1, &m_pGridNormalMapSRV );
        }

        pd3dImmediateContext->DrawIndexed( g_nNumGridIndices[nTriangleDensity], 0, 0 );
    }

    //--------------------------------------------------------------------------------------
    // Return one of the lights-per-tile visualization shaders, based on nDebugDrawType
    //--------------------------------------------------------------------------------------
    ID3D11PixelShader * CommonUtil::GetDebugDrawNumLightsPerTilePS( int nDebugDrawType, bool bVPLsEnabled, bool bForTransparentObjects ) const
    {
        if ( ( nDebugDrawType != DEBUG_DRAW_RADAR_COLORS ) && ( nDebugDrawType != DEBUG_DRAW_GRAYSCALE ) )
        {
            return NULL;
        }

        if( ( nDebugDrawType == DEBUG_DRAW_RADAR_COLORS ) && bVPLsEnabled && !bForTransparentObjects)
        {
            return m_pDebugDrawNumLightsAndVPLsPerTileRadarColorsPS;
        }
        else if( ( nDebugDrawType == DEBUG_DRAW_RADAR_COLORS ) && !bVPLsEnabled )
        {
            return m_pDebugDrawNumLightsPerTileRadarColorsPS;
        }
        else if( ( nDebugDrawType == DEBUG_DRAW_GRAYSCALE ) && bVPLsEnabled && !bForTransparentObjects )
        {
            return m_pDebugDrawNumLightsAndVPLsPerTileGrayscalePS;
        }
        else if( ( nDebugDrawType == DEBUG_DRAW_GRAYSCALE ) && !bVPLsEnabled )
        {
            return m_pDebugDrawNumLightsPerTileGrayscalePS;
        }
        else if( ( nDebugDrawType == DEBUG_DRAW_RADAR_COLORS ) && bVPLsEnabled && bForTransparentObjects )
        {
            return m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledRadarColorsPS;
        }
        else if( ( nDebugDrawType == DEBUG_DRAW_GRAYSCALE ) && bVPLsEnabled && bForTransparentObjects )
        {
            return m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledGrayscalePS;
        }
        else
        {
            // default
            return m_pDebugDrawNumLightsPerTileGrayscalePS;
        }
    }

    //--------------------------------------------------------------------------------------
    // Return one of the full-screen pixel shaders, based on MSAA settings
    //--------------------------------------------------------------------------------------
    ID3D11PixelShader * CommonUtil::GetFullScreenPS( unsigned uMSAASampleCount ) const
    {
        // sanity check
        assert(NUM_FULL_SCREEN_PIXEL_SHADERS == NUM_MSAA_SETTINGS);

        switch( uMSAASampleCount )
        {
        case 1: return m_pFullScreenPS[MSAA_SETTING_NO_MSAA]; break;
        case 2: return m_pFullScreenPS[MSAA_SETTING_2X_MSAA]; break;
        case 4: return m_pFullScreenPS[MSAA_SETTING_4X_MSAA]; break;
        default: assert(false); break;
        }

        return NULL;
    }

    //--------------------------------------------------------------------------------------
    // Return one of the light culling compute shaders, based on MSAA settings
    //--------------------------------------------------------------------------------------
    ID3D11ComputeShader * CommonUtil::GetLightCullCSForBlendedObjects( unsigned uMSAASampleCount ) const
    {
        // sanity check
        assert(NUM_LIGHT_CULLING_COMPUTE_SHADERS_FOR_BLENDED_OBJECTS == NUM_MSAA_SETTINGS);

        switch( uMSAASampleCount )
        {
        case 1: return m_pLightCullCSForBlendedObjects[MSAA_SETTING_NO_MSAA]; break;
        case 2: return m_pLightCullCSForBlendedObjects[MSAA_SETTING_2X_MSAA]; break;
        case 4: return m_pLightCullCSForBlendedObjects[MSAA_SETTING_4X_MSAA]; break;
        default: assert(false); break;
        }

        return NULL;
    }

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
