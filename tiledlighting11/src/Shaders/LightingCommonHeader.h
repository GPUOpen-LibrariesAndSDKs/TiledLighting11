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
// File: LightingCommonHeader.h
//
// HLSL file for the TiledLighting11 sample. Header file for lighting.
//--------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------------------
// Textures and Buffers
//-----------------------------------------------------------------------------------------
Texture2D      g_PointShadowAtlas                : register( t13 );
Texture2D      g_SpotShadowAtlas                 : register( t14 );

//-----------------------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------------------

int Vector3ToFace( float3 vec )
{
    int face = 0;
    float x = vec.x;
    float y = vec.y;
    float z = vec.z;
    if((abs(x) > abs(y)) && (abs(x) > abs(z)))
    {
        if (x>=0)
        {
            face = 0;
        }
        else
        {
            face = 1;
        }
    }
    else if((abs(y) > abs(x)) && (abs(y) > abs(z)))
    {
        if (y>=0)
        {
            face = 2;
        }
        else
        {
            face = 3;
        }
    }
    else
    {
        if (z>=0)
        {
            face = 4;
        }
        else
        {
            face = 5;
        }
    }

    return face;
}

float FilterShadow( Texture2D atlas, float3 uv )
{
    float shadow = 0.0;

    static const int kernelLevel = 2;
    static const int kernelWidth = 2*kernelLevel + 1;
    [unroll] for ( int i = -kernelLevel; i <= kernelLevel; i++ )
    {
        [unroll] for ( int j = -kernelLevel; j <= kernelLevel; j++ )
        {
            shadow += atlas.SampleCmpLevelZero( g_ShadowSampler, uv.xy, uv.z, int2( i, j ) ).r;
        }
    }

    shadow /= (kernelWidth*kernelWidth);

    return shadow;
}

float DoShadow( uint nShadowIndex, in float3 vPosition, float3 vLightDir, float fDistanceTerm )
{
    float3 uv = -vLightDir;
    uv.z = -uv.z;

    int face = Vector3ToFace( uv.xyz );

    float4 shadowTexCoord = mul( float4( vPosition, 1 ), g_mPointShadowViewProj[ nShadowIndex ][ face ] );
    shadowTexCoord.xyz = shadowTexCoord.xyz / shadowTexCoord.w;

    shadowTexCoord.x = shadowTexCoord.x/2 + 0.5;
    shadowTexCoord.y = shadowTexCoord.y/-2 + 0.5;

    shadowTexCoord.xy *= g_ShadowBias.xx;
    shadowTexCoord.xy += g_ShadowBias.yy;

    shadowTexCoord.x += face;
    shadowTexCoord.x *= rcp( 6 );

    shadowTexCoord.y += nShadowIndex;
    shadowTexCoord.y *= rcp( MAX_NUM_SHADOWCASTING_POINTS );

    // the lerp below is to increase the shadow bias when the light is close to the
    // surface pixel being shaded
    shadowTexCoord.z -= lerp( 10.0, 1.0, saturate(5*fDistanceTerm) )*g_ShadowBias.z;

    return FilterShadow( g_PointShadowAtlas, shadowTexCoord.xyz );
}

float DoSpotShadow( uint nShadowIndex, in float3 vPosition )
{
    float4 shadowTexCoord = mul( float4( vPosition, 1 ), g_mSpotShadowViewProj[ nShadowIndex ] );
    shadowTexCoord.xyz = shadowTexCoord.xyz / shadowTexCoord.w;

    shadowTexCoord.x = shadowTexCoord.x/2 + 0.5;
    shadowTexCoord.y = shadowTexCoord.y/-2 + 0.5;

    shadowTexCoord.x += nShadowIndex;
    shadowTexCoord.x *= rcp( MAX_NUM_SHADOWCASTING_SPOTS );

    shadowTexCoord.z -= g_ShadowBias.w;

    return FilterShadow( g_SpotShadowAtlas, shadowTexCoord.xyz );
}

void DoLighting(uniform bool bDoShadow, in Buffer<float4> PointLightBufferCenterAndRadius, in Buffer<float4> PointLightBufferColor, in uint nLightIndex, in float3 vPosition, in float3 vNorm, in float3 vViewDir, out float3 LightColorDiffuseResult, out float3 LightColorSpecularResult)
{
    float4 CenterAndRadius = PointLightBufferCenterAndRadius[nLightIndex];

    float3 vToLight = CenterAndRadius.xyz - vPosition;
    float3 vLightDir = normalize(vToLight);
    float fLightDistance = length(vToLight);

    LightColorDiffuseResult = float3(0,0,0);
    LightColorSpecularResult = float3(0,0,0);

    float fRad = CenterAndRadius.w;
    if( fLightDistance < fRad )
    {
        float x = fLightDistance / fRad;
        // fake inverse squared falloff:
        // -(1/k)*(1-(k+1)/(1+k*x^2))
        // k=20: -(1/20)*(1 - 21/(1+20*x^2))
        float fFalloff = -0.05 + 1.05/(1+20*x*x);
        LightColorDiffuseResult = PointLightBufferColor[nLightIndex].rgb * saturate(dot(vLightDir,vNorm)) * fFalloff;

        float3 vHalfAngle = normalize( vViewDir + vLightDir );
        LightColorSpecularResult = PointLightBufferColor[nLightIndex].rgb * pow( saturate(dot( vHalfAngle, vNorm )), 8 ) * fFalloff;

        if( bDoShadow )
        {
            float fShadowResult = DoShadow( nLightIndex, vPosition, vLightDir, x );
            LightColorDiffuseResult  *= fShadowResult;
            LightColorSpecularResult *= fShadowResult;
        }
    }
}

void DoSpotLighting(uniform bool bDoShadow, in Buffer<float4> SpotLightBufferCenterAndRadius, in Buffer<float4> SpotLightBufferColor, in Buffer<float4> SpotLightBufferSpotParams, in uint nLightIndex, in float3 vPosition, in float3 vNorm, in float3 vViewDir, out float3 LightColorDiffuseResult, out float3 LightColorSpecularResult)
{
    float4 BoundingSphereCenterAndRadius = SpotLightBufferCenterAndRadius[nLightIndex];
    float4 SpotParams = SpotLightBufferSpotParams[nLightIndex];

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

    LightColorDiffuseResult = float3(0,0,0);
    LightColorSpecularResult = float3(0,0,0);

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
        LightColorDiffuseResult = SpotLightBufferColor[nLightIndex].rgb * saturate(dot(vToLightNormalized,vNorm)) * fFalloff * fRadialAttenuation;

        float3 vHalfAngle = normalize( vViewDir + vToLightNormalized );
        LightColorSpecularResult = SpotLightBufferColor[nLightIndex].rgb * pow( saturate(dot( vHalfAngle, vNorm )), 8 ) * fFalloff * fRadialAttenuation;

        if( bDoShadow )
        {
            float fShadowResult = DoSpotShadow( nLightIndex, vPosition );
            LightColorDiffuseResult  *= fShadowResult;
            LightColorSpecularResult *= fShadowResult;
        }
    }
}

void DoVPLLighting(in StructuredBuffer<float4> VPLBufferCenterAndRadius, in StructuredBuffer<VPLData> VPLBufferData, in uint nLightIndex, in float3 vPosition, in float3 vNorm, out float3 LightColorDiffuseResult)
{
    float4 CenterAndRadius = VPLBufferCenterAndRadius[nLightIndex];
    VPLData Data = VPLBufferData[nLightIndex];

    float3 vToLight = CenterAndRadius.xyz - vPosition;
    float3 vLightDir = normalize(vToLight);
    float fLightDistance = length(vToLight);

    LightColorDiffuseResult = float3(0,0,0);

    float fRad = CenterAndRadius.w;
    float fVPLNormalDotDir = max( 0, dot( Data.Direction.xyz, -vLightDir ) );

    if( fLightDistance < fRad && fVPLNormalDotDir > 0 )
    {
        float3 LightColor = Data.Color.rgb;

        float x = fLightDistance / fRad;
        float fFalloff = smoothstep( 1.0, 0.0, x );

        float fSourceLightNdotL = dot( Data.SourceLightDirection.xyz, vNorm );
        if ( fSourceLightNdotL < 0.0 )
        {
            fSourceLightNdotL = 1.0 + ( fSourceLightNdotL / g_fVPLRemoveBackFaceContrib );
        } 
        else
        {
            fSourceLightNdotL = 1.0;
        }

        LightColorDiffuseResult = LightColor * saturate(dot(vLightDir,vNorm)) * fFalloff * fVPLNormalDotDir * fSourceLightNdotL;
    }
}
