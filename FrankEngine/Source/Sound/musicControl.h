////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Ogg Vorbis Player
	Copyright 2013 Frank Force - http://www.frankforce.com

	- simple music player using ogg vorbis

	- directx portion adapted from Bjorn Paetzel's "Ogg Vorbis Player Class" example
	- http://www.flipcode.com/archives/Ogg_Vorbis_Player_Class.shtml

	- memory streaming adapted from Spree Tree's "Loading OggVorbis Files From Memory" tutorial
	- http://www.devmaster.net/articles/openal-ogg-file/
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

typedef struct IDirectSound8		*LPDIRECTSOUND8;
typedef struct IDirectSoundBuffer	*LPDIRECTSOUNDBUFFER;

struct SOggFile
{
	WCHAR		filename[256];		// filename
	char*		dataPtr;			// Pointer to the data in memoru
	int			dataSize;			// Sizeo fo the data
	int			dataRead;			// How much data we have read so far
	bool		isPreLoaded;
};

class MusicControl : private Uncopyable
{

public:

	MusicControl();
	~MusicControl();

	void ShutDown();
	void SetDirectSound( LPDIRECTSOUND8 _ds )		{ ds = _ds; }
	
	SOggFile* GetLoaded(WCHAR* filename);
	bool Load(WCHAR* filename);

	bool Open( WCHAR* filename );
	bool IsOpen() const								{ return fileOpened; }
	void Close();
	void Close(SOggFile& oggFile);
	void Update();   
	
	void Transition(const WCHAR* filename = L"", float outTime = 1, float inTime = 1, bool loop = true);
	void Reset();

	void Play( bool _loop = true );
	bool IsPlaying() const							{ return !done; }
	void SetLoop( bool _loop )						{ loop = _loop; }

	void Pause( bool pause );
	bool IsPaused() const							{ return pause; }

	void EnableFrequencyScale( bool _enable )		{ enableFrequencyScale = _enable; }
	void SetFrequencyScale( float _frequencyScale ) { frequencyScale = _frequencyScale; }
	float GetFrequencyScale() const					{ return frequencyScale; }
	
	void SetVolumeScale( float _volumeScale )		{ volumeScale = _volumeScale; }
	float GetVolumeScale() const					{ return volumeScale; }

	static bool musicEnable;
	static float masterVolume;

protected:
	
	bool LoadInternal(WCHAR* filename, SOggFile& oggFile);
	void Stop();  

	bool almostDone;				// only one half of the buffer to play
	bool done;						// done playing
	bool loop;						// should sound loop?
	bool pause;						// is the music paused?
	bool fileOpened;				// have we opened an ogg yet?
	int lastSection;				// which half of the buffer is the end?
	int curSection;					// which half of the buffer are/were we playing?

	bool enableFrequencyScale;		// if false music is always played at correct frequency
	float frequencyScale;			// frequency scaler for the music
	float volumeScale;				// control for scaling music volume

	LPDIRECTSOUND8 ds;				// the directsound 8 object
	LPDIRECTSOUNDBUFFER dsBuffer;	// the buffer
	static const DWORD bufferSize;	// how big is the buffer
	struct OggVorbis_File* vf;		// for the vorbisfile interface

	WCHAR transitionFilename[GameObjectStub::attributesLength];
	WCHAR openFilename[GameObjectStub::attributesLength];
	bool transitionLoop = true;
	bool transitionIn = true;
	float transitionPercent = 1;
	float transitionOutTime = 0;
	float transitionInTime = 0;

	list<SOggFile> loadedOggFiles;
};
