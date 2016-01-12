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
// File: Deferred.hlsl
//
// HLSL file for the TiledLighting11 sample. G-Buffer rendering.
//--------------------------------------------------------------------------------------


#include "CommonHeader.h"


//-----------------------------------------------------------------------------------------
// Textures and Buffers
//-----------------------------------------------------------------------------------------
Texture2D              g_TxDiffuse     : register( t0 );
Texture2D              g_TxNormal      : register( t1 );

//--------------------------------------------------------------------------------------
// shader input/output structure
//--------------------------------------------------------------------------------------
struct VS_INPUT_SCENE
{
    float3 Position     : POSITION;  // vertex position
    float3 Normal       : NORMAL;    // vertex normal vector
    float2 TextureUV    : TEXCOORD0; // vertex texture coords
    float3 Tangent      : TANGENT;   // vertex tangent vector
};

struct VS_OUTPUT_SCENE
{
    float4 Position     : SV_POSITION; // vertex position
    float3 Normal       : NORMAL;      // vertex normal vector
    float2 TextureUV    : TEXCOORD0;   // vertex texture coords
    float3 Tangent      : TEXCOORD1;   // vertex tangent vector
};

struct PS_OUTPUT
{
    float4 RT0 : SV_TARGET0;  // Diffuse
    float4 RT1 : SV_TARGET1;  // Normal
#if ( NUM_GBUFFER_RTS >= 3 )
    float4 RT2 : SV_TARGET2;  // Dummy
#endif
#if ( NUM_GBUFFER_RTS >= 4 )
    float4 RT3 : SV_TARGET3;  // Dummy
#endif
#if ( NUM_GBUFFER_RTS >= 5 )
    float4 RT4 : SV_TARGET4;  // Dummy
#endif
};


//--------------------------------------------------------------------------------------
// This shader transforms position, calculates world-space position, normal, 
// and tangent, and passes tex coords through to the pixel shader.
//--------------------------------------------------------------------------------------
VS_OUTPUT_SCENE RenderSceneToGBufferVS( VS_INPUT_SCENE Input )
{
    VS_OUTPUT_SCENE Output;
    
    // Transform the position from object space to homogeneous projection space
    float4 vWorldPos = mul( float4(Input.Position,1), g_mWorld );
    Output.Position = mul( vWorldPos, g_mViewProjection );

    // Normal and tangent in world space
    Output.Normal = mul( Input.Normal, (float3x3)g_mWorld );
    Output.Tangent = mul( Input.Tangent, (float3x3)g_mWorld );
    
    // Just copy the texture coordinate through
    Output.TextureUV = Input.TextureUV; 
    
    return Output;
}

//--------------------------------------------------------------------------------------
// This shader calculates diffuse and specular lighting for all lights.
//--------------------------------------------------------------------------------------
PS_OUTPUT RenderSceneToGBufferPS( VS_OUTPUT_SCENE Input )
{ 
    PS_OUTPUT Output;

    // diffuse rgb, and spec mask in the alpha channel
    float4 DiffuseTex = g_TxDiffuse.Sample( g_Sampler, Input.TextureUV );

#if ( USE_ALPHA_TEST == 1 )
    float fAlpha = DiffuseTex.a;
    if( fAlpha < g_fAlphaTest ) discard;
#endif

    // get normal from normal map
    float3 vNorm = g_TxNormal.Sample( g_Sampler, Input.TextureUV ).xyz;
    vNorm *= 2;
    vNorm -= float3(1,1,1);
    
    // transform normal into world space
    float3 vBinorm = normalize( cross( Input.Normal, Input.Tangent ) );
    float3x3 BTNMatrix = float3x3( vBinorm, Input.Tangent, Input.Normal );
    vNorm = normalize(mul( vNorm, BTNMatrix ));

#if ( USE_ALPHA_TEST == 1 )
    Output.RT0 = DiffuseTex;
    Output.RT1 = float4(0.5*vNorm + 0.5, 0);
#else
    Output.RT0 = float4(DiffuseTex.rgb, 1);
    Output.RT1 = float4(0.5*vNorm + 0.5, DiffuseTex.a);
#endif

    // write dummy data to consume more bandwidth, 
    // for performance testing
#if ( NUM_GBUFFER_RTS >= 3 )
    Output.RT2 = float4(1,1,1,1);
#endif
#if ( NUM_GBUFFER_RTS >= 4 )
    Output.RT3 = float4(1,1,1,1);
#endif
#if ( NUM_GBUFFER_RTS >= 5 )
    Output.RT4 = float4(1,1,1,1);
#endif

    return Output;
}

