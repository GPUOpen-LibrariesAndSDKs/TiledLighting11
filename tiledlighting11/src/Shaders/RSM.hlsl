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
// File: RSM.hlsl
//
// HLSL file for the TiledLighting11 sample. RSM generation.
//--------------------------------------------------------------------------------------


#include "CommonHeader.h"


//-----------------------------------------------------------------------------------------
// Textures and Buffers
//-----------------------------------------------------------------------------------------
Texture2D              g_TxDiffuse     : register( t0 );

//--------------------------------------------------------------------------------------
// shader input/output structure
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float3 Position     : POSITION;  // vertex position
    float3 Normal       : NORMAL;    // vertex normal vector
    float2 TextureUV    : TEXCOORD0; // vertex texture coords
    float3 Tangent      : TANGENT;   // vertex tangent vector
};

struct VS_OUTPUT
{
    float3 Normal       : NORMAL;      // vertex normal vector
    float2 TextureUV    : TEXCOORD0;   // vertex texture coords
    float3 PositionWS   : TEXCOORD1;
    float4 Position     : SV_POSITION; // vertex position
};

struct PS_INPUT
{
    float3 Normal       : NORMAL;      // vertex normal vector
    float2 TextureUV    : TEXCOORD0;   // vertex texture coords
    float3 PositionWS   : TEXCOORD1;
};

struct PS_OUTPUT
{
    float4 Normal       : SV_TARGET0;
    float4 Diffuse      : SV_TARGET1;
};


VS_OUTPUT RSMVS( VS_INPUT Input )
{
    VS_OUTPUT Output;
    
    float4 vWorldPos = mul( float4(Input.Position,1), g_mWorld );

    Output.PositionWS = vWorldPos.xyz;
    Output.Position = mul( vWorldPos, g_mViewProjection );

    Output.Normal = mul( Input.Normal, (float3x3)g_mWorld );
    
    Output.TextureUV = Input.TextureUV;
    
    return Output;
}


PS_OUTPUT RSMPS( PS_INPUT Input )
{
    PS_OUTPUT Output = (PS_OUTPUT)0;

    Output.Normal.xyz = 0.5 * (1 + normalize( Input.Normal ));
    Output.Diffuse = g_TxDiffuse.Sample( g_Sampler, Input.TextureUV );
    Output.Diffuse.a = 1;
    
    return Output;
}
