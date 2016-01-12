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
// File: TiledLighting11.cpp
//
// Implements the Forward+ algorithm and the tiled deferred algorithm, for comparison.
//--------------------------------------------------------------------------------------

// DXUT now sits one directory up
#include "..\\..\\DXUT\\Core\\DXUT.h"
#include "..\\..\\DXUT\\Core\\DXUTmisc.h"
#include "..\\..\\DXUT\\Optional\\DXUTgui.h"
#include "..\\..\\DXUT\\Optional\\DXUTCamera.h"
#include "..\\..\\DXUT\\Optional\\DXUTSettingsDlg.h"
#include "..\\..\\DXUT\\Optional\\SDKmisc.h"
#include "..\\..\\DXUT\\Optional\\SDKmesh.h"

// AMD SDK also sits one directory up
#include "..\\..\\AMD_SDK\\inc\\AMD_SDK.h"
#include "..\\..\\AMD_SDK\\inc\\ShaderCacheSampleHelper.h"

// Project includes
#include "resource.h"

#include "CommonUtil.h"
#include "ForwardPlusUtil.h"
#include "LightUtil.h"
#include "TiledDeferredUtil.h"
#include "ShadowRenderer.h"
#include "RSMRenderer.h"

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

using namespace DirectX;
using namespace TiledLighting11;

//-----------------------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------------------
static const int TEXT_LINE_HEIGHT = 15;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CFirstPersonCamera          g_Camera;                // A first-person camera
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;           // Device settings dialog
CDXUTTextHelper*            g_pTxtHelper = NULL;

// Direct3D 11 resources
CDXUTSDKMesh                g_SceneMesh;
CDXUTSDKMesh                g_AlphaMesh;
Scene                       g_Scene;

// depth buffer data
DepthStencilBuffer          g_DepthStencilBuffer;
DepthStencilBuffer          g_DepthStencilBufferForTransparency;

// GUI state
GuiState                    g_CurrentGuiState;

// Number of currently active point lights
static AMD::Slider*         g_NumPointLightsSlider = NULL;
static int                  g_iNumActivePointLights = MAX_NUM_LIGHTS;

// Number of currently active spot lights
static AMD::Slider*         g_NumSpotLightsSlider = NULL;
static int                  g_iNumActiveSpotLights = 0;

// Current triangle density (i.e. the "lots of triangles" system)
static int                  g_iTriangleDensity = TRIANGLE_DENSITY_LOW;
static const WCHAR*         g_szTriangleDensityLabel[TRIANGLE_DENSITY_NUM_TYPES] = { L"Low", L"Med", L"High" };

// Number of currently active grid objects (i.e. the "lots of triangles" system)
static AMD::Slider*         g_NumGridObjectsSlider = NULL;
static int                  g_iNumActiveGridObjects = MAX_NUM_GRID_OBJECTS;

// Number of currently active G-Buffer render targets
static AMD::Slider*         g_NumGBufferRTsSlider = NULL;
static int                  g_iNumActiveGBufferRTs = 3;

// Shadow bias
static int                  g_PointShadowBias = 100;
static int                  g_SpotShadowBias = 12;

// VPL settings
static int                  g_VPLSpotStrength = 30;
static int                  g_VPLPointStrength = 30;
static int                  g_VPLSpotRadius = 200;
static int                  g_VPLPointRadius = 100;
static int                  g_VPLThreshold = 70;
static int                  g_VPLBrightnessCutOff = 18;
static int                  g_VPLBackFace = 50;

static AMD::Slider*         g_VPLThresholdSlider = 0;
static AMD::Slider*         g_VPLBrightnessCutOffSlider = 0;

// Current lighting mode
static LightingMode         g_LightingMode = LIGHTING_SHADOWS;
static int					g_UpdateShadowMap = 4;
static int					g_UpdateRSMs = 4;

// The max distance the camera can travel
static float                g_fMaxDistance = 500.0f;

//--------------------------------------------------------------------------------------
// Constant buffers
//--------------------------------------------------------------------------------------
#pragma pack(push,1)
struct CB_PER_OBJECT
{
    XMMATRIX  m_mWorld;
};

struct CB_PER_CAMERA
{
    XMMATRIX  m_mViewProjection;
};

struct CB_PER_FRAME
{
    XMMATRIX m_mView;
    XMMATRIX m_mProjection;
    XMMATRIX m_mProjectionInv;
    XMMATRIX m_mViewProjectionInvViewport;
    XMVECTOR m_AmbientColorUp;
    XMVECTOR m_AmbientColorDown;
    XMVECTOR m_vCameraPosAndAlphaTest;
    unsigned m_uNumLights;
    unsigned m_uNumSpotLights;
    unsigned m_uWindowWidth;
    unsigned m_uWindowHeight;
    unsigned m_uMaxNumLightsPerTile;
    unsigned m_uMaxNumElementsPerTile;
    unsigned m_uNumTilesX;
    unsigned m_uNumTilesY;
    unsigned m_uMaxVPLs;
    unsigned m_uMaxNumVPLsPerTile;
    unsigned m_uMaxNumVPLElementsPerTile;
    float    m_fVPLSpotStrength;
    float    m_fVPLSpotRadius;
    float    m_fVPLPointStrength;
    float    m_fVPLPointRadius;
    float    m_fVPLRemoveBackFaceContrib;
    float    m_fVPLColorThreshold;
    float    m_fVPLBrightnessThreshold;
    float    m_fPerFramePad1;
    float    m_fPerFramePad2;
};

struct CB_SHADOW_CONSTANTS
{
    XMMATRIX m_mPointShadowViewProj[ MAX_NUM_SHADOWCASTING_POINTS ][ 6 ];
    XMMATRIX m_mSpotShadowViewProj[ MAX_NUM_SHADOWCASTING_SPOTS ];
    XMVECTOR m_ShadowBias;
};
#pragma pack(pop)

ID3D11Buffer*                       g_pcbPerObject11 = NULL;
ID3D11Buffer*                       g_pcbPerCamera11 = NULL;
ID3D11Buffer*                       g_pcbPerFrame11 = NULL;
ID3D11Buffer*                       g_pcbShadowConstants11 = NULL;

//--------------------------------------------------------------------------------------
// Set up AMD shader cache here
//--------------------------------------------------------------------------------------
AMD::ShaderCache            g_ShaderCache(AMD::ShaderCache::SHADER_AUTO_RECOMPILE_ENABLED, AMD::ShaderCache::ERROR_DISPLAY_ON_SCREEN);

//--------------------------------------------------------------------------------------
// AMD helper classes defined here
//--------------------------------------------------------------------------------------
static AMD::HUD             g_HUD;

// Global boolean for HUD rendering
bool                        g_bRenderHUD = true;

static CommonUtil        g_CommonUtil;
static ForwardPlusUtil   g_ForwardPlusUtil;
static LightUtil         g_LightUtil;
static TiledDeferredUtil g_TiledDeferredUtil;
static ShadowRenderer    g_ShadowRenderer;
static RSMRenderer       g_RSMRenderer;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
enum
{
    IDC_BUTTON_TOGGLEFULLSCREEN = 1,
    IDC_BUTTON_CHANGEDEVICE,
    IDC_COMBO_LIGHTING_MODE,
    IDC_RADIOBUTTON_FORWARD_PLUS,
    IDC_RADIOBUTTON_TILED_DEFERRED,
    IDC_CHECKBOX_ENABLE_LIGHT_DRAWING,
    IDC_SLIDER_NUM_POINT_LIGHTS,
    IDC_SLIDER_NUM_SPOT_LIGHTS,
    IDC_CHECKBOX_ENABLE_TRANSPARENT_OBJECTS,
    IDC_CHECKBOX_ENABLE_SHADOWS,
    IDC_CHECKBOX_ENABLE_VPLS,
    IDC_SLIDER_VPL_THRESHOLD_COLOR,
    IDC_SLIDER_VPL_THRESHOLD_BRIGHTNESS,
    IDC_CHECKBOX_ENABLE_DEBUG_DRAWING,
    IDC_RADIOBUTTON_DEBUG_DRAWING_ONE,
    IDC_RADIOBUTTON_DEBUG_DRAWING_TWO,
    IDC_STATIC_TRIANGLE_DENSITY,
    IDC_SLIDER_TRIANGLE_DENSITY,
    IDC_SLIDER_NUM_GRID_OBJECTS,
    IDC_SLIDER_NUM_GBUFFER_RTS,
    IDC_RENDERING_METHOD_GROUP,
    IDC_TILE_DRAWING_GROUP,
    IDC_NUM_CONTROL_IDS
};

const int AMD::g_MaxApplicationControlID = IDC_NUM_CONTROL_IDS;

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

HRESULT AddShadersToCache();
void ClearD3D11DeviceContext();
void UpdateShadowConstants();
void UpdateCameraConstantBuffer( const XMMATRIX& mViewProjAlreadyTransposed );
void UpdateCameraConstantBufferWithTranspose( const XMMATRIX& mViewProj );
void RenderDepthOnlyScene();
void UpdateUI();

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) || defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Set DXUT callbacks
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"TiledLighting11 v1.3" );

    // Require D3D_FEATURE_LEVEL_11_0
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1920, 1080 );

    DXUTMainLoop(); // Enter into the DXUT render loop

    // Ensure the ShaderCache aborts if in a lengthy generation process
    g_ShaderCache.Abort();

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    WCHAR szTemp[256];

    D3DCOLOR DlgColor = 0x88888888; // Semi-transparent background for the dialog

    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.m_GUI.Init( &g_DialogResourceManager );
    g_HUD.m_GUI.SetBackgroundColors( DlgColor );
    g_HUD.m_GUI.SetCallback( OnGUIEvent );

    int iY = AMD::HUD::iElementDelta;

    g_HUD.m_GUI.AddButton( IDC_BUTTON_TOGGLEFULLSCREEN, L"Toggle full screen", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight );
    g_HUD.m_GUI.AddButton( IDC_BUTTON_CHANGEDEVICE, L"Change device (F2)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F2 );

    AMD::InitApp( g_ShaderCache, g_HUD, iY );

    iY += AMD::HUD::iGroupDelta;

    g_HUD.m_GUI.AddRadioButton( IDC_RADIOBUTTON_FORWARD_PLUS, IDC_RENDERING_METHOD_GROUP, L"Forward+", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true );
    g_HUD.m_GUI.AddRadioButton( IDC_RADIOBUTTON_TILED_DEFERRED, IDC_RENDERING_METHOD_GROUP, L"Tiled Deferred", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, false );

    iY += AMD::HUD::iGroupDelta;

    g_HUD.m_GUI.AddComboBox( IDC_COMBO_LIGHTING_MODE, AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth + 20, AMD::HUD::iElementHeight );
    CDXUTComboBox* pComboBox = g_HUD.m_GUI.GetComboBox( IDC_COMBO_LIGHTING_MODE );
    if( pComboBox )
    {
        pComboBox->SetDropHeight( 40 );
        pComboBox->AddItem( L"Shadow Casters", NULL );
        pComboBox->AddItem( L"Random Lights", NULL );
        pComboBox->SetSelectedByIndex( g_LightingMode );
    }

    iY += AMD::HUD::iGroupDelta;

    g_HUD.m_GUI.AddCheckBox( IDC_CHECKBOX_ENABLE_LIGHT_DRAWING, L"Show Lights", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true );

    // CDXUTDialog g_HUD.m_GUI will clean up these allocations in the CDXUTDialog destructor (see CDXUTDialog::RemoveAllControls)
    g_NumPointLightsSlider = new AMD::Slider( g_HUD.m_GUI, IDC_SLIDER_NUM_POINT_LIGHTS, iY, L"Active Point Lights", 0, MAX_NUM_LIGHTS, g_iNumActivePointLights );
    g_NumSpotLightsSlider = new AMD::Slider( g_HUD.m_GUI, IDC_SLIDER_NUM_SPOT_LIGHTS, iY, L"Active Spot Lights", 0, MAX_NUM_LIGHTS, g_iNumActiveSpotLights );

    iY += AMD::HUD::iGroupDelta;

    g_HUD.m_GUI.AddCheckBox( IDC_CHECKBOX_ENABLE_TRANSPARENT_OBJECTS, L"Transparent Objects", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true );
    g_HUD.m_GUI.AddCheckBox( IDC_CHECKBOX_ENABLE_SHADOWS, L"Shadows", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true );
    g_HUD.m_GUI.AddCheckBox( IDC_CHECKBOX_ENABLE_VPLS, L"VPLs", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, false );

    // CDXUTDialog g_HUD.m_GUI will clean up these allocations in the CDXUTDialog destructor (see CDXUTDialog::RemoveAllControls)
    g_VPLThresholdSlider = new AMD::Slider( g_HUD.m_GUI, IDC_SLIDER_VPL_THRESHOLD_COLOR, iY, L"VPL Color Threshold", 50, 100, g_VPLThreshold );
    g_VPLBrightnessCutOffSlider = new AMD::Slider( g_HUD.m_GUI, IDC_SLIDER_VPL_THRESHOLD_BRIGHTNESS, iY, L"VPL Brightness Threshold", 13, 100, g_VPLBrightnessCutOff );
    g_VPLThresholdSlider->SetEnabled(false);
    g_VPLBrightnessCutOffSlider->SetEnabled(false);

    iY += AMD::HUD::iGroupDelta;

    g_HUD.m_GUI.AddCheckBox( IDC_CHECKBOX_ENABLE_DEBUG_DRAWING, L"Show Lights Per Tile", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, false );
    g_HUD.m_GUI.AddRadioButton( IDC_RADIOBUTTON_DEBUG_DRAWING_ONE, IDC_TILE_DRAWING_GROUP, L"Radar Colors", 2 * AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth - AMD::HUD::iElementOffset, AMD::HUD::iElementHeight, true );
    g_HUD.m_GUI.AddRadioButton( IDC_RADIOBUTTON_DEBUG_DRAWING_TWO, IDC_TILE_DRAWING_GROUP, L"Grayscale", 2 * AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth - AMD::HUD::iElementOffset, AMD::HUD::iElementHeight, false );
    g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_DEBUG_DRAWING_ONE )->SetEnabled(false);
    g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_DEBUG_DRAWING_TWO )->SetEnabled(false);

    iY += AMD::HUD::iGroupDelta;

    // Use a standard DXUT slider here (not AMD::Slider), since we display a word for the slider value ("Low", "Med", "High") instead of a number
    wcscpy_s( szTemp, 256, L"Triangle Density: " );
    wcscat_s( szTemp, 256, g_szTriangleDensityLabel[g_iTriangleDensity] );
    g_HUD.m_GUI.AddStatic( IDC_STATIC_TRIANGLE_DENSITY, szTemp, AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight );
    g_HUD.m_GUI.AddSlider( IDC_SLIDER_TRIANGLE_DENSITY, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, TRIANGLE_DENSITY_NUM_TYPES-1, g_iTriangleDensity );

    // CDXUTDialog g_HUD.m_GUI will clean up these allocations in the CDXUTDialog destructor (see CDXUTDialog::RemoveAllControls)
    g_NumGridObjectsSlider = new AMD::Slider( g_HUD.m_GUI, IDC_SLIDER_NUM_GRID_OBJECTS, iY, L"Active Grid Objects", 0, MAX_NUM_GRID_OBJECTS, g_iNumActiveGridObjects );
    g_NumGBufferRTsSlider = new AMD::Slider( g_HUD.m_GUI, IDC_SLIDER_NUM_GBUFFER_RTS, iY, L"Active G-Buffer RTs", 2, MAX_NUM_GBUFFER_RENDER_TARGETS, g_iNumActiveGBufferRTs );
    g_NumGBufferRTsSlider->SetEnabled(false);

    // Initialize the static data in CommonUtil
    g_CommonUtil.InitStaticData();

    // Hook functions up to the shadow renderer
    g_ShadowRenderer.SetCallbacks( UpdateCameraConstantBuffer, RenderDepthOnlyScene );
    g_RSMRenderer.SetCallbacks( UpdateCameraConstantBuffer );

    UpdateUI();
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    bool bForwardPlus = g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_FORWARD_PLUS )->GetEnabled() &&
        g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_FORWARD_PLUS )->GetChecked();

    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( XMVectorSet( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    float fGpuTime = (float)TIMER_GetTime( Gpu, L"Render|Core algorithm" ) * 1000.0f;
    if( !bForwardPlus )
    {
        // add the forward transparency time for Tiled Deferred
        // (it's already included in the core algorithm time for Forward+)
        fGpuTime += (float)TIMER_GetTime( Gpu, L"Render|Forward transparency" ) * 1000.0f;
    }

    // count digits in the total time
    int iIntegerPart = (int)fGpuTime;
    int iNumDigits = 0;
    while( iIntegerPart > 0 )
    {
        iIntegerPart /= 10;
        iNumDigits++;
    }
    iNumDigits = ( iNumDigits == 0 ) ? 1 : iNumDigits;
    // three digits after decimal, 
    // plus the decimal point itself
    int iNumChars = iNumDigits + 4;

    // dynamic formatting for swprintf_s
    WCHAR szPrecision[16];
    swprintf_s( szPrecision, 16, L"%%%d.3f", iNumChars );

    WCHAR szBuf[256];
    WCHAR szFormat[256];
    swprintf_s( szFormat, 256, L"Total:           %s", szPrecision );
    swprintf_s( szBuf, 256, szFormat, fGpuTime );
    g_pTxtHelper->DrawTextLine( szBuf );

    float fGpuPerfStat0, fGpuPerfStat1, fGpuPerfStat2;
    if( bForwardPlus )
    {
        const float fGpuTimeDepthPrePass = (float)TIMER_GetTime( Gpu, L"Render|Core algorithm|Depth pre-pass" ) * 1000.0f;
        swprintf_s( szFormat, 256, L"+--------Z Pass: %s", szPrecision );
        swprintf_s( szBuf, 256, szFormat, fGpuTimeDepthPrePass );
        g_pTxtHelper->DrawTextLine( szBuf );

        const float fGpuTimeLightCulling = (float)TIMER_GetTime( Gpu, L"Render|Core algorithm|Light culling" ) * 1000.0f;
        swprintf_s( szFormat, 256, L"+----------Cull: %s", szPrecision );
        swprintf_s( szBuf, 256, szFormat, fGpuTimeLightCulling );
        g_pTxtHelper->DrawTextLine( szBuf );

        const float fGpuTimeForwardRendering = (float)TIMER_GetTime( Gpu, L"Render|Core algorithm|Forward rendering" ) * 1000.0f;
        swprintf_s( szFormat, 256, L"\\-------Forward: %s", szPrecision );
        swprintf_s( szBuf, 256, szFormat, fGpuTimeForwardRendering );
        g_pTxtHelper->DrawTextLine( szBuf );

        fGpuPerfStat0 = fGpuTimeDepthPrePass;
        fGpuPerfStat1 = fGpuTimeLightCulling;
        fGpuPerfStat2 = fGpuTimeForwardRendering;
    }
    else
    {
        const float fGpuTimeBuildGBuffer = (float)TIMER_GetTime( Gpu, L"Render|Core algorithm|G-Buffer" ) * 1000.0f;
        swprintf_s( szFormat, 256, L"+------G-Buffer: %s", szPrecision );
        swprintf_s( szBuf, 256, szFormat, fGpuTimeBuildGBuffer );
        g_pTxtHelper->DrawTextLine( szBuf );

        const float fGpuTimeLightCullingAndShading = (float)TIMER_GetTime( Gpu, L"Render|Core algorithm|Cull and light" ) * 1000.0f;
        swprintf_s( szFormat, 256, L"+--Cull & Light: %s", szPrecision );
        swprintf_s( szBuf, 256, szFormat, fGpuTimeLightCullingAndShading );
        g_pTxtHelper->DrawTextLine( szBuf );

        const float fGpuTimeForwardTransparency = (float)TIMER_GetTime( Gpu, L"Render|Forward transparency" ) * 1000.0f;
        swprintf_s( szFormat, 256, L"\\--Transparency: %s", szPrecision );
        swprintf_s( szBuf, 256, szFormat, fGpuTimeForwardTransparency );
        g_pTxtHelper->DrawTextLine( szBuf );

        fGpuPerfStat0 = fGpuTimeBuildGBuffer;
        fGpuPerfStat1 = fGpuTimeLightCullingAndShading;
        fGpuPerfStat2 = fGpuTimeForwardTransparency;
    }

    g_pTxtHelper->SetInsertionPos( 5, DXUTGetDXGIBackBufferSurfaceDesc()->Height - AMD::HUD::iElementDelta );
    g_pTxtHelper->DrawTextLine( L"Toggle GUI    : F1" );

    g_pTxtHelper->End();

    bool bDebugDrawingEnabled = g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_DEBUG_DRAWING )->GetEnabled() &&
            g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_DEBUG_DRAWING )->GetChecked();
    if( bDebugDrawingEnabled )
    {
        // method 1 is radar colors, method 2 is grayscale
        bool bDebugDrawMethodOne = g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_DEBUG_DRAWING_ONE )->GetEnabled() &&
            g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_DEBUG_DRAWING_ONE )->GetChecked();
        int nDebugDrawType = bDebugDrawMethodOne ? DEBUG_DRAW_RADAR_COLORS : DEBUG_DRAW_GRAYSCALE;
        bool bVPLsEnabled = ( g_CurrentGuiState.m_nLightingMode == LIGHTING_SHADOWS ) && g_CurrentGuiState.m_bVPLsEnabled;
        g_CommonUtil.RenderLegend( g_pTxtHelper, TEXT_LINE_HEIGHT, XMFLOAT4( 1.0f, 1.0f, 1.0f, 0.75f ), nDebugDrawType, bVPLsEnabled );
    }
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    XMVECTOR SceneMin, SceneMax;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, TEXT_LINE_HEIGHT );

    // Load the scene mesh
    g_SceneMesh.Create( pd3dDevice, L"sponza\\sponza.sdkmesh", false );
    g_CommonUtil.CalculateSceneMinMax( g_SceneMesh, &SceneMin, &SceneMax );

    // Load the alpha-test mesh
    g_AlphaMesh.Create( pd3dDevice, L"sponza\\sponza_alpha.sdkmesh", false );

    // Put the mesh pointers in the wrapper struct that gets passed around
    g_Scene.m_pSceneMesh = &g_SceneMesh;
    g_Scene.m_pAlphaMesh = &g_AlphaMesh;

    // And the camera
    g_Scene.m_pCamera = &g_Camera;

    // Create constant buffers
    D3D11_BUFFER_DESC CBDesc;
    ZeroMemory( &CBDesc, sizeof(CBDesc) );
    CBDesc.Usage = D3D11_USAGE_DYNAMIC;
    CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    CBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    CBDesc.ByteWidth = sizeof( CB_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &CBDesc, NULL, &g_pcbPerObject11 ) );
    DXUT_SetDebugName( g_pcbPerObject11, "CB_PER_OBJECT" );

    CBDesc.ByteWidth = sizeof( CB_PER_CAMERA );
    V_RETURN( pd3dDevice->CreateBuffer( &CBDesc, NULL, &g_pcbPerCamera11 ) );
    DXUT_SetDebugName( g_pcbPerCamera11, "CB_PER_CAMERA" );

    CBDesc.ByteWidth = sizeof( CB_PER_FRAME );
    V_RETURN( pd3dDevice->CreateBuffer( &CBDesc, NULL, &g_pcbPerFrame11 ) );
    DXUT_SetDebugName( g_pcbPerFrame11, "CB_PER_FRAME" );

    CBDesc.ByteWidth = sizeof( CB_SHADOW_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &CBDesc, NULL, &g_pcbShadowConstants11 ) );
    DXUT_SetDebugName( g_pcbShadowConstants11, "CB_SHADOW_CONSTANTS" );

    // Create AMD_SDK resources here
    g_HUD.OnCreateDevice( pd3dDevice );
    TIMER_Init( pd3dDevice );

    static bool bFirstPass = true;

    // One-time setup
    if( bFirstPass )
    {
        // Setup the camera's view parameters
        XMVECTOR SceneCenter  = 0.5f * (SceneMax + SceneMin);
        XMVECTOR SceneExtents = 0.5f * (SceneMax - SceneMin);
        XMVECTOR BoundaryMin  = SceneCenter - 2.0f*SceneExtents;
        XMVECTOR BoundaryMax  = SceneCenter + 2.0f*SceneExtents;
        XMVECTOR BoundaryDiff = 4.0f*SceneExtents;  // BoundaryMax - BoundaryMin
        g_fMaxDistance = XMVectorGetX(XMVector3Length(BoundaryDiff));
        XMVECTOR vEye = SceneCenter - XMVectorSet(0.45f*XMVectorGetX(SceneExtents), 0.35f*XMVectorGetY(SceneExtents), 0.0f, 0.0f);
        XMVECTOR vAt  = SceneCenter - XMVectorSet(0.0f, 0.35f*XMVectorGetY(SceneExtents), 0.0f, 0.0f);
        g_Camera.SetRotateButtons( true, false, false );
        g_Camera.SetEnablePositionMovement( true );
        g_Camera.SetViewParams( vEye, vAt );
        g_Camera.SetScalers( 0.005f, 0.1f*g_fMaxDistance );

        XMFLOAT3 vBoundaryMin, vBoundaryMax;
        XMStoreFloat3( &vBoundaryMin, BoundaryMin );
        XMStoreFloat3( &vBoundaryMax, BoundaryMax );
        g_Camera.SetClipToBoundary( true, &vBoundaryMin, &vBoundaryMax );

        // Init light buffer data
        LightUtil::InitLights( SceneMin, SceneMax );
    }

    // Create helper resources here
    g_CommonUtil.OnCreateDevice( pd3dDevice );
    g_ForwardPlusUtil.OnCreateDevice( pd3dDevice );
    g_LightUtil.OnCreateDevice( pd3dDevice );
    g_TiledDeferredUtil.OnCreateDevice( pd3dDevice );
    g_ShadowRenderer.OnCreateDevice( pd3dDevice );
    g_RSMRenderer.OnCreateDevice( pd3dDevice );

    // Generate shaders ( this is an async operation - call AMD::ShaderCache::ShadersReady() to find out if they are complete ) 
    if( bFirstPass )
    {
        // Add the applications shaders to the cache
        AddShadersToCache();
        g_ShaderCache.GenerateShaders( AMD::ShaderCache::CREATE_TYPE_COMPILE_CHANGES );    // Only compile shaders that have changed (development mode)
        bFirstPass = false;
    }
    
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    // Note, we are using inverted 32-bit float depth for better precision, 
    // so reverse near and far below
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, g_fMaxDistance, 0.1f );

    // Set the location and size of the AMD standard HUD
    g_HUD.m_GUI.SetLocation( pBackBufferSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0 );
    g_HUD.m_GUI.SetSize( AMD::HUD::iDialogWidth, pBackBufferSurfaceDesc->Height );
    g_HUD.OnResizedSwapChain( pBackBufferSurfaceDesc );

    // Create our own depth stencil surface that's bindable as a shader resource
    V_RETURN( AMD::CreateDepthStencilSurface( &g_DepthStencilBuffer.m_pDepthStencilTexture, &g_DepthStencilBuffer.m_pDepthStencilSRV, &g_DepthStencilBuffer.m_pDepthStencilView, 
        DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count ) );

    // And another depth buffer for transparent objects
    V_RETURN( AMD::CreateDepthStencilSurface( &g_DepthStencilBufferForTransparency.m_pDepthStencilTexture, &g_DepthStencilBufferForTransparency.m_pDepthStencilSRV, &g_DepthStencilBufferForTransparency.m_pDepthStencilView, 
        DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count ) );

    g_CommonUtil.OnResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc, TEXT_LINE_HEIGHT );
    g_ForwardPlusUtil.OnResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc );
    g_LightUtil.OnResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc );
    g_TiledDeferredUtil.OnResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc );
    g_ShadowRenderer.OnResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc );
    g_RSMRenderer.OnResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc );

    g_UpdateShadowMap = 4;
    g_UpdateRSMs = 4;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext )
{
    // Reset the timer at start of frame
    TIMER_Reset();

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }       

    ClearD3D11DeviceContext();

    const DXGI_SURFACE_DESC * BackBufferDesc = DXUTGetDXGIBackBufferSurfaceDesc();
    bool bMSAAEnabled = ( BackBufferDesc->SampleDesc.Count > 1 );

    g_CurrentGuiState.m_uMSAASampleCount = BackBufferDesc->SampleDesc.Count;
    g_CurrentGuiState.m_uNumPointLights = (unsigned)g_iNumActivePointLights;
    g_CurrentGuiState.m_uNumSpotLights = (unsigned)g_iNumActiveSpotLights;

    // Check GUI state for lighting mode
    g_CurrentGuiState.m_nLightingMode = g_HUD.m_GUI.GetComboBox( IDC_COMBO_LIGHTING_MODE )->GetSelectedIndex();

    // Check GUI state for debug drawing
    bool bDebugDrawingEnabled = g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_DEBUG_DRAWING )->GetEnabled() &&
            g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_DEBUG_DRAWING )->GetChecked();
    bool bDebugDrawMethodOne = g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_DEBUG_DRAWING_ONE )->GetEnabled() &&
            g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_DEBUG_DRAWING_ONE )->GetChecked();

    g_CurrentGuiState.m_nDebugDrawType = DEBUG_DRAW_NONE;
    if( bDebugDrawingEnabled )
    {
        g_CurrentGuiState.m_nDebugDrawType = bDebugDrawMethodOne ? DEBUG_DRAW_RADAR_COLORS : DEBUG_DRAW_GRAYSCALE;
    }

    // Check GUI state for light drawing enabled
    g_CurrentGuiState.m_bLightDrawingEnabled = g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_LIGHT_DRAWING )->GetEnabled() &&
        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_LIGHT_DRAWING )->GetChecked();

    // Check GUI state for transparent objects enabled
    g_CurrentGuiState.m_bTransparentObjectsEnabled = g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_TRANSPARENT_OBJECTS )->GetEnabled() &&
        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_TRANSPARENT_OBJECTS )->GetChecked();

    // Check GUI state for shadows enabled
    g_CurrentGuiState.m_bShadowsEnabled = g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_SHADOWS )->GetEnabled() &&
        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_SHADOWS )->GetChecked();

    // Check GUI state for virtual point lights (VPLs) enabled
    g_CurrentGuiState.m_bVPLsEnabled = g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_VPLS )->GetEnabled() &&
        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_VPLS )->GetChecked();

    g_CurrentGuiState.m_nGridObjectTriangleDensity = g_iTriangleDensity;
    g_CurrentGuiState.m_nNumGridObjects = g_iNumActiveGridObjects;
    g_CurrentGuiState.m_nNumGBufferRenderTargets = g_iNumActiveGBufferRTs;

    XMMATRIX mWorld = XMMatrixIdentity();

    // Get the projection & view matrix from the camera class
    XMMATRIX mView = g_Camera.GetViewMatrix();
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mViewProjection = mView * mProj;

    // we need the inverse proj matrix in the per-tile light culling 
    // compute shader
    XMFLOAT4X4 f4x4Proj, f4x4InvProj;
    XMStoreFloat4x4( &f4x4Proj, mProj );
    XMStoreFloat4x4( &f4x4InvProj, XMMatrixIdentity() );
    f4x4InvProj._11 = 1.0f / f4x4Proj._11;
    f4x4InvProj._22 = 1.0f / f4x4Proj._22;
    f4x4InvProj._33 = 0.0f;
    f4x4InvProj._34 = 1.0f / f4x4Proj._43;
    f4x4InvProj._43 = 1.0f;
    f4x4InvProj._44 = -f4x4Proj._33 / f4x4Proj._43;
    XMMATRIX mInvProj = XMLoadFloat4x4( &f4x4InvProj );

    // we need the inverse viewproj matrix with viewport mapping, 
    // for converting from depth back to world-space position
    XMMATRIX mInvViewProj = XMMatrixInverse( NULL, mViewProjection );
    XMFLOAT4X4 f4x4Viewport ( 2.0f / (float)BackBufferDesc->Width, 0.0f,                                 0.0f, 0.0f,
                              0.0f,                               -2.0f / (float)BackBufferDesc->Height, 0.0f, 0.0f,
                              0.0f,                                0.0f,                                 1.0f, 0.0f,
                             -1.0f,                                1.0f,                                 0.0f, 1.0f  );
    XMMATRIX mInvViewProjViewport = XMLoadFloat4x4(&f4x4Viewport) * mInvViewProj;

    XMFLOAT4 CameraPosAndAlphaTest;
    XMStoreFloat4( &CameraPosAndAlphaTest, g_Camera.GetEyePt() );
    // different alpha test for MSAA enabled vs. disabled
    CameraPosAndAlphaTest.w = bMSAAEnabled ? 0.003f : 0.5f;

    // Set the constant buffers
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;

    // per-camera constants
    V( pd3dImmediateContext->Map( g_pcbPerCamera11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_PER_CAMERA* pPerCamera = ( CB_PER_CAMERA* )MappedResource.pData;
    pPerCamera->m_mViewProjection = XMMatrixTranspose( mViewProjection );
    pd3dImmediateContext->Unmap( g_pcbPerCamera11, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 1, 1, &g_pcbPerCamera11 );
    pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &g_pcbPerCamera11 );
    pd3dImmediateContext->CSSetConstantBuffers( 1, 1, &g_pcbPerCamera11 );

    // per-frame constants
    V( pd3dImmediateContext->Map( g_pcbPerFrame11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_PER_FRAME* pPerFrame = ( CB_PER_FRAME* )MappedResource.pData;
    pPerFrame->m_mView = XMMatrixTranspose( mView );
    pPerFrame->m_mProjection = XMMatrixTranspose( mProj );
    pPerFrame->m_mProjectionInv = XMMatrixTranspose( mInvProj );
    pPerFrame->m_mViewProjectionInvViewport = XMMatrixTranspose( mInvViewProjViewport );
    pPerFrame->m_AmbientColorUp = XMVectorSet( 0.013f, 0.015f, 0.050f, 1.0f );
    pPerFrame->m_AmbientColorDown = XMVectorSet( 0.0013f, 0.0015f, 0.0050f, 1.0f );
    pPerFrame->m_vCameraPosAndAlphaTest = XMLoadFloat4( &CameraPosAndAlphaTest );
    pPerFrame->m_uNumLights = (unsigned)g_iNumActivePointLights;
    pPerFrame->m_uNumSpotLights = (unsigned)g_iNumActiveSpotLights;
    pPerFrame->m_uWindowWidth = BackBufferDesc->Width;
    pPerFrame->m_uWindowHeight = BackBufferDesc->Height;
    pPerFrame->m_uMaxNumLightsPerTile = g_CommonUtil.GetMaxNumLightsPerTile();
    pPerFrame->m_uMaxNumElementsPerTile = g_CommonUtil.GetMaxNumElementsPerTile();
    pPerFrame->m_uNumTilesX = g_CommonUtil.GetNumTilesX();
    pPerFrame->m_uNumTilesY = g_CommonUtil.GetNumTilesY();
    pPerFrame->m_uMaxVPLs = ( g_CurrentGuiState.m_nLightingMode == LIGHTING_SHADOWS && g_CurrentGuiState.m_bVPLsEnabled ) ? USHRT_MAX : 0;
    pPerFrame->m_uMaxNumVPLsPerTile = g_CommonUtil.GetMaxNumVPLsPerTile();
    pPerFrame->m_uMaxNumVPLElementsPerTile = g_CommonUtil.GetMaxNumVPLElementsPerTile();
    pPerFrame->m_fVPLSpotStrength = 0.2f * ( (float)g_VPLSpotStrength / 100.0f );
    pPerFrame->m_fVPLSpotRadius = (float)g_VPLSpotRadius;
    pPerFrame->m_fVPLPointStrength = 0.2f * ( (float)g_VPLPointStrength / 100.0f );
    pPerFrame->m_fVPLPointRadius = (float)g_VPLPointRadius;
    pPerFrame->m_fVPLRemoveBackFaceContrib = 1.0f * ( (float)g_VPLBackFace / 100.0f );
    pPerFrame->m_fVPLColorThreshold = 1.0f * ( (float)g_VPLThreshold / 100.0f );
    pPerFrame->m_fVPLBrightnessThreshold = 0.01f * ( (float)g_VPLBrightnessCutOff / 100.0f );
    pPerFrame->m_fPerFramePad1 = 0.0f;
    pPerFrame->m_fPerFramePad2 = 0.0f;
    pd3dImmediateContext->Unmap( g_pcbPerFrame11, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 2, 1, &g_pcbPerFrame11 );
    pd3dImmediateContext->PSSetConstantBuffers( 2, 1, &g_pcbPerFrame11 );
    pd3dImmediateContext->CSSetConstantBuffers( 2, 1, &g_pcbPerFrame11 );

    // per-object constants
    V( pd3dImmediateContext->Map( g_pcbPerObject11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_PER_OBJECT* pPerObject = ( CB_PER_OBJECT* )MappedResource.pData;
    pPerObject->m_mWorld = XMMatrixTranspose( mWorld );
    pd3dImmediateContext->Unmap( g_pcbPerObject11, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pcbPerObject11 );
    pd3dImmediateContext->PSSetConstantBuffers( 0, 1, &g_pcbPerObject11 );
    pd3dImmediateContext->CSSetConstantBuffers( 0, 1, &g_pcbPerObject11 );

    // shadow constants
    UpdateShadowConstants();

    bool bForwardPlus = g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_FORWARD_PLUS )->GetEnabled() &&
        g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_FORWARD_PLUS )->GetChecked();

    // Render objects here...
    if( g_ShaderCache.ShadersReady() )
    {
        if ( g_CurrentGuiState.m_nLightingMode == LIGHTING_SHADOWS && g_UpdateShadowMap > 0 )
        {
            g_ShadowRenderer.RenderPointMap( MAX_NUM_SHADOWCASTING_POINTS );
            g_ShadowRenderer.RenderSpotMap( MAX_NUM_SHADOWCASTING_SPOTS );

            // restore main camera viewProj
            UpdateCameraConstantBufferWithTranspose( mViewProjection );

            g_UpdateShadowMap--;
        }

        if ( g_CurrentGuiState.m_nLightingMode == LIGHTING_SHADOWS && g_CurrentGuiState.m_bVPLsEnabled )
        {
            if ( g_UpdateRSMs > 0 )
            {
                g_RSMRenderer.RenderSpotRSMs( g_iNumActiveSpotLights, g_CurrentGuiState, g_Scene, g_CommonUtil );
                g_RSMRenderer.RenderPointRSMs( g_iNumActivePointLights, g_CurrentGuiState, g_Scene, g_CommonUtil );

                // restore main camera viewProj
                UpdateCameraConstantBufferWithTranspose( mViewProjection );

                g_UpdateRSMs--;
            }

            g_RSMRenderer.GenerateVPLs( g_iNumActiveSpotLights, g_iNumActivePointLights, g_LightUtil );
        }

        TIMER_Begin( 0, L"Render" );

        if( bForwardPlus )
        {
            g_ForwardPlusUtil.OnRender( fElapsedTime, g_CurrentGuiState, g_DepthStencilBuffer, g_DepthStencilBufferForTransparency, g_Scene, g_CommonUtil, g_LightUtil, g_ShadowRenderer, g_RSMRenderer );
        }
        else
        {
            g_TiledDeferredUtil.OnRender( fElapsedTime, g_CurrentGuiState, g_DepthStencilBuffer, g_DepthStencilBufferForTransparency, g_Scene, g_CommonUtil, g_LightUtil, g_ShadowRenderer, g_RSMRenderer );
        }

        TIMER_End(); // Render
	}

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );

	AMD::ProcessUIChanges();

	if ( g_ShaderCache.ShadersReady() )
	{

		// Render the HUD
		if( g_bRenderHUD )
		{
			g_HUD.OnRender( fElapsedTime );
		}

		RenderText();

		AMD::RenderHUDUpdates( g_pTxtHelper );
	}    
    else
    {
        float ClearColor[4] = { 0.0013f, 0.0015f, 0.0050f, 0.0f };
        ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
        ID3D11DepthStencilView* pNULLDSV = NULL;
        pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );

        // Render shader cache progress if still processing
        pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, pNULLDSV );
        pd3dImmediateContext->OMSetDepthStencilState( g_CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DISABLE_DEPTH_TEST), 0x00 );
        g_ShaderCache.RenderProgress( g_pTxtHelper, TEXT_LINE_HEIGHT, XMVectorSet( 1.0f, 1.0f, 0.0f, 1.0f ) );
    }
    
	DXUT_EndPerfEvent();

    static DWORD dwTimefirst = GetTickCount();
    if ( GetTickCount() - dwTimefirst > 5000 )
    {    
        OutputDebugString( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
        OutputDebugString( L"\n" );
        dwTimefirst = GetTickCount();
    }
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_CommonUtil.OnReleasingSwapChain();
    g_ForwardPlusUtil.OnReleasingSwapChain();
    g_LightUtil.OnReleasingSwapChain();
    g_TiledDeferredUtil.OnReleasingSwapChain();
    g_ShadowRenderer.OnReleasingSwapChain();
    g_RSMRenderer.OnReleasingSwapChain();

    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

    SAFE_RELEASE( g_DepthStencilBuffer.m_pDepthStencilTexture );
    SAFE_RELEASE( g_DepthStencilBuffer.m_pDepthStencilView );
    SAFE_RELEASE( g_DepthStencilBuffer.m_pDepthStencilSRV );

    SAFE_RELEASE( g_DepthStencilBufferForTransparency.m_pDepthStencilTexture );
    SAFE_RELEASE( g_DepthStencilBufferForTransparency.m_pDepthStencilView );
    SAFE_RELEASE( g_DepthStencilBufferForTransparency.m_pDepthStencilSRV );
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    SAFE_RELEASE( g_DepthStencilBufferForTransparency.m_pDepthStencilTexture );
    SAFE_RELEASE( g_DepthStencilBufferForTransparency.m_pDepthStencilView );
    SAFE_RELEASE( g_DepthStencilBufferForTransparency.m_pDepthStencilSRV );

    SAFE_RELEASE( g_DepthStencilBuffer.m_pDepthStencilTexture );
    SAFE_RELEASE( g_DepthStencilBuffer.m_pDepthStencilView );
    SAFE_RELEASE( g_DepthStencilBuffer.m_pDepthStencilSRV );

    // Delete additional render resources here...
    g_SceneMesh.Destroy();
    g_AlphaMesh.Destroy();

    SAFE_RELEASE( g_pcbPerObject11 );
    SAFE_RELEASE( g_pcbPerCamera11 );
    SAFE_RELEASE( g_pcbPerFrame11 );
    SAFE_RELEASE( g_pcbShadowConstants11 );

    g_CommonUtil.OnDestroyDevice();
    g_ForwardPlusUtil.OnDestroyDevice();
    g_LightUtil.OnDestroyDevice();
    g_TiledDeferredUtil.OnDestroyDevice();
    g_ShadowRenderer.OnDestroyDevice();
    g_RSMRenderer.OnDestroyDevice();

    // Destroy AMD_SDK resources here
    g_ShaderCache.OnDestroyDevice();
    g_HUD.OnDestroyDevice();
    TIMER_Destroy();
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;

        // For the first device created if its a REF device, optionally display a warning dialog box
        if( pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE )
        {
            DXUTDisplaySwitchingToREFWarning();
        }

        // Default to 4x MSAA
        else
        {
            pDeviceSettings->d3d11.sd.SampleDesc.Count = 4;
        }

        // Start with vsync disabled
        pDeviceSettings->d3d11.SyncInterval = 0;
    }

    // Sample quality is always zero
    pDeviceSettings->d3d11.sd.SampleDesc.Quality = 0;

    // The tiled deferred path currently only supports 4x MSAA
    if( pDeviceSettings->d3d11.sd.SampleDesc.Count > 1 )
    {
        pDeviceSettings->d3d11.sd.SampleDesc.Count = 4;
    }

    // Don't auto create a depth buffer, as this sample requires a depth buffer 
    // be created such that it's bindable as a shader resource
    pDeviceSettings->d3d11.AutoCreateDepthStencil = false;

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    switch( uMsg )
    {
        case WM_GETMINMAXINFO:
            // override DXUT_MIN_WINDOW_SIZE_X and DXUT_MIN_WINDOW_SIZE_Y
            // to prevent windows that are too small
            ( ( MINMAXINFO* )lParam )->ptMinTrackSize.x = 512;
            ( ( MINMAXINFO* )lParam )->ptMinTrackSize.y = 512;
            *pbNoFurtherProcessing = true;
            break;
    }
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.m_GUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    
    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
        case VK_F1:
            g_bRenderHUD = !g_bRenderHUD;
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR szTemp[256];

    switch( nControlID )
    {
        case IDC_BUTTON_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_BUTTON_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
            break;
        case IDC_COMBO_LIGHTING_MODE:
            {
                const LightingMode PreviousLightingMode = g_LightingMode;
                g_LightingMode = (LightingMode)((CDXUTComboBox*)pControl)->GetSelectedIndex();

                // only update if the lighting mode actually changed
                if ( g_LightingMode != PreviousLightingMode )
                {
                    if ( g_LightingMode == LIGHTING_SHADOWS )
                    {
                        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_SHADOWS )->SetEnabled(true);
                        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_SHADOWS )->SetChecked(true);
                        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_VPLS )->SetEnabled(true);
                        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_VPLS )->SetChecked(false);

                        g_UpdateShadowMap = 4;
                        g_UpdateRSMs = 4;
                    }
                    else
                    {
                        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_SHADOWS )->SetChecked(false);
                        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_SHADOWS )->SetEnabled(false);
                        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_VPLS )->SetChecked(false);
                        g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_VPLS )->SetEnabled(false);
                    }

                    UpdateUI();
                }
            }
            break;
        case IDC_RADIOBUTTON_FORWARD_PLUS:
        case IDC_RADIOBUTTON_TILED_DEFERRED:
            {
                bool bForwardPlus = g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_FORWARD_PLUS )->GetEnabled() &&
                    g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_FORWARD_PLUS )->GetChecked();
                g_NumGBufferRTsSlider->SetEnabled(!bForwardPlus);
            }
            break;
        case IDC_SLIDER_NUM_POINT_LIGHTS:
            {
                g_NumPointLightsSlider->OnGuiEvent();
                bool bVPLsEnabled = g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_VPLS )->GetEnabled() &&
                    g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_VPLS )->GetChecked();
				g_UpdateRSMs += bVPLsEnabled ? 4 : 0;
            }
            break;
        case IDC_SLIDER_NUM_SPOT_LIGHTS:
            {
                g_NumSpotLightsSlider->OnGuiEvent();
                bool bVPLsEnabled = g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_VPLS )->GetEnabled() &&
                    g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_VPLS )->GetChecked();
                g_UpdateRSMs += bVPLsEnabled ? 4 : 0;
            }
            break;
        case IDC_CHECKBOX_ENABLE_VPLS:
            {
                CDXUTCheckBox* VPLCheckBox = (CDXUTCheckBox*)pControl;
                bool bVPLsEnabled = VPLCheckBox->GetEnabled() && VPLCheckBox->GetChecked();
                g_UpdateRSMs += bVPLsEnabled ? 4 : 0;
                g_VPLThresholdSlider->SetEnabled(bVPLsEnabled);
                g_VPLBrightnessCutOffSlider->SetEnabled(bVPLsEnabled);
            }
            break;
        case IDC_SLIDER_VPL_THRESHOLD_COLOR:
            g_VPLThresholdSlider->OnGuiEvent();
            break;

        case IDC_SLIDER_VPL_THRESHOLD_BRIGHTNESS:
            g_VPLBrightnessCutOffSlider->OnGuiEvent();
            break;
        case IDC_CHECKBOX_ENABLE_DEBUG_DRAWING:
            {
                bool bTileDrawingEnabled = g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_DEBUG_DRAWING )->GetEnabled() &&
                    g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_DEBUG_DRAWING )->GetChecked();
                g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_DEBUG_DRAWING_ONE )->SetEnabled(bTileDrawingEnabled);
                g_HUD.m_GUI.GetRadioButton( IDC_RADIOBUTTON_DEBUG_DRAWING_TWO )->SetEnabled(bTileDrawingEnabled);
            }
            break;
        case IDC_SLIDER_TRIANGLE_DENSITY:
            {
                // update
                g_iTriangleDensity = ((CDXUTSlider*)pControl)->GetValue();
                wcscpy_s( szTemp, 256, L"Triangle Density: " );
                wcscat_s( szTemp, 256, g_szTriangleDensityLabel[g_iTriangleDensity] );
                g_HUD.m_GUI.GetStatic( IDC_STATIC_TRIANGLE_DENSITY )->SetText( szTemp );
            }
            break;
        case IDC_SLIDER_NUM_GRID_OBJECTS:
            {
                // update slider
                g_NumGridObjectsSlider->OnGuiEvent();

                if ( g_LightingMode == LIGHTING_SHADOWS )
                {
                    // need to update the shadow maps when grid object count changes
                    g_UpdateShadowMap = 4;
                    g_UpdateRSMs = 4;
                }
            }
            break;
        case IDC_SLIDER_NUM_GBUFFER_RTS:
            g_NumGBufferRTsSlider->OnGuiEvent();
            break;

		default:
			AMD::OnGUIEvent( nEvent, nControlID, pControl, pUserContext );
			break;
    }
}


//--------------------------------------------------------------------------------------
// Adds all shaders to the shader cache
//--------------------------------------------------------------------------------------
HRESULT AddShadersToCache()
{
    g_CommonUtil.AddShadersToCache(&g_ShaderCache);
    g_ForwardPlusUtil.AddShadersToCache(&g_ShaderCache);
    g_LightUtil.AddShadersToCache(&g_ShaderCache);
    g_TiledDeferredUtil.AddShadersToCache(&g_ShaderCache);
    g_RSMRenderer.AddShadersToCache(&g_ShaderCache);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Stripped down version of DXUT ClearD3D11DeviceContext.
// For this sample, the HS, DS, and GS are not used. And it 
// is assumed that drawing code will always call VSSetShader, 
// PSSetShader, IASetVertexBuffers, IASetIndexBuffer (if applicable), 
// and IASetInputLayout.
//--------------------------------------------------------------------------------------
void ClearD3D11DeviceContext()
{
    ID3D11DeviceContext* pd3dDeviceContext = DXUTGetD3D11DeviceContext();

    ID3D11ShaderResourceView* pSRVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    ID3D11RenderTargetView* pRTVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    ID3D11DepthStencilView* pDSV = NULL;
    ID3D11Buffer* pBuffers[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    ID3D11SamplerState* pSamplers[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };


    // Constant buffers
    pd3dDeviceContext->VSSetConstantBuffers( 0, 14, pBuffers );
    pd3dDeviceContext->PSSetConstantBuffers( 0, 14, pBuffers );
    pd3dDeviceContext->CSSetConstantBuffers( 0, 14, pBuffers );

    // Resources
    pd3dDeviceContext->VSSetShaderResources( 0, 16, pSRVs );
    pd3dDeviceContext->PSSetShaderResources( 0, 16, pSRVs );
    pd3dDeviceContext->CSSetShaderResources( 0, 16, pSRVs );

    // Samplers
    pd3dDeviceContext->VSSetSamplers( 0, 16, pSamplers );
    pd3dDeviceContext->PSSetSamplers( 0, 16, pSamplers );
    pd3dDeviceContext->CSSetSamplers( 0, 16, pSamplers );

    // Render targets
    pd3dDeviceContext->OMSetRenderTargets( 8, pRTVs, pDSV );

    // States
    FLOAT BlendFactor[4] = { 0,0,0,0 };
    pd3dDeviceContext->OMSetBlendState( NULL, BlendFactor, 0xFFFFFFFF );
    pd3dDeviceContext->OMSetDepthStencilState( g_CommonUtil.GetDepthStencilState(DEPTH_STENCIL_STATE_DEPTH_GREATER), 0x00 );  // we are using inverted 32-bit float depth for better precision
    pd3dDeviceContext->RSSetState( NULL );
}

void UpdateShadowConstants()
{
    static const int POINT_SHADOW_SIZE = 256;

    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    V( pd3dImmediateContext->Map( g_pcbShadowConstants11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );

    CB_SHADOW_CONSTANTS* pShadowConstants = ( CB_SHADOW_CONSTANTS* )MappedResource.pData;

    memcpy(pShadowConstants->m_mPointShadowViewProj, LightUtil::GetShadowCastingPointLightViewProjTransposedArray(), sizeof(pShadowConstants->m_mPointShadowViewProj));
    memcpy(pShadowConstants->m_mSpotShadowViewProj, LightUtil::GetShadowCastingSpotLightViewProjTransposedArray(), sizeof(pShadowConstants->m_mSpotShadowViewProj));

    float pointShadowMapSize = (float)POINT_SHADOW_SIZE;
    float pointBlurSize = 2.5f;
    XMFLOAT4 ShadowBias;
    ShadowBias.x = (pointShadowMapSize - 2.0f * pointBlurSize) / pointShadowMapSize;
    ShadowBias.y = pointBlurSize / pointShadowMapSize;
    ShadowBias.z = (float)g_PointShadowBias * 0.00001f;
    ShadowBias.w = (float)g_SpotShadowBias * 0.00001f;
    pShadowConstants->m_ShadowBias = XMLoadFloat4(&ShadowBias);

    pd3dImmediateContext->Unmap( g_pcbShadowConstants11, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 3, 1, &g_pcbShadowConstants11 );
    pd3dImmediateContext->PSSetConstantBuffers( 3, 1, &g_pcbShadowConstants11 );
    pd3dImmediateContext->CSSetConstantBuffers( 3, 1, &g_pcbShadowConstants11 );
}


void UpdateCameraConstantBuffer( const XMMATRIX& mViewProjAlreadyTransposed )
{
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    V( pd3dImmediateContext->Map( g_pcbPerCamera11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_PER_CAMERA* pPerCamera = ( CB_PER_CAMERA* )MappedResource.pData;
    memcpy( &pPerCamera->m_mViewProjection, &mViewProjAlreadyTransposed, sizeof(XMMATRIX) );
    pd3dImmediateContext->Unmap( g_pcbPerCamera11, 0 );
}


void UpdateCameraConstantBufferWithTranspose( const XMMATRIX& mViewProj )
{
    XMMATRIX mViewProjTransposed = XMMatrixTranspose( mViewProj );
    UpdateCameraConstantBuffer( mViewProjTransposed );
}


void RenderDepthOnlyScene()
{
    g_ForwardPlusUtil.RenderSceneForShadowMaps( g_CurrentGuiState, g_Scene, g_CommonUtil );
}


void UpdateUI()
{
    bool bShadowMode = ( g_LightingMode == LIGHTING_SHADOWS );

    int maxPointLights = bShadowMode ? MAX_NUM_SHADOWCASTING_POINTS : MAX_NUM_LIGHTS;
    g_iNumActivePointLights = bShadowMode ? MAX_NUM_SHADOWCASTING_POINTS : MAX_NUM_LIGHTS;
    g_HUD.m_GUI.GetSlider( IDC_SLIDER_NUM_POINT_LIGHTS )->SetRange( 0, maxPointLights );
    g_HUD.m_GUI.GetSlider( IDC_SLIDER_NUM_POINT_LIGHTS )->SetValue( g_iNumActivePointLights );

    int maxSpotLights = bShadowMode ? MAX_NUM_SHADOWCASTING_SPOTS : MAX_NUM_LIGHTS;
    g_iNumActiveSpotLights = bShadowMode ? 0 : 0;
    g_HUD.m_GUI.GetSlider( IDC_SLIDER_NUM_SPOT_LIGHTS )->SetRange( 0, maxSpotLights );
    g_HUD.m_GUI.GetSlider( IDC_SLIDER_NUM_SPOT_LIGHTS )->SetValue( g_iNumActiveSpotLights );

    g_NumPointLightsSlider->OnGuiEvent();
    g_NumSpotLightsSlider->OnGuiEvent();
}

//--------------------------------------------------------------------------------------
// EOF.
//--------------------------------------------------------------------------------------
