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
// File: LightUtil.cpp
//
// Helper functions for the TiledLighting11 sample.
//--------------------------------------------------------------------------------------

#include "..\\..\\DXUT\\Core\\DXUT.h"
#include "..\\..\\DXUT\\Optional\\SDKmisc.h"

#include "..\\..\\AMD_SDK\\inc\\AMD_SDK.h"

#include "LightUtil.h"
#include "CommonUtil.h"

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

using namespace DirectX;

// Packs integer RGB color values for into ABGR format (i.e. DXGI_FORMAT_R8G8B8A8_UNORM).
#define COLOR( r, g, b ) (DWORD)((255 << 24) | ((b) << 16) | ((g) << 8) | (r))


struct LightUtilSpriteVertex
{
    XMFLOAT3 v3Pos;
    XMFLOAT2 v2TexCoord;
};

// static array for sprite quad vertex data
static LightUtilSpriteVertex         g_QuadForLightsVertexData[6];

// static arrays for the point light data
static XMFLOAT4             g_PointLightDataArrayCenterAndRadius[TiledLighting11::MAX_NUM_LIGHTS];
static DWORD                g_PointLightDataArrayColor[TiledLighting11::MAX_NUM_LIGHTS];

static XMFLOAT4             g_ShadowCastingPointLightDataArrayCenterAndRadius[TiledLighting11::MAX_NUM_SHADOWCASTING_POINTS];
static DWORD                g_ShadowCastingPointLightDataArrayColor[TiledLighting11::MAX_NUM_SHADOWCASTING_POINTS];

static XMMATRIX             g_ShadowCastingPointLightViewProjTransposed[TiledLighting11::MAX_NUM_SHADOWCASTING_POINTS][6];
static XMMATRIX             g_ShadowCastingPointLightViewProjInvTransposed[TiledLighting11::MAX_NUM_SHADOWCASTING_POINTS][6];

struct LightUtilConeVertex
{
    XMFLOAT3 v3Pos;
    XMFLOAT3 v3Norm;
    XMFLOAT2 v2TexCoord;
};

// static arrays for cone vertex and index data (for visualizing spot lights)
static const int            g_nConeNumTris = 90;
static const int            g_nConeNumVertices = 2*g_nConeNumTris;
static const int            g_nConeNumIndices = 3*g_nConeNumTris;
static LightUtilConeVertex  g_ConeForSpotLightsVertexData[g_nConeNumVertices];
static unsigned short       g_ConeForSpotLightsIndexData[g_nConeNumIndices];

// these are half-precision (i.e. 16-bit) float values, 
// stored as unsigned shorts
struct LightUtilSpotParams
{
    unsigned short fLightDirX;
    unsigned short fLightDirY;
    unsigned short fCosineOfConeAngleAndLightDirZSign;
    unsigned short fFalloffRadius;
};

// static arrays for the spot light data
static XMFLOAT4             g_SpotLightDataArrayCenterAndRadius[TiledLighting11::MAX_NUM_LIGHTS];
static DWORD                g_SpotLightDataArrayColor[TiledLighting11::MAX_NUM_LIGHTS];
static LightUtilSpotParams  g_SpotLightDataArraySpotParams[TiledLighting11::MAX_NUM_LIGHTS];

static XMFLOAT4             g_ShadowCastingSpotLightDataArrayCenterAndRadius[TiledLighting11::MAX_NUM_SHADOWCASTING_SPOTS];
static DWORD                g_ShadowCastingSpotLightDataArrayColor[TiledLighting11::MAX_NUM_SHADOWCASTING_SPOTS];
static LightUtilSpotParams  g_ShadowCastingSpotLightDataArraySpotParams[TiledLighting11::MAX_NUM_SHADOWCASTING_SPOTS];

// rotation matrices used when visualizing the spot lights
static XMMATRIX             g_SpotLightDataArraySpotMatrices[TiledLighting11::MAX_NUM_LIGHTS];
static XMMATRIX             g_ShadowCastingSpotLightDataArraySpotMatrices[TiledLighting11::MAX_NUM_SHADOWCASTING_SPOTS];

static XMMATRIX             g_ShadowCastingSpotLightViewProjTransposed[TiledLighting11::MAX_NUM_SHADOWCASTING_SPOTS];
static XMMATRIX             g_ShadowCastingSpotLightViewProjInvTransposed[TiledLighting11::MAX_NUM_SHADOWCASTING_SPOTS];

// miscellaneous constants
static const float TWO_PI = 6.28318530718f;

// there should only be one LightUtil object
static int LightUtilObjectCounter = 0;

static float GetRandFloat( float fRangeMin, float fRangeMax )
{
    // Generate random numbers in the half-closed interval
    // [rangeMin, rangeMax). In other words,
    // rangeMin <= random number < rangeMax
    return  (float)rand() / (RAND_MAX + 1) * (fRangeMax - fRangeMin) + fRangeMin;
}

static DWORD GetRandColor()
{
    static unsigned uCounter = 0;
    uCounter++;

    XMFLOAT4 Color;
    if( uCounter%2 == 0 )
    {
        // since green contributes the most to perceived brightness, 
        // cap it's min value to avoid overly dim lights
        Color = XMFLOAT4(GetRandFloat(0.0f,1.0f),GetRandFloat(0.27f,1.0f),GetRandFloat(0.0f,1.0f),1.0f);
    }
    else
    {
        // else ensure the red component has a large value, again 
        // to avoid overly dim lights
        Color = XMFLOAT4(GetRandFloat(0.9f,1.0f),GetRandFloat(0.0f,1.0f),GetRandFloat(0.0f,1.0f),1.0f);
    }

    DWORD dwR = (DWORD)(Color.x * 255.0f + 0.5f);
    DWORD dwG = (DWORD)(Color.y * 255.0f + 0.5f);
    DWORD dwB = (DWORD)(Color.z * 255.0f + 0.5f);

    return COLOR(dwR, dwG, dwB);
}

static XMFLOAT3 GetRandLightDirection()
{
    static unsigned uCounter = 0;
    uCounter++;

    XMFLOAT3 vLightDir;
    vLightDir.x = GetRandFloat(-1.0f,1.0f);
    vLightDir.y = GetRandFloat( 0.1f,1.0f);
    vLightDir.z = GetRandFloat(-1.0f,1.0f);

    if( uCounter%2 == 0 )
    {
        vLightDir.y = -vLightDir.y;
    }

    XMFLOAT3 vResult;
    XMVECTOR NormalizedLightDir = XMVector3Normalize( XMLoadFloat3( &vLightDir) );
    XMStoreFloat3( &vResult, NormalizedLightDir );

    return vResult;
}

static LightUtilSpotParams PackSpotParams(const XMFLOAT3& vLightDir, float fCosineOfConeAngle, float fFalloffRadius)
{
    assert( fCosineOfConeAngle > 0.0f );
    assert( fFalloffRadius > 0.0f );

    LightUtilSpotParams PackedParams;
    PackedParams.fLightDirX = AMD::ConvertF32ToF16( vLightDir.x );
    PackedParams.fLightDirY = AMD::ConvertF32ToF16( vLightDir.y );
    PackedParams.fCosineOfConeAngleAndLightDirZSign = AMD::ConvertF32ToF16( fCosineOfConeAngle );
    PackedParams.fFalloffRadius = AMD::ConvertF32ToF16( fFalloffRadius );

    // put the sign bit for light dir z in the sign bit for the cone angle
    // (we can do this because we know the cone angle is always positive)
    if( vLightDir.z < 0.0f )
    {
        PackedParams.fCosineOfConeAngleAndLightDirZSign |= 0x8000;
    }
    else
    {
        PackedParams.fCosineOfConeAngleAndLightDirZSign &= 0x7FFF;
    }

    return PackedParams;
}

static void CalcPointLightView( const XMFLOAT4& PositionAndRadius, int nFace, XMMATRIX& View )
{
    const XMVECTOR dir[ 6 ] = 
    {
        XMVectorSet(  1.0f,  0.0f,  0.0f, 0.0f ),
        XMVectorSet( -1.0f,  0.0f,  0.0f, 0.0f ),
        XMVectorSet(  0.0f,  1.0f,  0.0f, 0.0f ),
        XMVectorSet(  0.0f, -1.0f,  0.0f, 0.0f ),
        XMVectorSet(  0.0f,  0.0f, -1.0f, 0.0f ),
        XMVectorSet(  0.0f,  0.0f,  1.0f, 0.0f ),
    };

    const XMVECTOR up[ 6 ] = 
    {
        XMVectorSet( 0.0f, 1.0f,  0.0f, 0.0f ),
        XMVectorSet( 0.0f, 1.0f,  0.0f, 0.0f ),
        XMVectorSet( 0.0f, 0.0f,  1.0f, 0.0f ),
        XMVectorSet( 0.0f, 0.0f, -1.0f, 0.0f ),
        XMVectorSet( 0.0f, 1.0f,  0.0f, 0.0f ),
        XMVectorSet( 0.0f, 1.0f,  0.0f, 0.0f ),
    };

    XMVECTOR eye = XMVectorSet( PositionAndRadius.x, PositionAndRadius.y, PositionAndRadius.z, 0.0f );

    XMVECTOR at = eye + dir[ nFace ];
    View = XMMatrixLookAtLH( eye, at, up[ nFace ] );
}

static void CalcPointLightProj( const XMFLOAT4& PositionAndRadius, XMMATRIX& Proj )
{
    Proj = XMMatrixPerspectiveFovLH( XMConvertToRadians( 90.0f ), 1.0f, 2.0f, PositionAndRadius.w );
}

static void CalcPointLightViewProj( const XMFLOAT4& positionAndRadius, XMMATRIX* viewProjTransposed, XMMATRIX* viewProjInvTransposed )
{
    XMMATRIX proj;
    XMMATRIX view;
    CalcPointLightProj( positionAndRadius, proj );
    for ( int i = 0; i < 6; i++ )
    {
        CalcPointLightView( positionAndRadius, i, view );
        XMMATRIX viewProj = view * proj;
        viewProjTransposed[i] = XMMatrixTranspose( viewProj );
        XMMATRIX viewProjInv = XMMatrixInverse( NULL, viewProj );
        viewProjInvTransposed[i] = XMMatrixTranspose( viewProjInv );
    }
}

static void AddShadowCastingPointLight( const XMFLOAT4& positionAndRadius, DWORD color )
{
    static unsigned uShadowCastingPointLightCounter = 0;

    assert( uShadowCastingPointLightCounter < TiledLighting11::MAX_NUM_SHADOWCASTING_POINTS );

    g_ShadowCastingPointLightDataArrayCenterAndRadius[ uShadowCastingPointLightCounter ] = positionAndRadius;
    g_ShadowCastingPointLightDataArrayColor[ uShadowCastingPointLightCounter ] = color;

    CalcPointLightViewProj( positionAndRadius, g_ShadowCastingPointLightViewProjTransposed[uShadowCastingPointLightCounter], g_ShadowCastingPointLightViewProjInvTransposed[uShadowCastingPointLightCounter] );

    uShadowCastingPointLightCounter++;
}

static void CalcSpotLightView( const XMFLOAT3& Eye, const XMFLOAT3& LookAt, XMMATRIX& View )
{
    XMFLOAT3 Up( 0.0f, 1.0f, 0.0f );
    View = XMMatrixLookAtLH( XMLoadFloat3(&Eye), XMLoadFloat3(&LookAt), XMLoadFloat3(&Up) );
}

static void CalcSpotLightProj( float radius, XMMATRIX& Proj )
{
    Proj = XMMatrixPerspectiveFovLH( XMConvertToRadians( 70.52877936f ), 1.0f, 2.0f, radius );
}

static void CalcSpotLightViewProj( const XMFLOAT4& positionAndRadius, const XMFLOAT3& lookAt, XMMATRIX* viewProjTransposed, XMMATRIX* viewProjInvTransposed )
{
    XMMATRIX proj;
    XMMATRIX view;
    CalcSpotLightProj( positionAndRadius.w, proj );
    CalcSpotLightView( *(const XMFLOAT3 *)(&positionAndRadius), lookAt, view );
    XMMATRIX viewProj = view * proj;
    *viewProjTransposed = XMMatrixTranspose( viewProj );
    XMMATRIX viewProjInv = XMMatrixInverse( NULL, viewProj );
    *viewProjInvTransposed = XMMatrixTranspose( viewProjInv );
}

static void AddShadowCastingSpotLight( const XMFLOAT4& positionAndRadius, const XMFLOAT3& lookAt, DWORD color )
{
    static unsigned uShadowCastingSpotLightCounter = 0;

    assert( uShadowCastingSpotLightCounter < TiledLighting11::MAX_NUM_SHADOWCASTING_SPOTS );

    XMVECTOR eye = XMLoadFloat3( (XMFLOAT3*)(&positionAndRadius) );
    XMVECTOR dir = XMLoadFloat3(&lookAt) - eye;
    dir = XMVector3Normalize( dir );
    XMFLOAT3 f3Dir;
    XMStoreFloat3(&f3Dir, dir);

    XMVECTOR boundingSpherePos = eye + (dir * positionAndRadius.w);

    g_ShadowCastingSpotLightDataArrayCenterAndRadius[ uShadowCastingSpotLightCounter ] = XMFLOAT4( XMVectorGetX(boundingSpherePos), XMVectorGetY(boundingSpherePos), XMVectorGetZ(boundingSpherePos), positionAndRadius.w );
    g_ShadowCastingSpotLightDataArrayColor[ uShadowCastingSpotLightCounter ] = color;

    // cosine of cone angle is cosine(35.26438968 degrees) = 0.816496580927726
    g_ShadowCastingSpotLightDataArraySpotParams[ uShadowCastingSpotLightCounter ] = PackSpotParams( f3Dir, 0.816496580927726f, positionAndRadius.w * 1.33333333f );

    CalcSpotLightViewProj( positionAndRadius, lookAt, &g_ShadowCastingSpotLightViewProjTransposed[uShadowCastingSpotLightCounter], &g_ShadowCastingSpotLightViewProjInvTransposed[uShadowCastingSpotLightCounter] );

    // build a "rotate from one vector to another" matrix, to point the spot light 
    // cone along its light direction
    XMVECTOR s = XMVectorSet(0.0f,-1.0f,0.0f,0.0f);
    XMVECTOR t = dir;
    XMFLOAT3 v;
    XMStoreFloat3( &v, XMVector3Cross(s,t) );
    float e = XMVectorGetX(XMVector3Dot(s,t));
    float h = 1.0f / (1.0f + e);

    XMFLOAT4X4 f4x4Rotation;
    XMStoreFloat4x4( &f4x4Rotation, XMMatrixIdentity() );
    f4x4Rotation._11 = e + h*v.x*v.x;
    f4x4Rotation._12 = h*v.x*v.y - v.z;
    f4x4Rotation._13 = h*v.x*v.z + v.y;
    f4x4Rotation._21 = h*v.x*v.y + v.z;
    f4x4Rotation._22 = e + h*v.y*v.y;
    f4x4Rotation._23 = h*v.y*v.z - v.x;
    f4x4Rotation._31 = h*v.x*v.z - v.y;
    f4x4Rotation._32 = h*v.y*v.z + v.x;
    f4x4Rotation._33 = e + h*v.z*v.z;
    XMMATRIX mRotation = XMLoadFloat4x4( &f4x4Rotation );

    g_ShadowCastingSpotLightDataArraySpotMatrices[ uShadowCastingSpotLightCounter ] = XMMatrixTranspose(mRotation);

    uShadowCastingSpotLightCounter++;
}

namespace TiledLighting11
{

    //--------------------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------------------
    LightUtil::LightUtil()
        :m_pPointLightBufferCenterAndRadius(NULL)
        ,m_pPointLightBufferCenterAndRadiusSRV(NULL)
        ,m_pPointLightBufferColor(NULL)
        ,m_pPointLightBufferColorSRV(NULL)
        ,m_pShadowCastingPointLightBufferCenterAndRadius(NULL)
        ,m_pShadowCastingPointLightBufferCenterAndRadiusSRV(NULL)
        ,m_pShadowCastingPointLightBufferColor(NULL)
        ,m_pShadowCastingPointLightBufferColorSRV(NULL)
        ,m_pSpotLightBufferCenterAndRadius(NULL)
        ,m_pSpotLightBufferCenterAndRadiusSRV(NULL)
        ,m_pSpotLightBufferColor(NULL)
        ,m_pSpotLightBufferColorSRV(NULL)
        ,m_pSpotLightBufferSpotParams(NULL)
        ,m_pSpotLightBufferSpotParamsSRV(NULL)
        ,m_pSpotLightBufferSpotMatrices(NULL)
        ,m_pSpotLightBufferSpotMatricesSRV(NULL)
        ,m_pShadowCastingSpotLightBufferCenterAndRadius(NULL)
        ,m_pShadowCastingSpotLightBufferCenterAndRadiusSRV(NULL)
        ,m_pShadowCastingSpotLightBufferColor(NULL)
        ,m_pShadowCastingSpotLightBufferColorSRV(NULL)
        ,m_pShadowCastingSpotLightBufferSpotParams(NULL)
        ,m_pShadowCastingSpotLightBufferSpotParamsSRV(NULL)
        ,m_pShadowCastingSpotLightBufferSpotMatrices(NULL)
        ,m_pShadowCastingSpotLightBufferSpotMatricesSRV(NULL)
        ,m_pQuadForLightsVB(NULL)
        ,m_pConeForSpotLightsVB(NULL)
        ,m_pConeForSpotLightsIB(NULL)
        ,m_pDebugDrawPointLightsVS(NULL)
        ,m_pDebugDrawPointLightsPS(NULL)
        ,m_pDebugDrawPointLightsLayout11(NULL)
        ,m_pDebugDrawSpotLightsVS(NULL)
        ,m_pDebugDrawSpotLightsPS(NULL)
        ,m_pDebugDrawSpotLightsLayout11(NULL)
        ,m_pBlendStateAdditive(NULL)
    {
        assert( LightUtilObjectCounter == 0 );
        LightUtilObjectCounter++;
    }


    //--------------------------------------------------------------------------------------
    // Destructor
    //--------------------------------------------------------------------------------------
    LightUtil::~LightUtil()
    {
        SAFE_RELEASE(m_pPointLightBufferCenterAndRadius);
        SAFE_RELEASE(m_pPointLightBufferCenterAndRadiusSRV);
        SAFE_RELEASE(m_pPointLightBufferColor);
        SAFE_RELEASE(m_pPointLightBufferColorSRV);
        SAFE_RELEASE(m_pShadowCastingPointLightBufferCenterAndRadius);
        SAFE_RELEASE(m_pShadowCastingPointLightBufferCenterAndRadiusSRV);
        SAFE_RELEASE(m_pShadowCastingPointLightBufferColor);
        SAFE_RELEASE(m_pShadowCastingPointLightBufferColorSRV);
        SAFE_RELEASE(m_pSpotLightBufferCenterAndRadius);
        SAFE_RELEASE(m_pSpotLightBufferCenterAndRadiusSRV);
        SAFE_RELEASE(m_pSpotLightBufferColor);
        SAFE_RELEASE(m_pSpotLightBufferColorSRV);
        SAFE_RELEASE(m_pSpotLightBufferSpotParams);
        SAFE_RELEASE(m_pSpotLightBufferSpotParamsSRV);
        SAFE_RELEASE(m_pSpotLightBufferSpotMatrices);
        SAFE_RELEASE(m_pSpotLightBufferSpotMatricesSRV);
        SAFE_RELEASE(m_pShadowCastingSpotLightBufferCenterAndRadius);
        SAFE_RELEASE(m_pShadowCastingSpotLightBufferCenterAndRadiusSRV);
        SAFE_RELEASE(m_pShadowCastingSpotLightBufferColor);
        SAFE_RELEASE(m_pShadowCastingSpotLightBufferColorSRV);
        SAFE_RELEASE(m_pShadowCastingSpotLightBufferSpotParams);
        SAFE_RELEASE(m_pShadowCastingSpotLightBufferSpotParamsSRV);
        SAFE_RELEASE(m_pShadowCastingSpotLightBufferSpotMatrices);
        SAFE_RELEASE(m_pShadowCastingSpotLightBufferSpotMatricesSRV);
        SAFE_RELEASE(m_pQuadForLightsVB);
        SAFE_RELEASE(m_pConeForSpotLightsVB);
        SAFE_RELEASE(m_pConeForSpotLightsIB);
        SAFE_RELEASE(m_pDebugDrawPointLightsVS);
        SAFE_RELEASE(m_pDebugDrawPointLightsPS);
        SAFE_RELEASE(m_pDebugDrawPointLightsLayout11);
        SAFE_RELEASE(m_pDebugDrawSpotLightsVS);
        SAFE_RELEASE(m_pDebugDrawSpotLightsPS);
        SAFE_RELEASE(m_pDebugDrawSpotLightsLayout11);
        SAFE_RELEASE(m_pBlendStateAdditive);

        assert( LightUtilObjectCounter == 1 );
        LightUtilObjectCounter--;
    }

    //--------------------------------------------------------------------------------------
    // Device creation hook function
    //--------------------------------------------------------------------------------------
    HRESULT LightUtil::OnCreateDevice( ID3D11Device* pd3dDevice )
    {
        HRESULT hr;

        D3D11_SUBRESOURCE_DATA InitData;

        // Create the point light buffer (center and radius)
        D3D11_BUFFER_DESC LightBufferDesc;
        ZeroMemory( &LightBufferDesc, sizeof(LightBufferDesc) );
        LightBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        LightBufferDesc.ByteWidth = sizeof( g_PointLightDataArrayCenterAndRadius );
        LightBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        InitData.pSysMem = g_PointLightDataArrayCenterAndRadius;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pPointLightBufferCenterAndRadius ) );
        DXUT_SetDebugName( m_pPointLightBufferCenterAndRadius, "PointLightBufferCenterAndRadius" );

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        ZeroMemory( &SRVDesc, sizeof( D3D11_SHADER_RESOURCE_VIEW_DESC ) );
        SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.ElementOffset = 0;
        SRVDesc.Buffer.ElementWidth = MAX_NUM_LIGHTS;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pPointLightBufferCenterAndRadius, &SRVDesc, &m_pPointLightBufferCenterAndRadiusSRV ) );

        // Create the point light buffer (color)
        LightBufferDesc.ByteWidth = sizeof( g_PointLightDataArrayColor );
        InitData.pSysMem = g_PointLightDataArrayColor;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pPointLightBufferColor ) );
        DXUT_SetDebugName( m_pPointLightBufferColor, "PointLightBufferColor" );

        SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pPointLightBufferColor, &SRVDesc, &m_pPointLightBufferColorSRV ) );

        // Create the shadow-casting point light buffer (center and radius)
        LightBufferDesc.ByteWidth = sizeof( g_ShadowCastingPointLightDataArrayCenterAndRadius );
        InitData.pSysMem = g_ShadowCastingPointLightDataArrayCenterAndRadius;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pShadowCastingPointLightBufferCenterAndRadius ) );
        DXUT_SetDebugName( m_pShadowCastingPointLightBufferCenterAndRadius, "ShadowCastingPointLightBufferCenterAndRadius" );

        SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        SRVDesc.Buffer.ElementWidth = MAX_NUM_SHADOWCASTING_POINTS;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pShadowCastingPointLightBufferCenterAndRadius, &SRVDesc, &m_pShadowCastingPointLightBufferCenterAndRadiusSRV ) );

        // Create the shadow-casting point light buffer (color)
        LightBufferDesc.ByteWidth = sizeof( g_ShadowCastingPointLightDataArrayColor );
        InitData.pSysMem = g_ShadowCastingPointLightDataArrayColor;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pShadowCastingPointLightBufferColor ) );
        DXUT_SetDebugName( m_pShadowCastingPointLightBufferColor, "ShadowCastingPointLightBufferColor" );

        SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pShadowCastingPointLightBufferColor, &SRVDesc, &m_pShadowCastingPointLightBufferColorSRV ) );

        // Create the spot light buffer (center and radius)
        ZeroMemory( &LightBufferDesc, sizeof(LightBufferDesc) );
        LightBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        LightBufferDesc.ByteWidth = sizeof( g_SpotLightDataArrayCenterAndRadius );
        LightBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        InitData.pSysMem = g_SpotLightDataArrayCenterAndRadius;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pSpotLightBufferCenterAndRadius ) );
        DXUT_SetDebugName( m_pSpotLightBufferCenterAndRadius, "SpotLightBufferCenterAndRadius" );

        ZeroMemory( &SRVDesc, sizeof( D3D11_SHADER_RESOURCE_VIEW_DESC ) );
        SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.ElementOffset = 0;
        SRVDesc.Buffer.ElementWidth = MAX_NUM_LIGHTS;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pSpotLightBufferCenterAndRadius, &SRVDesc, &m_pSpotLightBufferCenterAndRadiusSRV ) );

        // Create the spot light buffer (color)
        LightBufferDesc.ByteWidth = sizeof( g_SpotLightDataArrayColor );
        InitData.pSysMem = g_SpotLightDataArrayColor;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pSpotLightBufferColor ) );
        DXUT_SetDebugName( m_pSpotLightBufferColor, "SpotLightBufferColor" );

        SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pSpotLightBufferColor, &SRVDesc, &m_pSpotLightBufferColorSRV ) );

        // Create the spot light buffer (spot light parameters)
        LightBufferDesc.ByteWidth = sizeof( g_SpotLightDataArraySpotParams );
        InitData.pSysMem = g_SpotLightDataArraySpotParams;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pSpotLightBufferSpotParams ) );
        DXUT_SetDebugName( m_pSpotLightBufferSpotParams, "SpotLightBufferSpotParams" );

        SRVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pSpotLightBufferSpotParams, &SRVDesc, &m_pSpotLightBufferSpotParamsSRV ) );

        // Create the light buffer (spot light matrices, only used for debug drawing the spot lights)
        LightBufferDesc.ByteWidth = sizeof( g_SpotLightDataArraySpotMatrices );
        InitData.pSysMem = g_SpotLightDataArraySpotMatrices;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pSpotLightBufferSpotMatrices ) );
        DXUT_SetDebugName( m_pSpotLightBufferSpotMatrices, "SpotLightBufferSpotMatrices" );

        SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        SRVDesc.Buffer.ElementWidth = 4*MAX_NUM_LIGHTS;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pSpotLightBufferSpotMatrices, &SRVDesc, &m_pSpotLightBufferSpotMatricesSRV ) );

        // Create the shadow-casting spot light buffer (center and radius)
        LightBufferDesc.ByteWidth = sizeof( g_ShadowCastingSpotLightDataArrayCenterAndRadius );
        InitData.pSysMem = g_ShadowCastingSpotLightDataArrayCenterAndRadius;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pShadowCastingSpotLightBufferCenterAndRadius ) );
        DXUT_SetDebugName( m_pShadowCastingSpotLightBufferCenterAndRadius, "ShadowCastingSpotLightBufferCenterAndRadius" );

        SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        SRVDesc.Buffer.ElementWidth = MAX_NUM_SHADOWCASTING_SPOTS;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pShadowCastingSpotLightBufferCenterAndRadius, &SRVDesc, &m_pShadowCastingSpotLightBufferCenterAndRadiusSRV ) );

        // Create the shadow-casting spot light buffer (color)
        LightBufferDesc.ByteWidth = sizeof( g_ShadowCastingSpotLightDataArrayColor );
        InitData.pSysMem = g_ShadowCastingSpotLightDataArrayColor;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pShadowCastingSpotLightBufferColor ) );
        DXUT_SetDebugName( m_pShadowCastingSpotLightBufferColor, "ShadowCastingSpotLightBufferColor" );

        SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pShadowCastingSpotLightBufferColor, &SRVDesc, &m_pShadowCastingSpotLightBufferColorSRV ) );

        // Create the shadow-casting spot light buffer (spot light parameters)
        LightBufferDesc.ByteWidth = sizeof( g_ShadowCastingSpotLightDataArraySpotParams );
        InitData.pSysMem = g_ShadowCastingSpotLightDataArraySpotParams;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pShadowCastingSpotLightBufferSpotParams ) );
        DXUT_SetDebugName( m_pShadowCastingSpotLightBufferSpotParams, "ShadowCastingSpotLightBufferSpotParams" );

        SRVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pShadowCastingSpotLightBufferSpotParams, &SRVDesc, &m_pShadowCastingSpotLightBufferSpotParamsSRV ) );

        // Create the shadow-casting spot light buffer (spot light matrices, only used for debug drawing the spot lights)
        LightBufferDesc.ByteWidth = sizeof( g_ShadowCastingSpotLightDataArraySpotMatrices );
        InitData.pSysMem = g_ShadowCastingSpotLightDataArraySpotMatrices;
        V_RETURN( pd3dDevice->CreateBuffer( &LightBufferDesc, &InitData, &m_pShadowCastingSpotLightBufferSpotMatrices ) );
        DXUT_SetDebugName( m_pShadowCastingSpotLightBufferSpotMatrices, "ShadowCastingSpotLightBufferSpotMatrices" );

        SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        SRVDesc.Buffer.ElementWidth = 4*MAX_NUM_SHADOWCASTING_SPOTS;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pShadowCastingSpotLightBufferSpotMatrices, &SRVDesc, &m_pShadowCastingSpotLightBufferSpotMatricesSRV ) );

        // Create the vertex buffer for the sprites (a single quad)
        D3D11_BUFFER_DESC VBDesc;
        ZeroMemory( &VBDesc, sizeof(VBDesc) );
        VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
        VBDesc.ByteWidth = sizeof( g_QuadForLightsVertexData );
        VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        InitData.pSysMem = g_QuadForLightsVertexData;
        V_RETURN( pd3dDevice->CreateBuffer( &VBDesc, &InitData, &m_pQuadForLightsVB ) );
        DXUT_SetDebugName( m_pQuadForLightsVB, "QuadForLightsVB" );

        // Create the vertex buffer for the cone
        VBDesc.ByteWidth = sizeof( g_ConeForSpotLightsVertexData );
        InitData.pSysMem = g_ConeForSpotLightsVertexData;
        V_RETURN( pd3dDevice->CreateBuffer( &VBDesc, &InitData, &m_pConeForSpotLightsVB ) );
        DXUT_SetDebugName( m_pConeForSpotLightsVB, "ConeForSpotLightsVB" );

        // Create the index buffer for the cone
        D3D11_BUFFER_DESC IBDesc;
        ZeroMemory( &IBDesc, sizeof(IBDesc) );
        IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
        IBDesc.ByteWidth = sizeof( g_ConeForSpotLightsIndexData );
        IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        InitData.pSysMem = g_ConeForSpotLightsIndexData;
        V_RETURN( pd3dDevice->CreateBuffer( &IBDesc, &InitData, &m_pConeForSpotLightsIB ) );
        DXUT_SetDebugName( m_pConeForSpotLightsIB, "ConeForSpotLightsIB" );

        // Create blend states 
        D3D11_BLEND_DESC BlendStateDesc;
        ZeroMemory( &BlendStateDesc, sizeof( D3D11_BLEND_DESC ) );
        BlendStateDesc.AlphaToCoverageEnable = FALSE;
        BlendStateDesc.IndependentBlendEnable = FALSE;
        BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
        BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; 
        BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE; 
        BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO; 
        BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE; 
        BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        V_RETURN( pd3dDevice->CreateBlendState( &BlendStateDesc, &m_pBlendStateAdditive ) );

        return hr;
    }


    //--------------------------------------------------------------------------------------
    // Device destruction hook function
    //--------------------------------------------------------------------------------------
    void LightUtil::OnDestroyDevice()
    {
        SAFE_RELEASE( m_pPointLightBufferCenterAndRadius );
        SAFE_RELEASE( m_pPointLightBufferCenterAndRadiusSRV );
        SAFE_RELEASE( m_pPointLightBufferColor );
        SAFE_RELEASE( m_pPointLightBufferColorSRV );

        SAFE_RELEASE( m_pShadowCastingPointLightBufferCenterAndRadius );
        SAFE_RELEASE( m_pShadowCastingPointLightBufferCenterAndRadiusSRV );
        SAFE_RELEASE( m_pShadowCastingPointLightBufferColor );
        SAFE_RELEASE( m_pShadowCastingPointLightBufferColorSRV );

        SAFE_RELEASE( m_pSpotLightBufferCenterAndRadius );
        SAFE_RELEASE( m_pSpotLightBufferCenterAndRadiusSRV );
        SAFE_RELEASE( m_pSpotLightBufferColor );
        SAFE_RELEASE( m_pSpotLightBufferColorSRV );
        SAFE_RELEASE( m_pSpotLightBufferSpotParams );
        SAFE_RELEASE( m_pSpotLightBufferSpotParamsSRV );

        SAFE_RELEASE( m_pSpotLightBufferSpotMatrices );
        SAFE_RELEASE( m_pSpotLightBufferSpotMatricesSRV );

        SAFE_RELEASE( m_pShadowCastingSpotLightBufferCenterAndRadius );
        SAFE_RELEASE( m_pShadowCastingSpotLightBufferCenterAndRadiusSRV );
        SAFE_RELEASE( m_pShadowCastingSpotLightBufferColor );
        SAFE_RELEASE( m_pShadowCastingSpotLightBufferColorSRV );
        SAFE_RELEASE( m_pShadowCastingSpotLightBufferSpotParams );
        SAFE_RELEASE( m_pShadowCastingSpotLightBufferSpotParamsSRV );

        SAFE_RELEASE( m_pShadowCastingSpotLightBufferSpotMatrices );
        SAFE_RELEASE( m_pShadowCastingSpotLightBufferSpotMatricesSRV );

        SAFE_RELEASE( m_pQuadForLightsVB );

        SAFE_RELEASE( m_pConeForSpotLightsVB );
        SAFE_RELEASE( m_pConeForSpotLightsIB );

        SAFE_RELEASE( m_pDebugDrawPointLightsVS );
        SAFE_RELEASE( m_pDebugDrawPointLightsPS );
        SAFE_RELEASE( m_pDebugDrawPointLightsLayout11 );

        SAFE_RELEASE( m_pDebugDrawSpotLightsVS );
        SAFE_RELEASE( m_pDebugDrawSpotLightsPS );
        SAFE_RELEASE( m_pDebugDrawSpotLightsLayout11 );

        SAFE_RELEASE( m_pBlendStateAdditive );
    }


    //--------------------------------------------------------------------------------------
    // Resized swap chain hook function
    //--------------------------------------------------------------------------------------
    HRESULT LightUtil::OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
    {
        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Releasing swap chain hook function
    //--------------------------------------------------------------------------------------
    void LightUtil::OnReleasingSwapChain()
    {
    }

    //--------------------------------------------------------------------------------------
    // Render hook function, to draw the lights (as instanced quads)
    //--------------------------------------------------------------------------------------
    void LightUtil::RenderLights( float fElapsedTime, unsigned uNumPointLights, unsigned uNumSpotLights, int nLightingMode, const CommonUtil& CommonUtil ) const
    {
        ID3D11ShaderResourceView* pNULLSRV = NULL;
        ID3D11SamplerState* pNULLSampler = NULL;

        ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

        // save blend state (for later restore)
        ID3D11BlendState* pBlendStateStored11 = NULL;
        FLOAT afBlendFactorStored11[4];
        UINT uSampleMaskStored11;
        pd3dImmediateContext->OMGetBlendState( &pBlendStateStored11, afBlendFactorStored11, &uSampleMaskStored11 );
        FLOAT BlendFactor[4] = { 0,0,0,0 };

        // save depth state (for later restore)
        ID3D11DepthStencilState* pDepthStencilStateStored11 = NULL;
        UINT uStencilRefStored11;
        pd3dImmediateContext->OMGetDepthStencilState( &pDepthStencilStateStored11, &uStencilRefStored11 );

        // point lights
        if( uNumPointLights > 0 )
        {
            // additive blending, enable depth test but don't write depth, disable culling
            pd3dImmediateContext->OMSetBlendState( m_pBlendStateAdditive, BlendFactor, 0xFFFFFFFF );
            pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DISABLE_DEPTH_WRITE), 0x00 );
            pd3dImmediateContext->RSSetState( CommonUtil.GetRasterizerState(RASTERIZER_STATE_DISABLE_CULLING) );

            // Set the input layout
            pd3dImmediateContext->IASetInputLayout( m_pDebugDrawPointLightsLayout11 );

            // Set vertex buffer
            UINT uStride = sizeof( LightUtilSpriteVertex );
            UINT uOffset = 0;
            pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pQuadForLightsVB, &uStride, &uOffset );

            // Set primitive topology
            pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

            pd3dImmediateContext->VSSetShader( m_pDebugDrawPointLightsVS, NULL, 0 );
            pd3dImmediateContext->VSSetShaderResources( 2, 1, GetPointLightBufferCenterAndRadiusSRVParam(nLightingMode) );
            pd3dImmediateContext->VSSetShaderResources( 3, 1, GetPointLightBufferColorSRVParam(nLightingMode) );
            pd3dImmediateContext->PSSetShader( m_pDebugDrawPointLightsPS, NULL, 0 );
            pd3dImmediateContext->PSSetShaderResources( 0, 1, &pNULLSRV );
            pd3dImmediateContext->PSSetShaderResources( 1, 1, &pNULLSRV );
            pd3dImmediateContext->PSSetSamplers( 0, 1, &pNULLSampler );

            pd3dImmediateContext->DrawInstanced(6,uNumPointLights,0,0);

            // restore to default
            pd3dImmediateContext->VSSetShaderResources( 2, 1, &pNULLSRV );
            pd3dImmediateContext->VSSetShaderResources( 3, 1, &pNULLSRV );
        }

        // spot lights
        if( uNumSpotLights > 0 )
        {
            // render spot lights as ordinary opaque geometry
            pd3dImmediateContext->OMSetBlendState( m_pBlendStateAdditive, BlendFactor, 0xFFFFFFFF );
            pd3dImmediateContext->OMSetDepthStencilState( CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_GREATER), 0x00 );
            pd3dImmediateContext->RSSetState( CommonUtil.GetRasterizerState(RASTERIZER_STATE_DISABLE_CULLING) );

            // Set the input layout
            pd3dImmediateContext->IASetInputLayout( m_pDebugDrawSpotLightsLayout11 );

            // Set vertex buffer
            UINT uStride = sizeof( LightUtilConeVertex );
            UINT uOffset = 0;
            pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pConeForSpotLightsVB, &uStride, &uOffset );
            pd3dImmediateContext->IASetIndexBuffer(m_pConeForSpotLightsIB, DXGI_FORMAT_R16_UINT, 0 );

            // Set primitive topology
            pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

            pd3dImmediateContext->VSSetShader( m_pDebugDrawSpotLightsVS, NULL, 0 );
            pd3dImmediateContext->VSSetShaderResources( 5, 1, GetSpotLightBufferCenterAndRadiusSRVParam(nLightingMode) );
            pd3dImmediateContext->VSSetShaderResources( 6, 1, GetSpotLightBufferColorSRVParam(nLightingMode) );
            pd3dImmediateContext->VSSetShaderResources( 7, 1, GetSpotLightBufferSpotParamsSRVParam(nLightingMode) );
            pd3dImmediateContext->VSSetShaderResources( 12, 1, GetSpotLightBufferSpotMatricesSRVParam(nLightingMode) );
            pd3dImmediateContext->PSSetShader( m_pDebugDrawSpotLightsPS, NULL, 0 );
            pd3dImmediateContext->PSSetShaderResources( 0, 1, &pNULLSRV );
            pd3dImmediateContext->PSSetShaderResources( 1, 1, &pNULLSRV );
            pd3dImmediateContext->PSSetSamplers( 0, 1, &pNULLSampler );

            pd3dImmediateContext->DrawIndexedInstanced(g_nConeNumIndices,uNumSpotLights,0,0,0);

            // restore to default
            pd3dImmediateContext->VSSetShaderResources( 5, 1, &pNULLSRV );
            pd3dImmediateContext->VSSetShaderResources( 6, 1, &pNULLSRV );
            pd3dImmediateContext->VSSetShaderResources( 7, 1, &pNULLSRV );
            pd3dImmediateContext->VSSetShaderResources( 12, 1, &pNULLSRV );
        }

        // restore to default
        pd3dImmediateContext->RSSetState( NULL );

        // restore to previous
        pd3dImmediateContext->OMSetDepthStencilState( pDepthStencilStateStored11, uStencilRefStored11 );
        pd3dImmediateContext->OMSetBlendState( pBlendStateStored11, afBlendFactorStored11, uSampleMaskStored11 );
        SAFE_RELEASE( pDepthStencilStateStored11 );
        SAFE_RELEASE( pBlendStateStored11 );

    }

    //--------------------------------------------------------------------------------------
    // Fill in the data for the lights (center, radius, and color).
    // Also fill in the vertex data for the sprite quad.
    //--------------------------------------------------------------------------------------
    void LightUtil::InitLights( const XMVECTOR &BBoxMin, const XMVECTOR &BBoxMax )
    {
        // init the random seed to 1, so that results are deterministic 
        // across different runs of the sample
        srand(1);

        // scale the size of the lights based on the size of the scene
        XMVECTOR BBoxExtents = 0.5f * (BBoxMax - BBoxMin);
        float fRadius = 0.075f * XMVectorGetX(XMVector3Length(BBoxExtents));

        // For point lights, the radius of the bounding sphere for the light (used for culling) 
        // and the falloff distance of the light (used for lighting) are the same. Not so for 
        // spot lights. A spot light is a right circular cone. The height of the cone is the 
        // falloff distance. We want to fit the cone of the spot light inside the bounding sphere. 
        // From calculus, we know the cone with maximum volume that can fit inside a sphere has height:
        // h_cone = (4/3)*r_sphere
        float fSpotLightFalloffRadius = 1.333333333333f * fRadius;

        XMFLOAT3 vBBoxMin, vBBoxMax;
        XMStoreFloat3( &vBBoxMin, BBoxMin );
        XMStoreFloat3( &vBBoxMax, BBoxMax );

        // initialize the point light data
        for (int i = 0; i < MAX_NUM_LIGHTS; i++)
        {
            g_PointLightDataArrayCenterAndRadius[i] = XMFLOAT4(GetRandFloat(vBBoxMin.x,vBBoxMax.x), GetRandFloat(vBBoxMin.y,vBBoxMax.y), GetRandFloat(vBBoxMin.z,vBBoxMax.z), fRadius);
            g_PointLightDataArrayColor[i] = GetRandColor();
        }

        // initialize the spot light data
        for (int i = 0; i < MAX_NUM_LIGHTS; i++)
        {
            g_SpotLightDataArrayCenterAndRadius[i] = XMFLOAT4(GetRandFloat(vBBoxMin.x,vBBoxMax.x), GetRandFloat(vBBoxMin.y,vBBoxMax.y), GetRandFloat(vBBoxMin.z,vBBoxMax.z), fRadius);
            g_SpotLightDataArrayColor[i] = GetRandColor();

            XMFLOAT3 vLightDir = GetRandLightDirection();

            // Okay, so we fit a max-volume cone inside our bounding sphere for the spot light. We need to find 
            // the cone angle for that cone. Google on "cone inside sphere" (without the quotes) to find info 
            // on how to derive these formulas for the height and radius of the max-volume cone inside a sphere.
            // h_cone = (4/3)*r_sphere
            // r_cone = sqrt(8/9)*r_sphere
            // tan(theta) = r_cone/h_cone = sqrt(2)/2 = 0.7071067811865475244
            // theta = 35.26438968 degrees
            // store the cosine of this angle: cosine(35.26438968 degrees) = 0.816496580927726

            // random direction, cosine of cone angle, falloff radius calcuated above
            g_SpotLightDataArraySpotParams[i] = PackSpotParams(vLightDir, 0.816496580927726f, fSpotLightFalloffRadius);

            // build a "rotate from one vector to another" matrix, to point the spot light 
            // cone along its light direction
            XMVECTOR s = XMVectorSet(0.0f,-1.0f,0.0f,0.0f);
            XMVECTOR t = XMLoadFloat3( &vLightDir );
            XMFLOAT3 v;
            XMStoreFloat3( &v, XMVector3Cross(s,t) );
            float e = XMVectorGetX(XMVector3Dot(s,t));
            float h = 1.0f / (1.0f + e);

            XMFLOAT4X4 f4x4Rotation;
            XMStoreFloat4x4( &f4x4Rotation, XMMatrixIdentity() );
            f4x4Rotation._11 = e + h*v.x*v.x;
            f4x4Rotation._12 = h*v.x*v.y - v.z;
            f4x4Rotation._13 = h*v.x*v.z + v.y;
            f4x4Rotation._21 = h*v.x*v.y + v.z;
            f4x4Rotation._22 = e + h*v.y*v.y;
            f4x4Rotation._23 = h*v.y*v.z - v.x;
            f4x4Rotation._31 = h*v.x*v.z - v.y;
            f4x4Rotation._32 = h*v.y*v.z + v.x;
            f4x4Rotation._33 = e + h*v.z*v.z;
            XMMATRIX mRotation = XMLoadFloat4x4( &f4x4Rotation );

            g_SpotLightDataArraySpotMatrices[i] = XMMatrixTranspose(mRotation);
        }

        // initialize the shadow-casting point light data
        {
            // Hanging lamps
            AddShadowCastingPointLight( XMFLOAT4( -620.0f, 136.0f, 218.0f, 450.0f ), COLOR( 200, 100, 0 ) );
            AddShadowCastingPointLight( XMFLOAT4( -620.0f, 136.0f, -140.0f, 450.0f ), COLOR( 200, 100, 0 ) );
            AddShadowCastingPointLight( XMFLOAT4(  490.0f, 136.0f, 218.0f, 450.0f ), COLOR( 200, 100, 0 ) );
            AddShadowCastingPointLight( XMFLOAT4(  490.0f, 136.0f, -140.0f, 450.0f ), COLOR( 200, 100, 0 ) );

            // Corner lights
            AddShadowCastingPointLight( XMFLOAT4( -1280.0f, 120.0f, -300.0f, 500.0f ), COLOR( 120, 60, 60 ) );
            AddShadowCastingPointLight( XMFLOAT4( -1280.0f, 200.0f,  430.0f, 600.0f ), COLOR( 50, 50, 128 ) );
            AddShadowCastingPointLight( XMFLOAT4( 1030.0f, 200.0f, 545.0f, 500.0f ), COLOR( 255, 128, 0 ) );
            AddShadowCastingPointLight( XMFLOAT4( 1180.0f, 220.0f, -390.0f, 500.0f ), COLOR( 100, 100, 255 ) );

            // Midpoint lights
            AddShadowCastingPointLight( XMFLOAT4( -65.0f, 100.0f, 220.0f, 500.0f ), COLOR( 200, 200, 200 ) );
            AddShadowCastingPointLight( XMFLOAT4( -65.0f, 100.0f,-140.0f, 500.0f ), COLOR( 200, 200, 200 ) );

            // High gallery lights
            AddShadowCastingPointLight( XMFLOAT4( 600.0f, 660.0f, -30.0f, 800.0f ), COLOR( 100, 100, 100 ) );
            AddShadowCastingPointLight( XMFLOAT4( -700.0f, 660.0f, 80.0f, 800.0f ), COLOR( 100, 100, 100 ) );
        }

        {
            // Curtain spot
            AddShadowCastingSpotLight( XMFLOAT4(  -772.0f, 254.0f, -503.0f, 800.0f ), XMFLOAT3( -814.0f, 180.0f, -250.0f ), COLOR( 255, 255, 255 ) );

            // Lion spots
            AddShadowCastingSpotLight( XMFLOAT4(  1130.0f, 378.0f, 40.0f, 500.0f ), XMFLOAT3( 1150.0f, 290.0f, 40.0f ), COLOR( 200, 200, 100 ) );
            AddShadowCastingSpotLight( XMFLOAT4( -1260.0f, 378.0f, 40.0f, 500.0f ), XMFLOAT3( -1280.0f, 290.0f, 40.0f ), COLOR( 200, 200, 100 ) );

            // Gallery spots
            AddShadowCastingSpotLight( XMFLOAT4( -115.0f, 660.0f, -100.0f, 800.0f ), XMFLOAT3( -115.0f, 630.0f, 0.0f ), COLOR( 200, 200, 200 ) );
            AddShadowCastingSpotLight( XMFLOAT4( -115.0f, 660.0f,  100.0f, 800.0f ), XMFLOAT3( -115.0f, 630.0f, -100.0f ), COLOR( 200, 200, 200 ) );

            AddShadowCastingSpotLight( XMFLOAT4( -770.0f, 660.0f, -100.0f, 800.0f ), XMFLOAT3( -770.0f, 630.0f, 0.0f ), COLOR( 200, 200, 200 ) );
            AddShadowCastingSpotLight( XMFLOAT4( -770.0f, 660.0f,  100.0f, 800.0f ), XMFLOAT3( -770.0f, 630.0f, -100.0f ), COLOR( 200, 200, 200 ) );

            AddShadowCastingSpotLight( XMFLOAT4( 500.0f, 660.0f, -100.0f, 800.0f ), XMFLOAT3( 500.0f, 630.0f, 0.0f ), COLOR( 200, 200, 200 ) );
            AddShadowCastingSpotLight( XMFLOAT4( 500.0f, 660.0f,  100.0f, 800.0f ), XMFLOAT3( 500.0f, 630.0f, -100.0f ), COLOR( 200, 200, 200 ) );

            // Red corner spots
            AddShadowCastingSpotLight( XMFLOAT4( -1240.0f, 90.0f, -70.0f, 700.0f ), XMFLOAT3( -1240.0f, 140.0f, -405.0f ), COLOR( 200, 0, 0 ) );
            AddShadowCastingSpotLight( XMFLOAT4( -1000.0f, 90.0f, -260.0f, 700.0f ), XMFLOAT3( -1240.0f, 140.0f, -405.0f ), COLOR( 200, 0, 0 ) );

            // Green corner spot
            AddShadowCastingSpotLight( XMFLOAT4( -900.0f, 60.0f, 340.0f, 700.0f ), XMFLOAT3( -1360.0f, 255.0f, 555.0f ), COLOR( 100, 200, 100 ) );
        }

        // initialize the vertex buffer data for a quad (for drawing the lights)
        float fQuadHalfSize = 0.083f * fRadius;
        g_QuadForLightsVertexData[0].v3Pos = XMFLOAT3(-fQuadHalfSize, -fQuadHalfSize, 0.0f );
        g_QuadForLightsVertexData[0].v2TexCoord = XMFLOAT2( 0.0f, 0.0f );
        g_QuadForLightsVertexData[1].v3Pos = XMFLOAT3(-fQuadHalfSize,  fQuadHalfSize, 0.0f );
        g_QuadForLightsVertexData[1].v2TexCoord = XMFLOAT2( 0.0f, 1.0f );
        g_QuadForLightsVertexData[2].v3Pos = XMFLOAT3( fQuadHalfSize, -fQuadHalfSize, 0.0f );
        g_QuadForLightsVertexData[2].v2TexCoord = XMFLOAT2( 1.0f, 0.0f );
        g_QuadForLightsVertexData[3].v3Pos = XMFLOAT3(-fQuadHalfSize,  fQuadHalfSize, 0.0f );
        g_QuadForLightsVertexData[3].v2TexCoord = XMFLOAT2( 0.0f, 1.0f );
        g_QuadForLightsVertexData[4].v3Pos = XMFLOAT3( fQuadHalfSize,  fQuadHalfSize, 0.0f );
        g_QuadForLightsVertexData[4].v2TexCoord = XMFLOAT2( 1.0f, 1.0f );
        g_QuadForLightsVertexData[5].v3Pos = XMFLOAT3( fQuadHalfSize, -fQuadHalfSize, 0.0f );
        g_QuadForLightsVertexData[5].v2TexCoord = XMFLOAT2( 1.0f, 0.0f );

        // initialize the vertex and index buffer data for a cone (for drawing the spot lights)
        {
            // h_cone = (4/3)*r_sphere
            // r_cone = sqrt(8/9)*r_sphere
            float fConeSphereRadius = 0.033f * fRadius;
            float fConeHeight = 1.333333333333f * fConeSphereRadius;
            float fConeRadius = 0.942809041582f * fConeSphereRadius;

            for (int i = 0; i < g_nConeNumTris; i++)
            {
                // We want to calculate points along the circle at the end of the cone.
                // The parametric equations for this circle are:
                // x=r_cone*cosine(t)
                // z=r_cone*sine(t)
                float t = ((float)i / (float)g_nConeNumTris) * TWO_PI;
                g_ConeForSpotLightsVertexData[2*i+1].v3Pos = XMFLOAT3( fConeRadius*cos(t), -fConeHeight, fConeRadius*sin(t) );
                g_ConeForSpotLightsVertexData[2*i+1].v2TexCoord = XMFLOAT2( 0.0f, 1.0f );

                // normal = (h_cone*cosine(t), r_cone, h_cone*sine(t))
                XMFLOAT3 vNormal = XMFLOAT3( fConeHeight*cos(t), fConeRadius, fConeHeight*sin(t) );
                XMStoreFloat3( &vNormal, XMVector3Normalize( XMLoadFloat3( &vNormal ) ) );
                g_ConeForSpotLightsVertexData[2*i+1].v3Norm = vNormal;
#ifdef _DEBUG
                // check that the normal is actually perpendicular
                float dot = XMVectorGetX( XMVector3Dot( XMLoadFloat3( &g_ConeForSpotLightsVertexData[2*i+1].v3Pos ), XMLoadFloat3( &vNormal ) ) );
                assert( abs(dot) < 0.001f );
#endif
            }

            // create duplicate points for the top of the cone, each with its own normal
            for (int i = 0; i < g_nConeNumTris; i++)
            {
                g_ConeForSpotLightsVertexData[2*i].v3Pos = XMFLOAT3( 0.0f, 0.0f, 0.0f );
                g_ConeForSpotLightsVertexData[2*i].v2TexCoord = XMFLOAT2( 0.0f, 0.0f );

                XMFLOAT3 vNormal;
                XMVECTOR Normal = XMLoadFloat3(&g_ConeForSpotLightsVertexData[2*i+1].v3Norm) + XMLoadFloat3(&g_ConeForSpotLightsVertexData[2*i+3].v3Norm);
                XMStoreFloat3( &vNormal, XMVector3Normalize( Normal ) );
                g_ConeForSpotLightsVertexData[2*i].v3Norm = vNormal;
            }

            // fill in the index buffer for the cone
            for (int i = 0; i < g_nConeNumTris; i++)
            {
                g_ConeForSpotLightsIndexData[3*i+0] = (unsigned short)(2*i);
                g_ConeForSpotLightsIndexData[3*i+1] = (unsigned short)(2*i+3);
                g_ConeForSpotLightsIndexData[3*i+2] = (unsigned short)(2*i+1);
            }

            // fix up the last triangle
            g_ConeForSpotLightsIndexData[3*g_nConeNumTris-2] = 1;
        }
    }

    //--------------------------------------------------------------------------------------
    // Return a pointer to the beginning of the transposed viewProj array
    // Note: returning a 2D array as XMMATRIX*[6], please forgive this ugly syntax
    //--------------------------------------------------------------------------------------
    const XMMATRIX (*LightUtil::GetShadowCastingPointLightViewProjTransposedArray())[6]
    {
        return g_ShadowCastingPointLightViewProjTransposed;
    }

    //--------------------------------------------------------------------------------------
    // Return a pointer to the beginning of the transposed viewProjInv array
    // Note: returning a 2D array as XMMATRIX*[6], please forgive this ugly syntax
    //--------------------------------------------------------------------------------------
    const XMMATRIX (*LightUtil::GetShadowCastingPointLightViewProjInvTransposedArray())[6]
    {
        return g_ShadowCastingPointLightViewProjInvTransposed;
    }

    //--------------------------------------------------------------------------------------
    // Return a pointer to the beginning of the transposed viewProj array
    //--------------------------------------------------------------------------------------
    const XMMATRIX* LightUtil::GetShadowCastingSpotLightViewProjTransposedArray()
    {
        return g_ShadowCastingSpotLightViewProjTransposed;
    }

    //--------------------------------------------------------------------------------------
    // Return a pointer to the beginning of the transposed viewProjInv array
    //--------------------------------------------------------------------------------------
    const XMMATRIX* LightUtil::GetShadowCastingSpotLightViewProjInvTransposedArray()
    {
        return g_ShadowCastingSpotLightViewProjInvTransposed;
    }

    //--------------------------------------------------------------------------------------
    // Add shaders to the shader cache
    //--------------------------------------------------------------------------------------
    void LightUtil::AddShadersToCache( AMD::ShaderCache *pShaderCache )
    {
        // Ensure all shaders (and input layouts) are released
        SAFE_RELEASE( m_pDebugDrawPointLightsVS );
        SAFE_RELEASE( m_pDebugDrawPointLightsPS );
        SAFE_RELEASE( m_pDebugDrawPointLightsLayout11 );
        SAFE_RELEASE( m_pDebugDrawSpotLightsVS );
        SAFE_RELEASE( m_pDebugDrawSpotLightsPS );
        SAFE_RELEASE( m_pDebugDrawSpotLightsLayout11 );

        const D3D11_INPUT_ELEMENT_DESC LayoutForSprites[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        const D3D11_INPUT_ELEMENT_DESC LayoutForCone[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawPointLightsVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"DebugDrawPointLightsVS",
            L"DebugDraw.hlsl", 0, NULL, &m_pDebugDrawPointLightsLayout11, LayoutForSprites, ARRAYSIZE( LayoutForSprites ) );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawPointLightsPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawPointLightsPS",
            L"DebugDraw.hlsl", 0, NULL, NULL, NULL, 0 );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawSpotLightsVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"DebugDrawSpotLightsVS",
            L"DebugDraw.hlsl", 0, NULL, &m_pDebugDrawSpotLightsLayout11, LayoutForCone, ARRAYSIZE( LayoutForCone ) );

        pShaderCache->AddShader( (ID3D11DeviceChild**)&m_pDebugDrawSpotLightsPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawSpotLightsPS",
            L"DebugDraw.hlsl", 0, NULL, NULL, NULL, 0 );
    }

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
