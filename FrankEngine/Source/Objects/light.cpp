////////////////////////////////////////////////////////////////////////////////////////
/*
	2D Dynamic Lighting System
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../rendering/frankRender.h"
#include "light.h"

float Light::haloAlpha = 0.5f;
ConsoleCommand(Light::haloAlpha, lightHaloAlpha);

float Light::defaultHaloRadius = 0;
ConsoleCommand(Light::defaultHaloRadius, lightHaloRadius);

TextureID Light::defaultHaloTexture = Texture_Dot;

////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Light
*/
////////////////////////////////////////////////////////////////////////////////////////

Light::Light(const GameObjectStub& stub) :
	GameObject(stub),
	color(Color::White(0.7f)),
	radius(DeferredRender::defaultLightRadius),
	overbrightRadius(0),
	haloRadius(defaultHaloRadius),
	height(DeferredRender::shadowLightHeightDefault),
	coneAngle(0),
	coneFadeAngle(0),
	coneFadeColor(0),
	castShadows(true),
	fadeOut(false),
	fadeIn(false),
	isPersistant(false),
	wasRendered(false),
	wasCreatedFromStub(true),
	isActive(true),
	gelTexture(Texture_Invalid),
	haloTexture(defaultHaloTexture)
{
	int renderGroup = GetRenderGroup();

	// radius, red, green, blue, alpha, overbrightRadius, haloRadius, coneAngle, coneFadeAngle, castShadows, renderGroup, height, isActive, gelTexture
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(radius);
	parser.ParseValue(color);
	parser.ParseValue(overbrightRadius);
	parser.ParseValue(haloRadius);
	parser.ParseValue(coneAngle);
	parser.ParseValue(coneFadeAngle);
	parser.ParseValue(castShadows);
	parser.ParseValue(renderGroup);
	parser.ParseValue(height);
	parser.ParseValue(isActive);
	if (parser.SkipToMarker('#'))
		parser.ParseTextureName(gelTexture);

	// protect against bad values
	SetRenderGroup(renderGroup);
	radius = Max(radius, 0.0f);
	color = color.CapValues();
	overbrightRadius = Max(overbrightRadius, 0.0f);
	haloRadius = Max(haloRadius, 0.0f);
	coneAngle = Cap(coneAngle, 0.0f, 2*PI);
	coneFadeAngle = Cap(coneFadeAngle, 0.0f, 2*PI);
	if (coneAngle == 0 && coneFadeAngle == 0)
		coneAngle = 2*PI; // no cone
	gelTexture = Max(gelTexture, Texture_Invalid);
}
	
Light::Light(const XForm2& xf, GameObject* _parent, float _radius, const Color& _color, bool _castShadows) :
	GameObject(xf, _parent),
	color(_color),
	radius(_radius),
	overbrightRadius(0),
	haloRadius(0),
	height(DeferredRender::shadowLightHeightDefault),
	coneAngle(2*PI),
	coneFadeAngle(0),
	coneFadeColor(0),
	castShadows(_castShadows),
	fadeOut(false),
	fadeIn(false),
	isPersistant(false),
	wasRendered(false),
	wasCreatedFromStub(false),
	isActive(true),
	gelTexture(Texture_Invalid),
	haloTexture(defaultHaloTexture)
{
	// render halos on top of everything
	SetRenderGroup(10000);
}

void Light::Update()
{
	if (fadeTimer.HasElapsed())
	{
		fadeTimer.Unset();
		if (fadeOut || !fadeIn)
		{
			Kill();
			return;
		}
	}
	
	if (DeferredRender::lightDebug && !DeferredRender::renderPass && radius > 0 && wasRendered && isActive)
	{
		const XForm2 xf = GetXFWorld();
		xf.position.RenderDebug(color);
		Circle(xf.position, overbrightRadius).RenderDebug(color);
	
		if (coneAngle < 2*PI)
		{
			Cone(xf, radius, coneAngle + coneFadeAngle).RenderDebug(color.ScaleAlpha(0.3f));
			Cone(xf, radius, coneAngle).RenderDebug(color);
		}
		else
		{
			Circle(xf.position, radius).RenderDebug(color);
		}
	}
}

void Light::Render()
{
	if (!isActive)
		return;
	
	if (DeferredRender::lightEnable && haloRadius > 0)
	{
		// render a light halo texture
		const XForm2 xf = GetXFInterpolated();
		DeferredRender::EmissiveRenderBlock emissiveRenderBlock;
		DeferredRender::AdditiveRenderBlock additiveRenderBlock;
		Color haloColor = color;
		haloColor.a *= GetFadeAlpha() * haloAlpha;
		g_render->RenderQuad(xf.position, Vector2(haloRadius), haloColor, haloTexture);
	}
}

bool Light::IsSimpleLight() const
{
	// dynamic lights need special rendering for shadows or cone
	return !(castShadows || (coneAngle < 2*PI));
}

float Light::GetFadeAlpha() const
{
	if (!fadeTimer.IsSet())
		return 1;

	// scale alpha if light is fading
	const float p = fadeTimer;
	if (fadeIn)
	{
		if (fadeOut)
		{
			// fading in then out again
			return sinf(p*PI);
		}
		else
			return p;
	}
	else if (fadeOut)
		return 1 - p;

	// light can be timing out but not fading
	return 1;
}

void Light::Kill()
{
	if (wasCreatedFromStub)
		g_terrain->RemoveStub(GetHandle(), g_terrain->GetPatch(GetPosWorld()));
	Destroy();
}

void Light::StubRender(const GameObjectStub& stub, float alpha)
{
	// show a preview of the light radius and colors when editing
	float radius = DeferredRender::defaultLightRadius;
	Color color = Color::White(0.8f);
	float overbrightRadius = 0;
	float haloRadius = 0;
	float coneAngle = 0;
	float coneFadeAngle = 0;
	TextureID gelTexture = Texture_Invalid;

	// radius, red, green, blue, alpha, overbrightRadius, haloRadius, coneAngle, coneFadeAngle, castShadows, renderGroup, height, isActive, gelTexture
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(radius);
	
	if (!g_cameraBase->CameraTest(stub.xf.position, radius))
		return;

	parser.ParseValue(color);
	parser.ParseValue(overbrightRadius);
	parser.ParseValue(haloRadius);
	parser.ParseValue(coneAngle);
	parser.ParseValue(coneFadeAngle);
	if (parser.SkipToMarker('#'))
		parser.ParseTextureName(gelTexture);
	
	color.a *= alpha;
	color = color.CapValues();
	coneAngle = Cap(coneAngle, 0.0f, 2*PI);
	coneFadeAngle = Cap(coneFadeAngle, 0.0f, 2*PI);
	if (coneAngle == 0)
		coneAngle = 2*PI;

	g_render->RenderQuad(stub.xf, stub.size, color, stub.GetObjectInfo().GetTexture());
	{
		g_render->SetSimpleVertsAreAdditive(true);
		g_render->DrawSolidCone(stub.xf, radius, coneAngle, color.ScaleAlpha(alpha*0.3f), color.ZeroAlpha());
		g_render->SetSimpleVertsAreAdditive(false);
	}
	
	if (!stub.IsThisTypeSelected())
	{
		// only show preview when this object type is selected
		return;
	}

	if (coneAngle > 0 || coneFadeAngle > 0)
	{
		if (coneAngle > 0)
			g_render->DrawCone(stub.xf, radius, coneAngle, color.ScaleAlpha(0.3f));
		if (coneFadeAngle > 0)
			g_render->DrawCone(stub.xf, radius, coneAngle + coneFadeAngle, color.ScaleAlpha(0.1f));
	}
	else
		g_render->DrawCircle(stub.xf, radius, color.ScaleAlpha(0.2f));

	g_render->DrawCircle(stub.xf, Vector2(overbrightRadius), color);

	if (gelTexture != Texture_Invalid)
	{
		DeferredRender::AdditiveRenderBlock additiveRenderBlock;
		g_render->RenderQuad(stub.xf, Vector2(radius), color, gelTexture);
	}

}
bool Light::SortCompare(const Light* first, const Light* second) 
{ 
	// non fading lights are first, and sorted by radius
	if (first->fadeOut == false && second->fadeOut == true)
		return true;
	if (first->fadeOut == true && second->fadeOut == false)
		return false;
	return first->radius > second->radius;
}