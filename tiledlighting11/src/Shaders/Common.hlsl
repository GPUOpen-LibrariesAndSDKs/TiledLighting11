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
// File: Common.hlsl
//
// HLSL file for the TiledLighting11 sample. Common shaders.
//--------------------------------------------------------------------------------------


#include "CommonHeader.h"


//-----------------------------------------------------------------------------------------
// Textures and Buffers
//-----------------------------------------------------------------------------------------

// Save two slots for CDXUTSDKMesh diffuse and normal, 
// so start with the third slot, t2
#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
Texture2D<float4> g_OffScreenBuffer : register( t2 );
#else                           // MSAA
Texture2DMS<float4,NUM_MSAA_SAMPLES> g_OffScreenBuffer : register( t2 );
#endif

//--------------------------------------------------------------------------------------
// shader input/output structure
//--------------------------------------------------------------------------------------
struct VS_QUAD_OUTPUT
{
    float4 Position     : SV_POSITION;
    float2 TextureUV    : TEXCOORD0;

#if ( NUM_MSAA_SAMPLES > 1 )   // MSAA
    uint uSample : SV_SAMPLEINDEX;
#endif
};


//--------------------------------------------------------------------------------------
// Function:    FullScreenQuadVS
//
// Description: Vertex shader that generates a fullscreen quad with texcoords.
//              To use draw 3 vertices with primitive type triangle strip
//--------------------------------------------------------------------------------------
VS_QUAD_OUTPUT FullScreenQuadVS( uint id : SV_VertexID )
{
    VS_QUAD_OUTPUT Out = (VS_QUAD_OUTPUT)0;

    float2 vTexCoord = float2( (id << 1) & 2, id & 2 );

    // z = 1 below because we have inverted depth
    Out.Position = float4( vTexCoord * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f), 1.0f, 1.0f );
    Out.TextureUV = vTexCoord;

    return Out;
}

//--------------------------------------------------------------------------------------
// Function:    FullScreenBlitPS
//
// Description: Copy input to output.
//--------------------------------------------------------------------------------------
float4 FullScreenBlitPS( VS_QUAD_OUTPUT i ) : SV_TARGET
{
#if ( NUM_MSAA_SAMPLES <= 1 )   // non-MSAA
    return g_OffScreenBuffer.SampleLevel(g_Sampler, i.TextureUV, 0);
#else                           // MSAA
    // Convert screen coordinates to integer
    int2 nScreenCoordinates = int2(i.Position.xy);
    return g_OffScreenBuffer.Load(i.Position.xy, i.uSample);
#endif
}

