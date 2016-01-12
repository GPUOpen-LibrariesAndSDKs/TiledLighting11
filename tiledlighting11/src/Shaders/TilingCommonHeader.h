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
// File: TilingCommonHeader.h
//
// HLSL file for the TiledLighting11 sample. Header file for tiled light culling.
//--------------------------------------------------------------------------------------


#define FLT_MAX         3.402823466e+38F

//-----------------------------------------------------------------------------------------
// Parameters for the light culling shader
//-----------------------------------------------------------------------------------------
#define NUM_THREADS_X TILE_RES
#define NUM_THREADS_Y TILE_RES
#define NUM_THREADS_PER_TILE (NUM_THREADS_X*NUM_THREADS_Y)

// TILED_CULLING_COMPUTE_SHADER_MODE:
// 0: Forward+, VPLs disabled
// 1: Forward+, VPLs enabled
// 2: Tiled Deferred, VPLs disabled
// 3: Tiled Deferred, VPLs enabled
// 4: Blended geometry (which implies Forward+ and no VPLs)
#if   ( TILED_CULLING_COMPUTE_SHADER_MODE == 0 )
#define FORWARD_PLUS 1
#define VPLS_ENABLED 0
#define BLENDED_PASS 0
#elif ( TILED_CULLING_COMPUTE_SHADER_MODE == 1 )
#define FORWARD_PLUS 1
#define VPLS_ENABLED 1
#define BLENDED_PASS 0
#elif ( TILED_CULLING_COMPUTE_SHADER_MODE == 2 )
#define FORWARD_PLUS 0
#define VPLS_ENABLED 0
#define BLENDED_PASS 0
#elif ( TILED_CULLING_COMPUTE_SHADER_MODE == 3 )
#define FORWARD_PLUS 0
#define VPLS_ENABLED 1
#define BLENDED_PASS 0
#elif ( TILED_CULLING_COMPUTE_SHADER_MODE == 4 )
#define FORWARD_PLUS 1
#define VPLS_ENABLED 0
#define BLENDED_PASS 1
#endif

//-----------------------------------------------------------------------------------------
// Textures and Buffers
//-----------------------------------------------------------------------------------------
Buffer<float4> g_PointLightBufferCenterAndRadius : register( t0 );
Buffer<float4> g_SpotLightBufferCenterAndRadius  : register( t1 );

// Only process VPLs for opaque geometry. The effect is barely 
// noticeable on transparent geometry, so skip it, to save perf.
#if ( VPLS_ENABLED == 1 )
StructuredBuffer<float4> g_VPLBufferCenterAndRadius : register( t2 );
#endif

#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
Texture2D<float>   g_DepthTexture : register( t3 );
#elif ( FORWARD_PLUS == 1 )     // Forward+ MSAA
Texture2DMS<float> g_DepthTexture : register( t3 );
#else                           // Tiled Deferred MSAA
Texture2DMS<float,NUM_MSAA_SAMPLES> g_DepthTexture : register( t3 );
#endif

#if ( BLENDED_PASS == 1 )
#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
Texture2D<float> g_BlendedDepthTexture : register( t4 );
#else                           // Forward+ MSAA
Texture2DMS<float> g_BlendedDepthTexture : register( t4 );
#endif
#endif

//-----------------------------------------------------------------------------------------
// Group Shared Memory (aka local data share, or LDS)
//-----------------------------------------------------------------------------------------
// min and max depth per tile
groupshared uint ldsZMax;
groupshared uint ldsZMin;

// per-tile light list
groupshared uint ldsLightIdxCounterA;
groupshared uint ldsLightIdxCounterB;
groupshared uint ldsLightIdx[2*MAX_NUM_LIGHTS_PER_TILE];

// per-tile spot light list
groupshared uint ldsSpotIdxCounterA;
groupshared uint ldsSpotIdxCounterB;
groupshared uint ldsSpotIdx[2*MAX_NUM_LIGHTS_PER_TILE];

#if ( VPLS_ENABLED == 1 )
// per-tile VPL list
groupshared uint ldsVPLIdxCounterA;
groupshared uint ldsVPLIdxCounterB;
groupshared uint ldsVPLIdx[2*MAX_NUM_VPLS_PER_TILE];
#endif

#if ( NUM_MSAA_SAMPLES > 1 ) && ( FORWARD_PLUS == 0 )  // Tiled Deferred MSAA
// per-tile list of edge pixel indices
groupshared uint ldsEdgePixelIdxCounter;
groupshared uint ldsEdgePixelIdx[NUM_THREADS_PER_TILE];
#endif

//-----------------------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------------------

// this creates the standard Hessian-normal-form plane equation from three points, 
// except it is simplified for the case where the first point is the origin
float4 CreatePlaneEquation( float4 b, float4 c )
{
    float4 n;

    // normalize(cross( b.xyz-a.xyz, c.xyz-a.xyz )), except we know "a" is the origin
    n.xyz = normalize(cross( b.xyz, c.xyz ));

    // -(n dot a), except we know "a" is the origin
    n.w = 0;

    return n;
}

// point-plane distance, simplified for the case where 
// the plane passes through the origin
float GetSignedDistanceFromPlane( float4 p, float4 eqn )
{
    // dot( eqn.xyz, p.xyz ) + eqn.w, , except we know eqn.w is zero 
    // (see CreatePlaneEquation above)
    return dot( eqn.xyz, p.xyz );
}

#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
void CalculateMinMaxDepthInLds( uint3 globalIdx )
{
    float opaqueDepth = g_DepthTexture.Load( uint3(globalIdx.x,globalIdx.y,0) ).x;
    float opaqueViewPosZ = ConvertProjDepthToView( opaqueDepth );
    uint opaqueZ = asuint( opaqueViewPosZ );

#if ( BLENDED_PASS == 1 )
    float blendedDepth = g_BlendedDepthTexture.Load( uint3(globalIdx.x,globalIdx.y,0) ).x;
    float blendedViewPosZ = ConvertProjDepthToView( blendedDepth );
    uint blendedZ = asuint( blendedViewPosZ );

    // Blended path
    if( blendedDepth != 0.f )
    {
        InterlockedMax( ldsZMax, opaqueZ );
        InterlockedMin( ldsZMin, blendedZ );
    }

#else
    // Opaque only path
    if( opaqueDepth != 0.f )
    {
        InterlockedMax( ldsZMax, opaqueZ );
        InterlockedMin( ldsZMin, opaqueZ );
    }
#endif
}
#endif

#if ( NUM_MSAA_SAMPLES > 1 )    // MSAA
bool CalculateMinMaxDepthInLdsMSAA( uint3 globalIdx, uint depthBufferNumSamples)
{
    float minZForThisPixel = FLT_MAX;
    float maxZForThisPixel = 0.f;

    float opaqueDepth0 = g_DepthTexture.Load( uint2(globalIdx.x,globalIdx.y), 0 ).x;
    float opaqueViewPosZ0 = ConvertProjDepthToView( opaqueDepth0 );

#if ( BLENDED_PASS == 1 )
    float blendedDepth0 = g_BlendedDepthTexture.Load( uint2(globalIdx.x,globalIdx.y), 0 ).x;
    float blendedViewPosZ0 = ConvertProjDepthToView( blendedDepth0 );

    // Blended path
    if( blendedDepth0 != 0.f )
    {
        maxZForThisPixel = max( maxZForThisPixel, opaqueViewPosZ0 );
        minZForThisPixel = min( minZForThisPixel, blendedViewPosZ0 );
    }

#else
    // Opaque only path
    if( opaqueDepth0 != 0.f )
    {
        maxZForThisPixel = max( maxZForThisPixel, opaqueViewPosZ0 );
        minZForThisPixel = min( minZForThisPixel, opaqueViewPosZ0 );
    }
#endif

    for( uint sampleIdx=1; sampleIdx<depthBufferNumSamples; sampleIdx++ )
    {
        float opaqueDepth = g_DepthTexture.Load( uint2(globalIdx.x,globalIdx.y), sampleIdx ).x;
        float opaqueViewPosZ = ConvertProjDepthToView( opaqueDepth );

#if ( BLENDED_PASS == 1 )
        float blendedDepth = g_BlendedDepthTexture.Load( uint2(globalIdx.x,globalIdx.y), sampleIdx ).x;
        float blendedViewPosZ = ConvertProjDepthToView( blendedDepth );

        // Blended path
        if( blendedDepth != 0.f )
        {
            maxZForThisPixel = max( maxZForThisPixel, opaqueViewPosZ );
            minZForThisPixel = min( minZForThisPixel, blendedViewPosZ );
        }

#else
        // Opaque only path
        if( opaqueDepth != 0.f )
        {
            maxZForThisPixel = max( maxZForThisPixel, opaqueViewPosZ );
            minZForThisPixel = min( minZForThisPixel, opaqueViewPosZ );
        }
#endif
    }

    uint zMaxForThisPixel = asuint( maxZForThisPixel );
    uint zMinForThisPixel = asuint( minZForThisPixel );
    InterlockedMax( ldsZMax, zMaxForThisPixel );
    InterlockedMin( ldsZMin, zMinForThisPixel );

    // return depth-based edge detection result
    return ((maxZForThisPixel - minZForThisPixel) > 50.0f);
}
#endif

#if ( NUM_MSAA_SAMPLES <= 1 ) || ( FORWARD_PLUS == 1 )  // non-MSAA, or Forward+ MSAA
void
#else                                                   // Tiled Deferred MSAA (return depth-based edge detection result)
bool
#endif
DoLightCulling( in uint3 globalIdx, in uint localIdxFlattened, in uint3 groupIdx, out float fHalfZ )
{
    if( localIdxFlattened == 0 )
    {
        ldsZMin = 0x7f7fffff;  // FLT_MAX as a uint
        ldsZMax = 0;
        ldsLightIdxCounterA = 0;
        ldsLightIdxCounterB = MAX_NUM_LIGHTS_PER_TILE;
        ldsSpotIdxCounterA = 0;
        ldsSpotIdxCounterB = MAX_NUM_LIGHTS_PER_TILE;
#if ( VPLS_ENABLED == 1 )
        ldsVPLIdxCounterA = 0;
        ldsVPLIdxCounterB = MAX_NUM_VPLS_PER_TILE;
#endif
#if ( NUM_MSAA_SAMPLES > 1 ) && ( FORWARD_PLUS == 0 )  // Tiled Deferred MSAA
        ldsEdgePixelIdxCounter = 0;
#endif
    }

    float4 frustumEqn[4];
    {   // construct frustum for this tile
        uint pxm = TILE_RES*groupIdx.x;
        uint pym = TILE_RES*groupIdx.y;
        uint pxp = TILE_RES*(groupIdx.x+1);
        uint pyp = TILE_RES*(groupIdx.y+1);

        uint uWindowWidthEvenlyDivisibleByTileRes = TILE_RES*g_uNumTilesX;
        uint uWindowHeightEvenlyDivisibleByTileRes = TILE_RES*g_uNumTilesY;

        // four corners of the tile, clockwise from top-left
        float4 frustum[4];
        frustum[0] = ConvertProjToView( float4( pxm/(float)uWindowWidthEvenlyDivisibleByTileRes*2.f-1.f, (uWindowHeightEvenlyDivisibleByTileRes-pym)/(float)uWindowHeightEvenlyDivisibleByTileRes*2.f-1.f,1.f,1.f) );
        frustum[1] = ConvertProjToView( float4( pxp/(float)uWindowWidthEvenlyDivisibleByTileRes*2.f-1.f, (uWindowHeightEvenlyDivisibleByTileRes-pym)/(float)uWindowHeightEvenlyDivisibleByTileRes*2.f-1.f,1.f,1.f) );
        frustum[2] = ConvertProjToView( float4( pxp/(float)uWindowWidthEvenlyDivisibleByTileRes*2.f-1.f, (uWindowHeightEvenlyDivisibleByTileRes-pyp)/(float)uWindowHeightEvenlyDivisibleByTileRes*2.f-1.f,1.f,1.f) );
        frustum[3] = ConvertProjToView( float4( pxm/(float)uWindowWidthEvenlyDivisibleByTileRes*2.f-1.f, (uWindowHeightEvenlyDivisibleByTileRes-pyp)/(float)uWindowHeightEvenlyDivisibleByTileRes*2.f-1.f,1.f,1.f) );

        // create plane equations for the four sides of the frustum, 
        // with the positive half-space outside the frustum (and remember, 
        // view space is left handed, so use the left-hand rule to determine 
        // cross product direction)
        for(uint i=0; i<4; i++)
            frustumEqn[i] = CreatePlaneEquation( frustum[i], frustum[(i+1)&3] );
    }

    GroupMemoryBarrierWithGroupSync();

    // calculate the min and max depth for this tile, 
    // to form the front and back of the frustum

#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
    CalculateMinMaxDepthInLds( globalIdx );
#else                           // MSAA
#if ( FORWARD_PLUS == 1 )       // Forward+ MSAA
    uint depthBufferWidth, depthBufferHeight, depthBufferNumSamples;
    g_DepthTexture.GetDimensions( depthBufferWidth, depthBufferHeight, depthBufferNumSamples );
    CalculateMinMaxDepthInLdsMSAA( globalIdx, depthBufferNumSamples );
#else                           // Tiled Deferred MSAA
    uint depthBufferNumSamples = NUM_MSAA_SAMPLES;
    bool bIsEdge = CalculateMinMaxDepthInLdsMSAA( globalIdx, depthBufferNumSamples );
#endif
#endif

    GroupMemoryBarrierWithGroupSync();
    float maxZ = asfloat( ldsZMax );
    float minZ = asfloat( ldsZMin );
    fHalfZ = (minZ + maxZ) / 2.0f;

    // loop over the lights and do a sphere vs. frustum intersection test
    for(uint i=localIdxFlattened; i<g_uNumLights; i+=NUM_THREADS_PER_TILE)
    {
        float4 center = g_PointLightBufferCenterAndRadius[i];
        float r = center.w;
        center.xyz = mul( float4(center.xyz, 1), g_mView ).xyz;

        // test if sphere is intersecting or inside frustum
        if( ( GetSignedDistanceFromPlane( center, frustumEqn[0] ) < r ) &&
            ( GetSignedDistanceFromPlane( center, frustumEqn[1] ) < r ) &&
            ( GetSignedDistanceFromPlane( center, frustumEqn[2] ) < r ) &&
            ( GetSignedDistanceFromPlane( center, frustumEqn[3] ) < r ) )
        {
            if( -center.z + minZ < r && center.z - fHalfZ < r )
            {
                // do a thread-safe increment of the list counter 
                // and put the index of this light into the list
                uint dstIdx = 0;
                InterlockedAdd( ldsLightIdxCounterA, 1, dstIdx );
                ldsLightIdx[dstIdx] = i;
            }
            if( -center.z + fHalfZ < r && center.z - maxZ < r )
            {
                // do a thread-safe increment of the list counter 
                // and put the index of this light into the list
                uint dstIdx = 0;
                InterlockedAdd( ldsLightIdxCounterB, 1, dstIdx );
                ldsLightIdx[dstIdx] = i;
            }
        }
    }

    // loop over the spot lights and do a sphere vs. frustum intersection test
    for(uint j=localIdxFlattened; j<g_uNumSpotLights; j+=NUM_THREADS_PER_TILE)
    {
        float4 center = g_SpotLightBufferCenterAndRadius[j];
        float r = center.w;
        center.xyz = mul( float4(center.xyz, 1), g_mView ).xyz;

        // test if sphere is intersecting or inside frustum
        if( ( GetSignedDistanceFromPlane( center, frustumEqn[0] ) < r ) &&
            ( GetSignedDistanceFromPlane( center, frustumEqn[1] ) < r ) &&
            ( GetSignedDistanceFromPlane( center, frustumEqn[2] ) < r ) &&
            ( GetSignedDistanceFromPlane( center, frustumEqn[3] ) < r ) )
        {
            if( -center.z + minZ < r && center.z - fHalfZ < r )
            {
                // do a thread-safe increment of the list counter 
                // and put the index of this light into the list
                uint dstIdx = 0;
                InterlockedAdd( ldsSpotIdxCounterA, 1, dstIdx );
                ldsSpotIdx[dstIdx] = j;
            }
            if( -center.z + fHalfZ < r && center.z - maxZ < r )
            {
                // do a thread-safe increment of the list counter 
                // and put the index of this light into the list
                uint dstIdx = 0;
                InterlockedAdd( ldsSpotIdxCounterB, 1, dstIdx );
                ldsSpotIdx[dstIdx] = j;
            }
        }
    }

// Only process VPLs for opaque geometry. The effect is barely 
// noticeable on transparent geometry, so skip it, to save perf.
#if ( VPLS_ENABLED == 1 )
    // if VPLs are disabled we can't rely on the internal UAV counter being zero hence check against MaxVPLs which is then zero
    uint numVPLs = min( g_uMaxVPLs, g_uNumVPLs );

    // loop over the VPLs and do a sphere vs. frustum intersection test
    for(uint k=localIdxFlattened; k<numVPLs; k+=NUM_THREADS_PER_TILE)
    {
        float4 center = g_VPLBufferCenterAndRadius[k];
        float r = center.w;
        center.xyz = mul( float4(center.xyz, 1), g_mView ).xyz;

        // test if sphere is intersecting or inside frustum
        if( ( GetSignedDistanceFromPlane( center, frustumEqn[0] ) < r ) &&
            ( GetSignedDistanceFromPlane( center, frustumEqn[1] ) < r ) &&
            ( GetSignedDistanceFromPlane( center, frustumEqn[2] ) < r ) &&
            ( GetSignedDistanceFromPlane( center, frustumEqn[3] ) < r ) )
        {
            if( -center.z + minZ < r && center.z - fHalfZ < r )
            {
                // do a thread-safe increment of the list counter 
                // and put the index of this VPL into the list
                uint dstIdx = 0;
                InterlockedAdd( ldsVPLIdxCounterA, 1, dstIdx );
                ldsVPLIdx[dstIdx] = k;
            }
            if( -center.z + fHalfZ < r && center.z - maxZ < r )
            {
                // do a thread-safe increment of the list counter 
                // and put the index of this VPL into the list
                uint dstIdx = 0;
                InterlockedAdd( ldsVPLIdxCounterB, 1, dstIdx );
                ldsVPLIdx[dstIdx] = k;
            }
        }
    }
#endif

    GroupMemoryBarrierWithGroupSync();

#if ( NUM_MSAA_SAMPLES <= 1 ) || ( FORWARD_PLUS == 1 )  // non-MSAA, or Forward+ MSAA
    return;
#else                                                   // Tiled Deferred MSAA (return depth-based edge detection result)
    return bIsEdge;
#endif
}

