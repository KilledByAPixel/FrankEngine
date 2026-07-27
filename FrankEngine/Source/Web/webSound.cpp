////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Web Sound (OpenAL)
	Copyright 2013 Frank Force - http://www.frankforce.com

	- phase 6: implements the CSound/CSoundManager surface (Frank's modified SDKsound
	  API) and the DirectSound music-streaming buffer over OpenAL, so the REAL
	  soundControl.cpp and musicControl.cpp run unmodified on web
	- volume replicates the windows mapping exactly: percent maps linearly into
	  centibels (DSBVOLUME_MIN..MAX), i.e. AL gain = 10^((p-1)*5)
	- frequency replicates f = DSBFREQUENCY_MIN + (rate-MIN)*scale -> AL pitch f/rate
	- music: 128KB ring + 4 queued 16KB AL chunks; the virtual play cursor feeds
	  GetCurrentPosition so musicControl's half-buffer refill logic works untouched
	- headless (node): no AL device -> Initialize fails -> engine's !soundManager
	  guards keep everything silent and safe
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "../frankEngine.h"

#ifndef FRANK_PLATFORM_WEB
#error webSound.cpp is web-build only
#endif

#include <AL/al.h>
#include <AL/alc.h>
#include <emscripten.h>

static ALCdevice* webALDevice = NULL;
static ALCcontext* webALContext = NULL;
static bool webALActive = false;

////////////////////////////////////////////////////////////////////////////////////////
// volume / pitch conversion (replicating the windows DS mappings)
////////////////////////////////////////////////////////////////////////////////////////

// percent -> centibels (linear, like SDKsound.cpp:389) -> amplitude
static float WebVolumePercentToGain(float percent)
{
	if (percent <= 0)
		return 0;
	if (percent >= 1)
		return 1;
	// cB = -10000 + p*10000; amplitude = 10^(cB/2000)
	return powf(10.0f, (percent - 1.0f) * 5.0f);
}

static float WebCentibelsToGain(LONG centibels)
{
	if (centibels <= DSBVOLUME_MIN)
		return 0;
	if (centibels >= DSBVOLUME_MAX)
		return 1;
	return powf(10.0f, centibels / 2000.0f);
}

// scale -> Hz (like SDKsound.cpp:402) -> AL pitch
static float WebFrequencyScaleToPitch(float scale, int sampleRate)
{
	float f = DSBFREQUENCY_MIN + (sampleRate - DSBFREQUENCY_MIN) * scale;
	if (f < DSBFREQUENCY_MIN) f = DSBFREQUENCY_MIN;
	if (f > DSBFREQUENCY_MAX) f = DSBFREQUENCY_MAX;
	return f / (float)sampleRate;
}

////////////////////////////////////////////////////////////////////////////////////////
// WAV loading (RIFF PCM 8/16-bit mono/stereo) from the preloaded FS
////////////////////////////////////////////////////////////////////////////////////////

static bool WebLoadWav(const char* path, ALuint& alBuffer, int& sampleRate)
{
	FILE* f = fopen(path, "rb");
	if (!f)
		return false;

	fseek(f, 0, SEEK_END);
	const long fileSize = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fileSize < 44)
		{ fclose(f); return false; }
	unsigned char* data = (unsigned char*)malloc(fileSize);
	fread(data, 1, fileSize, f);
	fclose(f);

	if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
		{ free(data); return false; }

	int channels = 0, bits = 0, rate = 0;
	const unsigned char* sampleData = NULL;
	unsigned int sampleBytes = 0;

	// walk the chunks
	long pos = 12;
	while (pos + 8 <= fileSize)
	{
		const unsigned int chunkSize = data[pos+4] | (data[pos+5] << 8) | (data[pos+6] << 16) | (data[pos+7] << 24);
		if (memcmp(data + pos, "fmt ", 4) == 0 && chunkSize >= 16)
		{
			const unsigned char* c = data + pos + 8;
			const int format = c[0] | (c[1] << 8);
			channels = c[2] | (c[3] << 8);
			rate = c[4] | (c[5] << 8) | (c[6] << 16) | (c[7] << 24);
			bits = c[14] | (c[15] << 8);
			if (format != 1)	// PCM only
				{ free(data); return false; }
		}
		else if (memcmp(data + pos, "data", 4) == 0)
		{
			sampleData = data + pos + 8;
			sampleBytes = chunkSize;
			if (pos + 8 + (long)sampleBytes > fileSize)
				sampleBytes = (unsigned int)(fileSize - pos - 8);
		}
		pos += 8 + chunkSize + (chunkSize & 1);
	}

	if (!sampleData || !channels || !rate || (bits != 8 && bits != 16))
		{ free(data); return false; }

	const ALenum format = (channels == 2)
		? (bits == 16 ? AL_FORMAT_STEREO16 : AL_FORMAT_STEREO8)
		: (bits == 16 ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8);

	alGenBuffers(1, &alBuffer);
	alBufferData(alBuffer, format, sampleData, sampleBytes, rate);
	sampleRate = rate;
	free(data);
	return alGetError() == AL_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////////////
// CSoundManager
////////////////////////////////////////////////////////////////////////////////////////

HRESULT CSoundManager::Initialize(HWND hWnd, DWORD dwCoopLevel)
{
	if (webALActive)
		return S_OK;
	webALDevice = alcOpenDevice(NULL);
	if (!webALDevice)
	{
		printf("webSound: no OpenAL device (headless?), audio disabled\n");
		return E_FAIL;
	}
	webALContext = alcCreateContext(webALDevice, NULL);
	alcMakeContextCurrent(webALContext);
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
	webALActive = true;
	printf("webSound: OpenAL ready\n");
	return S_OK;
}

IDirectSound8* CSoundManager::GetDirectSound()
{
	static IDirectSound8 fake;
	return &fake;
}

HRESULT CSoundManager::Get3DListenerInterface(IDirectSound3DListener** ppDSListener)
{
	if (!webALActive)
		return E_FAIL;
	static IDirectSound3DListener fake;
	if (ppDSListener)
		*ppDSListener = &fake;
	return S_OK;
}

HRESULT CSoundManager::Create(CSound** ppSound, LPWSTR strWaveFileName, DWORD dwCreationFlags, GUID guid3DAlgorithm, DWORD dwNumBuffers)
{
	if (!webALActive || !ppSound)
		return E_FAIL;

	char name[FILENAME_STRING_LENGTH];
	wcstombs(name, strWaveFileName, FILENAME_STRING_LENGTH - 1);
	name[FILENAME_STRING_LENGTH - 1] = 0;

	// probe like the windows loader: as passed (soundControl already prepends
	// data/sound/), with .wav appended, and absolute forms
	char path[FILENAME_STRING_LENGTH + 16];
	ALuint alBuffer = 0;
	int sampleRate = 0;
	bool ok = false;
	const char* patterns[] = { "%s.wav", "%s", "/%s.wav", "/%s" };
	for (int i = 0; i < 4 && !ok; ++i)
	{
		snprintf(path, sizeof(path), patterns[i], name);
		ok = WebLoadWav(path, alBuffer, sampleRate);
	}
	if (!ok)
		return E_FAIL;

	printf("webSound: loaded %s (%d Hz)\n", path, sampleRate);

	CSound* sound = new CSound();
	sound->alBuffer = alBuffer;
	sound->sampleRate = sampleRate;
	sound->creationFlags = dwCreationFlags;
	sound->sourceCount = dwNumBuffers > 0 ? dwNumBuffers : 1;
	sound->sources = new FrankWebSoundSource[sound->sourceCount];
	for (DWORD i = 0; i < sound->sourceCount; ++i)
		sound->sources[i].owner = sound;

	*ppSound = sound;
	return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
// CSound + sources
////////////////////////////////////////////////////////////////////////////////////////

CSound::~CSound()
{
	if (sources)
	{
		for (DWORD i = 0; i < sourceCount; ++i)
		{
			if (sources[i].alSource)
			{
				alSourceStop(sources[i].alSource);
				alDeleteSources(1, &sources[i].alSource);
			}
		}
		delete [] sources;
	}
	if (alBuffer)
		alDeleteBuffers(1, &alBuffer);
}

FrankWebSoundSource* CSound::GetFreeSource()
{
	for (DWORD i = 0; i < sourceCount; ++i)
	{
		FrankWebSoundSource& s = sources[i];
		if (s.inUse)
		{
			// reclaim finished sources
			ALint state = AL_STOPPED;
			alGetSourcei(s.alSource, AL_SOURCE_STATE, &state);
			if (state == AL_PLAYING || state == AL_PAUSED)
				continue;
			s.inUse = false;
		}
		if (!s.alSource)
			alGenSources(1, &s.alSource);
		if (!s.alSource)
			return NULL;
		s.inUse = true;
		return &s;
	}
	return NULL;
}

// current rolloff from the last listener update (DS sets it on the listener,
// AL wants it per source)
static float webListenerRolloff = 1.0f;

HRESULT CSound::Play(DWORD dwPriority, DWORD dwFlags, float volumePercent, float frequencyScale, LONG lPan, BufferWrapper* buffer, float frequencyTimeScale)
{
	if (!webALActive)
		return CO_E_NOTINITIALIZED;

	FrankWebSoundSource* source = GetFreeSource();
	if (!source)
		return E_FAIL;

	BufferWrapper bufferWrapper;
	bufferWrapper.buffer = source;
	bufferWrapper.sound = this;
	bufferWrapper.volumePercent = volumePercent;
	bufferWrapper.frequencyScale = frequencyScale;

	const ALuint src = source->alSource;
	alSourceStop(src);
	alSourcei(src, AL_BUFFER, alBuffer);
	alSourcei(src, AL_LOOPING, (dwFlags & DSBPLAY_LOOPING) ? AL_TRUE : AL_FALSE);
	// non positional: listener relative at the origin
	alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
	alSource3f(src, AL_POSITION, 0, 0, 0);
	alSource3f(src, AL_VELOCITY, 0, 0, 0);
	alSourcef(src, AL_GAIN, WebVolumePercentToGain(SoundControl::soundVolumeScale * SoundControl::masterVolume * volumePercent));
	alSourcef(src, AL_PITCH, WebFrequencyScaleToPitch(frequencyScale * frequencyTimeScale, sampleRate));
	alSourcePlay(src);

	if (buffer)
		*buffer = bufferWrapper;
	return S_OK;
}

HRESULT CSound::Play3D(LPDS3DBUFFER p3DBuffer, DWORD dwPriority, DWORD dwFlags, float volumePercent, float frequencyScale, BufferWrapper* buffer, float frequencyTimeScale, float volumeScale)
{
	if (!webALActive)
		return CO_E_NOTINITIALIZED;

	FrankWebSoundSource* source = GetFreeSource();
	if (!source)
		return E_FAIL;

	BufferWrapper bufferWrapper;
	bufferWrapper.buffer = source;
	bufferWrapper.sound = this;
	bufferWrapper.volumePercent = volumePercent;
	bufferWrapper.frequencyScale = frequencyScale;

	const ALuint src = source->alSource;
	alSourceStop(src);
	alSourcei(src, AL_BUFFER, alBuffer);
	alSourcei(src, AL_LOOPING, (dwFlags & DSBPLAY_LOOPING) ? AL_TRUE : AL_FALSE);
	alSourcef(src, AL_GAIN, WebVolumePercentToGain(SoundControl::soundVolumeScale * SoundControl::masterVolume * volumePercent * volumeScale));
	alSourcef(src, AL_PITCH, WebFrequencyScaleToPitch(frequencyScale * frequencyTimeScale, sampleRate));
	source->SetAllParameters(p3DBuffer, DS3D_IMMEDIATE);
	alSourcePlay(src);

	if (buffer)
		*buffer = bufferWrapper;
	return S_OK;
}

HRESULT FrankWebSoundSource::SetAllParameters(const DS3DBUFFER* p, DWORD flags)
{
	if (!webALActive || !alSource || !p)
		return E_FAIL;

	if (p->dwMode == DS3DMODE_DISABLE)
	{
		// listener-relative (the player's own sounds)
		alSourcei(alSource, AL_SOURCE_RELATIVE, AL_TRUE);
		alSource3f(alSource, AL_POSITION, 0, 0, 0);
		alSource3f(alSource, AL_VELOCITY, 0, 0, 0);
		alSourcef(alSource, AL_ROLLOFF_FACTOR, 0);
	}
	else
	{
		alSourcei(alSource, AL_SOURCE_RELATIVE, AL_FALSE);
		alSource3f(alSource, AL_POSITION, p->vPosition.x, p->vPosition.y, p->vPosition.z);
		alSource3f(alSource, AL_VELOCITY, p->vVelocity.x, p->vVelocity.y, p->vVelocity.z);
		alSourcef(alSource, AL_ROLLOFF_FACTOR, webListenerRolloff);
	}
	alSourcef(alSource, AL_REFERENCE_DISTANCE, p->flMinDistance > 0 ? p->flMinDistance : 0.01f);
	alSourcef(alSource, AL_MAX_DISTANCE, p->flMaxDistance);
	return S_OK;
}

HRESULT IDirectSound3DListener::SetAllParameters(const DS3DLISTENER* p, DWORD flags)
{
	if (!webALActive || !p)
		return E_FAIL;

	alListener3f(AL_POSITION, p->vPosition.x, p->vPosition.y, p->vPosition.z);
	alListener3f(AL_VELOCITY, p->vVelocity.x, p->vVelocity.y, p->vVelocity.z);
	// DS is left handed (+z into screen), AL right handed: face -z, up from the camera
	const float orientation[6] = { 0, 0, -1, p->vOrientTop.x, p->vOrientTop.y, p->vOrientTop.z };
	alListenerfv(AL_ORIENTATION, orientation);
	alDopplerFactor(p->flDopplerFactor >= 0 ? p->flDopplerFactor : 1.0f);
	webListenerRolloff = p->flRolloffFactor;
	return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
// BufferWrapper
////////////////////////////////////////////////////////////////////////////////////////

void CSound::BufferWrapper::Stop()
{
	if (buffer && buffer->alSource)
		alSourceStop(buffer->alSource);
}

void CSound::BufferWrapper::Release()
{
	if (buffer)
	{
		if (buffer->alSource)
			alSourceStop(buffer->alSource);
		buffer->inUse = false;
	}
	buffer = NULL;
}

bool CSound::BufferWrapper::IsPlaying() const
{
	if (!buffer || !buffer->alSource)
		return false;
	ALint state = AL_STOPPED;
	alGetSourcei(buffer->alSource, AL_SOURCE_STATE, &state);
	return state == AL_PLAYING || state == AL_PAUSED;
}

void CSound::BufferWrapper::UpdateTimeScale(float scale)
{
	if (!buffer || !IsPlaying())
		return;
	SetFrequency(scale * frequencyScale);
}

void CSound::BufferWrapper::SetVolume(float volume)
{
	if (!buffer || !buffer->alSource)
		return;
	alSourcef(buffer->alSource, AL_GAIN, WebVolumePercentToGain(SoundControl::soundVolumeScale * SoundControl::masterVolume * volume));
}

void CSound::BufferWrapper::SetFrequency(float frequency)
{
	if (!buffer || !buffer->alSource || !sound)
		return;
	alSourcef(buffer->alSource, AL_PITCH, WebFrequencyScaleToPitch(frequency, sound->sampleRate));
}

void CSound::BufferWrapper::Restart()
{
	if (buffer && buffer->alSource)
	{
		alSourceRewind(buffer->alSource);
		alSourcePlay(buffer->alSource);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// music streaming buffer (the DS half-buffer model over AL queued chunks)
////////////////////////////////////////////////////////////////////////////////////////

static const DWORD webMusicChunkSize = 16384;
// queue depth must stay UNDER half the ring (bufferSize/chunk): the game refills a
// ring half only when the play cursor leaves it, so a queue that reaches exactly half
// a ring ahead races the refill at every boundary and can feed stale (already played)
// chunks - audible as periodic static, worse under load when several chunks pump at
// once. 12 chunks (~1.1s) also rides out multi-hundred-ms frame stalls without underrun.
static const int webMusicQueueDepth = 12;
static_assert(webMusicQueueDepth <= 16, "alBuffers array in frankPlatformWeb.h holds 16");

HRESULT IDirectSound8::CreateSoundBuffer(const DSBUFFERDESC* desc, IDirectSoundBuffer** out, void* unused)
{
	if (!webALActive || !out || !desc || !desc->lpwfxFormat)
		return E_FAIL;

	// one music buffer exists at a time; reuse a singleton because the web
	// SAFE_RELEASE never calls Release
	static IDirectSoundBuffer music;
	if (music.alSource)
	{
		alSourceStop(music.alSource);
		ALint queued = 0;
		alGetSourcei(music.alSource, AL_BUFFERS_QUEUED, &queued);
		while (queued--)
		{
			ALuint b;
			alSourceUnqueueBuffers(music.alSource, 1, &b);
		}
	}
	else
	{
		alGenSources(1, &music.alSource);
		alGenBuffers(webMusicQueueDepth, music.alBuffers);
		alSourcei(music.alSource, AL_SOURCE_RELATIVE, AL_TRUE);
		alSource3f(music.alSource, AL_POSITION, 0, 0, 0);
	}

	free(music.ring);
	music.ringSize = desc->dwBufferBytes;
	music.ring = (unsigned char*)calloc(1, music.ringSize ? music.ringSize : 1);
	music.feedPos = 0;
	music.consumed = 0;
	music.channels = desc->lpwfxFormat->nChannels;
	music.sampleRate = (int)desc->lpwfxFormat->nSamplesPerSec;
	music.playing = false;
	music.primed = false;

	*out = &music;
	return S_OK;
}

// feed finished AL chunks from the ring; advances the virtual play cursor
static void WebMusicPump(IDirectSoundBuffer& b)
{
	if (!webALActive || !b.alSource || !b.playing)
		return;

	const ALenum format = (b.channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
	ALint processed = 0;
	alGetSourcei(b.alSource, AL_BUFFERS_PROCESSED, &processed);
	while (processed-- > 0)
	{
		ALuint chunk = 0;
		alSourceUnqueueBuffers(b.alSource, 1, &chunk);
		b.consumed += webMusicChunkSize;
		alBufferData(chunk, format, b.ring + (b.feedPos % b.ringSize), webMusicChunkSize, b.sampleRate);
		alSourceQueueBuffers(b.alSource, 1, &chunk);
		b.feedPos += webMusicChunkSize;
	}

	// recover from an underrun (tab throttled, slow frame)
	ALint state = AL_STOPPED;
	alGetSourcei(b.alSource, AL_SOURCE_STATE, &state);
	if (state != AL_PLAYING && b.playing)
		alSourcePlay(b.alSource);
}

HRESULT IDirectSoundBuffer::Lock(DWORD offset, DWORD size, void** ptr1, DWORD* size1, void** ptr2, DWORD* size2, DWORD flags)
{
	if (!ring || !ptr1)
		return E_FAIL;
	if (flags & DSBLOCK_ENTIREBUFFER)
	{
		*ptr1 = ring;
		if (size1) *size1 = ringSize;
	}
	else
	{
		if (offset >= ringSize)
			return E_FAIL;
		*ptr1 = ring + offset;
		if (size1) *size1 = (size <= ringSize - offset) ? size : (ringSize - offset);
	}
	if (ptr2) *ptr2 = NULL;
	if (size2) *size2 = 0;
	return S_OK;
}

HRESULT IDirectSoundBuffer::Unlock(void* ptr1, DWORD size1, void* ptr2, DWORD size2)
{
	return S_OK;
}

HRESULT IDirectSoundBuffer::Play(DWORD reserved, DWORD priority, DWORD flags)
{
	if (!webALActive || !alSource || !ring)
		return E_FAIL;

	playing = true;
	if (!primed)
	{
		// queue the first chunks from the ring the game just filled
		const ALenum format = (channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
		feedPos = 0;
		consumed = 0;
		for (int i = 0; i < webMusicQueueDepth; ++i)
		{
			alBufferData(alBuffers[i], format, ring + (feedPos % ringSize), webMusicChunkSize, sampleRate);
			alSourceQueueBuffers(alSource, 1, &alBuffers[i]);
			feedPos += webMusicChunkSize;
		}
		primed = true;
	}
	alSourcePlay(alSource);
	return S_OK;
}

HRESULT IDirectSoundBuffer::Stop()
{
	if (alSource)
		alSourcePause(alSource);
	playing = false;
	return S_OK;
}

HRESULT IDirectSoundBuffer::SetVolume(LONG centibels)
{
	if (alSource)
		alSourcef(alSource, AL_GAIN, WebCentibelsToGain(centibels));
	return S_OK;
}

HRESULT IDirectSoundBuffer::SetFrequency(DWORD hz)
{
	if (alSource && sampleRate > 0)
	{
		DWORD f = hz;
		if (f < DSBFREQUENCY_MIN) f = DSBFREQUENCY_MIN;
		alSourcef(alSource, AL_PITCH, f / (float)sampleRate);
	}
	return S_OK;
}

HRESULT IDirectSoundBuffer::GetFormat(WAVEFORMATEX* format, DWORD size, DWORD* written)
{
	if (!format)
		return E_FAIL;
	memset(format, 0, sizeof(WAVEFORMATEX));
	format->wFormatTag = 1;
	format->nChannels = (WORD)channels;
	format->nSamplesPerSec = sampleRate;
	format->wBitsPerSample = 16;
	format->nBlockAlign = (WORD)(2 * channels);
	format->nAvgBytesPerSec = sampleRate * 2 * channels;
	if (written) *written = sizeof(WAVEFORMATEX);
	return S_OK;
}

HRESULT IDirectSoundBuffer::GetCurrentPosition(DWORD* playPos, DWORD* writePos)
{
	WebMusicPump(*this);
	if (playPos)
		*playPos = ringSize ? (consumed % ringSize) : 0;
	if (writePos)
		*writePos = ringSize ? (feedPos % ringSize) : 0;
	return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
// music over Web Audio (bypasses the AL streaming path above)
//
// the AL chunk-queue streaming needs the MAIN THREAD to keep feeding it, so heavy
// frames caused clicks/static no matter how deep the buffer. instead: decode each ogg
// ONCE with decodeAudioData and loop an AudioBufferSourceNode - playback then lives
// entirely on the browser's audio thread and cannot glitch however long a frame takes.
// pattern borrowed from LittleJS's audio system (FrankEngine/local/engineAudio.js).
// musicControl.cpp calls these through the FrankWebMusic* wrappers at the bottom.
////////////////////////////////////////////////////////////////////////////////////////

EM_JS(void, WebMusicJSInit, (), {
	if (typeof AudioContext === 'undefined' || globalThis.frankMusic)
		return;
	var fm = globalThis.frankMusic = { tracks: {}, cur: null, pending: null, vol: 0, rate: 1, userPaused: false };
	fm.ctx = new AudioContext();
	fm.master = fm.ctx.createGain();
	// dev convenience: ?nomusic on the url silences music without touching autoexec
	fm.muted = (typeof location !== 'undefined') &&
		(/nomusic/.test(location.search) || /nomusic/.test(location.hash));
	if (fm.muted)
	{
		fm.master.gain.value = 0;
		console.log('frankMusic: MUTED via ?nomusic');
	}
	fm.master.connect(fm.ctx.destination);
	fm.canRun = function() { return !fm.userPaused && !document.hidden; };
	// autoplay policy: contexts start suspended until a user gesture
	var resume = function() { if (fm.canRun() && fm.ctx.state !== 'running') fm.ctx.resume(); };
	window.addEventListener('pointerdown', resume);
	window.addEventListener('keydown', resume);
	// freeze music while the tab is hidden (rAF stops, the engine can't do it from C)
	document.addEventListener('visibilitychange', function() {
		if (document.hidden) fm.ctx.suspend();
		else if (fm.canRun()) fm.ctx.resume();
	});
	fm.stop = function() {
		fm.pending = null;
		if (fm.cur) {
			if (fm.cur.src) { fm.cur.src.onended = null; try { fm.cur.src.stop(); } catch(e) {} }
			fm.cur.gain.disconnect();
			fm.cur = null;
		}
	};
	fm.start = function(name, loop) {
		fm.stop();
		if (fm.muted) return;	// ?nomusic: never start a source at all
		var buf = fm.tracks[name];
		if (!buf || buf === 'loading') { fm.pending = { name: name, loop: loop }; return; }
		var src = fm.ctx.createBufferSource();
		src.buffer = buf;
		src.loop = !!loop;
		src.playbackRate.value = fm.rate;
		var gain = fm.ctx.createGain();
		gain.gain.value = fm.vol;
		src.connect(gain).connect(fm.master);
		src.onended = function() { if (fm.cur && fm.cur.src === src) fm.cur.src = null; };
		src.start();
		fm.cur = { src: src, gain: gain, name: name };
	};
});

EM_JS(void, WebMusicJSLoad, (const char* namePtr, const char* dataPtr, int size), {
	var fm = globalThis.frankMusic;
	if (!fm) return;
	var name = UTF8ToString(namePtr);
	if (fm.tracks[name]) return;	// cached or already decoding
	fm.tracks[name] = 'loading';
	// slice copies out of wasm memory synchronously, so the C side can free its copy
	var bytes = HEAPU8.slice(dataPtr, dataPtr + size);
	fm.ctx.decodeAudioData(bytes.buffer).then(function(buf) {
		fm.tracks[name] = buf;
		if (fm.pending && fm.pending.name === name) {
			var p = fm.pending;
			fm.pending = null;
			fm.start(p.name, p.loop);
		}
	}).catch(function(e) {
		delete fm.tracks[name];
		console.log('music decode failed: ' + name + ': ' + e);
	});
});

EM_JS(void, WebMusicJSPlay, (const char* namePtr, int loop), {
	var fm = globalThis.frankMusic;
	if (fm) fm.start(UTF8ToString(namePtr), !!loop);
});

EM_JS(void, WebMusicJSStop, (), {
	var fm = globalThis.frankMusic;
	if (fm) fm.stop();
});

EM_JS(void, WebMusicJSSetVolume, (double gain), {
	var fm = globalThis.frankMusic;
	if (!fm) return;
	fm.vol = gain;
	if (fm.muted) return;
	if (fm.cur) fm.cur.gain.gain.value = gain;
});

EM_JS(void, WebMusicJSSetRate, (double rate), {
	var fm = globalThis.frankMusic;
	if (!fm) return;
	fm.rate = rate;
	if (fm.cur && fm.cur.src) fm.cur.src.playbackRate.value = rate;
});

EM_JS(void, WebMusicJSPause, (int pause), {
	var fm = globalThis.frankMusic;
	if (!fm) return;
	fm.userPaused = !!pause;
	if (pause) fm.ctx.suspend();
	else if (fm.canRun()) fm.ctx.resume();
});

EM_JS(int, WebMusicJSIsPlaying, (), {
	var fm = globalThis.frankMusic;
	return (fm && (fm.pending || (fm.cur && fm.cur.src))) ? 1 : 0;
});

// C wrappers used by musicControl.cpp (declared there)

static char webMusicCurrentName[256];

bool FrankWebMusicOpen(const WCHAR* filename, const char* data, int dataSize)
{
	if (!data || dataSize <= 0)
		return false;
	WebMusicJSInit();
	// music filenames are plain ascii; drop anything that isn't
	int o = 0;
	for (int i = 0; filename[i] && o < 255; ++i)
		if (filename[i] > 0 && filename[i] < 128)
			webMusicCurrentName[o++] = (char)filename[i];
	webMusicCurrentName[o] = 0;
	WebMusicJSLoad(webMusicCurrentName, data, dataSize);
	return true;
}

void FrankWebMusicPlay(bool loop)		{ WebMusicJSPlay(webMusicCurrentName, loop); }
void FrankWebMusicStop()				{ WebMusicJSStop(); }
void FrankWebMusicPause(bool pause)		{ WebMusicJSPause(pause); }
void FrankWebMusicSetVolume(float percent)	{ WebMusicJSSetVolume(WebVolumePercentToGain(percent)); }
void FrankWebMusicSetRate(float rate)	{ WebMusicJSSetRate(rate < 0 ? 0 : rate); }
bool FrankWebMusicIsPlaying()			{ return WebMusicJSIsPlaying() != 0; }
