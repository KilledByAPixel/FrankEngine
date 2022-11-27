////////////////////////////////////////////////////////////////////////////////////////
/*
	2D Dynamic Lighting System
	Copyright 2013 - Frank Force
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class Light : public GameObject
{
public:
	
	Light(const GameObjectStub& stub);
	Light(const XForm2& xf, GameObject* _parent, float radius, const Color& color, bool castShadows = true);

	void SetRadius(float _radius)					{ radius = _radius; }
	void SetHaloRadius(float _radius)				{ haloRadius = _radius; }
	void SetOverbrightRadius(float _radius)			{ overbrightRadius = _radius; }
	void SetHeight(float _height)					{ height = _height; }
	void SetColor(const Color& _color)				{ color = _color; }
	void SetAlpha(float alpha)						{ color.a = alpha; }
	void SetCastShadows(bool _castShadows)			{ castShadows = _castShadows; }
	void SetIsPersistant(bool _isPersistant)		{ isPersistant = _isPersistant; }
	void SetGelTexture(TextureID _gelTexture)	{ gelTexture = _gelTexture; }
	void SetHaloTexture(TextureID _haloTexture)	{ haloTexture = _haloTexture; }
	void SetConeAngles(float _coneAngle = PI, float _coneFadeAngle=0, float _coneFadeColor = 0) 
	{ 
		ASSERT(_coneAngle >= 0 && _coneAngle <= PI && _coneFadeAngle >= 0 && _coneFadeAngle <= PI && _coneFadeColor >= 0 && _coneFadeColor <= 1);
		coneAngle = _coneAngle; coneFadeAngle = _coneFadeAngle; coneFadeColor = _coneFadeColor; 
		coneFadeAngle = Min(coneFadeAngle, PI - coneAngle);
	}
	void SetFadeTimer(float time, bool _fadeOut = true, bool _fadeIn = false)					{ fadeTimer.Set(time); fadeOut = _fadeOut; fadeIn = _fadeIn; }
	void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0) override	{ isActive = activate; }

	float GetRadius() const							{ return radius; }
	float GetHeight() const							{ return height; }
	float GetHaloRadius() const						{ return haloRadius; }
	float GetOverbrightRadius() const				{ return overbrightRadius; }
	float GetConeAngle() const						{ return coneAngle; }
	float GetConeFadeAngle() const					{ return coneFadeAngle; }
	Color GetColor() const							{ return color; }
	bool  GetCastShadows() const					{ return castShadows; }
	float GetFadeAlpha() const;
	bool  GetIsActive() const						{ return isActive; }
	
	bool IsLight() const { return true; }
	bool IsSimpleLight() const;

	static float haloAlpha;
	static float defaultHaloRadius;
	static TextureID defaultHaloTexture;
	
public:	// stub function

	static void StubRender(const GameObjectStub& stub, float alpha);
	static WCHAR* StubAttributesDescription() { return L"radius r g b a overbright halo cone coneFade shadows renderGroup height isActive #gelTexture"; }
	static WCHAR* StubDescription() { return L"Dynamic game light"; }
	static char* StubAttributesDefault() { return "10 1 1 1 0.7"; }

private:

	void Update() override;
	void Render() override;
	bool ShouldStreamOut() const override { return isPersistant ? false : GameObject::ShouldStreamOut(); }
	void Kill() override;
	static bool SortCompare(const Light* first, const Light* second);

	Color color;
	float radius;
	float overbrightRadius;
	float haloRadius;
	float height;
	float coneAngle;
	float coneFadeAngle;
	float coneFadeColor;
	bool castShadows;
	bool fadeOut;
	bool fadeIn;
	bool isPersistant;
	bool wasRendered;
	bool wasCreatedFromStub;
	bool isActive;
	TextureID gelTexture;
	TextureID haloTexture;
	GameTimerPercent fadeTimer;

	friend DeferredRender;
};
