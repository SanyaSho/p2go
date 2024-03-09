//
// bink/audio.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "audio.h"
#include "bink_include.h"

#include "audiohardware/ringbuffer.h"
#include "fragment/cacheheap.h"
#include "fragment/manager.h"
#include "file/asset.h"
#include "file/stream.h"
#include "system/memops.h"
#include "system/new.h"

#define CHUNK_COUNT 16
#define TOTAL_BUFFER_MS 1000  // must be at least 800

using namespace rage;

//
// Sound channel descriptor and access macro
//

typedef struct SNDCHANNEL        // Kept in bs->snddata array, must not exceed 128 bytes in size!
{
	audReferencedRingBuffer *ringBuffer;
	HBINK bink;
	S32 status;
	S32 paused;    // 1 if paused, 0 if playing
	S32 fill_frag;
	S32 last_frag;
	U32 submitted;
	U32 preloadsize;
	U32 maxpreload;
	U32 bufsize;
	U32 mbins;
	U8* SoundBuffer;
} SNDCHANNEL;

#define BS ( ( SNDCHANNEL * ) ( & ( bs->snddata[ 0 ] ) ) )

#define BS_PRELOADING 0
#define BS_STREAMING  1

namespace rage
{

Functor2Ret<void*,u32,u32>	bwAudio::sm_Allocator	= NULL;
Functor1<void*>				bwAudio::sm_Deallocator	= NULL;
bool						bwAudio::sm_IgnoreSoundOffCallback = false;

void bwAudio::SubmitBuffer( struct BINKSND * bs, S32 index, U32 len )
{
	if(len)
	{
		// Submit the data to the voice
		BS->ringBuffer->WriteData(BS->SoundBuffer + index * BS->bufsize, len);
	}
}

int bwAudio::Init( struct BINKSND * bs )
{
  U32 rate = ( bs->freq * bs->bits * bs->chans ) / 8;
  
  BS->bufsize = ( ( ( rate * TOTAL_BUFFER_MS ) / ( 1000 * CHUNK_COUNT ) ) + 31 ) & ~31;
  BS->preloadsize = ( ( ( rate * TOTAL_BUFFER_MS ) / ( 1000 * 2 ) ) + 31 ) & ~31;
  if ( BS->preloadsize > BS->maxpreload)
    BS->preloadsize = BS->maxpreload;

  BS->status = BS_STREAMING;//BS_PRELOADING;
  BS->submitted = 0;

  BS->last_frag = -1;
  BS->fill_frag = -1;

	// create sound buffers and memory
	if ( BS->SoundBuffer == 0 )
	{
#if !__WIN32PC
		// Bink Allocations should NOT come out of Game Virtual!
		Assert(sm_Allocator);
		BS->SoundBuffer = (U8*) sm_Allocator(BS->bufsize * CHUNK_COUNT, 16);
#else
		BS->SoundBuffer = (U8*) rage_new u8[BS->bufsize * CHUNK_COUNT];
#endif
		if ( BS->SoundBuffer == 0 )
			return( 0 );
	}

  sysMemSet( BS->SoundBuffer, ( bs->bits == 16 ) ? 0 : 0x80, BS->bufsize * CHUNK_COUNT );

  return( 1 );
}


static void XASoundPause( struct BINKSND *UNUSED_PARAM(bs) )
{
	// TODO: pause underlying stream player?
}


static void XASoundResume( struct BINKSND * UNUSED_PARAM(bs) )
{
	// TODO: pause underlying stream player?
}

static void XASoundShutdown( struct BINKSND * bs )
{
	if(BS->ringBuffer)
	{	
		BS->ringBuffer->Release();
		BS->ringBuffer = NULL;
	}
}

/**********************************

    returns a one if a buffer range is ready
    if the mixer system starved, then record a sound dropout in bs->SoundDroppedOut

**********************************/

static S32 RADLINK Ready( struct BINKSND * bs )
{
  S32 i;

  if ( ( BS->paused ) || ( bs->OnOff == 0 ) )
  {
      return( 0 );
  }

  const u32 bytesAvailableToWrite = BS->ringBuffer->GetBufferSize() - BS->ringBuffer->GetBytesAvailableToRead();

  if(bytesAvailableToWrite >= BS->bufsize)
  {
	  i = ( BS->last_frag + 1 ) % CHUNK_COUNT;
	  BS->fill_frag = i;
	  return 1;
  }

  return 0;
}


// this function is only called if ready has returned a non-zero value
static S32 RADLINK Lock( struct BINKSND * bs, U8 * * addr, U32 * len )
{
  if (BS->fill_frag==-1)
  {
      return 0;
  }

  *addr = &BS->SoundBuffer[BS->fill_frag*BS->bufsize];
  *len = BS->bufsize;
  return 1;
}


// called when the buffer has been filled
static S32  RADLINK Unlock (struct BINKSND * bs,U32 filled)
{
  if ( BS->fill_frag == -1 )
  {
    return 0;
  }

  BS->last_frag = BS->fill_frag;
  BS->fill_frag = -1;
  BS->submitted += filled;

  if ( BS->status == BS_PRELOADING )
  {
    if ( BS->submitted >= BS->preloadsize )
    {
      S32 i;

      BS->status = BS_STREAMING;
      for( i = 0 ; i <= BS->last_frag ; i++ )
		  bwAudio::SubmitBuffer( bs, i, BS->bufsize );

      /*if ( BS->SourceVoice )
      {
        // start all the voices only when we have loaded the last one
        if ( bs == &BS->bink->bsnd[BS->bink->playingtracks-1] )
        {
          bs = BS->bink->bsnd;
          for( i = BS->bink->playingtracks ; i ; i-- )
          {
           // BS->SourceVoice->Start( 0, XAUDIO2_COMMIT_NOW );
            ++bs;
          }
          bs = 0; // cause a crash, if we later try to access.
        }
      }*/
    }
  }
  else
  {
	  bwAudio::SubmitBuffer( bs, BS->last_frag, filled );
  }

  return( 1 );
}


static void RADLINK Volume ( struct BINKSND * UNUSED_PARAM(bs), S32 UNUSED_PARAM(volume) )
{
	// set the volume 0=quiet, 32767=max, 65536=double volume (not always supported)
	// We control the volume from a higher level
}


static void RADLINK Pan( struct BINKSND * UNUSED_PARAM(bs), S32 UNUSED_PARAM(pan) )
{
	// set the pan 0=left, 32767=middle, 65536=right
	// We control panning from a higher level
}


// called to turn the sound on or off
static S32  RADLINK SetOnOff( struct BINKSND * bs, S32 status )
{
  if ( ( status == 1 ) && ( bs->OnOff == 0 ) )
  {
      bs->OnOff = 1;
  } 
  else if ( ( status == 0 ) && ( bs->OnOff == 1 ) && !bwAudio::ShouldIgnoreSoundOffCallback() )
  {
  //  XASoundShutdown( bs );
    bs->OnOff = 0;
  }

  return( bs->OnOff );
}


// pauses/resumes the playback
static S32  RADLINK Pause( struct BINKSND *bs, S32 status )  // 1=pause, 0=resume, -1=resume and clear
{
  if ( status )
  {
    XASoundPause( bs );
    BS->paused = 1;
  }
  else
  {
    XASoundResume( bs );
    BS->paused = 0;
  }

  return( BS->paused );
}



static void RADLINK SpeakerVolumes( struct BINKSND * UNUSED_PARAM(bs), F32 * UNUSED_PARAM(volumes), U32 UNUSED_PARAM(total) )
{
	// set the spk vol values - volumes are LRLRLR... for stereo and just VVVV... for mono
	//    currently, total will always be 8
	// We control panning/speaker volumes from a higher level
}

// closes this channel
void RADLINK bwAudio::Close( struct BINKSND * bs )
{
	XASoundShutdown( bs );

	if ( BS->SoundBuffer )
	{
#if !__WIN32PC
		// Bink Allocations should NOT come out of Game Virtual!
		Assert(sm_Deallocator);
		sm_Deallocator(BS->SoundBuffer);
#else
		delete [] BS->SoundBuffer;
#endif
		BS->SoundBuffer = NULL;
	}

	bwAudio::Unregister(BS->bink);
}

S32 RADLINK bwAudio::Open( struct BINKSND * bs, U32 freq, S32 bits, S32 chans, U32 UNUSED_PARAM(flags), HBINK bink )
{
	U32 i;

	CompileTimeAssert( sizeof( bs->snddata ) >= sizeof( SNDCHANNEL ) );

	//
	// Init sound descriptor
	//

	sysMemZeroBytes<sizeof(*bs)>(bs);

	bs->SoundDroppedOut = 0;

	bs->freq  = freq;
	bs->bits  = bits;
	bs->chans = chans;

	BS->mbins = 1;

	// save for Bink
	bink->limit_speakers = BS->mbins;

	i = bs->freq * ( bs->bits >> 3 ) * bs->chans;

	if ( ( bink->FrameRate ) && ( bink->FrameRateDiv ) )
		BS->maxpreload = ( U32 ) ( ( (U64) bink->Frames * (U64) bink->FrameRateDiv * (U64) i ) / (U64) bink->FrameRate );
	else
		BS->maxpreload = i;

	BS->bink = bink;

	//
	// Set initial status
	//

	BS->paused    = 0;
	bs->OnOff     = 1;

	//
	// Set up the function calls
	//

	bs->Ready    = Ready;
	bs->Lock     = Lock;
	bs->Unlock   = Unlock;
	bs->Volume   = Volume;
	bs->Pan      = Pan;
	bs->Pause    = Pause;
	bs->SetOnOff = SetOnOff;
	bs->Close    = Close;
	bs->SpeakerVols  = SpeakerVolumes;

	//
	// Return success
	//

	if ( bwAudio::Init( bs ) == 0 )
		return( 0 );

	//
	// Set preferred amount of data to be passed in 16-bit sample units
	//

	bs->BestSizeIn16 = BS->bufsize;
	if ( bs->bits != 16 )
		bs->BestSizeIn16 *= 2;

	bs->Latency = 0;

	// Set up RAGE Audio ringbuffer
	
	const size_t ringBufferSizeBytes = 2 * BS->bufsize;
	u8 *ringBufferData = rage_aligned_new(16) u8[ringBufferSizeBytes];
	sysMemSet(ringBufferData, 0, ringBufferSizeBytes);
	BS->ringBuffer = rage_new audReferencedRingBuffer();
	BS->ringBuffer->Init(ringBufferData, ringBufferSizeBytes, true);

	bwAudio::Register(BS->bink, BS->ringBuffer, bs->chans, bs->freq);

	return(1);
}

atRangeArray<bwAudio::Info, bwAudio::kMaxBinkAudioStreams> bwAudio::sm_ActiveStreams;

void bwAudio::Register(HBINK bink, audReferencedRingBuffer *ringBuffer, const u32 numChannels, const u32 sampleRate)
{
	for(s32 i = 0; i < sm_ActiveStreams.GetMaxCount(); i++)
	{
		if(sm_ActiveStreams[i].bink == NULL)
		{
			sm_ActiveStreams[i].bink = bink;
			sm_ActiveStreams[i].ringBuffer = ringBuffer;
			sm_ActiveStreams[i].numChannels = numChannels;
			sm_ActiveStreams[i].sampleRate = sampleRate;
			return;
		}
	}
}

void bwAudio::Unregister(HBINK bink)
{
	for(s32 i = 0; i < sm_ActiveStreams.GetMaxCount(); i++)
	{
		if(sm_ActiveStreams[i].bink == bink)
		{
			sm_ActiveStreams[i].bink = NULL;
		}
	}
}

const bwAudio::Info *bwAudio::FindInfo(HBINK bink)
{
	if(bink != NULL)
	{
		for(s32 i = 0; i < sm_ActiveStreams.GetMaxCount(); i++)
		{
			if(sm_ActiveStreams[i].bink == bink)
			{
				return &sm_ActiveStreams[i];
			}
		}
	}
	return NULL;
}

} // namespace rage

#if !RSG_PC
RADEXPFUNC 
#endif
BINKSNDOPEN RADEXPLINK BinkOpenRAGEAudio( UINTa UNUSED_PARAM(param) )
{
	return (rage::bwAudio::Open);
}
