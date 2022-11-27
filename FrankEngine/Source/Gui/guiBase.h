////////////////////////////////////////////////////////////////////////////////////////
/*
	GUI Base
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class GuiBase
{
public:
	
	GuiBase();
	virtual ~GuiBase()	{}
	virtual void Init() {}
	virtual void OnResetDevice() {}
	virtual void OnLostDevice() {}
	virtual HRESULT Render(float delta) { return S_OK; }
	virtual void Refresh() {}
	virtual void Reset() { mainDialog.ClearFocus(); mainDialog.FocusDefaultControl(); }

	virtual bool MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
	{ return mainDialog.MsgProc(hWnd, uMsg, wParam, lParam); }

	CDXUTDialog & GetDialog() { return mainDialog; }
	CDXUTDialog mainDialog;
};
