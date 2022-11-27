////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Utilities
	Copyright 2013 Frank Force - http://www.frankforce.com

	- Utility library, helper functions for game stuff
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../core/frankMath.h"

#define DEBUG_OUTPUT( v ) _DEBUG_OUTPUT( #v, v )
#define _DEBUG_OUTPUT( n, v )					\
{												\
	std::wostringstream string;					\
	string << (n) << " = " << (v) << L"\n";		\
	OutputDebugStringW(string.str().c_str());	\
}

// string functions
string TrimString(const string& str, const string& whitespace = " \t");
wstring TrimString(const wstring& str, const wstring& whitespace = L" \t");

namespace FrankUtil
{
	// returns best angle to hit target
	// returns false if it is not possible to hit the target
	bool GetTrajectoryAngleWorld(const Vector2& startPos, const Vector2& targetPos, float speed, float& angle, float gravityScale = 1);
	bool GetTrajectoryAngleWorld(const GameObject& attacker, const GameObject& target, float speed, float& angle, float gravityScale = 1);
	bool GetTrajectoryAngleWorld(const GameObject& attacker, const Vector2& targetPos, float speed, float& angle, float gravityScale = 1);
	bool GetTrajectoryAngleWorld(const Vector2& startPos, const Vector2& attackerVelocity, const Vector2& targetPos, float speed, float& angle, float gravityScale = 1);

	// simple way to aim a weapon taking gravity and owner velocity into account (speed 0 will aim direct)
	float GetPlayerAimAngle(const Vector2& ownerPos, const Vector2& ownerVelocity, float speed = 0);

	inline Vector2 CalculateOrbitalVelocity(const Vector2& pos, float planetGravityConstant)
	{
		if (!g_terrain->isCircularPlanet)
			return Vector2::Zero();

		// use distance from origin
		Vector2 direction = pos;
		const float distance = direction.LengthAndNormalizeThis();
		if (distance < 10)
			return Vector2::Zero(); // prevent divide by zero

		const float speed = sqrtf(planetGravityConstant / distance);
		return speed * direction.Rotate(-PI/2);
	}
	
	void CopyToClipboard(const WCHAR* text, int maxLength = -1);
	void PasteFromClipboard(WCHAR* text, int maxLength);

};	// namespace FrankUtil

struct ObjectAttributesParser
{
	ObjectAttributesParser(const char* _string) : string(_string), stringStart(_string) {}
	
	void ParseValue(float& _value);
	void ParseValue(int& _value);
	void ParseValue(unsigned& _value);
	void ParseValue(short& _value);
	void ParseValue(unsigned char& _value);
	void ParseValue(char& _value);
	void ParseValue(bool& _value);
	void ParseValue(Color& _value);
	void ParseValue(Vector2& _value);
	void ParseValue(IntVector2& _value);
	void ParseValue(ByteVector2& _value);
	void ParseValue(GameObjectType& _value) { ParseValue((int&)_value); }
	void ParseTextureName(TextureID& _value);
	void ParseSoundName(SoundID& _value);

	bool SkipToNextValue(char stringMarker = 0);
	bool SkipToMarker(char marker);
	void SkipWhiteSpace() { ASSERT(string); while(*string && (*string == ' ' || *string == '\n')) ++string; }

	void SetString(const char* _string) { string = _string; }
	const char* GetString() const { return string; }
	const char* GetStringStart() const { return stringStart; }
	void GetStringPrintableCopy(char* text);
	
	int GetOffset() const { return string - stringStart; }
	void MoveBack() { if (string > stringStart) --string; }
	bool IsAtEnd() { ASSERT(string); return *string == 0 || *string == '#'; }

private:

	const char* string;
	const char* stringStart;
};