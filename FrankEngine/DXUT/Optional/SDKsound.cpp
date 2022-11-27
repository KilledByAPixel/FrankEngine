//-----------------------------------------------------------------------------
// File: DXUTsound.cpp
//
// Desc: DirectSound framework classes for playing wav files in DirectSound
//       buffers. Feel free to use these classes as a starting point for adding
//       extra functionality.
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#define STRICT
#include "frankEngine.h"
#include "DXUT.h"
#include <mmsystem.h>
#include <dsound.h>
#include "SDKsound.h"
#include "SDKwavefile.h"
#undef min // use __min instead
#undef max // use __max instead

// Allow global access to direct sound
IDirectSound8* CSoundManager::m_pDS = NULL;

//-----------------------------------------------------------------------------
// Name: CSoundManager::~CSoundManager()
// Desc: Destroys the class
//-----------------------------------------------------------------------------
CSoundManager::~CSoundManager()
{
    SAFE_RELEASE( m_pDS );
}

//-----------------------------------------------------------------------------
// Name: CSoundManager::Initialize()
// Desc: Initializes the IDirectSound object and also sets the primary buffer
//       format.  This function must be called before any others.
//-----------------------------------------------------------------------------
HRESULT CSoundManager::Initialize( HWND  hWnd, DWORD dwCoopLevel )
{
    HRESULT  hr;
    SAFE_RELEASE( m_pDS );

    // Create IDirectSound using the primary sound device
    if( FAILED( hr = DirectSoundCreate8( NULL, &m_pDS, NULL ) ) )
        return DXUT_ERR( L"DirectSoundCreate8", hr );

    // Set DirectSound coop level
    if( FAILED( hr = m_pDS->SetCooperativeLevel( hWnd, dwCoopLevel ) ) )
        return DXUT_ERR( L"SetCooperativeLevel", hr );

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: CSoundManager::SetPrimaryBufferFormat()
// Desc: Set primary buffer to a specified format
//       !WARNING! - Setting the primary buffer format and then using this
//                   same DirectSound object for DirectMusic messes up
//                   DirectMusic!
//       For example, to set the primary buffer format to 22kHz stereo, 16-bit
//       then:   dwPrimaryChannels = 2
//               dwPrimaryFreq     = 22050,
//               dwPrimaryBitRate  = 16
//-----------------------------------------------------------------------------
HRESULT CSoundManager::SetPrimaryBufferFormat( DWORD dwPrimaryChannels,
                                               DWORD dwPrimaryFreq,
                                               DWORD dwPrimaryBitRate )
{
    HRESULT             hr;
    LPDIRECTSOUNDBUFFER pDSBPrimary = NULL;

    if( !m_pDS )
        return CO_E_NOTINITIALIZED;

    // Get the primary buffer
    DSBUFFERDESC dsbd;
    ZeroMemory( &dsbd, sizeof(DSBUFFERDESC) );
    dsbd.dwSize        = sizeof(DSBUFFERDESC);
    dsbd.dwFlags       = DSBCAPS_PRIMARYBUFFER;
    dsbd.dwBufferBytes = 0;
    dsbd.lpwfxFormat   = NULL;

    if( FAILED( hr = m_pDS->CreateSoundBuffer( &dsbd, &pDSBPrimary, NULL ) ) )
        return DXUT_ERR( L"CreateSoundBuffer", hr );

    WAVEFORMATEX wfx;
    ZeroMemory( &wfx, sizeof(WAVEFORMATEX) );
    wfx.wFormatTag      = (WORD) WAVE_FORMAT_PCM;
    wfx.nChannels       = (WORD) dwPrimaryChannels;
    wfx.nSamplesPerSec  = (DWORD) dwPrimaryFreq;
    wfx.wBitsPerSample  = (WORD) dwPrimaryBitRate;
    wfx.nBlockAlign     = (WORD) (wfx.wBitsPerSample / 8 * wfx.nChannels);
    wfx.nAvgBytesPerSec = (DWORD) (wfx.nSamplesPerSec * wfx.nBlockAlign);

    if( FAILED( hr = pDSBPrimary->SetFormat(&wfx) ) )
        return DXUT_ERR( L"SetFormat", hr );

    SAFE_RELEASE( pDSBPrimary );
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CSoundManager::Get3DListenerInterface()
// Desc: Returns the 3D listener interface associated with primary buffer.
//-----------------------------------------------------------------------------
HRESULT CSoundManager::Get3DListenerInterface( LPDIRECTSOUND3DLISTENER* ppDSListener )
{
    HRESULT             hr;
    DSBUFFERDESC        dsbdesc;
    LPDIRECTSOUNDBUFFER pDSBPrimary = NULL;

    if( !ppDSListener  )
        return E_INVALIDARG;
    if( !m_pDS )
        return CO_E_NOTINITIALIZED;

    *ppDSListener = NULL;

    // Obtain primary buffer, asking it for 3D control
    ZeroMemory( &dsbdesc, sizeof(DSBUFFERDESC) );
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER;
    if( FAILED( hr = m_pDS->CreateSoundBuffer( &dsbdesc, &pDSBPrimary, NULL ) ) )
        return DXUT_ERR( L"CreateSoundBuffer", hr );

    if( FAILED( hr = pDSBPrimary->QueryInterface( IID_IDirectSound3DListener, (VOID**)ppDSListener ) ) )
    {
        SAFE_RELEASE( pDSBPrimary );
        return DXUT_ERR( L"QueryInterface", hr );
    }

    // Release the primary buffer, since it is not need anymore
    SAFE_RELEASE( pDSBPrimary );
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: CSoundManager::Create()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CSoundManager::Create( CSound** ppSound,
                               LPWSTR strWaveFileName,
                               DWORD dwCreationFlags,
                               GUID guid3DAlgorithm,
                               DWORD dwNumBuffers )
{
    HRESULT hr;
    HRESULT hrRet = S_OK;
    LPDIRECTSOUNDBUFFER  apDSBuffer     = NULL;
    DWORD                dwDSBufferSize = NULL;
    CWaveFile*           pWaveFile      = NULL;

    if( !m_pDS )
        return CO_E_NOTINITIALIZED;
    if( !strWaveFileName || !ppSound || dwNumBuffers < 1 )
        return E_INVALIDARG;

    pWaveFile = new CWaveFile();
    if( !pWaveFile )
    {
        hr = E_OUTOFMEMORY;
        goto LFail;
    }

    pWaveFile->Open( strWaveFileName, NULL, WAVEFILE_READ );
    if( pWaveFile->GetSize() == 0 )
    {
        // Wave is blank, so don't create it.
        hr = E_FAIL;
        goto LFail;
    }

    // Make the DirectSound buffer the same size as the wav file
    dwDSBufferSize = pWaveFile->GetSize();

    // Create the direct sound buffer, and only request the flags needed
    // since each requires some overhead and limits if the buffer can
    // be hardware accelerated
    DSBUFFERDESC dsbd;
    ZeroMemory( &dsbd, sizeof(DSBUFFERDESC) );
    dsbd.dwSize          = sizeof(DSBUFFERDESC);
    dsbd.dwFlags         = dwCreationFlags;
    dsbd.dwBufferBytes   = dwDSBufferSize;
    dsbd.guid3DAlgorithm = guid3DAlgorithm;
    dsbd.lpwfxFormat     = pWaveFile->m_pwfx;

    // DirectSound is only guarenteed to play PCM data.  Other
    // formats may or may not work depending the sound card driver.
    hr = m_pDS->CreateSoundBuffer( &dsbd, &apDSBuffer, NULL );

    // Be sure to return this error code if it occurs so the
    // callers knows this happened.
    if( hr == DS_NO_VIRTUALIZATION )
        hrRet = DS_NO_VIRTUALIZATION;

    if( FAILED(hr) )
    {
        // DSERR_BUFFERTOOSMALL will be returned if the buffer is
        // less than DSBSIZE_FX_MIN and the buffer is created
        // with DSBCAPS_CTRLFX.

        // It might also fail if hardware buffer mixing was requested
        // on a device that doesn't support it.
        DXUT_ERR( L"CreateSoundBuffer", hr );

        goto LFail;
    }

    // Create the sound
    *ppSound = new CSound( apDSBuffer, dwDSBufferSize, dwNumBuffers, pWaveFile, dwCreationFlags );

    return hrRet;

LFail:
    // Cleanup
    SAFE_DELETE( pWaveFile );
    return hr;
}

//-----------------------------------------------------------------------------
// Name: CSound::CSound()
// Desc: Constructs the class
//-----------------------------------------------------------------------------
CSound::CSound( LPDIRECTSOUNDBUFFER apDSBuffer, DWORD dwDSBufferSize,
                DWORD dwNumBuffers, CWaveFile* pWaveFile, DWORD dwCreationFlags )
{
    m_apDSBuffer.buffer = apDSBuffer;

    m_dwDSBufferSize = dwDSBufferSize;
    m_dwNumBuffers   = dwNumBuffers;
    m_pWaveFile      = pWaveFile;
    m_dwCreationFlags = dwCreationFlags;
	m_dwBufferCount  = 0;
    FillBufferWithSound( m_apDSBuffer.buffer, FALSE );
}


//-----------------------------------------------------------------------------
// Name: CSound::~CSound()
// Desc: Destroys the class
//-----------------------------------------------------------------------------
CSound::~CSound()
{
    SAFE_RELEASE( m_apDSBuffer.buffer );
    SAFE_DELETE( m_pWaveFile );
}


//-----------------------------------------------------------------------------
// Name: CSound::FillBufferWithSound()
// Desc: Fills a DirectSound buffer with a sound file
//-----------------------------------------------------------------------------
HRESULT CSound::FillBufferWithSound( LPDIRECTSOUNDBUFFER pDSB, BOOL bRepeatWavIfBufferLarger )
{
    HRESULT hr;
    VOID*   pDSLockedBuffer      = NULL; // Pointer to locked buffer memory
    DWORD   dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
    DWORD   dwWavDataRead        = 0;    // Amount of data read from the wav file

    if( !pDSB )
        return CO_E_NOTINITIALIZED;

    // Make sure we have focus, and we didn't just switch in from
    // an app which had a DirectSound device
    if( FAILED( hr = RestoreBuffer( pDSB, NULL ) ) )
        return DXUT_ERR( L"RestoreBuffer", hr );

    // Lock the buffer down
    if( FAILED( hr = pDSB->Lock( 0, m_dwDSBufferSize, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, NULL, 0L ) ) )
        return DXUT_ERR( L"Lock", hr );

    // Reset the wave file to the beginning
    m_pWaveFile->ResetFile();

    if( FAILED( hr = m_pWaveFile->Read( (BYTE*) pDSLockedBuffer, dwDSLockedBufferSize, &dwWavDataRead ) ) )
        return DXUT_ERR( L"Read", hr );

    if( dwWavDataRead == 0 )
    {
        // Wav is blank, so just fill with silence
        FillMemory( (BYTE*) pDSLockedBuffer, dwDSLockedBufferSize,  (BYTE)(m_pWaveFile->m_pwfx->wBitsPerSample == 8 ? 128 : 0 ) );
    }
    else if( dwWavDataRead < dwDSLockedBufferSize )
    {
        // If the wav file was smaller than the DirectSound buffer,
        // we need to fill the remainder of the buffer with data
        if( bRepeatWavIfBufferLarger )
        {
            // Reset the file and fill the buffer with wav data
            DWORD dwReadSoFar = dwWavDataRead;    // From previous call above.
            while( dwReadSoFar < dwDSLockedBufferSize )
            {
                // This will keep reading in until the buffer is full
                // for very short files
                if( FAILED( hr = m_pWaveFile->ResetFile() ) )
                    return DXUT_ERR( L"ResetFile", hr );

                hr = m_pWaveFile->Read( (BYTE*)pDSLockedBuffer + dwReadSoFar,
                                        dwDSLockedBufferSize - dwReadSoFar,
                                        &dwWavDataRead );
                if( FAILED(hr) )
                    return DXUT_ERR( L"Read", hr );

                dwReadSoFar += dwWavDataRead;
            }
        }
        else
        {
            // Don't repeat the wav file, just fill in silence
            FillMemory( (BYTE*) pDSLockedBuffer + dwWavDataRead,
                        dwDSLockedBufferSize - dwWavDataRead,
                        (BYTE)(m_pWaveFile->m_pwfx->wBitsPerSample == 8 ? 128 : 0 ) );
        }
    }

    // Unlock the buffer, we don't need it anymore.
    pDSB->Unlock( pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0 );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CSound::RestoreBuffer()
// Desc: Restores the lost buffer. *pbWasRestored returns TRUE if the buffer was
//       restored.  It can also NULL if the information is not needed.
//-----------------------------------------------------------------------------
HRESULT CSound::RestoreBuffer( LPDIRECTSOUNDBUFFER pDSB, BOOL* pbWasRestored )
{
    HRESULT hr;

    if( !pDSB )
        return CO_E_NOTINITIALIZED;
    if( pbWasRestored )
        *pbWasRestored = FALSE;

    DWORD dwStatus;
    if( FAILED( hr = pDSB->GetStatus( &dwStatus ) ) )
        return DXUT_ERR( L"GetStatus", hr );

    if( dwStatus & DSBSTATUS_BUFFERLOST )
    {
        // Since the app could have just been activated, then
        // DirectSound may not be giving us control yet, so
        // the restoring the buffer may fail.
        // If it does, sleep until DirectSound gives us control.
        do
        {
            hr = pDSB->Restore();
            if( hr == DSERR_BUFFERLOST )
                Sleep( 10 );
        }
        while( ( hr = pDSB->Restore() ) == DSERR_BUFFERLOST );

        if( pbWasRestored != NULL )
            *pbWasRestored = TRUE;

        return S_OK;
    }
    else
    {
        return S_FALSE;
    }
}

bool CSound::BufferWrapper::IsPlaying() const
{
	if (!buffer)
		return false;

	DWORD dwStatus = 0;
	buffer->GetStatus( &dwStatus );
	return ((dwStatus & DSBSTATUS_PLAYING) != 0);
}

void CSound::BufferWrapper::UpdateTimeScale(float scale)
{
	if (!buffer || !IsPlaying())
		return;

	SetFrequency( scale * frequencyScale );
}

void CSound::BufferWrapper::SetVolume(float volume)
{
	if (!buffer)
		return;

	LONG lVolume = (LONG)(DSBVOLUME_MIN + (SoundControl::soundVolumeScale*SoundControl::masterVolume*volume) * (DSBVOLUME_MAX - DSBVOLUME_MIN));
	buffer->SetVolume( lVolume );
}

void CSound::BufferWrapper::SetFrequency(float frequency)
{
	if (!buffer)
		return;
	
	// scale the frequency
	WAVEFORMATEX waveFormat;
	buffer->GetFormat(&waveFormat, sizeof(WAVEFORMATEX), NULL);
	const DWORD dwBaseFrequency = waveFormat.nSamplesPerSec;
	DWORD f = DWORD(DSBFREQUENCY_MIN + (dwBaseFrequency-DSBFREQUENCY_MIN) * frequency);
	buffer->SetFrequency( f );
}

void CSound::BufferWrapper::Stop()
{
	if (buffer)
		buffer->Stop();
}

void CSound::BufferWrapper::Restart()
{
	if (buffer)
		buffer->SetCurrentPosition( 0 );
}

void CSound::BufferWrapper::Release()
{
	if (buffer)
		--sound->m_dwBufferCount;
	SAFE_RELEASE(buffer);
}

//-----------------------------------------------------------------------------
// Name: CSound::GetFreeBuffer()
// Desc: Finding the first buffer that is not playing and return a pointer to
//       it, or if all are playing return a pointer to a randomly selected buffer.
//-----------------------------------------------------------------------------
CSound::BufferWrapper CSound::GetFreeBuffer()
{
	BufferWrapper bufferWrapper;
	bufferWrapper.sound = this;
	if (m_dwBufferCount >= m_dwNumBuffers)
		return bufferWrapper;

	if( m_apDSBuffer.buffer )
        CSoundManager::m_pDS->DuplicateSoundBuffer(m_apDSBuffer.buffer, &bufferWrapper.buffer);
	
	if (bufferWrapper.buffer)
		++m_dwBufferCount;
    return bufferWrapper;
}

//-----------------------------------------------------------------------------
// Name: CSound::Play()
// Desc: Plays the sound using voice management flags.  Pass in DSBPLAY_LOOPING
//       in the dwFlags to loop the sound
//-----------------------------------------------------------------------------
HRESULT CSound::Play( DWORD dwPriority, DWORD dwFlags, float volumePercent, float frequencyScale, LONG lPan, BufferWrapper* buffer, float frequencyTimeScale )
{
    if( !m_apDSBuffer.buffer )
        return CO_E_NOTINITIALIZED;

   BufferWrapper bufferWrapper = GetFreeBuffer();
    if( !bufferWrapper.buffer  )
        return E_FAIL;

	bufferWrapper.volumePercent = volumePercent;
	bufferWrapper.frequencyScale = frequencyScale;

    // Restore the buffer if it was lost
    //BOOL    bRestored;
    //if( FAILED( hr = RestoreBuffer( bufferWrapper.buffer, &bRestored ) ) )
    //    return DXUT_ERR( L"RestoreBuffer", hr );
	//
    //if( bRestored )
    //{
    //    // The buffer was restored, so we need to fill it with new data
    //    if( FAILED( hr = FillBufferWithSound( bufferWrapper.buffer, FALSE ) ) )
    //        return DXUT_ERR( L"FillBufferWithSound", hr );
    //}

    if( m_dwCreationFlags & DSBCAPS_CTRLVOLUME )
		bufferWrapper.SetVolume(volumePercent);

    if( m_dwCreationFlags & DSBCAPS_CTRLFREQUENCY )
		bufferWrapper.SetFrequency(frequencyScale * frequencyTimeScale);

    if( m_dwCreationFlags & DSBCAPS_CTRLPAN )
        bufferWrapper.buffer->SetPan( lPan );
	
	if (buffer)
		*buffer = bufferWrapper;
    return bufferWrapper.buffer->Play( 0, dwPriority, dwFlags );
}

//-----------------------------------------------------------------------------
// Name: CSound::Play3D()
// Desc: Plays the sound using voice management flags.  Pass in DSBPLAY_LOOPING
//       in the dwFlags to loop the sound
//-----------------------------------------------------------------------------
HRESULT CSound::Play3D( LPDS3DBUFFER p3DBuffer, DWORD dwPriority, DWORD dwFlags, float volumePercent, float frequencyScale, BufferWrapper* buffer, float frequencyTimeScale, float volumeScale)
{
    HRESULT hr;
	
    if( !m_apDSBuffer.buffer )
        return CO_E_NOTINITIALIZED;

    BufferWrapper bufferWrapper = GetFreeBuffer();
    if( !bufferWrapper.buffer )
        return DXUT_ERR( L"GetFreeBuffer", E_FAIL );

	bufferWrapper.volumePercent = volumePercent;
	bufferWrapper.frequencyScale = frequencyScale;

    // Restore the buffer if it was lost
    //BOOL    bRestored;
    //if( FAILED( hr = RestoreBuffer( pDSB->buffer, &bRestored ) ) )
    //    return DXUT_ERR( L"RestoreBuffer", hr );
	//
    //if( bRestored )
    //{
    //    // The buffer was restored, so we need to fill it with new data
    //    if( FAILED( hr = FillBufferWithSound( pDSB->buffer, FALSE ) ) )
    //        return DXUT_ERR( L"FillBufferWithSound", hr );
    //}
	
    if( m_dwCreationFlags & DSBCAPS_CTRLVOLUME )
		bufferWrapper.SetVolume(volumePercent*volumeScale);

    if( m_dwCreationFlags & DSBCAPS_CTRLFREQUENCY )
		bufferWrapper.SetFrequency(frequencyScale * frequencyTimeScale);

    // QI for the 3D buffer
    LPDIRECTSOUND3DBUFFER pDS3DBuffer;
    hr = bufferWrapper.buffer->QueryInterface( IID_IDirectSound3DBuffer, (VOID**) &pDS3DBuffer );
    if( SUCCEEDED( hr ) )
    {
        hr = pDS3DBuffer->SetAllParameters( p3DBuffer, DS3D_IMMEDIATE );
        pDS3DBuffer->Release();
    }
	
    if( SUCCEEDED( hr ) )
        hr = bufferWrapper.buffer->Play( 0, dwPriority, dwFlags );
	
	if (buffer)
		*buffer = bufferWrapper;
    return hr;
}
