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
// File: CommonUtil.h
//
// Helper functions for the TiledLighting11 sample.
//--------------------------------------------------------------------------------------

#pragma once

#include "..\\..\\DXUT\\Core\\DXUT.h"
#include "CommonConstants.h"

// Forward declarations
namespace AMD
{
    class ShaderCache;
}
class CDXUTSDKMesh;
class CDXUTTextHelper;
class CFirstPersonCamera;

namespace TiledLighting11
{
    enum DebugDrawType
    {
        DEBUG_DRAW_NONE,
        DEBUG_DRAW_RADAR_COLORS,
        DEBUG_DRAW_GRAYSCALE,
    };

    enum TriangleDensityType
    {
        TRIANGLE_DENSITY_LOW = 0,
        TRIANGLE_DENSITY_MEDIUM,
        TRIANGLE_DENSITY_HIGH,
        TRIANGLE_DENSITY_NUM_TYPES
    };

    enum DepthStencilStateType
    {
        DEPTH_STENCIL_STATE_DISABLE_DEPTH_WRITE = 0,
        DEPTH_STENCIL_STATE_DISABLE_DEPTH_TEST,
        DEPTH_STENCIL_STATE_DEPTH_GREATER,
        DEPTH_STENCIL_STATE_DEPTH_GREATER_AND_DISABLE_DEPTH_WRITE,
        DEPTH_STENCIL_STATE_DEPTH_EQUAL_AND_DISABLE_DEPTH_WRITE,
        DEPTH_STENCIL_STATE_DEPTH_LESS,
        DEPTH_STENCIL_STATE_NUM_TYPES
    };

    enum RasterizerStateType
    {
        RASTERIZER_STATE_DISABLE_CULLING = 0,
        RASTERIZER_STATE_WIREFRAME,
        RASTERIZER_STATE_WIREFRAME_DISABLE_CULLING,
        RASTERIZER_STATE_NUM_TYPES
    };

    enum SamplerStateType
    {
        SAMPLER_STATE_POINT = 0,
        SAMPLER_STATE_LINEAR,
        SAMPLER_STATE_ANISO,
        SAMPLER_STATE_SHADOW,
        SAMPLER_STATE_NUM_TYPES
    };

    struct GuiState
    {
        unsigned m_uMSAASampleCount;
        unsigned m_uNumPointLights;
        unsigned m_uNumSpotLights;
        int m_nLightingMode;
        int m_nDebugDrawType;
        bool m_bLightDrawingEnabled;
        bool m_bTransparentObjectsEnabled;
        bool m_bShadowsEnabled;
        bool m_bVPLsEnabled;
        int m_nGridObjectTriangleDensity;
        int m_nNumGridObjects;
        int m_nNumGBufferRenderTargets;
    };

    struct DepthStencilBuffer
    {
        ID3D11Texture2D*          m_pDepthStencilTexture;
        ID3D11DepthStencilView*   m_pDepthStencilView;
        ID3D11ShaderResourceView* m_pDepthStencilSRV;
    };

    struct Scene
    {
        CDXUTSDKMesh* m_pSceneMesh;
        CDXUTSDKMesh* m_pAlphaMesh;
        CFirstPersonCamera* m_pCamera;
    };

    class CommonUtil
    {
    public:
        // Constructor / destructor
        CommonUtil();
        ~CommonUtil();

        static void InitStaticData();
		static void CalculateSceneMinMax( CDXUTSDKMesh &Mesh, DirectX::XMVECTOR *pBBoxMinOut, DirectX::XMVECTOR *pBBoxMaxOut );

        void AddShadersToCache( AMD::ShaderCache *pShaderCache );

        void SortTransparentObjects(const DirectX::XMVECTOR& vEyePt) const;
        void RenderTransparentObjects(int nDebugDrawType, bool bShadowsEnabled, bool bVPLsEnabled, bool bDepthOnlyRendering) const;

        void RenderLegend( CDXUTTextHelper *pTxtHelper, int nLineHeight, DirectX::XMFLOAT4 Color, int nDebugDrawType, bool bVPLsEnabled ) const;

        // Various hook functions
        HRESULT OnCreateDevice( ID3D11Device* pd3dDevice );
        void OnDestroyDevice();
        HRESULT OnResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, int nLineHeight );
        void OnReleasingSwapChain();

        unsigned GetNumTilesX() const;
        unsigned GetNumTilesY() const;
        unsigned GetMaxNumLightsPerTile() const;
        unsigned GetMaxNumElementsPerTile() const;
        unsigned GetMaxNumVPLsPerTile() const;
        unsigned GetMaxNumVPLElementsPerTile() const;

        ID3D11ShaderResourceView * const * GetLightIndexBufferSRVParam() const { return &m_pLightIndexBufferSRV; }
        ID3D11UnorderedAccessView * const * GetLightIndexBufferUAVParam() const { return &m_pLightIndexBufferUAV; }

        ID3D11ShaderResourceView * const * GetLightIndexBufferForBlendedObjectsSRVParam() const { return &m_pLightIndexBufferForBlendedObjectsSRV; }
        ID3D11UnorderedAccessView * const * GetLightIndexBufferForBlendedObjectsUAVParam() const { return &m_pLightIndexBufferForBlendedObjectsUAV; }

        ID3D11ShaderResourceView * const * GetSpotIndexBufferSRVParam() const { return &m_pSpotIndexBufferSRV; }
        ID3D11UnorderedAccessView * const * GetSpotIndexBufferUAVParam() const { return &m_pSpotIndexBufferUAV; }

        ID3D11ShaderResourceView * const * GetSpotIndexBufferForBlendedObjectsSRVParam() const { return &m_pSpotIndexBufferForBlendedObjectsSRV; }
        ID3D11UnorderedAccessView * const * GetSpotIndexBufferForBlendedObjectsUAVParam() const { return &m_pSpotIndexBufferForBlendedObjectsUAV; }

        ID3D11ShaderResourceView * const * GetVPLIndexBufferSRVParam() const { return &m_pVPLIndexBufferSRV; }
        ID3D11UnorderedAccessView * const * GetVPLIndexBufferUAVParam() const { return &m_pVPLIndexBufferUAV; }

        void DrawGrid(int nGridNumber, int nTriangleDensity, bool bWithTextures = true) const;

        ID3D11PixelShader * GetDebugDrawNumLightsPerTilePS( int nDebugDrawType, bool bVPLsEnabled, bool bForTransparentObjects ) const;

        ID3D11VertexShader * GetFullScreenVS() const { return m_pFullScreenVS; }
        ID3D11PixelShader * GetFullScreenPS( unsigned uMSAASampleCount ) const;

        ID3D11ComputeShader * GetLightCullCSForBlendedObjects( unsigned uMSAASampleCount ) const;

        ID3D11DepthStencilState * GetDepthStencilState( int nDepthStencilStateType ) const { return m_pDepthStencilState[nDepthStencilStateType]; }
        ID3D11RasterizerState * GetRasterizerState( int nRasterizerStateType ) const { return m_pRasterizerState[nRasterizerStateType]; }

        ID3D11SamplerState * const * GetSamplerStateParam( int nSamplerStateType ) const { return &m_pSamplerState[nSamplerStateType]; }

    private:

        // Light culling constants.
        // These must match their counterparts in CommonHeader.h
        static const unsigned TILE_RES = 16;
        static const unsigned MAX_NUM_LIGHTS_PER_TILE = 272;
        static const unsigned MAX_NUM_VPLS_PER_TILE = 1024;

        // forward rendering render target width and height
        unsigned                    m_uWidth;
        unsigned                    m_uHeight;

        // buffers for light culling
        ID3D11Buffer*               m_pLightIndexBuffer;
        ID3D11ShaderResourceView*   m_pLightIndexBufferSRV;
        ID3D11UnorderedAccessView*  m_pLightIndexBufferUAV;

        // buffers for light culling with alpha-blended geometry
        ID3D11Buffer*               m_pLightIndexBufferForBlendedObjects;
        ID3D11ShaderResourceView*   m_pLightIndexBufferForBlendedObjectsSRV;
        ID3D11UnorderedAccessView*  m_pLightIndexBufferForBlendedObjectsUAV;

        // buffers for spot light culling
        ID3D11Buffer*               m_pSpotIndexBuffer;
        ID3D11ShaderResourceView*   m_pSpotIndexBufferSRV;
        ID3D11UnorderedAccessView*  m_pSpotIndexBufferUAV;

        // buffers for spot light culling with alpha-blended geometry
        ID3D11Buffer*               m_pSpotIndexBufferForBlendedObjects;
        ID3D11ShaderResourceView*   m_pSpotIndexBufferForBlendedObjectsSRV;
        ID3D11UnorderedAccessView*  m_pSpotIndexBufferForBlendedObjectsUAV;

        // buffers for VPL culling
        ID3D11Buffer*               m_pVPLIndexBuffer;
        ID3D11ShaderResourceView*   m_pVPLIndexBufferSRV;
        ID3D11UnorderedAccessView*  m_pVPLIndexBufferUAV;

        // cube VB and IB (for blended objects)
        ID3D11Buffer*               m_pBlendedVB;
        ID3D11Buffer*               m_pBlendedIB;

        // instance data for the blended objects
        ID3D11Buffer*               m_pBlendedTransform;
        ID3D11ShaderResourceView*   m_pBlendedTransformSRV;

        // grid VB and IB (for different triangle densities)
        ID3D11Buffer*               m_pGridVB[TRIANGLE_DENSITY_NUM_TYPES][MAX_NUM_GRID_OBJECTS];
        ID3D11Buffer*               m_pGridIB[TRIANGLE_DENSITY_NUM_TYPES];

        // grid diffuse and normal map textures
        ID3D11ShaderResourceView*   m_pGridDiffuseTextureSRV;
        ID3D11ShaderResourceView*   m_pGridNormalMapSRV;

        // sprite quad VB (for debug drawing the lights-per-tile legend texture)
        ID3D11Buffer*               m_pQuadForLegendVB;

        // shaders for transparency (these use forward rendering, but we put them 
        // here in CommonUtil instead of in ForwardPlusUtil because they are used 
        // by both the Forward+ path and the Tiled Deferred path)
        ID3D11VertexShader*         m_pSceneBlendedVS;
        ID3D11VertexShader*         m_pSceneBlendedDepthVS;
        ID3D11PixelShader*          m_pSceneBlendedPS;
        ID3D11PixelShader*          m_pSceneBlendedPSShadows;
        ID3D11InputLayout*          m_pSceneBlendedLayout;
        ID3D11InputLayout*          m_pSceneBlendedDepthLayout;

        // compute shaders for tiled culling
        static const int NUM_LIGHT_CULLING_COMPUTE_SHADERS_FOR_BLENDED_OBJECTS = NUM_MSAA_SETTINGS;  // one for each MSAA setting
        ID3D11ComputeShader*        m_pLightCullCSForBlendedObjects[NUM_LIGHT_CULLING_COMPUTE_SHADERS_FOR_BLENDED_OBJECTS];

        // debug draw shaders for the lights-per-tile visualization modes
        ID3D11PixelShader*          m_pDebugDrawNumLightsPerTileRadarColorsPS;
        ID3D11PixelShader*          m_pDebugDrawNumLightsPerTileGrayscalePS;
        ID3D11PixelShader*          m_pDebugDrawNumLightsAndVPLsPerTileRadarColorsPS;
        ID3D11PixelShader*          m_pDebugDrawNumLightsAndVPLsPerTileGrayscalePS;
        ID3D11PixelShader*          m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledRadarColorsPS;
        ID3D11PixelShader*          m_pDebugDrawNumLightsPerTileForTransparencyWithVPLsEnabledGrayscalePS;

        // debug draw shaders for the lights-per-tile legend
        ID3D11VertexShader*         m_pDebugDrawLegendForNumLightsPerTileVS;
        ID3D11PixelShader*          m_pDebugDrawLegendForNumLightsPerTileRadarColorsPS;
        ID3D11PixelShader*          m_pDebugDrawLegendForNumLightsPerTileGrayscalePS;
        ID3D11InputLayout*          m_pDebugDrawLegendForNumLightsLayout11;

        // shaders for full-screen blit/downsample
        static const int NUM_FULL_SCREEN_PIXEL_SHADERS = NUM_MSAA_SETTINGS;  // one for each MSAA setting
        ID3D11VertexShader*         m_pFullScreenVS;
        ID3D11PixelShader*          m_pFullScreenPS[NUM_FULL_SCREEN_PIXEL_SHADERS];

        // state
        ID3D11DepthStencilState*    m_pDepthStencilState[DEPTH_STENCIL_STATE_NUM_TYPES];
        ID3D11RasterizerState*      m_pRasterizerState[RASTERIZER_STATE_NUM_TYPES];
        ID3D11SamplerState*         m_pSamplerState[SAMPLER_STATE_NUM_TYPES];
    };

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
