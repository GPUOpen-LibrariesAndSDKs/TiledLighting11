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
// File: TilingForward.hlsl
//
// HLSL file for the TiledLighting11 sample. Tiled light culling.
//--------------------------------------------------------------------------------------


#include "CommonHeader.h"
#include "TilingCommonHeader.h"

//-----------------------------------------------------------------------------------------
// Textures and Buffers
//-----------------------------------------------------------------------------------------
RWBuffer<uint> g_PerTileLightIndexBufferOut : register( u0 );
RWBuffer<uint> g_PerTileSpotIndexBufferOut  : register( u1 );

// Only process VPLs for opaque geometry. The effect is barely 
// noticeable on transparent geometry, so skip it, to save perf.
#if ( VPLS_ENABLED == 1 )
RWBuffer<uint> g_PerTileVPLIndexBufferOut   : register( u2 );
#endif

//-----------------------------------------------------------------------------------------
// Light culling shader
//-----------------------------------------------------------------------------------------
[numthreads(NUM_THREADS_X, NUM_THREADS_Y, 1)]
void CullLightsCS( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    uint localIdxFlattened = localIdx.x + localIdx.y*NUM_THREADS_X;

    // after calling DoLightCulling, the per-tile list of light indices that intersect this tile 
    // will be in ldsLightIdx, and the number of lights that intersect this tile 
    // will be in ldsLightIdxCounterA and ldsLightIdxCounterB
    float fHalfZ;
    DoLightCulling( globalIdx, localIdxFlattened, groupIdx, fHalfZ );

    {   // write back (point lights)
        uint tileIdxFlattened = groupIdx.x + groupIdx.y*g_uNumTilesX;
        uint startOffset = g_uMaxNumElementsPerTile*tileIdxFlattened;

        for(uint i=localIdxFlattened; i<ldsLightIdxCounterA; i+=NUM_THREADS_PER_TILE)
        {
            // per-tile list of light indices
            g_PerTileLightIndexBufferOut[startOffset+i+4] = ldsLightIdx[i];
        }

        for(uint j=localIdxFlattened; j<ldsLightIdxCounterB-MAX_NUM_LIGHTS_PER_TILE; j+=NUM_THREADS_PER_TILE)
        {
            // per-tile list of light indices
            g_PerTileLightIndexBufferOut[startOffset+j+4+g_uMaxNumLightsPerTile] = ldsLightIdx[j+MAX_NUM_LIGHTS_PER_TILE];
        }

        if( localIdxFlattened == 0 )
        {
            // store fHalfZ for this tile as two 16-bit unsigned values
            uint uHalfZBits = asuint(fHalfZ);
            uint uHalfZBitsHigh = uHalfZBits >> 16;
            uint uHalfZBitsLow = uHalfZBits & 0x0000FFFF;
            g_PerTileLightIndexBufferOut[startOffset+0] = uHalfZBitsHigh;
            g_PerTileLightIndexBufferOut[startOffset+1] = uHalfZBitsLow;

            // store the light count for list A
            g_PerTileLightIndexBufferOut[startOffset+2] = ldsLightIdxCounterA;

            // store the light count for list B
            g_PerTileLightIndexBufferOut[startOffset+3] = ldsLightIdxCounterB-MAX_NUM_LIGHTS_PER_TILE;
        }
    }

    {   // write back (spot lights)
        uint tileIdxFlattened = groupIdx.x + groupIdx.y*g_uNumTilesX;
        uint startOffset = g_uMaxNumElementsPerTile*tileIdxFlattened;

        for(uint i=localIdxFlattened; i<ldsSpotIdxCounterA; i+=NUM_THREADS_PER_TILE)
        {
            // per-tile list of light indices
            g_PerTileSpotIndexBufferOut[startOffset+i+4] = ldsSpotIdx[i];
        }

        for(uint j=localIdxFlattened; j<ldsSpotIdxCounterB-MAX_NUM_LIGHTS_PER_TILE; j+=NUM_THREADS_PER_TILE)
        {
            // per-tile list of light indices
            g_PerTileSpotIndexBufferOut[startOffset+j+4+g_uMaxNumLightsPerTile] = ldsSpotIdx[j+MAX_NUM_LIGHTS_PER_TILE];
        }

        if( localIdxFlattened == 0 )
        {
            // store fHalfZ for this tile as two 16-bit unsigned values
            uint uHalfZBits = asuint(fHalfZ);
            uint uHalfZBitsHigh = uHalfZBits >> 16;
            uint uHalfZBitsLow = uHalfZBits & 0x0000FFFF;
            g_PerTileSpotIndexBufferOut[startOffset+0] = uHalfZBitsHigh;
            g_PerTileSpotIndexBufferOut[startOffset+1] = uHalfZBitsLow;

            // store the light count for list A
            g_PerTileSpotIndexBufferOut[startOffset+2] = ldsSpotIdxCounterA;

            // store the light count for list B
            g_PerTileSpotIndexBufferOut[startOffset+3] = ldsSpotIdxCounterB-MAX_NUM_LIGHTS_PER_TILE;
        }
    }

// Only process VPLs for opaque geometry. The effect is barely 
// noticeable on transparent geometry, so skip it, to save perf.
#if ( VPLS_ENABLED == 1 )
    {   // write back (VPLs)
        uint tileIdxFlattened = groupIdx.x + groupIdx.y*g_uNumTilesX;
        uint startOffset = g_uMaxNumVPLElementsPerTile*tileIdxFlattened;

        for(uint i=localIdxFlattened; i<ldsVPLIdxCounterA; i+=NUM_THREADS_PER_TILE)
        {
            // per-tile list of VPL indices
            g_PerTileVPLIndexBufferOut[startOffset+i+4] = ldsVPLIdx[i];
        }

        for(uint j=localIdxFlattened; j<ldsVPLIdxCounterB-MAX_NUM_VPLS_PER_TILE; j+=NUM_THREADS_PER_TILE)
        {
            // per-tile list of VPL indices
            g_PerTileVPLIndexBufferOut[startOffset+j+4+g_uMaxNumVPLsPerTile] = ldsVPLIdx[j+MAX_NUM_VPLS_PER_TILE];
        }

        if( localIdxFlattened == 0 )
        {
            // store fHalfZ for this tile as two 16-bit unsigned values
            uint uHalfZBits = asuint(fHalfZ);
            uint uHalfZBitsHigh = uHalfZBits >> 16;
            uint uHalfZBitsLow = uHalfZBits & 0x0000FFFF;
            g_PerTileVPLIndexBufferOut[startOffset+0] = uHalfZBitsHigh;
            g_PerTileVPLIndexBufferOut[startOffset+1] = uHalfZBitsLow;

            // store the light count for list A
            g_PerTileVPLIndexBufferOut[startOffset+2] = ldsVPLIdxCounterA;

            // store the light count for list B
            g_PerTileVPLIndexBufferOut[startOffset+3] = ldsVPLIdxCounterB-MAX_NUM_VPLS_PER_TILE;
        }
    }
#endif
}


