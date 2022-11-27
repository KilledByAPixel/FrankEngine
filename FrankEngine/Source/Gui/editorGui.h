////////////////////////////////////////////////////////////////////////////////////////
/*
	Editor GUI
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "gui/guiBase.h"

class EditorGui : public GuiBase
{
public:

	friend class TileEditor;
	friend class ObjectEditor;

	void Init() override;
	void OnResetDevice() override;
	HRESULT Render(float delta) override;
	void Refresh() override;
	void Reset() override;

	void ClearFocus();
	bool IsEditBoxFocused();
	int GetTileQuickPickId();
	int GetObjectQuickPickId();

private:

	void NewObjectSelected();
	const WCHAR* GetHoverHelp();
	void ClearEditBox();
	LPCWSTR GetEditBoxText();
	void RenderObjectAttributes();

	int GetTileQuickPickRows() const;
	int GetObjectQuickPickRows() const;
	static float quickPickSize;
	static int quickPickTileColumns;
	static int quickPickObjectColumns;

	GamePauseTimer hoverHelpTimer;
};
