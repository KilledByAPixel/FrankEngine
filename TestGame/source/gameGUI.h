////////////////////////////////////////////////////////////////////////////////////////
/*
	Game GUI
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class GameGui : public GuiBase
{
public:

	void Init() override;
	void OnResetDevice() override;
	HRESULT Render(float delta) override;
	void Refresh() override;
	void Reset() override;

private:

	void RenderFadeOverlay();
};
