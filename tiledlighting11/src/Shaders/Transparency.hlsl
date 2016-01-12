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
// File: Transparency.hlsl
//
// HLSL file for the TiledLighting11 sample. Forward rendering with transparency.
//--------------------------------------------------------------------------------------


#include "CommonHeader.h"
#include "LightingCommonHeader.h"


//-----------------------------------------------------------------------------------------
// Textures and Buffers
//-----------------------------------------------------------------------------------------
StructuredBuffer<matrix> g_InstanceTransform : register( t0 );

Buffer<float4> g_PointLightBufferCenterAndRadius : register( t2 );
Buffer<float4> g_PointLightBufferColor           : register( t3 );
Buffer<uint>   g_PerTileLightIndexBuffer         : register( t4 );

Buffer<float4> g_SpotLightBufferCenterAndRadius  : register( t5 );
Buffer<float4> g_SpotLightBufferColor            : register( t6 );
Buffer<float4> g_SpotLightBufferSpotParams       : register( t7 );
Buffer<uint>   g_PerTileSpotIndexBuffer          : register( t8 );

//--------------------------------------------------------------------------------------
// shader input/output structure
//--------------------------------------------------------------------------------------
struct VS_INPUT_ALPHA_BLENDED
{
    float3 Position     : POSITION;  // vertex position
    float3 Normal       : NORMAL;    // vertex normal vector
    float2 TextureUV    : TEXCOORD0; // vertex texture coords
};

struct VS_OUTPUT_ALPHA_BLENDED
{
    float4 Position     : SV_POSITION; // vertex position
    float3 Normal       : NORMAL;      // vertex normal vector
    float3 PositionWS   : TEXCOORD0;   // vertex position (world space)
};

struct VS_OUTPUT_ALPHA_BLENDED_DEPTH
{
    float4 Position     : SV_POSITION; // vertex position 
};

void DoLightingTwoSided(in uint nLightIndex, in float3 vPosition, in float3 vNorm, in float3 vViewDir, 
                        out float3 LightColorDiffuseResultFrontFace, out float3 LightColorSpecularResultFrontFace, 
                        out float3 LightColorDiffuseResultBackFace, out float3 LightColorSpecularResultBackFace)
{
    float4 CenterAndRadius = g_PointLightBufferCenterAndRadius[nLightIndex];

    float3 vToLight = CenterAndRadius.xyz - vPosition.xyz;
    float3 vLightDir = normalize(vToLight);
    float fLightDistance = length(vToLight);

    LightColorDiffuseResultFrontFace = float3(0,0,0);
    LightColorSpecularResultFrontFace = float3(0,0,0);
    LightColorDiffuseResultBackFace = float3(0,0,0);
    LightColorSpecularResultBackFace = float3(0,0,0);

    float fRad = CenterAndRadius.w;
    if( fLightDistance < fRad )
    {
        float x = fLightDistance / fRad;
        // fake inverse squared falloff:
        // -(1/k)*(1-(k+1)/(1+k*x^2))
        // k=20: -(1/20)*(1 - 21/(1+20*x^2))
        float fFalloff = -0.05 + 1.05/(1+20*x*x);

        float3 LightColor = g_PointLightBufferColor[nLightIndex].rgb;

        LightColorDiffuseResultFrontFace = LightColor * saturate(dot(vLightDir,vNorm)) * fFalloff;

        float3 vHalfAngle = normalize( vViewDir + vLightDir );
        LightColorSpecularResultFrontFace = LightColor * pow( saturate(dot( vHalfAngle, vNorm )), 8 ) * fFalloff;

        LightColorDiffuseResultBackFace = LightColor * saturate(dot(vLightDir,-vNorm)) * fFalloff;
        LightColorSpecularResultBackFace = LightColor * pow( saturate(dot( vHalfAngle, -vNorm )), 8 ) * fFalloff;

#if ( SHADOWS_ENABLED == 1 )
        float fShadowResult = DoShadow( nLightIndex, vPosition, vLightDir, x );
        LightColorDiffuseResultFrontFace  *= fShadowResult;
        LightColorSpecularResultFrontFace *= fShadowResult;
        LightColorDiffuseResultBackFace   *= fShadowResult;
        LightColorSpecularResultBackFace  *= fShadowResult;
#endif
    }
}

void DoSpotLightingTwoSided(in uint nLightIndex, in float3 vPosition, in float3 vNorm, in float3 vViewDir, 
                            out float3 LightColorDiffuseResultFrontFace, out float3 LightColorSpecularResultFrontFace, 
                            out float3 LightColorDiffuseResultBackFace, out float3 LightColorSpecularResultBackFace)
{
    float4 BoundingSphereCenterAndRadius = g_SpotLightBufferCenterAndRadius[nLightIndex];
    float4 SpotParams = g_SpotLightBufferSpotParams[nLightIndex];

    // reconstruct z component of the light dir from x and y
    float3 SpotLightDir;
    SpotLightDir.xy = SpotParams.xy;
    SpotLightDir.z = sqrt(1 - SpotLightDir.x*SpotLightDir.x - SpotLightDir.y*SpotLightDir.y);

    // the sign bit for cone angle is used to store the sign for the z component of the light dir
    SpotLightDir.z = (SpotParams.z > 0) ? SpotLightDir.z : -SpotLightDir.z;

    // calculate the light position from the bounding sphere (we know the top of the cone is 
    // r_bounding_sphere units away from the bounding sphere center along the negated light direction)
    float3 LightPosition = BoundingSphereCenterAndRadius.xyz - BoundingSphereCenterAndRadius.w*SpotLightDir;

    float3 vToLight = LightPosition - vPosition;
    float3 vToLightNormalized = normalize(vToLight);
    float fLightDistance = length(vToLight);
    float fCosineOfCurrentConeAngle = dot(-vToLightNormalized, SpotLightDir);

    LightColorDiffuseResultFrontFace = float3(0,0,0);
    LightColorSpecularResultFrontFace = float3(0,0,0);
    LightColorDiffuseResultBackFace = float3(0,0,0);
    LightColorSpecularResultBackFace = float3(0,0,0);

    float fRad = SpotParams.w;
    float fCosineOfConeAngle = (SpotParams.z > 0) ? SpotParams.z : -SpotParams.z;
    if( fLightDistance < fRad && fCosineOfCurrentConeAngle > fCosineOfConeAngle)
    {
        float fRadialAttenuation = (fCosineOfCurrentConeAngle - fCosineOfConeAngle) / (1.0 - fCosineOfConeAngle);
        fRadialAttenuation = fRadialAttenuation * fRadialAttenuation;

        float x = fLightDistance / fRad;
        // fake inverse squared falloff:
        // -(1/k)*(1-(k+1)/(1+k*x^2))
        // k=20: -(1/20)*(1 - 21/(1+20*x^2))
        float fFalloff = -0.05 + 1.05/(1+20*x*x);

        float3 LightColor = g_SpotLightBufferColor[nLightIndex].rgb;

        LightColorDiffuseResultFrontFace = LightColor * saturate(dot(vToLightNormalized,vNorm)) * fFalloff * fRadialAttenuation;

        float3 vHalfAngle = normalize( vViewDir + vToLightNormalized );
        LightColorSpecularResultFrontFace = LightColor * pow( saturate(dot( vHalfAngle, vNorm )), 8 ) * fFalloff * fRadialAttenuation;

        LightColorDiffuseResultBackFace = LightColor * saturate(dot(vToLightNormalized,-vNorm)) * fFalloff * fRadialAttenuation;
        LightColorSpecularResultBackFace = LightColor * pow( saturate(dot( vHalfAngle, -vNorm )), 8 ) * fFalloff * fRadialAttenuation;

#if ( SHADOWS_ENABLED == 1 )
        float fShadowResult = DoSpotShadow( nLightIndex, vPosition );
        LightColorDiffuseResultFrontFace  *= fShadowResult;
        LightColorSpecularResultFrontFace *= fShadowResult;
        LightColorDiffuseResultBackFace   *= fShadowResult;
        LightColorSpecularResultBackFace  *= fShadowResult;
#endif
    }
}

//--------------------------------------------------------------------------------------
// This shader just transforms position (e.g. to write depth to the depth buffer)
//--------------------------------------------------------------------------------------
VS_OUTPUT_ALPHA_BLENDED_DEPTH RenderBlendedDepthVS( VS_INPUT_ALPHA_BLENDED Input, uint InstanceID : SV_InstanceID )
{
    VS_OUTPUT_ALPHA_BLENDED_DEPTH Output;

    matrix mWorld = g_InstanceTransform[InstanceID];

    // Transform the position from object space to homogeneous projection space
    float4 vWorldPos = mul( float4(Input.Position,1), mWorld );
    Output.Position = mul( vWorldPos, g_mViewProjection );
    
    return Output;
}

//--------------------------------------------------------------------------------------
// This shader transforms position, calculates world-space position and normal, 
// and passes tex coords through to the pixel shader.
//--------------------------------------------------------------------------------------
VS_OUTPUT_ALPHA_BLENDED RenderBlendedVS( VS_INPUT_ALPHA_BLENDED Input, uint InstanceID : SV_InstanceID )
{
    VS_OUTPUT_ALPHA_BLENDED Output;

    matrix mWorld = g_InstanceTransform[InstanceID];

    // Transform the position from object space to homogeneous projection space
    float4 vWorldPos = mul( float4(Input.Position,1), mWorld );
    Output.Position = mul( vWorldPos, g_mViewProjection );

    // Position and normal in world space
    Output.PositionWS = vWorldPos.xyz;
    Output.Normal = mul( Input.Normal, (float3x3)mWorld );

    return Output;
}

//--------------------------------------------------------------------------------------
// This shader calculates diffuse and specular lighting for all lights.
//--------------------------------------------------------------------------------------
float4 RenderBlendedPS( VS_OUTPUT_ALPHA_BLENDED Input ) : SV_TARGET
{ 
    float3 vPositionWS = Input.PositionWS;

    float3 AccumDiffuseFrontFace = float3(0,0,0);
    float3 AccumSpecularFrontFace = float3(0,0,0);
    float3 AccumDiffuseBackFace = float3(0,0,0);
    float3 AccumSpecularBackFace = float3(0,0,0);

    float3 vNorm = Input.Normal;
    vNorm = normalize(vNorm);

    float3 vViewDir = normalize( g_vCameraPos - vPositionWS );

    // loop over the point lights
    {
        uint nStartIndex, nLightCount;
        GetLightListInfo(g_PerTileLightIndexBuffer, g_uMaxNumLightsPerTile, g_uMaxNumElementsPerTile, Input.Position, nStartIndex, nLightCount);

        [loop]
        for ( uint i = nStartIndex; i < nStartIndex+nLightCount; i++ )
        {
            uint nLightIndex = g_PerTileLightIndexBuffer[i];

            float3 LightColorDiffuseResultFrontFace;
            float3 LightColorSpecularResultFrontFace;
            float3 LightColorDiffuseResultBackFace;
            float3 LightColorSpecularResultBackFace;
            DoLightingTwoSided(nLightIndex, vPositionWS, vNorm, vViewDir, 
                LightColorDiffuseResultFrontFace, LightColorSpecularResultFrontFace, 
                LightColorDiffuseResultBackFace, LightColorSpecularResultBackFace);

            AccumDiffuseFrontFace += LightColorDiffuseResultFrontFace;
            AccumSpecularFrontFace += LightColorSpecularResultFrontFace;
            AccumDiffuseBackFace += LightColorDiffuseResultBackFace;
            AccumSpecularBackFace += LightColorSpecularResultBackFace;
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

            float3 LightColorDiffuseResultFrontFace;
            float3 LightColorSpecularResultFrontFace;
            float3 LightColorDiffuseResultBackFace;
            float3 LightColorSpecularResultBackFace;
            DoSpotLightingTwoSided(nLightIndex, vPositionWS, vNorm, vViewDir, 
                LightColorDiffuseResultFrontFace, LightColorSpecularResultFrontFace, 
                LightColorDiffuseResultBackFace, LightColorSpecularResultBackFace);

            AccumDiffuseFrontFace += LightColorDiffuseResultFrontFace;
            AccumSpecularFrontFace += LightColorSpecularResultFrontFace;
            AccumDiffuseBackFace += LightColorDiffuseResultBackFace;
            AccumSpecularBackFace += LightColorSpecularResultBackFace;
        }
    }

    // pump up the lights
    AccumDiffuseFrontFace *= 2;
    AccumSpecularFrontFace *= 8;
    AccumDiffuseBackFace *= 2;
    AccumSpecularBackFace *= 8;

    // This is a poor man's ambient cubemap (blend between an up color and a down color)
    float fAmbientBlendFrontFace = 0.5f * vNorm.y + 0.5;
    float3 AmbientFrontFace = g_AmbientColorUp.rgb * fAmbientBlendFrontFace + g_AmbientColorDown.rgb * (1-fAmbientBlendFrontFace);

    // and again for the back face
    float fAmbientBlendBackFace = 0.5f * -vNorm.y + 0.5;
    float3 AmbientBackFace = g_AmbientColorUp.rgb * fAmbientBlendBackFace + g_AmbientColorDown.rgb * (1-fAmbientBlendBackFace);

    // combine lighting
    float3 DiffuseAndAmbientFrontFace = AccumDiffuseFrontFace + AmbientFrontFace;
    float3 DiffuseAndAmbientBackFace = AccumDiffuseBackFace + AmbientBackFace;
    float BackFaceWeight = 0.5;
    float3 TotalLighting = DiffuseAndAmbientFrontFace + AccumSpecularFrontFace + (BackFaceWeight * ( DiffuseAndAmbientBackFace + AccumSpecularBackFace )); 

    return float4(TotalLighting,0.5);
}
