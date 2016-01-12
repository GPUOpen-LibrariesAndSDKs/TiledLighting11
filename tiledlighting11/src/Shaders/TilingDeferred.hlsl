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
// File: TilingDeferred.hlsl
//
// HLSL file for the TiledLighting11 sample. Tiled light culling.
//--------------------------------------------------------------------------------------


#include "CommonHeader.h"
#include "TilingCommonHeader.h"
#include "LightingCommonHeader.h"

//-----------------------------------------------------------------------------------------
// Textures and Buffers
//-----------------------------------------------------------------------------------------
Buffer<float4> g_PointLightBufferColor           : register( t4 );
Buffer<float4> g_SpotLightBufferColor            : register( t5 );
Buffer<float4> g_SpotLightBufferSpotParams       : register( t6 );

#if ( VPLS_ENABLED == 1 )
StructuredBuffer<VPLData> g_VPLBufferData        : register( t7 );
#endif

#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
Texture2D<float4> g_GBuffer0Texture : register( t8 );
Texture2D<float4> g_GBuffer1Texture : register( t9 );
#if ( NUM_GBUFFER_RTS >= 3 )
Texture2D<float4> g_GBuffer2Texture : register( t10 );
#endif
#if ( NUM_GBUFFER_RTS >= 4 )
Texture2D<float4> g_GBuffer3Texture : register( t11 );
#endif
#if ( NUM_GBUFFER_RTS >= 5 )
Texture2D<float4> g_GBuffer4Texture : register( t12 );
#endif

#else                           // MSAA
Texture2DMS<float4,NUM_MSAA_SAMPLES> g_GBuffer0Texture : register( t8 );
Texture2DMS<float4,NUM_MSAA_SAMPLES> g_GBuffer1Texture : register( t9 );
#if ( NUM_GBUFFER_RTS >= 3 )
Texture2DMS<float4,NUM_MSAA_SAMPLES> g_GBuffer2Texture : register( t10 );
#endif
#if ( NUM_GBUFFER_RTS >= 4 )
Texture2DMS<float4,NUM_MSAA_SAMPLES> g_GBuffer3Texture : register( t11 );
#endif
#if ( NUM_GBUFFER_RTS >= 5 )
Texture2DMS<float4,NUM_MSAA_SAMPLES> g_GBuffer4Texture : register( t12 );
#endif

#endif  // ( NUM_MSAA_SAMPLES <= 1 )

RWTexture2D<float4> g_OffScreenBufferOut : register( u0 );

//-----------------------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------------------

#if ( NUM_MSAA_SAMPLES > 1 )   // MSAA
float3 DoLightingForMSAA( uint2 globalIdx, uint sampleIdx, float fHalfZ )
{
    // get the surface normal from the G-Buffer
    float4 vNormAndSpecMask = g_GBuffer1Texture.Load( globalIdx, sampleIdx );
    float3 vNorm = vNormAndSpecMask.xyz;
    vNorm *= 2;
    vNorm -= float3(1,1,1);

    // convert depth and screen position to world-space position
    float fDepthBufferDepth = g_DepthTexture.Load( globalIdx, sampleIdx ).x;
    float4 vWorldSpacePosition = mul(float4((float)globalIdx.x+0.5, (float)globalIdx.y+0.5, fDepthBufferDepth, 1.0), g_mViewProjectionInvViewport);
    float3 vPositionWS = vWorldSpacePosition.xyz / vWorldSpacePosition.w;

    float3 vViewDir = normalize( g_vCameraPos - vPositionWS );

    float3 AccumDiffuse = float3(0,0,0);
    float3 AccumSpecular = float3(0,0,0);

    float fViewPosZ = ConvertProjDepthToView( fDepthBufferDepth );

    // loop over the point lights that intersect this tile
    {
        uint uStartIdx = (fViewPosZ < fHalfZ) ? 0 : MAX_NUM_LIGHTS_PER_TILE;
        uint uEndIdx = (fViewPosZ < fHalfZ) ? ldsLightIdxCounterA : ldsLightIdxCounterB;

        for(uint i=uStartIdx; i<uEndIdx; i++)
        {
            uint nLightIndex = ldsLightIdx[i];

            float3 LightColorDiffuseResult;
            float3 LightColorSpecularResult;
#if ( SHADOWS_ENABLED == 1 )
            DoLighting(true, g_PointLightBufferCenterAndRadius, g_PointLightBufferColor, nLightIndex, vPositionWS, vNorm, vViewDir, LightColorDiffuseResult, LightColorSpecularResult);
#else
            DoLighting(false, g_PointLightBufferCenterAndRadius, g_PointLightBufferColor, nLightIndex, vPositionWS, vNorm, vViewDir, LightColorDiffuseResult, LightColorSpecularResult);
#endif

            AccumDiffuse += LightColorDiffuseResult;
            AccumSpecular += LightColorSpecularResult;
        }
    }

    // loop over the spot lights that intersect this tile
    {
        uint uStartIdx = (fViewPosZ < fHalfZ) ? 0 : MAX_NUM_LIGHTS_PER_TILE;
        uint uEndIdx = (fViewPosZ < fHalfZ) ? ldsSpotIdxCounterA : ldsSpotIdxCounterB;

        for(uint i=uStartIdx; i<uEndIdx; i++)
        {
            uint nLightIndex = ldsSpotIdx[i];

            float3 LightColorDiffuseResult;
            float3 LightColorSpecularResult;
#if ( SHADOWS_ENABLED == 1 )
            DoSpotLighting(true, g_SpotLightBufferCenterAndRadius, g_SpotLightBufferColor, g_SpotLightBufferSpotParams, nLightIndex, vPositionWS, vNorm, vViewDir, LightColorDiffuseResult, LightColorSpecularResult);
#else
            DoSpotLighting(false, g_SpotLightBufferCenterAndRadius, g_SpotLightBufferColor, g_SpotLightBufferSpotParams, nLightIndex, vPositionWS, vNorm, vViewDir, LightColorDiffuseResult, LightColorSpecularResult);
#endif

            AccumDiffuse += LightColorDiffuseResult;
            AccumSpecular += LightColorSpecularResult;
        }
    }

#if ( VPLS_ENABLED == 1 )
    // loop over the VPLs that intersect this tile
    {
        uint uStartIdx = (fViewPosZ < fHalfZ) ? 0 : MAX_NUM_VPLS_PER_TILE;
        uint uEndIdx = (fViewPosZ < fHalfZ) ? ldsVPLIdxCounterA : ldsVPLIdxCounterB;

        for(uint i=uStartIdx; i<uEndIdx; i++)
        {
            uint nLightIndex = ldsVPLIdx[i];

            float3 LightColorDiffuseResult;
            DoVPLLighting(g_VPLBufferCenterAndRadius, g_VPLBufferData, nLightIndex, vPositionWS, vNorm, LightColorDiffuseResult);

            AccumDiffuse += LightColorDiffuseResult;
        }
    }
#endif

    // pump up the lights
    AccumDiffuse *= 2;
    AccumSpecular *= 8;

    // read dummy data to consume more bandwidth, 
    // for performance testing
#if ( NUM_GBUFFER_RTS >= 3 )
    float4 Dummy0 = g_GBuffer2Texture.Load( globalIdx, sampleIdx );
    AccumDiffuse *= Dummy0.xyz;
    AccumSpecular *= Dummy0.xyz;
#endif
#if ( NUM_GBUFFER_RTS >= 4 )
    float4 Dummy1 = g_GBuffer3Texture.Load( globalIdx, sampleIdx );
    AccumDiffuse *= Dummy1.xyz;
    AccumSpecular *= Dummy1.xyz;
#endif
#if ( NUM_GBUFFER_RTS >= 5 )
    float4 Dummy2 = g_GBuffer4Texture.Load( globalIdx, sampleIdx );
    AccumDiffuse *= Dummy2.xyz;
    AccumSpecular *= Dummy2.xyz;
#endif

    // This is a poor man's ambient cubemap (blend between an up color and a down color)
    float fAmbientBlend = 0.5f * vNorm.y + 0.5;
    float3 Ambient = g_AmbientColorUp.rgb * fAmbientBlend + g_AmbientColorDown.rgb * (1-fAmbientBlend);
    float3 DiffuseAndAmbient = AccumDiffuse + Ambient;

    // modulate mesh texture with lighting
    float3 DiffuseTex = g_GBuffer0Texture.Load( globalIdx, sampleIdx ).rgb;
    float fSpecMask = vNormAndSpecMask.a;

    float3 Result = DiffuseTex*(DiffuseAndAmbient + AccumSpecular*fSpecMask);

    // override result when one of the lights-per-tile visualization modes is enabled
#if ( LIGHTS_PER_TILE_MODE > 0 )
    uint uStartIdx = (fViewPosZ < fHalfZ) ? 0 : MAX_NUM_LIGHTS_PER_TILE;
    uint uEndIdx = (fViewPosZ < fHalfZ) ? ldsLightIdxCounterA : ldsLightIdxCounterB;
    uint nNumLightsInThisTile = uEndIdx-uStartIdx;
    uEndIdx = (fViewPosZ < fHalfZ) ? ldsSpotIdxCounterA : ldsSpotIdxCounterB;
    nNumLightsInThisTile += uEndIdx-uStartIdx;
    uint uMaxNumLightsPerTile = 2*g_uMaxNumLightsPerTile;  // max for points plus max for spots
#if ( VPLS_ENABLED == 1 )
    uStartIdx = (fViewPosZ < fHalfZ) ? 0 : MAX_NUM_VPLS_PER_TILE;
    uEndIdx = (fViewPosZ < fHalfZ) ? ldsVPLIdxCounterA : ldsVPLIdxCounterB;
    nNumLightsInThisTile += uEndIdx-uStartIdx;
    uMaxNumLightsPerTile += g_uMaxNumVPLsPerTile;
#endif
#if ( LIGHTS_PER_TILE_MODE == 1 )
    Result = ConvertNumberOfLightsToGrayscale(nNumLightsInThisTile, uMaxNumLightsPerTile).rgb;
#elif ( LIGHTS_PER_TILE_MODE == 2 )
    Result = ConvertNumberOfLightsToRadarColor(nNumLightsInThisTile, uMaxNumLightsPerTile).rgb;
#endif
#endif

    return Result;
}
#endif

//-----------------------------------------------------------------------------------------
// Light culling shader
//-----------------------------------------------------------------------------------------
[numthreads(NUM_THREADS_X, NUM_THREADS_Y, 1)]
void CullLightsAndDoLightingCS( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    uint localIdxFlattened = localIdx.x + localIdx.y*NUM_THREADS_X;

    // after calling DoLightCulling, the per-tile list of light indices that intersect this tile 
    // will be in ldsLightIdx, and the number of lights that intersect this tile 
    // will be in ldsLightIdxCounterA and ldsLightIdxCounterB
    float fHalfZ;
#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
    DoLightCulling( globalIdx, localIdxFlattened, groupIdx, fHalfZ );
#else                           // MSAA
    bool bIsEdge = DoLightCulling( globalIdx, localIdxFlattened, groupIdx, fHalfZ );
#endif

    // get the surface normal from the G-Buffer
#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
    float4 vNormAndSpecMask = g_GBuffer1Texture.Load( uint3(globalIdx.x,globalIdx.y,0) );
#else                           // MSAA
    float4 vNormAndSpecMask = g_GBuffer1Texture.Load( uint2(globalIdx.x,globalIdx.y), 0 );
#endif
    float3 vNorm = vNormAndSpecMask.xyz;
    vNorm *= 2;
    vNorm -= float3(1,1,1);

    // convert depth and screen position to world-space position
#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
    float fDepthBufferDepth = g_DepthTexture.Load( uint3(globalIdx.x,globalIdx.y,0) ).x;
#else                           // MSAA
    float fDepthBufferDepth = g_DepthTexture.Load( uint2(globalIdx.x,globalIdx.y), 0 ).x;
#endif
    float4 vWorldSpacePosition = mul(float4((float)globalIdx.x+0.5, (float)globalIdx.y+0.5, fDepthBufferDepth, 1.0), g_mViewProjectionInvViewport);
    float3 vPositionWS = vWorldSpacePosition.xyz / vWorldSpacePosition.w;

    float3 vViewDir = normalize( g_vCameraPos - vPositionWS );

    float3 AccumDiffuse = float3(0,0,0);
    float3 AccumSpecular = float3(0,0,0);

    float fViewPosZ = ConvertProjDepthToView( fDepthBufferDepth );

    // loop over the point lights that intersect this tile
    {
        uint uStartIdx = (fViewPosZ < fHalfZ) ? 0 : MAX_NUM_LIGHTS_PER_TILE;
        uint uEndIdx = (fViewPosZ < fHalfZ) ? ldsLightIdxCounterA : ldsLightIdxCounterB;

        for(uint i=uStartIdx; i<uEndIdx; i++)
        {
            uint nLightIndex = ldsLightIdx[i];

            float3 LightColorDiffuseResult;
            float3 LightColorSpecularResult;
#if ( SHADOWS_ENABLED == 1 )
            DoLighting(true, g_PointLightBufferCenterAndRadius, g_PointLightBufferColor, nLightIndex, vPositionWS, vNorm, vViewDir, LightColorDiffuseResult, LightColorSpecularResult);
#else
            DoLighting(false, g_PointLightBufferCenterAndRadius, g_PointLightBufferColor, nLightIndex, vPositionWS, vNorm, vViewDir, LightColorDiffuseResult, LightColorSpecularResult);
#endif

            AccumDiffuse += LightColorDiffuseResult;
            AccumSpecular += LightColorSpecularResult;
        }
    }

    // loop over the spot lights that intersect this tile
    {
        uint uStartIdx = (fViewPosZ < fHalfZ) ? 0 : MAX_NUM_LIGHTS_PER_TILE;
        uint uEndIdx = (fViewPosZ < fHalfZ) ? ldsSpotIdxCounterA : ldsSpotIdxCounterB;

        for(uint i=uStartIdx; i<uEndIdx; i++)
        {
            uint nLightIndex = ldsSpotIdx[i];

            float3 LightColorDiffuseResult;
            float3 LightColorSpecularResult;
#if ( SHADOWS_ENABLED == 1 )
            DoSpotLighting(true, g_SpotLightBufferCenterAndRadius, g_SpotLightBufferColor, g_SpotLightBufferSpotParams, nLightIndex, vPositionWS, vNorm, vViewDir, LightColorDiffuseResult, LightColorSpecularResult);
#else
            DoSpotLighting(false, g_SpotLightBufferCenterAndRadius, g_SpotLightBufferColor, g_SpotLightBufferSpotParams, nLightIndex, vPositionWS, vNorm, vViewDir, LightColorDiffuseResult, LightColorSpecularResult);
#endif

            AccumDiffuse += LightColorDiffuseResult;
            AccumSpecular += LightColorSpecularResult;
        }
    }

#if ( VPLS_ENABLED == 1 )
    // loop over the VPLs that intersect this tile
    {
        uint uStartIdx = (fViewPosZ < fHalfZ) ? 0 : MAX_NUM_VPLS_PER_TILE;
        uint uEndIdx = (fViewPosZ < fHalfZ) ? ldsVPLIdxCounterA : ldsVPLIdxCounterB;

        for(uint i=uStartIdx; i<uEndIdx; i++)
        {
            uint nLightIndex = ldsVPLIdx[i];

            float3 LightColorDiffuseResult;
            DoVPLLighting(g_VPLBufferCenterAndRadius, g_VPLBufferData, nLightIndex, vPositionWS, vNorm, LightColorDiffuseResult);

            AccumDiffuse += LightColorDiffuseResult;
        }
    }
#endif

    // pump up the lights
    AccumDiffuse *= 2;
    AccumSpecular *= 8;

    // read dummy data to consume more bandwidth, 
    // for performance testing
#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
#if ( NUM_GBUFFER_RTS >= 3 )
    float4 Dummy0 = g_GBuffer2Texture.Load( uint3(globalIdx.x,globalIdx.y,0) );
    AccumDiffuse *= Dummy0.xyz;
    AccumSpecular *= Dummy0.xyz;
#endif
#if ( NUM_GBUFFER_RTS >= 4 )
    float4 Dummy1 = g_GBuffer3Texture.Load( uint3(globalIdx.x,globalIdx.y,0) );
    AccumDiffuse *= Dummy1.xyz;
    AccumSpecular *= Dummy1.xyz;
#endif
#if ( NUM_GBUFFER_RTS >= 5 )
    float4 Dummy2 = g_GBuffer4Texture.Load( uint3(globalIdx.x,globalIdx.y,0) );
    AccumDiffuse *= Dummy2.xyz;
    AccumSpecular *= Dummy2.xyz;
#endif

#else                           // MSAA
#if ( NUM_GBUFFER_RTS >= 3 )
    float4 Dummy0 = g_GBuffer2Texture.Load( uint2(globalIdx.x,globalIdx.y), 0 );
    AccumDiffuse *= Dummy0.xyz;
    AccumSpecular *= Dummy0.xyz;
#endif
#if ( NUM_GBUFFER_RTS >= 4 )
    float4 Dummy1 = g_GBuffer3Texture.Load( uint2(globalIdx.x,globalIdx.y), 0 );
    AccumDiffuse *= Dummy1.xyz;
    AccumSpecular *= Dummy1.xyz;
#endif
#if ( NUM_GBUFFER_RTS >= 5 )
    float4 Dummy2 = g_GBuffer4Texture.Load( uint2(globalIdx.x,globalIdx.y), 0 );
    AccumDiffuse *= Dummy2.xyz;
    AccumSpecular *= Dummy2.xyz;
#endif
#endif

    // This is a poor man's ambient cubemap (blend between an up color and a down color)
    float fAmbientBlend = 0.5f * vNorm.y + 0.5;
    float3 Ambient = g_AmbientColorUp.rgb * fAmbientBlend + g_AmbientColorDown.rgb * (1-fAmbientBlend);
    float3 DiffuseAndAmbient = AccumDiffuse + Ambient;

    // modulate mesh texture with lighting
#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
    float3 DiffuseTex = g_GBuffer0Texture.Load( uint3(globalIdx.x,globalIdx.y,0) ).rgb;
#else                           // MSAA
    float3 DiffuseTex = g_GBuffer0Texture.Load( uint2(globalIdx.x,globalIdx.y), 0 ).rgb;
#endif
    float fSpecMask = vNormAndSpecMask.a;

    float3 Result = DiffuseTex*(DiffuseAndAmbient + AccumSpecular*fSpecMask);


    // override result when one of the lights-per-tile visualization modes is enabled
#if ( LIGHTS_PER_TILE_MODE > 0 )
    uint uStartIdx = (fViewPosZ < fHalfZ) ? 0 : MAX_NUM_LIGHTS_PER_TILE;
    uint uEndIdx = (fViewPosZ < fHalfZ) ? ldsLightIdxCounterA : ldsLightIdxCounterB;
    uint nNumLightsInThisTile = uEndIdx-uStartIdx;
    uEndIdx = (fViewPosZ < fHalfZ) ? ldsSpotIdxCounterA : ldsSpotIdxCounterB;
    nNumLightsInThisTile += uEndIdx-uStartIdx;
    uint uMaxNumLightsPerTile = 2*g_uMaxNumLightsPerTile;  // max for points plus max for spots
#if ( VPLS_ENABLED == 1 )
    uStartIdx = (fViewPosZ < fHalfZ) ? 0 : MAX_NUM_VPLS_PER_TILE;
    uEndIdx = (fViewPosZ < fHalfZ) ? ldsVPLIdxCounterA : ldsVPLIdxCounterB;
    nNumLightsInThisTile += uEndIdx-uStartIdx;
    uMaxNumLightsPerTile += g_uMaxNumVPLsPerTile;
#endif
#if ( LIGHTS_PER_TILE_MODE == 1 )
    Result = ConvertNumberOfLightsToGrayscale(nNumLightsInThisTile, uMaxNumLightsPerTile).rgb;
#elif ( LIGHTS_PER_TILE_MODE == 2 )
    Result = ConvertNumberOfLightsToRadarColor(nNumLightsInThisTile, uMaxNumLightsPerTile).rgb;
#endif
#endif

#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
    g_OffScreenBufferOut[globalIdx.xy] = float4(Result,1);
#else                           // MSAA

    uint2 uavMsaaBufferCoord = globalIdx.xy * uint2(2,2);
    g_OffScreenBufferOut[uavMsaaBufferCoord] = float4(Result,1);

    for( uint sampleIdx=1; sampleIdx<NUM_MSAA_SAMPLES; sampleIdx++ )
    {
        float3 vNormSample = g_GBuffer1Texture.Load( uint2(globalIdx.x,globalIdx.y), sampleIdx ).xyz;
        vNormSample *= 2;
        vNormSample -= float3(1,1,1);
        bIsEdge = bIsEdge || dot(vNormSample, vNorm) < 0.984807753f;
    }

    if( bIsEdge )
    {
        // do a thread-safe increment of the list counter 
        // and put the global index of this thread into the list
        uint dstIdx = 0;
        InterlockedAdd( ldsEdgePixelIdxCounter, 1, dstIdx );
        ldsEdgePixelIdx[dstIdx] = (globalIdx.y << 16) | globalIdx.x;
    }
    else
    {
        g_OffScreenBufferOut[uavMsaaBufferCoord + uint2(1, 0)] = float4(Result,1);
        g_OffScreenBufferOut[uavMsaaBufferCoord + uint2(0, 1)] = float4(Result,1);
        g_OffScreenBufferOut[uavMsaaBufferCoord + uint2(1, 1)] = float4(Result,1);
    }

    GroupMemoryBarrierWithGroupSync();

    // light the MSAA samples
    {
        uint uNumSamplesToLight = (NUM_MSAA_SAMPLES-1) * ldsEdgePixelIdxCounter;

        for(uint i=localIdxFlattened; i<uNumSamplesToLight; i+=NUM_THREADS_PER_TILE)
        {
            uint edgePixelListIdx = i / (NUM_MSAA_SAMPLES-1);
            uint sampleIdx = (i % (NUM_MSAA_SAMPLES-1)) + 1;

            uint edgePixelIdxPacked = ldsEdgePixelIdx[edgePixelListIdx];
            uint2 globalIdxForThisEdgePixel = uint2(edgePixelIdxPacked & 0x0000FFFF, edgePixelIdxPacked >> 16);

            uint2 litSampleCoord = globalIdxForThisEdgePixel * uint2(2, 2);
            litSampleCoord.x += sampleIdx % 2;
            litSampleCoord.y += sampleIdx > 1;
            g_OffScreenBufferOut[litSampleCoord] = float4(DoLightingForMSAA(globalIdxForThisEdgePixel, sampleIdx, fHalfZ),1);
        }
    }
#endif
}


