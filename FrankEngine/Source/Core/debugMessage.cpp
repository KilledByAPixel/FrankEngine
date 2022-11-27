////////////////////////////////////////////////////////////////////////////////////////
/*
	Debug Message System
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../core/debugMessage.h"

// the one and only game message system
DebugMessageSystem g_debugMessageSystem;

const Color DebugMessageSystem::defaultMessageColor = Color::White();

ConsoleCommandSimple(int, messageWindowWidth, 500);

DebugMessageSystem::DebugMessageSystem()
{
	openTimer.Unset();
	openPercent = 0;
	mouseWasOver = false;
	
	wstring s = frankEngineName + wstring(L" ") + frankEngineVersion;
	Add(s, Color::Yellow());
	Add(L"", Color::White());
	openTimer.Unset();
}

Box2AABB DebugMessageSystem::GetBox()
{
	const int width = messageWindowWidth;
	const int height = g_backBufferHeight - 546;
	const float offPos = g_gameControlBase->IsGameplayMode()? 0.0f : 20.0f;
	const int x = g_backBufferWidth - int(Lerp(openPercent, offPos, float(width)));
	const int y = 130;
	return Box2AABB(IntVector2(x, y), IntVector2(x + width, y + height));
}

bool DebugMessageSystem::IsMouseOver()
{
	const Vector2 mousePos = g_input->GetMousePosScreenSpace();
	const Box2AABB box = GetBox();
	return (mousePos.x < g_backBufferWidth && box.Contains(mousePos));
}

void DebugMessageSystem::Update()
{
	const Vector2 mousePos = g_input->GetMousePosScreenSpace();

	const float autoCloseTime = 2;
	bool isMouseOver = !g_gameControlBase->IsGameplayMode() && IsMouseOver();
	if (isMouseOver && GetFocus() == DXUTGetHWND())
		openTimer.Set();
	else if (mouseWasOver && GetFocus() == DXUTGetHWND())
		openTimer.SetTimeElapsed(autoCloseTime - 0.1f);

	mouseWasOver = isMouseOver;

	if (openTimer.IsSet() && GetFocus() != DXUTGetHWND())
		openTimer.Set();

	if (openTimer > autoCloseTime)
		openTimer.Unset();

	if (openTimer.IsSet())
		openPercent += 0.1f;
	else
		openPercent += -0.1f;

	openPercent = CapPercent(openPercent);
}

void DebugMessageSystem::Render()
{
	if (openPercent <= 0)
		return;
	
	// fit to height of window
	const int height = g_backBufferHeight - 550;

	const Box2AABB box = GetBox();
	g_render->RenderScreenSpaceQuad(box, Color::Black(0.7f), Texture_Invalid, Color::White());

	g_textHelper->Begin();
	g_textHelper->SetInsertionPos( int(box.lowerBound.x) + 20, int(box.lowerBound.y) );
	
	int maxLines = height / g_textHelper->GetLineHeight();
	int lineCount = 0;
	int messageLines = messages.size();
	for (list<DebugMessage>::iterator it = messages.begin(); it != messages.end(); ) 
	{
		if (++lineCount <= messageLines - maxLines)
		{
			it = messages.erase(it);
			continue;
		}

		const DebugMessage& debugMessage = *it;

		g_textHelper->SetForegroundColor( debugMessage.color );
		g_textHelper->DrawTextLine( debugMessage.message.c_str());

		++it;
	}
	g_textHelper->End();
}

void DebugMessageSystem::Add(const wstring message, const Color& color)
{
	messages.push_back(DebugMessage(message, color));
	openTimer.Set();
}

void DebugMessageSystem::Add(const wstring message)
{
	messages.push_back(DebugMessage(message, defaultMessageColor));
	openTimer.Set();
}

void DebugMessageSystem::AddFormatted(const Color& color, const wstring messageFormat, ...)
{
    va_list args;
    va_start(args, messageFormat);
	WCHAR output[maxMessageLength];
	StringCchVPrintf( output, maxMessageLength, messageFormat.c_str(), args );
    va_end(args);
	output[maxMessageLength - 1] = L'\0';

	DebugMessage debugMessage(output, color);
	messages.push_back(debugMessage);
	openTimer.Set();
}

void DebugMessageSystem::AddFormatted(const wstring messageFormat, ...)
{
    va_list args;
    va_start(args, messageFormat);
	WCHAR output[maxMessageLength];
	StringCchVPrintf( output, maxMessageLength, messageFormat.c_str(), args );
    va_end(args);
	output[maxMessageLength - 1] = L'\0';

	DebugMessage debugMessage(output, defaultMessageColor);
	messages.push_back(debugMessage);
	openTimer.Set();
}

void DebugMessageSystem::AddError(const wstring messageFormat, ...)
{
    va_list args;
    va_start(args, messageFormat);
	WCHAR output[maxMessageLength];
	StringCchVPrintf( output, maxMessageLength, messageFormat.c_str(), args );
	output[maxMessageLength - 1] = L'\0';
    va_end(args);

	DebugMessage debugMessage(output, Color::Red());
	messages.push_back(debugMessage);
	openTimer.Set();
}