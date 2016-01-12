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
// File: CommonHeader.h
//
// HLSL file for the TiledLighting11 sample. Common header file for all shaders.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Miscellaneous constants
//--------------------------------------------------------------------------------------
static const float4 kRadarColors[14] = 
{
    {0,0.9255,0.9255,1},   // cyan
    {0,0.62745,0.9647,1},  // light blue
    {0,0,0.9647,1},        // blue
    {0,1,0,1},             // bright green
    {0,0.7843,0,1},        // green
    {0,0.5647,0,1},        // dark green
    {1,1,0,1},             // yellow
    {0.90588,0.75294,0,1}, // yellow-orange
    {1,0.5647,0,1},        // orange
    {1,0,0,1},             // bright red
    {0.8392,0,0,1},        // red
    {0.75294,0,0,1},       // dark red
    {1,0,1,1},             // magenta
    {0.6,0.3333,0.7882,1}, // purple
};

//--------------------------------------------------------------------------------------
// Light culling constants.
// These must match their counterparts in CommonUtil.h
//--------------------------------------------------------------------------------------
#define TILE_RES 16
#define MAX_NUM_LIGHTS_PER_TILE 272
#define MAX_NUM_VPLS_PER_TILE 1024

//--------------------------------------------------------------------------------------
// Shadow constants.
// These must match their counterparts in CommonConstants.h
//--------------------------------------------------------------------------------------
#define MAX_NUM_SHADOWCASTING_POINTS     12
#define MAX_NUM_SHADOWCASTING_SPOTS      12

//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
cbuffer cbPerObject : register( b0 )
{
    matrix  g_mWorld                   : packoffset( c0 );
}

cbuffer cbPerCamera : register( b1 )
{
    matrix  g_mViewProjection          : packoffset( c0 );
};

cbuffer cbPerFrame : register( b2 )
{
    matrix              g_mView                      : packoffset( c0 );
    matrix              g_mProjection                : packoffset( c4 );
    matrix              g_mProjectionInv             : packoffset( c8 );
    matrix              g_mViewProjectionInvViewport : packoffset( c12 );
    float4              g_AmbientColorUp             : packoffset( c16 );
    float4              g_AmbientColorDown           : packoffset( c17 );
    float3              g_vCameraPos                 : packoffset( c18 );
    float               g_fAlphaTest                 : packoffset( c18.w );
    uint                g_uNumLights                 : packoffset( c19 );
    uint                g_uNumSpotLights             : packoffset( c19.y );
    uint                g_uWindowWidth               : packoffset( c19.z );
    uint                g_uWindowHeight              : packoffset( c19.w );
    uint                g_uMaxNumLightsPerTile       : packoffset( c20 );
    uint                g_uMaxNumElementsPerTile     : packoffset( c20.y );
    uint                g_uNumTilesX                 : packoffset( c20.z );
    uint                g_uNumTilesY                 : packoffset( c20.w );
    uint                g_uMaxVPLs                   : packoffset( c21 );
    uint                g_uMaxNumVPLsPerTile         : packoffset( c21.y );
    uint                g_uMaxNumVPLElementsPerTile  : packoffset( c21.z );
    float               g_fVPLSpotStrength           : packoffset( c21.w );
    float               g_fVPLSpotRadius             : packoffset( c22 );
    float               g_fVPLPointStrength          : packoffset( c22.y );
    float               g_fVPLPointRadius            : packoffset( c22.z );
    float               g_fVPLRemoveBackFaceContrib  : packoffset( c22.w );
    float               g_fVPLColorThreshold         : packoffset( c23 );
    float               g_fVPLBrightnessThreshold    : packoffset( c23.y );
    float               g_fPerFramePad1              : packoffset( c23.z );
    float               g_fPerFramePad2              : packoffset( c23.w );
};

cbuffer cbShadowConstants : register( b3 )
{
    matrix              g_mPointShadowViewProj[ MAX_NUM_SHADOWCASTING_POINTS ][ 6 ];
    matrix              g_mSpotShadowViewProj[ MAX_NUM_SHADOWCASTING_SPOTS ];
    float4              g_ShadowBias;  // X: Texel offset from edge, Y: Underscan scale, Z: Z bias for points, W: Z bias for spots
};

cbuffer cbVPLConstants : register( b4 )
{
    uint                g_uNumVPLs;
    uint                g_uVPLPad[ 3 ];
};

struct VPLData
{
    float4 Direction;
    float4 Color;
    float4 SourceLightDirection;
};

//-----------------------------------------------------------------------------------------
// Samplers
//-----------------------------------------------------------------------------------------
SamplerState           g_Sampler       : register( s0 );
SamplerComparisonState g_ShadowSampler : register( s1 );


//-----------------------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------------------

// convert a point from post-projection space into view space
float4 ConvertProjToView( float4 p )
{
    p = mul( p, g_mProjectionInv );
    p /= p.w;
    return p;
}

// convert a depth value from post-projection space into view space
float ConvertProjDepthToView( float z )
{
    z = 1.f / (z*g_mProjectionInv._34 + g_mProjectionInv._44);
    return z;
}

uint GetTileIndex(float2 ScreenPos)
{
    float fTileRes = (float)TILE_RES;
    uint nTileIdx = floor(ScreenPos.x/fTileRes)+floor(ScreenPos.y/fTileRes)*g_uNumTilesX;
    return nTileIdx;
}

// PerTileLightIndexBuffer layout:
// | HalfZ High Bits | HalfZ Low Bits | Light Count List A | Light Count List B | space for max num lights per tile light indices (List A) | space for max num lights per tile light indices (List B) |
void GetLightListInfo(in Buffer<uint> PerTileLightIndexBuffer, in uint uMaxNumLightsPerTile, in uint uMaxNumElementsPerTile, in float4 SVPosition, out uint uFirstLightIndex, out uint uNumLights)
{
    uint nTileIndex = GetTileIndex(SVPosition.xy);
    uint nStartIndex = uMaxNumElementsPerTile*nTileIndex;

    // reconstruct fHalfZ
    uint uHalfZBitsHigh = PerTileLightIndexBuffer[nStartIndex];
    uint uHalfZBitsLow = PerTileLightIndexBuffer[nStartIndex+1];
    uint uHalfZBits = (uHalfZBitsHigh << 16) | uHalfZBitsLow;
    float fHalfZ = asfloat(uHalfZBits);

    float fViewPosZ = ConvertProjDepthToView( SVPosition.z );

    uFirstLightIndex = (fViewPosZ < fHalfZ) ? (nStartIndex + 4) : (nStartIndex + 4 + uMaxNumLightsPerTile);
    uNumLights = (fViewPosZ < fHalfZ) ? PerTileLightIndexBuffer[nStartIndex+2] : PerTileLightIndexBuffer[nStartIndex+3];
}

float4 ConvertNumberOfLightsToGrayscale(uint nNumLightsInThisTile, uint uMaxNumLightsPerTile)
{
    float fPercentOfMax = (float)nNumLightsInThisTile / (float)uMaxNumLightsPerTile;
    return float4(fPercentOfMax, fPercentOfMax, fPercentOfMax, 1.0f);
}

float4 ConvertNumberOfLightsToRadarColor(uint nNumLightsInThisTile, uint uMaxNumLightsPerTile)
{
    // black for no lights
    if( nNumLightsInThisTile == 0 ) return float4(0,0,0,1);
    // light purple for reaching the max
    else if( nNumLightsInThisTile == uMaxNumLightsPerTile ) return float4(0.847,0.745,0.921,1);
    // white for going over the max
    else if ( nNumLightsInThisTile > uMaxNumLightsPerTile ) return float4(1,1,1,1);
    // else use weather radar colors
    else
    {
        // use a log scale to provide more detail when the number of lights is smaller

        // want to find the base b such that the logb of uMaxNumLightsPerTile is 14
        // (because we have 14 radar colors)
        float fLogBase = exp2(0.07142857f*log2((float)uMaxNumLightsPerTile));

        // change of base
        // logb(x) = log2(x) / log2(b)
        uint nColorIndex = floor(log2((float)nNumLightsInThisTile) / log2(fLogBase));
        return kRadarColors[nColorIndex];
    }
}
