////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Sound Engine
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../sound/soundControl.h"

SoundControl* g_sound = NULL;

float SoundControl::soundVolumeScale = 1;

bool SoundControl::soundEnable = true;
ConsoleCommand(SoundControl::soundEnable, soundEnable);

bool SoundControl::soundDebug = false;
ConsoleCommand(SoundControl::soundDebug, soundDebug);

float SoundControl::masterVolume = 1.0f;
ConsoleCommand(SoundControl::masterVolume, soundVolume);

float SoundControl::rollOffFactor = 2.0f;
ConsoleCommand(SoundControl::rollOffFactor, soundRollOffFactor);

float SoundControl::dopplerFactor = 1.0f;
ConsoleCommand(SoundControl::dopplerFactor, soundDopplerFactor);

float SoundControl::dopplerDistanceFactor = 1.0f;
ConsoleCommand(SoundControl::dopplerDistanceFactor, soundDopplerDistanceFactor);

bool SoundControl::enableTimeScale = true;
ConsoleCommand(SoundControl::enableTimeScale, soundEnableTimeScale);

bool SoundControl::cameraIsListener = true;
ConsoleCommand(SoundControl::cameraIsListener, soundCameraIsListener);

int SoundControl::maxBufferCount = 8;
ConsoleCommand(SoundControl::maxBufferCount, soundMaxBufferCount);

bool SoundControl::soundDistanceFade = false;
ConsoleCommand(SoundControl::soundDistanceFade, soundDistanceFade);

float SoundControl::soundDistanceFadePow = 0.1f;
ConsoleCommand(SoundControl::soundDistanceFadePow, soundDistanceFadePow);

////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Sound Member Functions
*/
////////////////////////////////////////////////////////////////////////////////////////

SoundControl::SoundControl()
{
	ASSERT(!g_sound);

    // We need to set up DirectSound after we have a window.
	soundManager = new CSoundManager;
    soundManager->Initialize( DXUTGetHWND(), DSSCL_PRIORITY );

	// clear the sound list
	for (int i = 0; i < maxSoundAssets; i++)
		soundAssets[i] = SoundAsset();
	
	ResetSoundObjects();
	musicPlayer.SetDirectSound(soundManager->GetDirectSound());
}

SoundControl::~SoundControl()
{
	if (!soundManager)
		return;

	musicPlayer.ShutDown();
	ReleaseSounds();
	SAFE_DELETE(soundManager);
}

void SoundControl::ReleaseSounds()
{
	if (!soundManager)
		return;

	StopSounds();

	// clear the sound list
	for (int i = 0; i < maxSoundAssets; i++)
		SAFE_DELETE(soundAssets[i].sound);

	ResetSoundObjects();
	highestLoadedSoundID = 0;
	hasPlayedSound = false;
}

void SoundControl::ReloadModifiedSounds()
{
	StopSounds();
	
	for (int i = 0; i <= highestLoadedSoundID; i++)
	{
		SoundAsset& s = soundAssets[i];
		if (s.CheckIfNeedReload())
		{
			SAFE_DELETE(s.sound);
			LoadSoundInternal(s);

			g_debugMessageSystem.AddFormatted(L"Reloaded sound '%s'", s.name);
		}
	}
}

bool SoundAsset::CheckIfNeedReload() const
{
	FILETIME diskFileTime = GetDiskFiletime();
	return (CompareFileTime(&diskFileTime, &fileTime) != 0);
}

FILETIME SoundAsset::GetDiskFiletime() const
{
	WCHAR soundName[FILENAME_STRING_LENGTH];
	wcsncpy_s(soundName, FILENAME_STRING_LENGTH, name, _TRUNCATE );
	wcscat_s(soundName, FILENAME_STRING_LENGTH, L".wav");
	WIN32_FIND_DATA fileData;
	HANDLE hFind = FindFirstFile( soundName, &fileData);
	if (hFind != INVALID_HANDLE_VALUE)
		return fileData.ftLastWriteTime;

	wcsncpy_s(soundName, FILENAME_STRING_LENGTH, L"data/sound/", _TRUNCATE );
	wcscat_s(soundName, FILENAME_STRING_LENGTH, name);
	wcscat_s(soundName, FILENAME_STRING_LENGTH, L".wav");
	hFind = FindFirstFile( soundName, &fileData);
	if (hFind != INVALID_HANDLE_VALUE)
		return fileData.ftLastWriteTime;

	return { 0, 0 };
}

bool SoundControl::LoadSound(WCHAR* soundName, SoundID si, SoundFlags flags, DWORD bufferCount)
{
	if (!soundManager)
		return false;

	ASSERT(si < maxSoundAssets && bufferCount > 0);
	highestLoadedSoundID = Max(highestLoadedSoundID, int(si));

	// check if the sound already exists
	if (soundAssets[si].sound)
		return true;

	DWORD soundCreationFlags = DSBCAPS_CTRLFREQUENCY|DSBCAPS_CTRLVOLUME;
	if (!bool(flags & SoundFlags::NoPosition))
		soundCreationFlags |= DSBCAPS_CTRL3D;
	
	SoundAsset& s = soundAssets[si];
	wcsncpy_s(s.name, FILENAME_STRING_LENGTH, soundName, FILENAME_STRING_LENGTH);
	s.bufferCount = bufferCount;
	s.soundCreationFlags = soundCreationFlags;
	s.flags = flags;
	s.fileTime = { 0, 0 };

	return LoadSoundInternal(s);
}

SoundID SoundControl::FindSoundID(const wstring& soundName) const
{
	// todo, use a hashmap
	for (int i = 0; i < maxSoundAssets; i++)
	{
		if (soundAssets[i].name == soundName)
			return SoundID(i);
	}

	return Sound_Invalid;
}

bool SoundControl::LoadSoundInternal(SoundAsset& soundWrapper)
{
	/*
	// search through all the sub folders to find the file
	WCHAR sPath[MAX_PATH];
	HRESULT hr = DXUTFindDXSDKMediaFileCch( sPath, MAX_PATH, soundNameWav );
	if (FAILED(hr))
		StringCchCopy( sPath, MAX_PATH, soundName );
	*/

	// hack: check sound folder
	// todo: search subfolders, or way to add list of sub folders to search
	WCHAR soundName2[FILENAME_STRING_LENGTH];
	wcsncpy_s(soundName2, FILENAME_STRING_LENGTH, L"data/sound/", _TRUNCATE );
	wcscat_s(soundName2, FILENAME_STRING_LENGTH, soundWrapper.name);

	HRESULT hr = soundManager->Create( &soundWrapper.sound, soundName2, soundWrapper.soundCreationFlags, GUID_NULL, soundWrapper.bufferCount );
	if (FAILED(hr))
	{
		hr = soundManager->Create( &soundWrapper.sound, soundWrapper.name, soundWrapper.soundCreationFlags, GUID_NULL, soundWrapper.bufferCount );

		if (FAILED(hr))
		{
			g_debugMessageSystem.AddError( L"Sound \"%s\" not found.\n", soundWrapper.name );
			soundWrapper.sound = NULL;
			return false;
		}
	}

	soundWrapper.fileTime = soundWrapper.GetDiskFiletime();

	return true;
}

SoundObjectSmartPointer SoundControl::Play(SoundID si, float volume, float frequency, float frequencyRandomness)
{
	Vector2 position = Vector2(0);
	GameObject* listenerObject = cameraIsListener? g_cameraBase : g_gameControlBase->GetPlayer();
	if (listenerObject)
		position = listenerObject->GetPosWorld();
	return PlayInternal(si, position, volume, frequency, frequencyRandomness);
}

SoundObjectSmartPointer SoundControl::Play(SoundID si, const Vector2& position, float volume, float frequency, float frequencyRandomness, float distanceMin, float distanceMax)
{
	return PlayInternal(si, position, volume, frequency, frequencyRandomness, distanceMin, distanceMax);
}

SoundObjectSmartPointer SoundControl::PlayInternal(SoundID si, const GameObject& object, float volume, float frequency, float frequencyRandomness, float distanceMin, float distanceMax)
{
	const GameObject* physicalObject = object.GetAttachedPhysics();
	const bool listenerRelative = !cameraIsListener && physicalObject == g_gameControlBase->GetPlayer();
	const Vector2 position = object.GetPosWorld();
	const Vector2 velocity = physicalObject ? physicalObject->GetVelocity() : Vector2::Zero();
	return PlayInternal(si, position, volume, frequency, frequencyRandomness, distanceMin, distanceMax, velocity, listenerRelative, &object);
}

SoundObjectSmartPointer SoundControl::PlayInternal(SoundID si, const Vector2& position, float volume, float frequency, float frequencyRandomness, float distanceMin, float distanceMax, const Vector2& velocity, bool listenerRelative, const GameObject* object)
{
	if (!soundEnable || !soundManager)
		return NULL;
	
	SoundAsset& soundAsset = soundAssets[si];
	CSound* sound = soundAsset.sound;
	if (!sound)
		return NULL;
	
	ASSERT(volume >= 0 && volume <= 1 && frequency >= 0 && frequencyRandomness >= 0 && frequencyRandomness <= 1);
	ASSERT(distanceMin >= 0 && distanceMin <= distanceMax);
	
		// add a bit of randomness to the frequency
	if (soundAsset.ApplyFrequencyRandomness())
		frequency *= RAND_BETWEEN(1.0f - frequencyRandomness, 1.0f + frequencyRandomness);

	float frequencyScale = SoundControl::enableTimeScale && soundAsset.ApplyTimeScale()? g_gameControlBase->GetTimeScale() : 1.0f;
	float frequencyPositionScale = g_gameControlBase->GetSoundFrequencyScale(position);
	frequencyScale *= frequencyPositionScale;

	if (!sound->Is3D())
	{
		CSound::BufferWrapper bufferWrapper;
		if (FAILED(sound->Play(0, soundAsset.IsLoop() ? DSBPLAY_LOOPING : 0, volume, frequency, 0, &bufferWrapper, frequencyScale)))
			return NULL;

		SoundObject* soundObject = AddSoundObject();
		if (!soundObject)
			return NULL;

		soundObject->soundAsset = &soundAsset;
		soundObject->bufferWrapper = bufferWrapper;
		soundObject->parentObjectHandle = object;
		soundObject->lastPosition = position;
		soundObject->frequencyPositionScale = frequencyPositionScale;
		return soundObject;
	}

	const D3DVECTOR coneDirection = {0, 0, 1};
	const DWORD listenerMode = listenerRelative? DS3DMODE_DISABLE : DS3DMODE_NORMAL;

	DS3DBUFFER sound3DInfo =
	{
		sizeof(DS3DBUFFER),				// dwSize
		position.GetD3DXVECTOR3(),		// position
		velocity.GetD3DXVECTOR3(),		// velocity
		DS3D_MINCONEANGLE,				// insideConeAngle
		DS3D_MAXCONEANGLE,				// outsideConeAngle
		coneDirection,					// coneOrientation
		DS3D_DEFAULTCONEOUTSIDEVOLUME,	// coneOutsideVolume
		distanceMin,					// minDistance
		distanceMax,					// maxDistance
		listenerMode					// dwMode
	};
	
	float volumeDistanceScale = 1;
	if (SoundControl::soundDistanceFade)
	{
		const GameObject* listenerObject = cameraIsListener? g_cameraBase : g_gameControlBase->GetPlayer();
		const float distance = (position - listenerObject->GetPosWorld()).Length();
		volumeDistanceScale = Percent(distance, distanceMax, distanceMin);
		volumeDistanceScale = powf(volumeDistanceScale, soundDistanceFadePow);
		sound3DInfo.flMinDistance = 100000;
		sound3DInfo.flMaxDistance = 100001;
	}

	CSound::BufferWrapper bufferWrapper;
	if (FAILED(sound->Play3D(&sound3DInfo, 0, soundAsset.IsLoop() ? DSBPLAY_LOOPING : 0, volume, frequency, &bufferWrapper, frequencyScale, volumeDistanceScale)))
		return NULL;
	
	SoundObject* soundObject = AddSoundObject();
	if (!soundObject)
		return NULL;

	soundObject->soundAsset = &soundAsset;
	soundObject->bufferWrapper = bufferWrapper;
	soundObject->distanceMin = distanceMin;
	soundObject->distanceMax = distanceMax;
	soundObject->parentObjectHandle = object;
	soundObject->lastPosition = position;
	soundObject->frequencyPositionScale = frequencyPositionScale;
	return soundObject;
}

void SoundControl::Update()
{
	musicPlayer.Update();

	if (!soundManager)
		return;
	
	GameObject* listenerObject = cameraIsListener? g_cameraBase : g_gameControlBase->GetPlayer();
	if (!listenerObject || !g_cameraBase)
		return;

	// hack: only update listerner after a sound is played, it causes everyting to go super slow
	hasPlayedSound |= GetSoundObjectCount() > 0;
	if (!hasPlayedSound)
		return;

	IDirectSound3DListener* sound3DListenerInterface;
    HRESULT hr = soundManager->Get3DListenerInterface( &sound3DListenerInterface );
	if (SUCCEEDED(hr))
	{
		const XForm2 cameraXF = g_cameraBase->GetXFInterpolated();
		const D3DXVECTOR3 listenerPosition = listenerObject->GetXFInterpolated().position.GetD3DXVECTOR3();
		const D3DXVECTOR3 listenerVelocity = listenerObject->GetVelocity().GetD3DXVECTOR3();
		const D3DXVECTOR3 orientationFront(0, 0, 1);
		const D3DXVECTOR3 orientationTop = cameraXF.GetUp().GetD3DXVECTOR3();

		DS3DLISTENER sound3DListener =
		{
			sizeof(DS3DLISTENER),	// dwSize
			listenerPosition,		// position
			listenerVelocity,		// velocity
			orientationFront,		// orientFront
			orientationTop,			// orientTop
			dopplerDistanceFactor,	// distanceFactor
			rollOffFactor,			// rolloffFactor
			dopplerFactor			// dopplerFactor
		};
	
		sound3DListenerInterface->SetAllParameters( &sound3DListener, DS3D_IMMEDIATE );
	}

	// update sound objects
	for (SoundObjectHashTable::iterator it = soundObjectMap.begin(); it != soundObjectMap.end(); )
	{
		// remove soundAssets that aren't playing
		SoundObject& sound = *(*it).second;
		++it;

		sound.Update();
	}
}

void SoundControl::UpdateTimeScale(float timeScale)
{
	if (!soundManager || !hasPlayedSound)
		return;
	
	if (g_gameControlBase->IsPaused())
		timeScale = 0.0f;
	else if (!enableTimeScale)
		timeScale = 1.0f;

	// update sound objects
	for (SoundObjectHashPair hashPair : soundObjectMap)
	{
		// remove soundAssets that aren't playing
		SoundObject& sound = *hashPair.second;

		if (!sound.IsPlaying() || !sound.ApplyTimeScale())
			continue;
		
		float scale = timeScale;
		if (sound.IsLoop() && sound.soundAsset->IsPositional())
		{
			// poll position for loops
			GameObject* parentObject = sound.parentObjectHandle;
			if (parentObject)
				sound.frequencyPositionScale = g_gameControlBase->GetSoundFrequencyScale( parentObject->GetPosWorld());
		}

		sound.bufferWrapper.UpdateTimeScale(scale*sound.frequencyPositionScale);
	}
}

void SoundControl::StopSounds()
{
	for (SoundObjectHashPair hashPair : soundObjectMap)
	{
		// remove soundAssets that aren't playing
		SoundObject& sound = *hashPair.second;
		sound.bufferWrapper.Release();
	}
	ResetSoundObjects();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SoundControl::SoundObjectHandle SoundControl::nextUniqueHandleValue = 1;	// starting unique handle
SoundControl::SoundObjectHashTable SoundControl::soundObjectMap;			// the global group containing all game objects
SoundObject SoundControl::soundObjectPool[SoundControl::maxSoundObjects];
list<SoundObject*> SoundControl::freeSoundObjectList;

void SoundControl::ResetSoundObjects()
{
	soundObjectMap.clear();
	freeSoundObjectList.clear();
	for(int i = 0; i < maxSoundObjects; ++i)
		freeSoundObjectList.push_back(&soundObjectPool[i]);
	nextUniqueHandleValue = 1;
}

SoundObject* SoundControl::AddSoundObject()
{
	if (freeSoundObjectList.empty())
		return NULL; // ran out of sounds

	SoundObject* soundObject = freeSoundObjectList.back();
	freeSoundObjectList.pop_back();
	*soundObject = SoundObject();
	soundObject->handle = ++nextUniqueHandleValue;
	soundObjectMap.insert(SoundObjectHashPair(soundObject->handle, soundObject));
	return soundObject;
}

SoundObject* SoundControl::GetSoundObjectFromHandle(SoundObjectHandle handle)
{ 
	SoundObjectHashTable::iterator it = soundObjectMap.find(handle);
	return (it != soundObjectMap.end())? ((*it).second) : NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SoundObject::Stop()
{
	bufferWrapper.Release();
	g_sound->soundObjectMap.erase(handle);
	g_sound->freeSoundObjectList.push_back(this);
}

void SoundObject::Update()
{
	if (!bufferWrapper.buffer || !soundAsset)
		return;

	if (!SoundControl::soundEnable || !IsPlaying())
	{
		Stop();
		return;
	}
	
	GameObject* parentObject = parentObjectHandle;
	if (parentObject)
		lastPosition = parentObject->GetPosWorld();

	if (!soundAsset->IsPositional())
	{
		if (SoundControl::soundDebug)
		{
			lastPosition.RenderDebug(Color::Purple(0.5f), 1.0f);
			GameObject* listenerObject = SoundControl::cameraIsListener? g_cameraBase : g_gameControlBase->GetPlayer();
			if (listenerObject)
				Line2(lastPosition, listenerObject->GetPosWorld()).RenderDebug(Color::Purple(0.5f));
		}

		return; // only update positional sounds
	}

	if (SoundControl::soundDebug)
	{
		lastPosition.RenderDebug(Color::Red(0.5f), 1.0f);
		Circle(lastPosition, distanceMin).RenderDebug(Color::White(0.5f));
		Circle(lastPosition, distanceMax).RenderDebug(Color::Blue(0.5f));
		GameObject* listenerObject = SoundControl::cameraIsListener? g_cameraBase : g_gameControlBase->GetPlayer();
		if (listenerObject)
			Line2(lastPosition, listenerObject->GetPosWorld()).RenderDebug(Color::Red(0.5f));
	}

	float volumeDistanceScale = 1;
	if (SoundControl::soundDistanceFade)
	{
		GameObject* listenerObject = SoundControl::cameraIsListener? g_cameraBase : g_gameControlBase->GetPlayer();
		Vector2 deltaPos = lastPosition - listenerObject->GetPosWorld();
		float distance = deltaPos.Length();
		volumeDistanceScale = Percent(distance, distanceMax, distanceMin);
		volumeDistanceScale = powf(volumeDistanceScale, SoundControl::soundDistanceFadePow);
		bufferWrapper.SetVolume(volumeDistanceScale*bufferWrapper.volumePercent);
	}

	if (!parentObject)
		return;

	// QI for the 3D buffer
	LPDIRECTSOUND3DBUFFER pDS3DBuffer;
    HRESULT hr = bufferWrapper.buffer->QueryInterface( IID_IDirectSound3DBuffer, (VOID**) &pDS3DBuffer );
	if (SUCCEEDED(hr))
	{
		// special case if the object is the player/listener
		const GameObject* physicalObject = parentObject->GetAttachedPhysics();
		const bool listenerRelative = !SoundControl::cameraIsListener && physicalObject == g_gameControlBase->GetPlayer();
		const D3DXVECTOR3 position = lastPosition.GetD3DXVECTOR3();
		const D3DXVECTOR3 velocity = parentObject->GetVelocity().GetD3DXVECTOR3();
		const D3DVECTOR coneDirection = {0, 0, 1};
		const DWORD listenerMode = listenerRelative? DS3DMODE_DISABLE : DS3DMODE_NORMAL;

		DS3DBUFFER sound3DInfo =
		{
			sizeof(DS3DBUFFER),				// dwSize
			position,						// position
			velocity,						// velocity
			DS3D_MINCONEANGLE,				// insideConeAngle
			DS3D_MAXCONEANGLE,				// outsideConeAngle
			coneDirection,					// coneOrientation
			DS3D_DEFAULTCONEOUTSIDEVOLUME,	// coneOutsideVolume
			distanceMin,					// minDistance
			distanceMax,					// maxDistance
			listenerMode					// dwMode
		};
		
		if (SoundControl::soundDistanceFade)
		{
			sound3DInfo.flMinDistance = 100000;
			sound3DInfo.flMaxDistance = 100001;
		}

		pDS3DBuffer->SetAllParameters( &sound3DInfo, DS3D_IMMEDIATE );
		pDS3DBuffer->Release();
	}
}
	
void SoundObject::Restart()
{
	bufferWrapper.Restart();
}
	
void SoundObject::SetFrequency(float frequency)
{
	if (SoundControl::soundDebug)
		lastPosition.RenderDebug(Color::Cyan(0.3f), 0.5f+frequency);

	bufferWrapper.frequencyScale = frequency;
	//bufferWrapper.SetFrequency(frequency * frequencyScale);
}

void SoundObject::SetVolume(float volume)
{
	if (SoundControl::soundDebug)
		lastPosition.RenderDebug(Color::Green(0.3f), 0.5f+volume);
	
	bufferWrapper.volumePercent = volume;
	if (!SoundControl::soundDistanceFade || !soundAsset->IsPositional())
		bufferWrapper.SetVolume(volume);
}

bool SoundObject::IsPlaying() const
{
	return bufferWrapper.IsPlaying();
}