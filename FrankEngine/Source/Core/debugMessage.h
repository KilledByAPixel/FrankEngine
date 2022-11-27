////////////////////////////////////////////////////////////////////////////////////////
/*
	Debug Message System
	Copyright 2013 Frank Force - http://www.frankforce.com
	
	- slides out a message window when a message shows up
	- used by frank engine for important engine level messages
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../core/frankMath.h"

extern class DebugMessageSystem g_debugMessageSystem;

class DebugMessageSystem
{
public:

	DebugMessageSystem();
	
	void Update();
	void Render();
	bool IsMouseOver();
	Box2AABB GetBox();

	void Add(const wstring message, const Color& color);
	void Add(const wstring message);
	void AddFormatted(const wstring messageFormat, ...);
	void AddFormatted(const Color& color, const wstring messageFormat, ...);
	void AddError(const wstring messageFormat, ...);
	void Clear() { messages.clear(); }
	void Close() { openTimer.Unset(); }

	static const int maxMessageLength = 255;
	static const Color defaultMessageColor;

private:

	struct DebugMessage
	{
		wstring message;
		Color color;

		DebugMessage(const wstring _message, const Color& _color) :
			color(_color),
			message(_message)
		{}
	};

	list<DebugMessage> messages;

	GamePauseTimer openTimer;			// is curretly open
	float openPercent;					// used to move into view when it is opened
	bool mouseWasOver;
};
