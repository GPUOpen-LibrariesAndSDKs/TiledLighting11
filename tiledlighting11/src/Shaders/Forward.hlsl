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
// File: Forward.hlsl
//
// HLSL file for the TiledLighting11 sample. Depth pre-pass and forward rendering.
//--------------------------------------------------------------------------------------


#include "CommonHeader.h"
#include "LightingCommonHeader.h"


//-----------------------------------------------------------------------------------------
// Textures and Buffers
//-----------------------------------------------------------------------------------------
Texture2D              g_TxDiffuse     : register( t0 );
Texture2D              g_TxNormal      : register( t1 );

// Save two slots for CDXUTSDKMesh diffuse and normal, 
// so start with the third slot, t2
Buffer<float4> g_PointLightBufferCenterAndRadius : register( t2 );
Buffer<float4> g_PointLightBufferColor           : register( t3 );
Buffer<uint>   g_PerTileLightIndexBuffer         : register( t4 );

Buffer<float4> g_SpotLightBufferCenterAndRadius  : register( t5 );
Buffer<float4> g_SpotLightBufferColor            : register( t6 );
Buffer<float4> g_SpotLightBufferSpotParams       : register( t7 );
Buffer<uint>   g_PerTileSpotIndexBuffer          : register( t8 );

#if ( VPLS_ENABLED == 1 )
StructuredBuffer<float4> g_VPLBufferCenterAndRadius : register( t9 );
StructuredBuffer<VPLData> g_VPLBufferData           : register( t10 );
Buffer<uint>   g_PerTileVPLIndexBuffer              : register( t11 );
#endif

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
    float3 PositionWS   : TEXCOORD2;   // vertex position (world space)
};

struct VS_OUTPUT_POSITION_ONLY
{
    float4 Position     : SV_POSITION; // vertex position 
};

struct VS_OUTPUT_POSITION_AND_TEX
{
    float4 Position     : SV_POSITION; // vertex position 
    float2 TextureUV    : TEXCOORD0;   // vertex texture coords
};

//--------------------------------------------------------------------------------------
// This shader just transforms position (e.g. for depth pre-pass)
//--------------------------------------------------------------------------------------
VS_OUTPUT_POSITION_ONLY RenderScenePositionOnlyVS( VS_INPUT_SCENE Input )
{
    VS_OUTPUT_POSITION_ONLY Output;
    
    // Transform the position from object space to homogeneous projection space
    float4 vWorldPos = mul( float4(Input.Position,1), g_mWorld );
    Output.Position = mul( vWorldPos, g_mViewProjection );
    
    return Output;
}

//--------------------------------------------------------------------------------------
// This shader just transforms position and passes through tex coord 
// (e.g. for depth pre-pass with alpha test)
//--------------------------------------------------------------------------------------
VS_OUTPUT_POSITION_AND_TEX RenderScenePositionAndTexVS( VS_INPUT_SCENE Input )
{
    VS_OUTPUT_POSITION_AND_TEX Output;
    
    // Transform the position from object space to homogeneous projection space
    float4 vWorldPos = mul( float4(Input.Position,1), g_mWorld );
    Output.Position = mul( vWorldPos, g_mViewProjection );
    
    // Just copy the texture coordinate through
    Output.TextureUV = Input.TextureUV; 
    
    return Output;
}

//--------------------------------------------------------------------------------------
// This shader transforms position, calculates world-space position, normal, 
// and tangent, and passes tex coords through to the pixel shader.
//--------------------------------------------------------------------------------------
VS_OUTPUT_SCENE RenderSceneForwardVS( VS_INPUT_SCENE Input )
{
    VS_OUTPUT_SCENE Output;
    
    // Transform the position from object space to homogeneous projection space
    float4 vWorldPos = mul( float4(Input.Position,1), g_mWorld );
    Output.Position = mul( vWorldPos, g_mViewProjection );

    // Position, normal, and tangent in world space
    Output.PositionWS = vWorldPos.xyz;
    Output.Normal = mul( Input.Normal, (float3x3)g_mWorld );
    Output.Tangent = mul( Input.Tangent, (float3x3)g_mWorld );
    
    // Just copy the texture coordinate through
    Output.TextureUV = Input.TextureUV; 
    
    return Output;
}

//--------------------------------------------------------------------------------------
// This shader does alpha testing.
//--------------------------------------------------------------------------------------
float4 RenderSceneAlphaTestOnlyPS( VS_OUTPUT_POSITION_AND_TEX Input ) : SV_TARGET
{ 
    float4 DiffuseTex = g_TxDiffuse.Sample( g_Sampler, Input.TextureUV );
    float fAlpha = DiffuseTex.a;
    if( fAlpha < g_fAlphaTest ) discard;
    return DiffuseTex;
}

//--------------------------------------------------------------------------------------
// This shader calculates diffuse and specular lighting for all lights.
//--------------------------------------------------------------------------------------
float4 RenderSceneForwardPS( VS_OUTPUT_SCENE Input ) : SV_TARGET
{ 
    float3 vPositionWS = Input.PositionWS;

    float3 AccumDiffuse = float3(0,0,0);
    float3 AccumSpecular = float3(0,0,0);

    float4 DiffuseTex = g_TxDiffuse.Sample( g_Sampler, Input.TextureUV );

#if ( USE_ALPHA_TEST == 1 )
    float fSpecMask = 0.0f;
    float fAlpha = DiffuseTex.a;
    if( fAlpha < g_fAlphaTest ) discard;
#else
    float fSpecMask = DiffuseTex.a;
#endif

    // get normal from normal map
    float3 vNorm = g_TxNormal.Sample( g_Sampler, Input.TextureUV ).xyz;
    vNorm *= 2;
    vNorm -= float3(1,1,1);
    
    // transform normal into world space
    float3 vBinorm = normalize( cross( Input.Normal, Input.Tangent ) );
    float3x3 BTNMatrix = float3x3( vBinorm, Input.Tangent, Input.Normal );
    vNorm = normalize(mul( vNorm, BTNMatrix ));

    float3 vViewDir = normalize( g_vCameraPos - vPositionWS );

    // loop over the point lights
    {
        uint nStartIndex, nLightCount;
        GetLightListInfo(g_PerTileLightIndexBuffer, g_uMaxNumLightsPerTile, g_uMaxNumElementsPerTile, Input.Position, nStartIndex, nLightCount);

        [loop]
        for ( uint i = nStartIndex; i < nStartIndex+nLightCount; i++ )
        {
            uint nLightIndex = g_PerTileLightIndexBuffer[i];

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

    // loop over the spot lights
    {
        uint nStartIndex, nLightCount;
        GetLightListInfo(g_PerTileSpotIndexBuffer, g_uMaxNumLightsPerTile, g_uMaxNumElementsPerTile, Input.Position, nStartIndex, nLightCount);

        [loop]
        for ( uint i = nStartIndex; i < nStartIndex+nLightCount; i++ )
        {
            uint nLightIndex = g_PerTileSpotIndexBuffer[i];

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
    // loop over the VPLs
    {
        uint nStartIndex, nLightCount;
        GetLightListInfo(g_PerTileVPLIndexBuffer, g_uMaxNumVPLsPerTile, g_uMaxNumVPLElementsPerTile, Input.Position, nStartIndex, nLightCount);

        [loop]
        for ( uint i = nStartIndex; i < nStartIndex+nLightCount; i++ )
        {
            uint nLightIndex = g_PerTileVPLIndexBuffer[i];

            float3 LightColorDiffuseResult;

            DoVPLLighting(g_VPLBufferCenterAndRadius, g_VPLBufferData, nLightIndex, vPositionWS, vNorm, LightColorDiffuseResult);

            AccumDiffuse += LightColorDiffuseResult;
        }
    }
#endif

    // pump up the lights
    AccumDiffuse *= 2;
    AccumSpecular *= 8;

    // This is a poor man's ambient cubemap (blend between an up color and a down color)
    float fAmbientBlend = 0.5f * vNorm.y + 0.5;
    float3 Ambient = g_AmbientColorUp.rgb * fAmbientBlend + g_AmbientColorDown.rgb * (1-fAmbientBlend);

    // modulate mesh texture with lighting
    float3 DiffuseAndAmbient = AccumDiffuse + Ambient;
    return float4(DiffuseTex.xyz*(DiffuseAndAmbient + AccumSpecular*fSpecMask),1);
}
