////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Utilities
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
	
ConsoleCommandSimple(bool, trajectoryDebug, false);

string TrimString(const string& str, const string& whitespace)
{
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

wstring TrimString(const wstring& str, const wstring& whitespace)
{
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == string::npos)
        return L""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

namespace FrankUtil
{

// simple way to aim a weapon taking gravity and owner velocity into account
float GetPlayerAimAngle(const Vector2& ownerPos, const Vector2& ownerVelocity, float speed)
{
	float angle = 0;

	// update weapon aiming
	if (g_gameControlBase->IsUsingGamepad())
	{
		// gamepad control
		Vector2 gamepadRightStick = g_input->GetGamepadRightStick(0);
		gamepadRightStick.y -= 0.007f * Terrain::gravity.y;
		gamepadRightStick = gamepadRightStick.Rotate(g_cameraBase->GetAngleWorld());
		angle = gamepadRightStick.Normalize().GetAngle();
	}
	else if (speed == 0)
	{
		angle = (g_input->GetMousePosWorldSpace() - ownerPos).GetAngle();
	}
	else
	{
		// automatically aim gun at best possible trajectory to hit mouse pos
		const Vector2 mousePos = g_input->GetMousePosWorldSpace();

		// calculate the trajectory angle to hit the mouse pos
		bool canHit = FrankUtil::GetTrajectoryAngleWorld(ownerPos, ownerVelocity, mousePos, speed, angle);
		if (!canHit)
		{
			// if we cant hit mouse just try to shoot farthest towards target as possible
			Vector2 startPos = ownerPos;
			Vector2 endPos = mousePos;
			if (trajectoryDebug)
				mousePos.RenderDebug();
			for (int i = 0; i < 20; ++i)
			{
				// iterate to find a close solution

				// test half way betwen start and end
				const Vector2 testPos = startPos + (endPos - startPos) * 0.5f;
				canHit = FrankUtil::GetTrajectoryAngleWorld(ownerPos, ownerVelocity, testPos, speed, angle);
				if (canHit)
				{
					if (trajectoryDebug)
						testPos.RenderDebug(Color::Green());
					startPos = testPos;
				}
				else
				{
					if (trajectoryDebug)
						testPos.RenderDebug(Color::Yellow());
					endPos = testPos;
				}
			}
			if (trajectoryDebug)
				startPos.RenderDebug(Color::Red());

			// get the best trajectory
			canHit = FrankUtil::GetTrajectoryAngleWorld(ownerPos, ownerVelocity, startPos, speed, angle);
		}

		if (!canHit)
		{
			// just point the weapon in the direction of the mouse
			const Vector2 deltaPos = mousePos - ownerPos;
			angle = deltaPos.GetAngle();
		}
	}
	
	return angle;
}

// returns trajectory angles for a deltaPos with gravity in the -y direction
// returns false if it is not possible to hit the target
static bool GetTrajectoryAngle(Vector2 deltaPos, float speed, float gravity, float& angle1, float& angle2)
{
	if (gravity == 0)
	{
		angle1 = angle2 = deltaPos.GetAngle();
		return true;
	}

	// equation from wikipedia article on trajectory of a projectile under "Angle required to hit coordinate (x,y)"
	const float speed2 = speed * speed;
	const float temp = speed2 * speed2 - gravity * (gravity * deltaPos.x * deltaPos.x + 2 * deltaPos.y * speed2);

	if (temp <= 0)
		return false;

	if (gravity * deltaPos.x == 0)
		return false;

	angle1 = atanf((speed2 + sqrtf(temp)) / (gravity * deltaPos.x));
	angle2 = atanf((speed2 - sqrtf(temp)) / (gravity * deltaPos.x));
	if (deltaPos.x > 0)
	{
		// reverse the angle when shooting to the left
		angle1 -= PI;
		angle2 -= PI;
	}

	// correct for space where 0 radians is up instead of right
	angle1 += PI/2;
	angle2 += PI/2;

	return true;
}

static bool GetTrajectoryAngleWorldInternal(const Vector2& startPos, const Vector2& targetPos, float speed, float& angle, float gravityScale)
{
	// just use gravity at the start position to simplify
	const Vector2 gravity = g_gameControlBase->GetGravity(startPos);

	// get transform where gravity is down
	float gravityAngle = (-gravity).GetAngle();

	// put delta pos in gravity space
	const Vector2 deltaPos = (targetPos - startPos).Rotate(-gravityAngle);

	// calculate trajectory
	float angle1, angle2;
	if (!GetTrajectoryAngle(deltaPos, speed, gravityScale*gravity.Length(), angle1, angle2))
	{
		//Line2(startPos, startPos + 20 * Vector2::BuildFromAngle(angle)).RenderDebug(Color::White());
		return false;
	}

	// figure out which angle is closer to a straight shot
	const float targetAngle = deltaPos.GetAngle();
	const float deltaAngle1 = CapAngle(angle1 - targetAngle);
	const float deltaAngle2 = CapAngle(angle2 - targetAngle);
	angle = (fabs(deltaAngle1) < fabs(deltaAngle2))? angle1 : angle2;

	// put result angle back in world space
	angle += gravityAngle;

	// debug render
	if (trajectoryDebug)
	{
		Line2(startPos, startPos + 20 * Vector2::BuildFromAngle(angle)).RenderDebug(Color::White());
		Line2(startPos, startPos + 10 * Vector2::BuildFromAngle(angle1 + gravityAngle)).RenderDebug(Color::Red());
		Line2(startPos, startPos + 10 * Vector2::BuildFromAngle(angle2 + gravityAngle)).RenderDebug(Color::Blue());
		Line2(startPos, startPos + 10*Vector2::BuildFromAngle(targetAngle + gravityAngle)).RenderDebug(Color::Yellow());
	}
	
	if (trajectoryDebug && speed > 0)
	{
		// trajectory display
		float deltaTime = 0.1f;
		float time = 0;
		Vector2 velocityStart = Vector2::BuildFromAngle(angle) * speed;
		Vector2 posStart = startPos;
		Vector2 pos = posStart;
		Vector2 lastPos = pos;
		while (time < 2)
		{
			pos = posStart + velocityStart * time + 0.5f * gravityScale*g_gameControlBase->GetGravity(lastPos) * time * time;
			Line2(lastPos, pos).RenderDebug();
			lastPos = pos;
			time += deltaTime;
		}
	}

	return true;
}

bool GetTrajectoryAngleWorld(const Vector2& startPos, const Vector2& targetPos, float speed, float& angle, float gravityScale)
{
	if (GetTrajectoryAngleWorldInternal(startPos, targetPos, speed, angle, gravityScale))
		return true;

	// set angle to be closest to target as possible
	// if we cant hit mouse just try to shoot farthest towards target as possible
	Vector2 pos0 = startPos;
	Vector2 pos1 = targetPos;
	for (int i = 0; i < 10; ++i)
	{
		// iterate to find a close solution
		// test half way betwen start and end
		const Vector2 testPos = Lerp(0.5f, pos0, pos1);
		bool canHit = FrankUtil::GetTrajectoryAngleWorldInternal(startPos, testPos, speed, angle, gravityScale);
		if (canHit)
			pos0 = testPos;
		else
			pos1 = testPos;
		//testPos.RenderDebug(canHit ? Color::Green() : Color::Yellow());
	}
	
	// get the best trajectory
	return FrankUtil::GetTrajectoryAngleWorldInternal(startPos, pos0, speed, angle, gravityScale);
}

bool GetTrajectoryAngleWorld(const GameObject& attacker, const GameObject& target, float speed, float& angle, float gravityScale)
{
	ASSERT(speed >= 0);

	const Vector2 startPos = attacker.GetPosWorld();
	const Vector2 endPos = target.GetPosWorld();
	const Vector2 deltaPos = endPos - startPos;
	Vector2 deltaVelocity = target.GetVelocity();
	if (Projectile::applyAttackerVelocity)
		deltaVelocity -= attacker.GetVelocity();

	// estimate where target would be if we had a straight shot
	float time = 0;
	for (int i = 0; i < 2; ++i)
	{
		// iterate a few times to get a better estimate
		time = (deltaPos + time * deltaVelocity).Length() / speed;
		//(endPos + time * target.GetVelocity()).RenderDebug();
	}

	return GetTrajectoryAngleWorld(startPos, endPos + time * deltaVelocity, speed, angle, gravityScale);
}

bool GetTrajectoryAngleWorld(const GameObject& attacker, const Vector2& targetPos, float speed, float& angle, float gravityScale)
{
	const Vector2 startPos = attacker.GetPosWorld();
	const Vector2 endPos = targetPos;
	const Vector2 deltaPos = endPos - startPos;
	const Vector2 deltaVelocity = Projectile::applyAttackerVelocity ? -attacker.GetVelocity() : Vector2(0);

	// estimate where target would be if we had a straight shot
	float time = 0;
	for (int i = 0; i < 2; ++i)
	{
		// iterate a few times to get a better estimate
		time = (deltaPos + time * deltaVelocity).Length() / speed;
		//(endPos + time * target.GetVelocity()).RenderDebug();
	}

	return GetTrajectoryAngleWorld(startPos, endPos + time * deltaVelocity, speed, angle, gravityScale);
}

bool GetTrajectoryAngleWorld(const Vector2& startPos, const Vector2& attackerVelocity, const Vector2& targetPos, float speed, float& angle, float gravityScale)
{
	const Vector2 endPos = targetPos;
	const Vector2 deltaPos = endPos - startPos;
	const Vector2 deltaVelocity = Projectile::applyAttackerVelocity ? -attackerVelocity : Vector2(0);

	// estimate where target would be if we had a straight shot
	float time = 0;
	for (int i = 0; i < 2; ++i)
	{
		// iterate a few times to get a better estimate
		time = (deltaPos + time * deltaVelocity).Length() / speed;
		//(endPos + time * target.GetVelocity()).RenderDebug();
	}

	return GetTrajectoryAngleWorld(startPos, endPos + time * deltaVelocity, speed, angle, gravityScale);
}

void CopyToClipboard(const WCHAR* text, int maxLength)
{
	if (!text || !OpenClipboard(NULL))
		return;

    EmptyClipboard();

	int length = (int)wcslen(text);
	if (maxLength > 0)
		Min(length, maxLength);
    HGLOBAL hBlock = GlobalAlloc(GMEM_MOVEABLE, sizeof(WCHAR) * (length + 1));
    if (hBlock)
    {
        WCHAR *pwszText = (WCHAR*)GlobalLock( hBlock );
        if (pwszText)
        {
            CopyMemory(pwszText, text, length * sizeof(WCHAR));
            pwszText[length] = L'\0';
            GlobalUnlock(hBlock);
        }
        SetClipboardData(CF_UNICODETEXT, hBlock);
    }

    CloseClipboard();
    // We must not free the object until CloseClipboard is called.
    if (hBlock)
        GlobalFree(hBlock);
}

void PasteFromClipboard(WCHAR* text, int maxLength)
{
	if (!OpenClipboard(NULL) || maxLength <= 0)
		return;

    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle)
    {
        WCHAR *pwszText = (WCHAR*)GlobalLock(handle);
        if (pwszText)
        {
			// hack: get rid of new lines and carrige returns, use them as delimiters
			for(int i = 0; i < maxLength; ++i)
			{
				WCHAR c = pwszText[i];
				if (c == 10 || c == 13 || c == 0)
					c = L'\0';
				if (c == 8217)
					c = L'\'';

				text[i] = c;
			}
            GlobalUnlock( handle );
        }
    }
    CloseClipboard();
}

};	// namespace FrankUtil

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Object Attributes Parser
//

bool ObjectAttributesParser::SkipToNextValue(char stringMarker)
{
	// skips over the next value, returns true if a value was found
	ASSERT(string);

	// skip start white space
	SkipWhiteSpace();

	// skip to next value
	if (!(*string))
		return false;

	if (*string == stringMarker)
	{
		++string;
		// skip until another string marker
		while(*string && (*string != stringMarker)) ++string;
	}
	else
	{
		// skip any non-white space
		while(*string && (*string != ' ')) ++string;
	}
	
	// skip end white space
	SkipWhiteSpace();
	
	return true;
}

bool ObjectAttributesParser::SkipToMarker(char marker)
{
	ASSERT(string);

	while(1)
	{
		if (!*string)
			return false;
		else if (*string == marker)
		{
			++string;
			return true;
		}
		++string;
	}
}

void ObjectAttributesParser::ParseValue(int& _value)
{
	ASSERT(string);
		
	SkipWhiteSpace();
	
	float valueFloat = float(_value);
	ParseValue(valueFloat);
	_value = int(valueFloat);
}

void ObjectAttributesParser::ParseValue(unsigned& _value)
{
	int valueInt = _value;
	ParseValue(valueInt);
	if (valueInt >= 0)
		_value = valueInt;
	else
		_value = 0;
}

void ObjectAttributesParser::ParseValue(unsigned char& _value)
{
	int valueInt = _value;
	ParseValue(valueInt);
	if (valueInt <= 255 && valueInt >= 0)
		_value = valueInt;
	else
		_value = 0;
}

void ObjectAttributesParser::ParseValue(char& _value)
{
	int valueInt = _value;
	ParseValue(valueInt);
	if (valueInt <= 127 && valueInt >= -127)
		_value = valueInt;
	else
		_value = 0;
}

void ObjectAttributesParser::ParseValue(short& _value)
{
	int valueInt = _value;
	ParseValue(valueInt);
	_value = valueInt;
}

void ObjectAttributesParser::ParseValue(Vector2& _value)
{
	ParseValue(_value.x);
	ParseValue(_value.y);
}

void ObjectAttributesParser::ParseValue(IntVector2& _value)
{
	ParseValue(_value.x);
	ParseValue(_value.y);
}

void ObjectAttributesParser::ParseValue(ByteVector2& _value)
{
	ParseValue(_value.x);
	ParseValue(_value.y);
}

void ObjectAttributesParser::ParseValue(Color& _value)
{
	ParseValue(_value.r);
	ParseValue(_value.g);
	ParseValue(_value.b);
	ParseValue(_value.a);
}

void ObjectAttributesParser::ParseValue(bool& _value)
{
	int valueInt = _value;
	ParseValue(valueInt);
	_value = (valueInt != 0);
}

void ObjectAttributesParser::ParseValue(float& _value)
{
	ASSERT(string);
		
	SkipWhiteSpace();

	// check the sign
	float sign = 1;
	if (*string == '-')
	{
		sign = -1;
		++string;
	}

	if ((*string < '0' || *string > '9') &&  *string != '.')
	{
		// value is not changed if a number isn't found
		if (*string != '#')
			SkipToNextValue();
		return;
	}

	// read in the integer part
	float value = 0;
	while (*string >= '0' && *string <= '9')
	{
		value = 10*value + *string - '0';
		++string;
	}
	
	// check for a decimal point
	if (*string == '.')
	{
		// skip the decimal point
		++string;
	}
	else
	{
		// save the value
		_value = value * sign;
		return;
	}
	
	// read in the fractional part
	float place = 0;
	while (*string >= '0' && *string <= '9')
	{
		float a =  (*string - '0') / (powf(10, ++place));
		value = value + a;
		++string;
	}
		
	// save the value
	_value = value * sign;
}

void ObjectAttributesParser::ParseTextureName(TextureID& ti)
{
	// read in a string and convert it to an asset image
	SkipWhiteSpace();
	
	int i = 0;
	char imageName[256];
	while (*string != ' ' && *string != '#' && *string != '\0')
	{
		// convert to lower case
		char c = *string;
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		imageName[i] = c;
		++i;
		++string;
	} 
	imageName[i] = '\0';

	// look for a match
	wchar_t nameWide[256];
	mbstowcs_s(NULL, nameWide, 256, imageName, _TRUNCATE);
	ti = g_render->FindTextureID(nameWide);
}

void ObjectAttributesParser::ParseSoundName(SoundID& sid)
{
	// read in a string and convert it to an asset sound
	SkipWhiteSpace();
	
	int i = 0;
	char imageName[256];
	while (*string != ' ' && *string != '#' && *string != '\0')
	{
		// convert to lower case
		char c = *string;
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		imageName[i] = c;
		++i;
		++string;
	} 
	imageName[i] = '\0';

	// look for a match
	wchar_t nameWide[256];
	mbstowcs_s(NULL, nameWide, 256, imageName, _TRUNCATE);
	if (g_sound)
		sid = g_sound->FindSoundID(nameWide);
}

void ObjectAttributesParser::GetStringPrintableCopy(char* text)
{
	strncpy_s(text, GameObjectStub::attributesLength, string, GameObjectStub::attributesLength);

	// replace /n with new lines
	int j = 0;
	for(int i = 0; i < GameObjectStub::attributesLength-2; ++i)
	{
		if (text[i] == '\\' && text[i+1] == 'n')
		{
			++i;
			text[i] = 10;
		}

		text[j] = text[i];
		++j;
	}
	text[j] = 0;
}