//-----------------------------------------------------------------------------
// File: DXUTsound.h
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef DXUTSOUND_H
#define DXUTSOUND_H

//-----------------------------------------------------------------------------
// Header Includes
//-----------------------------------------------------------------------------
#include <dsound.h>
#include <ks.h>

//-----------------------------------------------------------------------------
// Classes used by this header
//-----------------------------------------------------------------------------
class CSoundManager;
class CSound;
class CWaveFile;

//-----------------------------------------------------------------------------
// Name: class CSoundManager
// Desc: 
//-----------------------------------------------------------------------------
class CSoundManager
{
public:
    ~CSoundManager();

    HRESULT Initialize( HWND hWnd, DWORD dwCoopLevel );
    inline  LPDIRECTSOUND8 GetDirectSound() { return m_pDS; }
    HRESULT SetPrimaryBufferFormat( DWORD dwPrimaryChannels, DWORD dwPrimaryFreq, DWORD dwPrimaryBitRate );
    HRESULT Get3DListenerInterface( LPDIRECTSOUND3DLISTENER* ppDSListener );

	HRESULT CSoundManager::SetSpeakerConfig(DWORD speakerConfig) { return m_pDS->SetSpeakerConfig(speakerConfig); }
    HRESULT Create( CSound** ppSound, LPWSTR strWaveFileName, DWORD dwCreationFlags = 0, GUID guid3DAlgorithm = GUID_NULL, DWORD dwNumBuffers = 1 );
	
    static IDirectSound8* m_pDS;
 };


//-----------------------------------------------------------------------------
// Name: class CSound
// Desc: Encapsulates functionality of a DirectSound buffer.
//-----------------------------------------------------------------------------
class CSound
{
public: 
	struct BufferWrapper
	{
		LPDIRECTSOUNDBUFFER buffer = NULL;
		float volumePercent = 1;
		float frequencyScale = 1;
		CSound* sound = NULL;

		void Stop();
		void Release();
		bool IsPlaying() const;
		void UpdateTimeScale(float scale);
		void SetVolume(float volume);
		void SetFrequency(float frequency);
		void Restart();
	};

protected:
    BufferWrapper		 m_apDSBuffer;
    DWORD                m_dwDSBufferSize;
    CWaveFile*           m_pWaveFile;
    DWORD                m_dwNumBuffers;
    DWORD                m_dwCreationFlags;
	DWORD				 m_dwBufferCount;

    HRESULT RestoreBuffer( LPDIRECTSOUNDBUFFER pDSB, BOOL* pbWasRestored );

public:
    CSound( LPDIRECTSOUNDBUFFER apDSBuffer, DWORD dwDSBufferSize, DWORD dwNumBuffers, CWaveFile* pWaveFile, DWORD dwCreationFlags );
    ~CSound();

    HRESULT FillBufferWithSound( LPDIRECTSOUNDBUFFER pDSB, BOOL bRepeatWavIfBufferLarger );
    BufferWrapper GetFreeBuffer();

    HRESULT Play( DWORD dwPriority = 0, DWORD dwFlags = 0, float volumePercent = 1.0f, float frequencyScale = 1.0f, LONG lPan = 0, BufferWrapper* buffer = NULL, float frequencyTimeScale = 1);
    HRESULT Play3D( LPDS3DBUFFER p3DBuffer, DWORD dwPriority = 0, DWORD dwFlags = 0, float volumePercent = 1.0f, float frequencyScale = 0, BufferWrapper* buffer = NULL , float frequencyTimeScale = 1, float volumeScale = 1);
	BOOL	Is3D() const { return m_dwCreationFlags & DSBCAPS_CTRL3D; }
};

#endif // DXUTSOUND_H
