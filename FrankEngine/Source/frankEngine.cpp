////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Core Functionality
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"

//#define DEBUG_VS   // Uncomment this line to debug D3D9 vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug D3D9 pixel shaders 

//--------------------------------------------------------------------------------------
// Frank Engine Globals
int								g_backBufferWidth = 0;
int								g_backBufferHeight = 0;
float							g_aspectRatio = 0;
GameControlBase*				g_gameControlBase = NULL;		// game control
GuiBase*						g_guiBase = NULL;				// game gui
Camera*							g_cameraBase = NULL;			// game camera
FrankRender*					g_render = NULL;				// renderer
Physics*						g_physics = NULL;				// physics system
CDXUTDialogResourceManager		g_dialogResourceManager;		// manager for shared resources of dialogs
EditorGui						g_editorGui;					// game editor gui
CDXUTTextHelper*				g_textHelper = NULL;			// text helper for basic text rendering
ID3DXFont*						g_pFont9 = NULL;				// font for text helper
ID3DXSprite*					g_pSprite9 = NULL;				// sprite for text helper
GameTimer						g_lostFocusTimer;				// how long since we were sleeping

Color	g_backBufferClearColor			= Color::Grey(1, 0.1f);
ConsoleCommand(g_backBufferClearColor, backBufferClearColor);
Color	g_editorBackBufferClearColor	= Color::Grey(1, 0.2f);
ConsoleCommand(g_editorBackBufferClearColor, editorBackBufferClearColor);

//--------------------------------------------------------------------------------------
// debug builds always start in window mode, release builds start full screen
#if defined(DEBUG) | defined(_DEBUG) | defined(PROFILE)
ConsoleCommandSimple(bool, startFullscreen, false);
#else
ConsoleCommandSimple(bool, startFullscreen, true);
#endif

bool enableVsync = true;
static void ConsoleCommandCallback_enableVsync(const wstring& text)
{
	int newEnableVsyncInt = enableVsync;
	swscanf_s(text.c_str(), L"%d", &newEnableVsyncInt);
	bool newEnableVsync = newEnableVsyncInt != 0;
	if (newEnableVsync != enableVsync)
	{
		enableVsync = newEnableVsync;
		DXUTResetDevice();
	}
}
ConsoleCommand(ConsoleCommandCallback_enableVsync, vsync);

// automatically try to reload assets on focus for faster iteration
bool g_reloadAssetsOnFocus = true;
ConsoleCommand(g_reloadAssetsOnFocus, reloadAssetsOnFocus);

ConsoleCommandSimple(float, mipBias, 0);

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext );
void    CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void    CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool    CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
bool    CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void    CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void    CALLBACK OnD3D9LostDevice( void* pUserContext );
void    CALLBACK OnD3D9DestroyDevice( void* pUserContext );

//--------------------------------------------------------------------------------------
void FrankEngineStartup(const WCHAR* title, int maxObjectCount, size_t maxObjectSize )
{
    // Enable run-time memory check for debug builds.
	#if defined(DEBUG) | defined(_DEBUG)
		_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	#endif

	// Set DXUT callbacks
	DXUTSetCallbackMsgProc( MsgProc );
	DXUTSetCallbackKeyboard( OnKeyboard );
	DXUTSetCallbackFrameMove( OnFrameMove );
	DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
	DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );
	DXUTSetCallbackD3D9DeviceCreated( OnD3D9CreateDevice );
	DXUTSetCallbackD3D9DeviceReset( OnD3D9ResetDevice );
	DXUTSetCallbackD3D9DeviceLost( OnD3D9LostDevice );
	DXUTSetCallbackD3D9DeviceDestroyed( OnD3D9DestroyDevice );
	DXUTSetCallbackD3D9FrameRender( OnD3D9FrameRender );
	DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
	DXUTSetCursorSettings( true, true );
	DXUTSetHotkeyHandling(true, true, false);	// hotkeys: fullscreen, escape, pause
	DXUTCreateWindow( title );

	// set random seeds
	srand(timeGetTime());
	FrankRand::SetSeed(timeGetTime());

	// allocate memory for objects
	g_objectManager.Init(maxObjectCount, maxObjectSize);
}

//--------------------------------------------------------------------------------------
void FrankEngineInit(int width, int height, GameControlBase* gameControl, GuiBase* gameGui, Camera* camera) 
{
	ASSERT(gameControl && !g_gameControlBase);
	ASSERT(gameGui && !g_guiBase);
	ASSERT(camera && !g_cameraBase);
	ASSERT(!g_sound);
	ASSERT(!g_input);
	ASSERT(!g_physics);

	// parse the autoexec debug commands
	GetDebugConsole().ParseFile(L"autoexec.cfg", false);
	GetDebugConsole().Init();

	// init high level game objects
	g_sound = new SoundControl();
	g_input	= new InputControl();
	g_physics = new Physics();
	g_physics->Init();
	ParticleEmitter::InitParticleSystem();
	TerrainTile::BuildCache();
	g_cameraBase = camera;
	g_gameControlBase = gameControl;
	g_guiBase = gameGui;
	g_guiBase->Init(); 
	g_editorGui.Init();
	
	// create the directx device
	g_backBufferWidth = width;
	g_backBufferHeight = height;
	g_gameControlBase->PreInit();
	if (startFullscreen)
	{
		// start in full screen mode
		DXUTCreateDevice( false, 0, 0 );
		DXUTSetWindowBackBufferSizeAtModeChange( width, height );
	}
	else
		DXUTCreateDevice( true, g_backBufferWidth, g_backBufferHeight );

	// init the game control before gameplay starts
	g_gameControlBase->Init();
}

void FrankEngineLoop()
{
	DXUTMainLoop();
}

void FrankEngineShutdown()
{
	// remove files in temp folder
	Editor::ClearTempFolder();

	GetDebugConsole().Save();

	if (g_gameControlBase)
	{
		g_gameControlBase->Shutdown();
		if (GameControlBase::autoSaveTerrain && !g_gameControlBase->IsGameplayMode() && g_terrain)
			g_terrain->Save(Terrain::terrainFilename);
		g_gameControlBase->DestroyDeviceObjects();
		delete g_gameControlBase;
		g_gameControlBase = NULL;
	}
	
	SAFE_DELETE(g_guiBase);
	SAFE_DELETE(g_sound);
	SAFE_DELETE(g_input);
	SAFE_DELETE(g_physics);
}

//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, 
                                      D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object(); 
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                    AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, 
                    D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    // No fallback defined by this app, so reject any device that 
    // doesn't support at least ps2.0
    //if( pCaps->PixelShaderVersion < D3DPS_VERSION(2,0) )
    //    return false;

    return true;
}

//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    if( pDeviceSettings->ver == DXUT_D3D9_DEVICE )
    {
        IDirect3D9* pD3D = DXUTGetD3D9Object();
        D3DCAPS9 Caps;
        pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType, &Caps );

		{
			// set to swap discard
			pDeviceSettings->d3d9.pp.BackBufferCount = 2;
			pDeviceSettings->d3d9.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
			pDeviceSettings->d3d9.pp.PresentationInterval = enableVsync? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
		}

        // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
        // then switch to SWVP.
        if( (Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) == 0 ||
            Caps.VertexShaderVersion < D3DVS_VERSION(1,1) )
        {
            pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        }

        // Debugging vertex shaders requires either REF or software vertex processing 
        // and debugging pixel shaders requires REF.  
#ifdef DEBUG_VS
        if( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF )
        {
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;                            
            pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        }
#endif
#ifdef DEBUG_PS
        pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
#endif
    }

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( (DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF) ||
            (DXUT_D3D10_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE) )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_dialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    
    V_RETURN( D3DXCreateFont( pd3dDevice, 24, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                              L"Arial", &g_pFont9 ) );

	// update back buffer globals
	g_backBufferWidth = pBackBufferSurfaceDesc->Width;
	g_backBufferHeight = pBackBufferSurfaceDesc->Height;
	g_aspectRatio = (float)g_backBufferWidth / (float)g_backBufferHeight;

	ASSERT(!g_render);
	g_render = new FrankRender();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, 
                                    const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_dialogResourceManager.OnD3D9ResetDevice() );

    if( g_pFont9 ) V_RETURN( g_pFont9->OnResetDevice() );

    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite9 ) );

	ASSERT(!g_textHelper);
    g_textHelper = new CDXUTTextHelper( g_pFont9, g_pSprite9, NULL, NULL, 24 );

	// update back buffer globals
	g_backBufferWidth = pBackBufferSurfaceDesc->Width;
	g_backBufferHeight = pBackBufferSurfaceDesc->Height;
	g_aspectRatio = (float)g_backBufferWidth / (float)g_backBufferHeight;

	if (g_gameControlBase)
	{
		g_gameControlBase->InitDeviceObjects();

		// reset input
		g_gameControlBase->GetInputControl().Clear();

		// reset the gui
		g_guiBase->OnResetDevice(); 
		g_editorGui.OnResetDevice(); 
	}

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9LostDevice( void* pUserContext )
{
    g_dialogResourceManager.OnD3D9LostDevice();
    if( g_pFont9 ) g_pFont9->OnLostDevice();
    SAFE_RELEASE( g_pSprite9 );
    SAFE_DELETE( g_textHelper );
	
	if (g_guiBase)
		g_guiBase->OnLostDevice(); 
	g_editorGui.OnLostDevice(); 

	if (g_gameControlBase)
		g_gameControlBase->DestroyDeviceObjects();
}

//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
	if (g_gameControlBase)
		g_gameControlBase->DestroyDeviceObjects();

	ASSERT(g_render);
	SAFE_DELETE(g_render);

	g_dialogResourceManager.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pFont9 );
}
//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
	FrankProfilerEntryDefine(L"OnFrameMove()", Color::Yellow(), 0);
	
	static bool lostFocus = false;
	static bool hasUpdated = false;
	bool forceUpdate = false;
	if (!hasUpdated)
	{
		// force an update the first time through
		hasUpdated = true;
		forceUpdate = true;
	}

	if (GetFocus() != DXUTGetHWND())
	{
		if (!lostFocus)
			g_input->ClearRumble();

		lostFocus = true;
		g_lostFocusTimer.Set();

		if (!g_gameControlBase->IsEditMode() && !GameControlBase::updateWithoutWindowFocus && !forceUpdate)
		{
			if (g_guiBase)
				g_guiBase->GetDialog().ClearAllPressed();
			g_editorGui.GetDialog().ClearAllPressed();

			// let it sleep when we lose focus, except when editing
			Sleep(100);
			return;
		}
	}
	else if (lostFocus)
	{
		g_lostFocusTimer.Unset();
		if (g_gameControlBase)
		{
			// wipe out the input
			g_gameControlBase->GetInputControl().Clear();
		}

		lostFocus = false;
		g_lostFocusTimer.Set();
		if (g_gameControlBase && g_reloadAssetsOnFocus)
		{
			// refresh sounds and textures on refocus
			if (g_render)
				g_render->ReloadModifiedTextures();
			if (g_sound)
				g_sound->ReloadModifiedSounds();
		}
	}

	{
		// control hiding the mouse cursor
		if (g_gameControlBase->ShouldHideMouseCursor() && (!DXUTIsWindowed() || g_cameraBase->CameraTest(g_gameControlBase->GetInputControl().GetMousePosWorldSpace())))
			SetCursor(LoadCursor(NULL, NULL));
		else
			SetCursor(LoadCursor(NULL, IDC_ARROW));
	}

	if (g_gameControlBase)
	{
		// save off the backbuffer
		LPDIRECT3DSURFACE9 backBuffer = NULL;
		DXUTGetD3D9Device()->GetRenderTarget(0, &backBuffer);
		
		if (forceUpdate)
			g_gameControlBase->ForceUpdate();
		else
			g_gameControlBase->Update(fElapsedTime);

		// set back the back buffer
		DXUTGetD3D9Device()->SetRenderTarget(0, backBuffer);
		SAFE_RELEASE(backBuffer);

		g_gameControlBase->UpdatePost();
	}
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr;

	FrankProfilerEntryDefine(L"OnFrameRender()", Color::Yellow(), 0);
	CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"OnFrameRender()" );

	// reset verts rendered for debug info
	g_render->ResetTotalSimpleVertsRendered();
	g_terrainRender.renderedPrimitiveCount = 0;
	g_terrainRender.renderedBatchCount = 0;

	// Set up the textures
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,	D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1,	D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2,	D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,	D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1,	D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2,	D3DTA_DIFFUSE );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER,		D3DTEXF_LINEAR );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER,		D3DTEXF_LINEAR );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER,		D3DTEXF_LINEAR );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU,		D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV,		D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSW,		D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER,		D3DTEXF_LINEAR );
	pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER,		D3DTEXF_LINEAR );
	pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER,		D3DTEXF_LINEAR );
	pd3dDevice->SetRenderState( D3DRS_SRCBLEND,				D3DBLEND_SRCALPHA );
	pd3dDevice->SetRenderState( D3DRS_DESTBLEND,			D3DBLEND_INVSRCALPHA );
	pd3dDevice->SetRenderState( D3DRS_BLENDOP,				D3DBLENDOP_ADD );

	pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&mipBias );
	pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&mipBias );
	pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&mipBias );

	// Set miscellaneous render states
	pd3dDevice->SetRenderState( D3DRS_DITHERENABLE,			FALSE );
	pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE,		FALSE );
	pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE,		TRUE);
	pd3dDevice->SetRenderState( D3DRS_ZENABLE,				FALSE);
	pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,			FALSE);
	pd3dDevice->SetRenderState( D3DRS_LIGHTING,				TRUE);
	pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,		FALSE);
	pd3dDevice->SetRenderState( D3DRS_AMBIENT,				0x00FFFFFF );
	pd3dDevice->SetRenderState( D3DRS_CULLMODE,				D3DCULL_NONE);
	pd3dDevice->SetRenderState( D3DRS_DIFFUSEMATERIALSOURCE,D3DMCS_MATERIAL  );

	if (g_gameControlBase)
	{
		CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"GameControlBase::SetupRender()" );

		// save off the backbuffer
		LPDIRECT3DSURFACE9 backBuffer = NULL;
		pd3dDevice->GetRenderTarget(0, &backBuffer);
		
		g_gameControlBase->SetupRender();

		// set back the back buffer
		pd3dDevice->SetRenderTarget(0, backBuffer);
		SAFE_RELEASE(backBuffer);
	}
	
	g_render->SetFiltering();

    // Render the scene
	const bool isEditor = g_gameControlBase? (g_gameControlBase->IsEditMode() && !g_gameControlBase->IsEditPreviewMode()) : false;
	const Color& clearColor = isEditor ? g_editorBackBufferClearColor : g_backBufferClearColor;
    if (g_render->BeginRender(true, clearColor, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER ))
    {
		if (g_gameControlBase)
		{
			{
				CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"GameControlBase::Render()" );
				g_gameControlBase->Render();
				g_render->RenderSimpleVerts();
			}

			{
				CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"GameGui::Render()" );
				FrankProfilerEntryDefine(L"GameGui::Render()", Color::White(), 1);
				DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" ); // These events are to help PIX identify what the code is doing
				pd3dDevice->SetRenderState(D3DRS_LIGHTING, TRUE);
				if (g_gameControlBase->IsGameplayMode())
					V(g_guiBase->Render(fElapsedTime))
				V(g_editorGui.Render(fElapsedTime))
				g_render->RenderSimpleVerts();
				DXUT_EndPerfEvent();
			}
			
			{
				CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"GameControlBase::RenderPost()" );
				g_gameControlBase->RenderPost();
			}
		}

		{
			// render out any simple verts left in the buffer
			g_render->RenderSimpleVerts();
		}
		DXUT_EndPerfEvent();

		g_render->EndRender();
	}
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext )
{
	if (!g_gameControlBase)
		return 0;

	// Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_dialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

	// hack: makew sure input gets the message, the gui was making it get skipped over
	if (uMsg == WM_MOUSEWHEEL)
		g_gameControlBase->GetInputControl().OnMouseMessage(uMsg, wParam);

    // Give the dialogs a chance to handle the message first
	if (g_gameControlBase && g_gameControlBase->IsGameplayMode())
	{
	    *pbNoFurtherProcessing = g_guiBase->MsgProc( hWnd, uMsg, wParam, lParam );
		if( *pbNoFurtherProcessing )
			return 0;
	}
	{
	    *pbNoFurtherProcessing = g_editorGui.MsgProc( hWnd, uMsg, wParam, lParam );
		if( *pbNoFurtherProcessing )
			return 0;
	}

    switch( uMsg )
    {
		case WM_NCACTIVATE:
			break;

		case WM_SETCURSOR:
		{
			//SetCursor(LoadCursor(NULL, IDC_CROSS));
			break;
		}

		case WM_SETFOCUS:
            break;

        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
		{
			SetCapture( hWnd );
			g_gameControlBase->GetInputControl().OnMouseMessage(uMsg, wParam);
            break;
		}

        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
		{
            ReleaseCapture();
			g_gameControlBase->GetInputControl().OnMouseMessage(uMsg, wParam);
            break;
		}

		case WM_MOUSEWHEEL:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
		{
			g_gameControlBase->GetInputControl().OnMouseMessage(uMsg, wParam);
            break;
		}
	}

    return 0;
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
	g_gameControlBase->GetInputControl().OnKeyboard( nChar, bKeyDown, bAltDown, pUserContext );
}