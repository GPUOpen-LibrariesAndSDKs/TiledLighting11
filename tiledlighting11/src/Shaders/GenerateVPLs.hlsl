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
// File: GenerateVPLs.hlsl
//
// HLSL file for the TiledLighting11 sample. VPL generation.
//--------------------------------------------------------------------------------------


#include "CommonHeader.h"


//-----------------------------------------------------------------------------------------
// Textures and Buffers
//-----------------------------------------------------------------------------------------
RWStructuredBuffer<float4>      g_VPLPositionBuffer                 : register( u0 );
RWStructuredBuffer<VPLData>     g_VPLDataBuffer                     : register( u1 );

Texture2D                       g_RSMDepthAtlas                     : register( t0 );
Texture2D                       g_RSMNormalAtlas                    : register( t1 );
Texture2D                       g_RSMDiffuseAtlas                   : register( t2 );

StructuredBuffer<float4x4>      g_invViewProjMatrices               : register( t3 );

#if ( SPOT_LIGHTS == 1 )
Buffer<float4> g_SpotLightBufferCenterAndRadius  : register( t4 );
Buffer<float4> g_SpotLightBufferColor            : register( t5 );
Buffer<float4> g_SpotLightBufferSpotParams       : register( t6 );
#else
Buffer<float4> g_PointLightBufferCenterAndRadius : register( t4 );
Buffer<float4> g_PointLightBufferColor           : register( t5 );
#endif


#if ( SPOT_LIGHTS == 1 )
#define RSM_SIZE        32
#else
#define RSM_SIZE        32
#endif

#define THREAD_SIZE     16
#define SAMPLE_WIDTH    ( RSM_SIZE / THREAD_SIZE )

[numthreads( THREAD_SIZE, THREAD_SIZE, 1 )]
void GenerateVPLsCS( uint3 globalIdx : SV_DispatchThreadID )
{
    uint2 uv00 = SAMPLE_WIDTH*globalIdx.xy;

#if ( SPOT_LIGHTS == 1 )
    uint lightIndex = SAMPLE_WIDTH*globalIdx.x / RSM_SIZE;
#else
    uint lightIndex = SAMPLE_WIDTH*globalIdx.y / RSM_SIZE;
    uint faceIndex = SAMPLE_WIDTH*globalIdx.x / RSM_SIZE;
#endif

    float3 color = 0;

    float3 normal = 0;

    float4 position = 1;

    uint2 uv = uv00;

    color = g_RSMDiffuseAtlas[ uv ].rgb;
    normal = (2*g_RSMNormalAtlas[ uv ].rgb)-1;

    float2 viewportUV = uv.xy;

    viewportUV.xy %= RSM_SIZE;

    float depth = g_RSMDepthAtlas[ uv ].r;

    float x = (2.0f * (((float)viewportUV.x + 0.5) / RSM_SIZE)) - 1.0;
    float y = (2.0f * -(((float)viewportUV.y + 0.5) / RSM_SIZE)) + 1.0;

    float4 screenSpacePos = float4( x, y, depth, 1.0 );

#if ( SPOT_LIGHTS == 1 )
    uint matrixIndex = lightIndex;
#else
    uint matrixIndex = (6*lightIndex)+faceIndex;
#endif

    position = mul( screenSpacePos, g_invViewProjMatrices[ matrixIndex ] );

    position.xyz /= position.w;


#if ( SPOT_LIGHTS == 1 )

    float4 SpotParams = g_SpotLightBufferSpotParams[lightIndex];
    float3 SpotLightDir;
    SpotLightDir.xy = SpotParams.xy;
    SpotLightDir.z = sqrt(1 - SpotLightDir.x*SpotLightDir.x - SpotLightDir.y*SpotLightDir.y);

    // the sign bit for cone angle is used to store the sign for the z component of the light dir
    SpotLightDir.z = (SpotParams.z > 0) ? SpotLightDir.z : -SpotLightDir.z;

    float4 sourceLightCentreAndRadius = g_SpotLightBufferCenterAndRadius[ lightIndex ];
    float3 lightPos = sourceLightCentreAndRadius.xyz - sourceLightCentreAndRadius.w*SpotLightDir;

#else

    float4 sourceLightCentreAndRadius = g_PointLightBufferCenterAndRadius[ lightIndex ];
    float3 lightPos = sourceLightCentreAndRadius.xyz;

#endif

    float3 sourceLightDir = position.xyz - lightPos;

    float lightDistance = length( sourceLightDir );

    {
        float fFalloff = 1.0 - length( sourceLightDir ) / sourceLightCentreAndRadius.w;

        color *= fFalloff;

        float3 normalizedColor = normalize( color );
        float dotR = dot( normalizedColor, float3( 1, 0, 0 ) );
        float dotG = dot( normalizedColor, float3( 0, 1, 0 ) );
        float dotB = dot( normalizedColor, float3( 0, 0, 1 ) );

        float threshold = g_fVPLColorThreshold;

        bool isInterestingColor = dotR > threshold || dotG > threshold || dotB > threshold;

        if ( isInterestingColor )
        {
            float4 positionAndRadius;

            float lightStrength = 1.0;

#if ( SPOT_LIGHTS == 1 )
            positionAndRadius.w = g_fVPLSpotRadius;
            lightStrength *= g_fVPLSpotStrength;
#else
            positionAndRadius.w = g_fVPLPointRadius;
            lightStrength *= g_fVPLPointStrength;
#endif

            positionAndRadius.xyz = position.xyz;

#if ( SPOT_LIGHTS == 1 )
            color = color * g_SpotLightBufferColor[ lightIndex ].rgb * lightStrength;
#else
            color = color * g_PointLightBufferColor[ lightIndex ].rgb * lightStrength;
#endif

            float colorStrength = length( color );
            if ( colorStrength > g_fVPLBrightnessThreshold )
            {
                VPLData data;

                data.Color = float4( color, 1 );
                data.Direction = float4( normal, 0 );

#if ( SPOT_LIGHTS == 1 )

                data.SourceLightDirection = float4( -SpotLightDir, 0 );
#else
                data.SourceLightDirection.xyz = normalize( -sourceLightDir );
                data.SourceLightDirection.w = 0;
#endif

                uint index = g_VPLPositionBuffer.IncrementCounter();

                g_VPLPositionBuffer[ index ] = positionAndRadius;
                g_VPLDataBuffer[ index ] = data;
            }
        }
    }
}

