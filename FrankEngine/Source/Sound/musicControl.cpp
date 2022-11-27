////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Ogg Vorbis Player
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../sound/musicControl.h"

#include <dsound.h>
#include "../../../oggvorbis/vorbis/include/vorbis/codec.h"
#include "../../../oggvorbis/vorbis/include/vorbis/vorbisfile.h"

const DWORD MusicControl::bufferSize = 65536;

////////////////////////////////////////////////////////////////////////////////////////////

// callbacks used to stream the file from mem
static long VorbisTell(void *datasource);
static int VorbisClose(void *datasourc);
static int VorbisSeek(void *datasource, ogg_int64_t offset, int whence);
static size_t VorbisRead(void *ptr, size_t byteSize, size_t sizeToRead, void *datasource);
static ov_callbacks vorbisCallbacks;

// struct that contains the pointer to our file in memory

static SOggFile oggMemoryFile;		// Data on the ogg file in memory

////////////////////////////////////////////////////////////////////////////////////////////

bool MusicControl::musicEnable = true;
ConsoleCommand(MusicControl::musicEnable, musicEnable);

float MusicControl::masterVolume = 1.0f;
ConsoleCommand(MusicControl::masterVolume, musicVolume);

MusicControl::MusicControl() :
	ds(NULL),
	dsBuffer(NULL),
	fileOpened(false),
	lastSection(0),
	curSection(0),
	loop(false),
	done(true),
	almostDone(false),
	pause(false),
	enableFrequencyScale(true),
	frequencyScale(1.0f),
	volumeScale(1.0f),
	vf(new OggVorbis_File())
{
}

MusicControl::~MusicControl() 
{
}

void MusicControl::ShutDown()
{
	if (fileOpened)
		Close(oggMemoryFile);

	for (list<SOggFile>::iterator it = loadedOggFiles.begin(); it != loadedOggFiles.end(); ++it)
	{
		SOggFile& oggFile = *it;
		
		if (oggFile.dataPtr)
			delete [] oggFile.dataPtr;
	}
	loadedOggFiles.clear();

	SAFE_DELETE(vf);
}

void MusicControl::Transition(const WCHAR *filename, float outTime, float inTime, bool loop)
{
	if (transitionIn && wcscmp(filename, transitionFilename) == 0)
		return;	// already transitioning in
	
	if (transitionIn && transitionPercent == 1 && wcscmp(L"", transitionFilename) == 0)
		transitionPercent = 0;	// skip transition out if mute

	wcsncpy_s(transitionFilename, filename, GameObjectStub::attributesLength);
	transitionOutTime = outTime;
	transitionInTime = inTime;
	transitionLoop = loop;
	transitionIn = false;
	wcsncpy_s(transitionFilename, filename, GameObjectStub::attributesLength);
}

void MusicControl::Reset()
{
	Close(oggMemoryFile);
	wcsncpy_s(openFilename, L"", GameObjectStub::attributesLength);
	wcsncpy_s(transitionFilename, L"", GameObjectStub::attributesLength);
	transitionIn = true;
	transitionPercent = 1;
}

SOggFile* MusicControl::GetLoaded(WCHAR* filename)
{
	for (list<SOggFile>::iterator it = loadedOggFiles.begin(); it != loadedOggFiles.end(); ++it)
	{
		SOggFile& oggFile = *it;

		if (wcscmp(filename, oggFile.filename) == 0)
			return &oggFile;
	}

	return NULL;
}

bool MusicControl::Load(WCHAR* filename)
{
	SOggFile* oggFileLoaded = GetLoaded(filename);
	if (oggFileLoaded)
		return true;

	SOggFile oggFile;
	if (!LoadInternal(filename, oggFile))
		return false;
	oggFile.isPreLoaded = true;
	loadedOggFiles.push_back(oggFile);

	return true;
}

bool MusicControl::LoadInternal(WCHAR* filename, SOggFile& oggFile)
{
	// read the file into memory
	FILE *f = NULL;
		
	char filenameChar[FILENAME_STRING_LENGTH];
	wcstombs_s(NULL, filenameChar, FILENAME_STRING_LENGTH, filename, FILENAME_STRING_LENGTH);

	fopen_s(&f, filenameChar, "rb");
	if (!f)
	{
		// hack: check music folder
		// todo: search subfolders, or way to add list of sub folders to search
		WCHAR filename2[FILENAME_STRING_LENGTH];
		wcsncpy_s(filename2, FILENAME_STRING_LENGTH, L"music/", _TRUNCATE );
		wcscat_s(filename2, FILENAME_STRING_LENGTH, filename);
		wcstombs_s(NULL, filenameChar, FILENAME_STRING_LENGTH, filename2, FILENAME_STRING_LENGTH);
		
		fopen_s(&f, filenameChar, "rb");
		if (!f)
		{
			g_debugMessageSystem.AddError(L"Could not find music file \"%s\"", filename);
			return false;
		}
	}
		
	// save filename
	wcsncpy_s(oggFile.filename, FILENAME_STRING_LENGTH, filename, FILENAME_STRING_LENGTH);

	// find out how big the file is
	int sizeOfFile = 0;
	while (!feof(f))
	{
		getc(f);
		++sizeOfFile;
	}

	// move the data into memory
	oggFile.dataPtr = new char[sizeOfFile];
	rewind(f);
	int i = 0;
	while (!feof(f))
		oggFile.dataPtr[i++] = getc(f);

	// close the file
	fclose(f);

	// Save the data in the ogg memory file because we need this when we are actually reading in the data
	// We havnt read anything yet
	oggFile.dataRead = 0;
	// Save the size so we know how much we need to read
	oggFile.dataSize = sizeOfFile;	
	oggFile.isPreLoaded = false;
	return true;
}

bool MusicControl::Open( WCHAR *filename )
{
	if (!ds || !musicEnable || !filename || filename[0] == 0)
		return false;
	
	wcsncpy_s(openFilename, filename, GameObjectStub::attributesLength);

	if (fileOpened)
		Close(oggMemoryFile);

	{
		SOggFile* oggFile = GetLoaded(filename);
		if (oggFile)
		{
			oggMemoryFile = *oggFile;
		}
		else
		{
			if (!LoadInternal(filename, oggMemoryFile))
				return false;
		}
	}

	vorbisCallbacks.read_func = VorbisRead;
	vorbisCallbacks.close_func = VorbisClose;
	vorbisCallbacks.seek_func = VorbisSeek;
	vorbisCallbacks.tell_func = VorbisTell;
	if (ov_open_callbacks(&oggMemoryFile, vf, NULL, 0, vorbisCallbacks) != 0)
	{
		delete [] oggMemoryFile.dataPtr;
		oggMemoryFile.dataPtr = NULL;
		return false;
	}

	// ok now the tricky part
	// the vorbis_info struct keeps the most of the interesting format info
	vorbis_info *vi = ov_info(vf,-1);

	// set the wave format
	WAVEFORMATEX wfm;
	memset(&wfm, 0, sizeof(wfm));

	wfm.cbSize			= sizeof(wfm);
	wfm.nChannels		= vi->channels;
	wfm.wBitsPerSample	= 16;                    // ogg vorbis is always 16 bit
	wfm.nSamplesPerSec	= vi->rate;
	wfm.nAvgBytesPerSec	= wfm.nSamplesPerSec*wfm.nChannels*2;
	wfm.nBlockAlign		= 2*wfm.nChannels;
	wfm.wFormatTag		= 1;

	// set up the buffer
	DSBUFFERDESC desc;
	desc.dwSize			= sizeof(desc);
	desc.dwFlags		= DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLFREQUENCY;
	desc.lpwfxFormat	= &wfm;
	desc.dwReserved		= 0;
	desc.dwBufferBytes  = bufferSize*2;
	ds->CreateSoundBuffer(&desc, &dsBuffer, NULL );

	// fill the buffer
	DWORD	pos = 0;
	int		sec = 0;
	int		ret = 1;
	DWORD	size = bufferSize*2;

	char *buf;
	dsBuffer->Lock(0, size, (LPVOID*)&buf, &size, NULL, NULL, DSBLOCK_ENTIREBUFFER);

	// now read in the bits
	while (ret && pos<size)
	{
		ret = ov_read(vf, buf+pos, size-pos, 0, 2, 1, &sec);
		pos += ret;
	}

	dsBuffer->Unlock( buf, size, NULL, NULL );

	curSection		= 0;
	lastSection		= 0;
	fileOpened		= true;

	return fileOpened;
}

void MusicControl::Close()
{
	Close(oggMemoryFile);
}

void MusicControl::Close(SOggFile& oggFile)
{
	if (!dsBuffer || !fileOpened)
		return;

	fileOpened = false;
	ov_clear(vf);

	if (oggFile.dataPtr && !oggFile.isPreLoaded)
	{
		delete [] oggFile.dataPtr;
		oggFile.dataPtr = NULL;
	}

	SAFE_RELEASE(dsBuffer);
	done = true;
}

// clear console
static void ConsoleCommandCallback_musicPause(const wstring& text)
{
	int pause = g_sound->GetMusicPlayer().IsPaused();
	swscanf_s(text.c_str(), L"%d", &pause);
	g_sound->GetMusicPlayer().Pause(pause != 0);
}
ConsoleCommand(ConsoleCommandCallback_musicPause, musicPause);

void MusicControl::Pause( bool _pause )
{
	if (!dsBuffer || pause == _pause)
		return;

	pause = _pause;

	if (pause)	
		dsBuffer->Stop();    
	else
		dsBuffer->Play(0, 0, DSBPLAY_LOOPING);
}

void MusicControl::Play( bool _loop )
{
	if (!dsBuffer || !fileOpened || !musicEnable)
		return;

	pause = false;
	loop = _loop;
	done = false;
	almostDone = false;
	
	LONG lVolume = (LONG)(DSBVOLUME_MIN + masterVolume * volumeScale * transitionPercent * (DSBVOLUME_MAX - DSBVOLUME_MIN));
	dsBuffer->SetVolume(lVolume);

	// always play looping because we will fill the buffer
	dsBuffer->Play(0, 0, DSBPLAY_LOOPING);
}

void MusicControl::Stop()
{
	if (!dsBuffer || !fileOpened)
		return;

	dsBuffer->Stop();
}

void MusicControl::Update()
{
	if (!transitionIn)
	{
		if (transitionOutTime > 0)
			transitionPercent -= GAME_TIME_STEP / transitionOutTime;
		else
			transitionPercent = 0;
		transitionPercent = CapPercent(transitionPercent);

		if (transitionPercent == 0)
		{
			transitionIn = true;
			Close(oggMemoryFile);
			if (Open(transitionFilename))
				Play(transitionLoop);
			wcsncpy_s(openFilename, transitionFilename, GameObjectStub::attributesLength);
		}
	}
	else if (transitionIn)
	{
		if (transitionInTime > 0)
			transitionPercent += GAME_TIME_STEP / transitionInTime;
		else
			transitionPercent = 1;
		transitionPercent = CapPercent(transitionPercent);
	}
	
	if (!dsBuffer)
		return;

	if (!musicEnable)
		Stop();

	{
		// scale the frequency
		WAVEFORMATEX waveFormat;
		dsBuffer->GetFormat(&waveFormat, sizeof(waveFormat), NULL);

		DWORD dwBaseFrequency = waveFormat.nSamplesPerSec;
		DWORD f = DWORD(dwBaseFrequency * (enableFrequencyScale? frequencyScale : 1.0f));

		// don't let it equal 0 caus that will set it to default
		if (f == 0)
			f = DSBFREQUENCY_MIN;
		else if (f > DSBFREQUENCY_MAX)
			f = DSBFREQUENCY_MAX;
		dsBuffer->SetFrequency( f );
	}

	LONG lVolume = (LONG)(DSBVOLUME_MIN + masterVolume * volumeScale * transitionPercent * (DSBVOLUME_MAX - DSBVOLUME_MIN));
	dsBuffer->SetVolume(lVolume);

	DWORD pos;
	dsBuffer->GetCurrentPosition(&pos, NULL);

	curSection = pos < bufferSize ? 0 : 1;

	// section changed?
	if (curSection != lastSection)
	{
		if (done && !loop)
			Stop();

		// gotta use this trick 'cause otherwise there wont be played all bits
		if (almostDone && !loop)
			done = true;

		DWORD size = bufferSize;
		char *buf;

		// fill the section we just left
		dsBuffer->Lock( lastSection*bufferSize, size, (LPVOID*)&buf, &size, NULL, NULL, 0 );

		DWORD	pos = 0;
		int		sec = 0;
		int		ret = 1;
	            
		while (ret && pos < size)
		{
			ret = ov_read(vf, buf+pos, size-pos, 0, 2, 1, &sec);
			pos += ret;
		}

		// reached the and?
		if (!ret && loop)
		{
			// we are looping so restart from the beginning
			// NOTE: sound with sizes smaller than bufferSize may be cut off

			ret = 1;
			ov_pcm_seek(vf, 0);
			while(ret && pos<size)
			{
				ret = ov_read(vf, buf+pos, size-pos, 0, 2, 1, &sec);
				pos += ret;
			}
		}
		else if (!ret && !loop)
		{
			// not looping so fill the rest with 0
			while (pos < size)
			{
				*(buf+pos)=0; 
				pos++;
			}

			// and say that after the current section no other sectin follows
			almostDone = true;
		}
	            
		dsBuffer->Unlock( buf, size, NULL, NULL );

		lastSection = curSection;
	}
}


/************************************************************************************************************************
	The following function are the vorbis callback functions.  As their names suggest, they are expected to work in exactly the
	same way as normal c io functions (fread, fclose etc.).  Its up to us to return the information that the libs need to parse
	the file from memory

	These call back functions are from a tutorial by Spree Tree at http://www.devmaster.net/articles/openal-ogg-file/
************************************************************************************************************************/
//---------------------------------------------------------------------------------
// Function	: VorbisRead
// Purpose	: Callback for the Vorbis read function
// Info		: 
//---------------------------------------------------------------------------------
static size_t VorbisRead(void *ptr			/* ptr to the data that the vorbis files need*/, 
				  size_t byteSize			/* how big a byte is*/, 
				  size_t sizeToRead			/* How much we can read*/, 
				  void *datasource			/* this is a pointer to the data we passed into ov_open_callbacks (our SOggFile struct*/)
{
	size_t				spaceToEOF;			// How much more we can read till we hit the EOF marker
	size_t				actualSizeToRead;	// How much data we are actually going to read from memory
	SOggFile*			vorbisData;			// Our vorbis data, for the typecast

	// Get the data in the right format
	vorbisData = (SOggFile*)datasource;

	// Calculate how much we need to read.  This can be sizeToRead*byteSize or less depending on how near the EOF marker we are
	spaceToEOF = vorbisData->dataSize - vorbisData->dataRead;
	if ((sizeToRead*byteSize) < spaceToEOF)
		actualSizeToRead = (sizeToRead*byteSize);
	else
		actualSizeToRead = spaceToEOF;	
	
	// A simple copy of the data from memory to the datastruct that the vorbis libs will use
	if (actualSizeToRead)
	{
		// Copy the data from the start of the file PLUS how much we have already read in
		memcpy(ptr, (char*)vorbisData->dataPtr + vorbisData->dataRead, actualSizeToRead);
		// Increase by how much we have read by
		vorbisData->dataRead += (actualSizeToRead);
	}

	// Return how much we read (in the same way fread would)
	return actualSizeToRead;
}

//---------------------------------------------------------------------------------
// Function	: VorbisSeek
// Purpose	: Callback for the Vorbis seek function
// Info		: 
//---------------------------------------------------------------------------------
static int VorbisSeek(void *datasource	/*this is a pointer to the data we passed into ov_open_callbacks (our SOggFile struct*/, 
			   ogg_int64_t offset		/*offset from the point we wish to seek to*/, 
			   int whence				/*where we want to seek to*/)
{
	size_t				spaceToEOF;		// How much more we can read till we hit the EOF marker
	ogg_int64_t			actualOffset;	// How much we can actually offset it by
	SOggFile*			vorbisData;		// The data we passed in (for the typecast)

	// Get the data in the right format
	vorbisData = (SOggFile*)datasource;

	// Goto where we wish to seek to
	switch (whence)
	{
	case SEEK_SET: // Seek to the start of the data file
		// Make sure we are not going to the end of the file
		if (vorbisData->dataSize >= offset)
			actualOffset = offset;
		else
			actualOffset = vorbisData->dataSize;
		// Set where we now are
		vorbisData->dataRead = (int)actualOffset;
		break;
	case SEEK_CUR: // Seek from where we are
		// Make sure we dont go past the end
		spaceToEOF = vorbisData->dataSize - vorbisData->dataRead;
		if (offset < spaceToEOF)
			actualOffset = (offset);
		else
			actualOffset = spaceToEOF;	
		// Seek from our currrent location
		vorbisData->dataRead += (int)actualOffset;
		break;
	case SEEK_END: // Seek from the end of the file
		vorbisData->dataRead = vorbisData->dataSize+1;
		break;
	default:
		printf("*** ERROR *** Unknown seek command in VorbisSeek\n");
		break;
	};

	return 0;
}

//---------------------------------------------------------------------------------
// Function	: VorbisClose
// Purpose	: Callback for the Vorbis close function
// Info		: 
//---------------------------------------------------------------------------------
static int VorbisClose(void *datasource /*this is a pointer to the data we passed into ov_open_callbacks (our SOggFile struct*/)
{
	// This file is called when we call ov_close.  If we wanted, we could free our memory here, but
	// in this case, we will free the memory in the main body of the program, so dont do anything
	return 1;
}

//---------------------------------------------------------------------------------
// Function	: VorbisTell
// Purpose	: Classback for the Vorbis tell function
// Info		: 
//---------------------------------------------------------------------------------
static long VorbisTell(void *datasource /*this is a pointer to the data we passed into ov_open_callbacks (our SOggFile struct*/)
{
	SOggFile*	vorbisData;

	// Get the data in the right format
	vorbisData = (SOggFile*)datasource;

	// We just want to tell the vorbis libs how much we have read so far
	return vorbisData->dataRead;
}

/************************************************************************************************************************
	End of Vorbis callback functions
************************************************************************************************************************/
