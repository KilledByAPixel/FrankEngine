////////////////////////////////////////////////////////////////////////////////////////
/*
	Game GUI
	Copyright 2013 Frank Force - http://www.frankforce.com

	- This is a simple gui for use with a game made with Frank Engine.
	- The gui will display when player pauses the game by pressing escape.
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "gameGlobals.h"
#include "gameGui.h"
#include "player.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	Game GUI Globals
*/
////////////////////////////////////////////////////////////////////////////////////////

// manager for shared resources of dialogs
extern CDXUTDialogResourceManager g_dialogResourceManager; 
static void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

enum ControlId
{
	ControlId_invalid = -1,

	// static elements
	ControlId_title,

	// button bar
	ControlId_button_play,
	ControlId_button_website,
	ControlId_button_fullscreen,
	ControlId_button_quit,
};


////////////////////////////////////////////////////////////////////////////////////////
/*
	Game GUI Member Functions
*/
////////////////////////////////////////////////////////////////////////////////////////

void GameGui::Init()
{
    mainDialog.Init( &g_dialogResourceManager );
    mainDialog.SetCallback( OnGUIEvent );
    mainDialog.SetFont( 0, L"Arial Bold", 36, FW_NORMAL );
    mainDialog.SetFont( 1, L"Arial Bold", 60, FW_NORMAL );
	mainDialog.EnableKeyboardInput( true );

	{	
		WCHAR output[256];
		swprintf_s(output, 256, gameTitle );
		mainDialog.AddStatic( ControlId_title, output, 0, 0, 0, 0 );
		mainDialog.GetControl( ControlId_title )->SetFont(1);
		mainDialog.GetControl( ControlId_title )->SetTextColor(Color::BuildBytes(255, 255, 255, 0));
		mainDialog.GetControl( ControlId_title )->GetElement( 0 )->dwTextFormat = DT_TOP | DT_CENTER | DT_NOCLIP;
		mainDialog.GetControl( ControlId_title )->SetVisible(true);
		
		mainDialog.AddButton( ControlId_button_play, L"Play", 0, 0, 0, 0, 0, true ); 
		mainDialog.GetControl( ControlId_button_play )->SetVisible(false);
		mainDialog.GetControl( ControlId_button_play )->SetTextColor(Color::White());

		mainDialog.AddButton( ControlId_button_website, L"Website", 0, 0, 0, 0 ); 
		mainDialog.GetControl( ControlId_button_website )->SetVisible(false);
		mainDialog.GetControl( ControlId_button_website )->SetTextColor(Color::White());

		mainDialog.AddButton( ControlId_button_fullscreen, L"Full", 0, 0, 0, 0 ); 
		mainDialog.GetControl( ControlId_button_fullscreen )->SetVisible(false);
		mainDialog.GetControl( ControlId_button_fullscreen )->SetTextColor(Color::White());

		mainDialog.AddButton( ControlId_button_quit, L"Quit", 0, 0, 0, 0 ); 
		mainDialog.GetControl( ControlId_button_quit )->SetVisible(false);
		mainDialog.GetControl( ControlId_button_quit )->SetTextColor(Color::White());
	}

	GameGui::OnResetDevice();
}

void GameGui::Reset()
{
    GuiBase::Reset();
}

void GameGui::Refresh()
{
	if (!g_gameControl)
		return;
	
	const float totalTime = g_gameControl->GetTotalTime();
	{
		// show title
		float fadePercent = 1;
		if (totalTime < 3)
			fadePercent = Percent(totalTime, 1.0f, 3.0f);
		else
			fadePercent = Percent(totalTime, 10.0f, 7.0f);
		mainDialog.GetControl( ControlId_title )->SetTextColor(Color::BuildBytes(255, 255, 255,  (int)(255*fadePercent)));
	}

	{
		// button bar
		const bool isPaused = g_gameControl->IsPaused();
		mainDialog.GetControl( ControlId_button_play )->SetVisible(isPaused);
		mainDialog.GetControl( ControlId_button_website )->SetVisible(isPaused);
		mainDialog.GetControl( ControlId_button_fullscreen )->SetVisible(isPaused);
		mainDialog.GetControl( ControlId_button_quit )->SetVisible(isPaused);
	}
}

void GameGui::OnResetDevice()
{
    mainDialog.SetLocation( 0, 0 );
    mainDialog.SetSize( g_backBufferWidth, g_backBufferHeight );

	{
		int x = g_backBufferWidth / 2;
		int y = 20;
		mainDialog.GetControl( ControlId_title )->SetLocation( x, y );
	}
	{
		// button bar
		mainDialog.GetButton( ControlId_button_fullscreen )->SetText(DXUTIsWindowed()? L"Full" : L"Window");

		Vector2 buttonSize( g_backBufferWidth * 1.0f / 5.2f, 46 );
		buttonSize.x = Min(buttonSize.x, 240.0f);
		
		int x, y;
		y = (int)(g_backBufferHeight - buttonSize.y/2 - 10);

		float buttonCount = 4.0f;
		float buttonOffset = 0.5f;

		x = (int)(g_backBufferWidth * buttonOffset / buttonCount);
		buttonOffset += 1;
		mainDialog.GetControl( ControlId_button_play )->SetLocation( (int)(x - buttonSize.x / 2), (int)(y - buttonSize.y / 2) );
		mainDialog.GetControl( ControlId_button_play )->SetSize( (int)(buttonSize.x),(int)(buttonSize.y) );

		x = (int)(g_backBufferWidth * buttonOffset / buttonCount);
		buttonOffset += 1;
		mainDialog.GetControl( ControlId_button_website )->SetLocation( (int)(x - buttonSize.x / 2), (int)(y - buttonSize.y / 2) );
		mainDialog.GetControl( ControlId_button_website )->SetSize( (int)(buttonSize.x),(int)(buttonSize.y) );

		x = (int)(g_backBufferWidth * buttonOffset / buttonCount);
		buttonOffset += 1;
		mainDialog.GetControl( ControlId_button_fullscreen )->SetLocation( (int)(x - buttonSize.x / 2), (int)(y - buttonSize.y / 2) );
		mainDialog.GetControl( ControlId_button_fullscreen )->SetSize( (int)(buttonSize.x),(int)(buttonSize.y) );

		x = (int)(g_backBufferWidth * buttonOffset / buttonCount);
		buttonOffset += 1;
		mainDialog.GetControl( ControlId_button_quit )->SetLocation( (int)(x - buttonSize.x / 2), (int)(y - buttonSize.y / 2) );
		mainDialog.GetControl( ControlId_button_quit )->SetSize( (int)(buttonSize.x),(int)(buttonSize.y) );
	}
}

void GameGui::RenderFadeOverlay()
{
	// fade in from black on start and fade out on death
	float fadePercent = 0;
	if (g_player->GetDeadTime() > 0)
		fadePercent = Percent(g_player->GetDeadTime(), 0.5f,  2.5f);
	else
		fadePercent = 1 - Percent(g_player->GetLifeTime(), 0.0f, 1.0f);

	// don't show fade in/out when debugging
	if (GameControlBase::devMode)
		fadePercent = 0;

	fadePercent = CapPercent(fadePercent);		
	fadePercent *= fadePercent;
	if (fadePercent > 0)
	{
		Matrix44 matrix = FrankRender::GetScreenSpaceMatrix(0, 0, (float)g_backBufferWidth, (float)g_backBufferHeight);
		g_render->RenderScreenSpaceQuad( matrix, Color(0, 0, 0, fadePercent) );
	}
}

HRESULT GameGui::Render(float delta)
{	
	const bool isPaused = g_gameControl->IsPaused();

	// when paused show the fade overlay before the gui so pause still works
	if (isPaused)
		RenderFadeOverlay();

	mainDialog.OnRender(delta);
	
	if (!isPaused)
		RenderFadeOverlay();

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
static void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
	if (!g_gameControl)
		return;

    switch( nControlID )
    {
		
		case ControlId_button_play:
		{	
			if (g_gameControl->IsPaused())
				g_gameControl->SetPaused(false);
			break;
		}
		case ControlId_button_website:
		{	
			// this code triggers the user's web broswer to open a website
			SHELLEXECUTEINFOW shellInfo;
			ZeroMemory (&shellInfo, sizeof (SHELLEXECUTEINFO));
			shellInfo.cbSize = sizeof(shellInfo);
			shellInfo.fMask = SEE_MASK_ASYNCOK;
			shellInfo.lpVerb = L"open";
			shellInfo.lpFile = gameWebsite;
			shellInfo.nShow = SW_SHOWNORMAL;
			ShellExecuteEx(&shellInfo);
			break;
		}
		case ControlId_button_fullscreen:
		{	
			DXUTToggleFullScreen();
			break;
		}
		case ControlId_button_quit:
		{	
            SendMessage(DXUTGetHWND(), WM_CLOSE, 0, 0);
			break;
		}

		default:
			break;
    }
}