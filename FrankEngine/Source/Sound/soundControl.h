////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Sound Engine
	Copyright 2013 Frank Force - http://www.frankforce.com

	- low level sound code for Frank lib
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dsound.h"
#include "../sound/musicControl.h"
#include "../objects/gameObject.h"
#include "../DXUT/optional/SDKsound.h"

extern class SoundControl* g_sound;

enum SoundID;
const SoundID Sound_Invalid	= SoundID(0);

enum class SoundFlags
{
	None					= 0,
	Loop					= (1 << 0),		// sound will loop (must be stopped manually)
	NoPosition				= (1 << 1),		// sound is not positional
	NoTimeScale				= (1 << 2),		// do not apply time scale effects
	NoFrequencyRandomness	= (1 << 3),		// do not appply frequency randomness
};

inline SoundFlags operator | (SoundFlags a, SoundFlags b) { return static_cast<SoundFlags>(static_cast<int>(a) | static_cast<int>(b)); }
inline SoundFlags operator & (SoundFlags a, SoundFlags b) { return static_cast<SoundFlags>(static_cast<int>(a) & static_cast<int>(b)); }

struct SoundObjectSmartPointer;

////////////////////////////////////////////////////////////////////////////////////////
/*
	SoundAsset - a sound and it's buffers that has been loaded for use
*/
////////////////////////////////////////////////////////////////////////////////////////

struct SoundAsset
{
	SoundAsset() {}
	
	friend class SoundObject;
	friend class SoundControl;

private:
	
	bool IsLoop() const						{ return bool(flags & SoundFlags::Loop); }
	bool IsPositional() const				{ return !bool(flags & SoundFlags::NoPosition); }
	bool ApplyTimeScale() const				{ return !bool(flags & SoundFlags::NoTimeScale); }
	bool ApplyFrequencyRandomness() const	{ return !bool(flags & SoundFlags::NoFrequencyRandomness); }

	WCHAR name[FILENAME_STRING_LENGTH] = L"";
	class CSound* sound = NULL;
	DWORD bufferCount = 0;
	DWORD soundCreationFlags = 0;
	FILETIME fileTime = { 0, 0 };
	SoundFlags flags = SoundFlags::None;

	bool CheckIfNeedReload() const;
	FILETIME GetDiskFiletime() const;
	bool IsLoadedFromDisk() const { return fileTime.dwLowDateTime != 0 || fileTime.dwHighDateTime != 0; }
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	SoundObject - a sound currently being played
*/
////////////////////////////////////////////////////////////////////////////////////////

class SoundObject
{
	friend class SoundControl;
	friend struct SoundObjectSmartPointer;

public:

	void Stop();
	void Restart();
	void SetFrequency(float frequency);
	void SetVolume(float volume);
	bool IsPlaying() const;
	
	bool IsLoop() const						{ return soundAsset && soundAsset->IsLoop(); }
	bool IsPositional() const				{ return soundAsset && soundAsset->IsPositional(); }
	bool ApplyTimeScale() const				{ return soundAsset && soundAsset->ApplyTimeScale(); }
	bool ApplyFrequencyRandomness() const	{ return soundAsset && soundAsset->ApplyFrequencyRandomness(); }

private:
	
	typedef unsigned Handle;
	
	void Update();

	GameObjectSmartPointer<> parentObjectHandle;
	Handle handle = 0;
	CSound::BufferWrapper bufferWrapper;
	SoundAsset* soundAsset = NULL;
	float distanceMin = 0;
	float distanceMax = 0;
	Vector2 lastPosition = Vector2(0);
	float frequencyPositionScale = 1;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	SoundControl - controls all sound and music functions
*/
////////////////////////////////////////////////////////////////////////////////////////

class SoundControl : private Uncopyable
{
public:
	
	friend class GameObject;
	friend class SoundObject;
	friend struct SoundObjectSmartPointer;
	SoundControl();
	~SoundControl();
	
	bool LoadSound(WCHAR* soundName, SoundID si, SoundFlags flags = SoundFlags::None, DWORD bufferCount = maxBufferCount);
	SoundID FindSoundID(const wstring& soundName) const;

	void ReleaseSounds();
	void ReloadModifiedSounds();

	SoundObjectSmartPointer Play(SoundID si, float volume = 1, float frequency = 1, float frequencyRandomness = GameObject::defaultFrequencyRandomness);
	SoundObjectSmartPointer Play(SoundID si, const Vector2& position, float volume = 1, float frequency = 1, float frequencyRandomness = GameObject::defaultFrequencyRandomness, float distanceMin = GameObject::defaultSoundDistanceMin, float distanceMax = GameObject::defaultSoundDistanceMax);

	void StopSounds();

	void Update();
	void UpdateTimeScale(float scale);

	MusicControl& GetMusicPlayer() { return musicPlayer; }
	
	int GetSoundObjectCount() { return soundObjectMap.size(); }
	int GetAssetCount() { return highestLoadedSoundID; }

	static float masterVolume;
	static float soundVolumeScale;
	static float rollOffFactor;
	static float dopplerFactor;
	static float dopplerDistanceFactor;
	static bool enableTimeScale;		// should we update soundAssets with the time scale
	static bool cameraIsListener;		// is camera or player listener
	static bool soundEnable;
	static bool soundDebug;
	static bool soundDistanceFade;
	static float soundDistanceFadePow;
	
	static const int maxSoundObjects = 32;	// max simultaneous playing sounds
	static const int maxSoundAssets = 256;	// max sounds that can be loaded
	static int maxBufferCount;

private:
	
	bool LoadSoundInternal(SoundAsset& sound);

	SoundObjectSmartPointer PlayInternal
	(
		SoundID si,
		const GameObject& object, 
		float volume = 1,
		float frequency = 1,
		float frequencyRandomness = GameObject::defaultFrequencyRandomness,
		float distanceMin = GameObject::defaultSoundDistanceMin, 
		float distanceMax = GameObject::defaultSoundDistanceMax
	);

	SoundObjectSmartPointer PlayInternal
	(
		SoundID si,
		const Vector2& position, 
		float volume = 1,
		float frequency = 1,
		float frequencyRandomness = GameObject::defaultFrequencyRandomness,
		float distanceMin = GameObject::defaultSoundDistanceMin, 
		float distanceMax = GameObject::defaultSoundDistanceMax,
		const Vector2& velocity = Vector2::Zero(), 
		bool listenerRelative = false,
		const GameObject* object = NULL
	);

	MusicControl musicPlayer;
	class CSoundManager* soundManager = NULL;
	bool hasPlayedSound = false;
	SoundAsset soundAssets[maxSoundAssets];
	int highestLoadedSoundID = 0;
	
private: // sound object tracking

	typedef SoundObject::Handle SoundObjectHandle;
	
	SoundObject* GetSoundObjectFromHandle(SoundObjectHandle handle);
	
	SoundObject* AddSoundObject();
	void ResetSoundObjects();

	typedef unordered_map<SoundObject::Handle, SoundObject*> SoundObjectHashTable;
	typedef pair<SoundObject::Handle, SoundObject*> SoundObjectHashPair;

	static list<SoundObject*> freeSoundObjectList;
	static SoundObject soundObjectPool[maxSoundObjects];
	static SoundObjectHashTable soundObjectMap;							// hash map of all sound soundAssets
	static SoundObject::Handle nextUniqueHandleValue;		// used only internaly to give out unique handles
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	SoundObjectSmartPointer - used to track sound objects (for loops, or setting volume/frequency)
*/
////////////////////////////////////////////////////////////////////////////////////////

struct SoundObjectSmartPointer
{
	SoundObjectSmartPointer() : handle(0)			{}
	SoundObjectSmartPointer(const SoundObject* object) : handle(object?object->handle : 0) {}

	operator SoundObject* () const					{ return g_sound->GetSoundObjectFromHandle(handle); }
	bool operator == (const SoundObject& o) const	{ return (handle == o.handle); }
	bool operator != (const SoundObject& o) const	{ return (handle != o.handle); }

private:

	SoundObject::Handle handle = 0;
};
