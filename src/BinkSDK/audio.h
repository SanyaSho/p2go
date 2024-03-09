//
// bink/audio.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef BNK_AUDIO_H
#define BNK_AUDIO_H

#include "bink_include.h"

#include "atl/array.h"
#include "atl/functor.h"

#if !RSG_PC
RADEXPFUNC 
#endif
BINKSNDOPEN RADEXPLINK BinkOpenRAGEAudio(UINTa param);

namespace rage
{

	class audReferencedRingBuffer;

class bwAudio
{
public:
	struct Info
	{
		Info()
			: ringBuffer(NULL)
			, bink(NULL)
		{

		}
		audReferencedRingBuffer *ringBuffer;
		HBINK bink;
		u32 numChannels;
		u32 sampleRate;
		
	};
	static void SubmitBuffer(struct BINKSND * bs, S32 index, U32 len);
	static int Init( struct BINKSND * bs );
	static S32 RADLINK Open( struct BINKSND * bs, U32 freq, S32 bits, S32 chans, U32 UNUSED_PARAM(flags), HBINK bink );
	static void RADLINK Close( struct BINKSND * bs );

	static const Info *FindInfo(HBINK bink);
	
	static void SetAllocator(Functor2Ret<void*, u32, u32> fn) { sm_Allocator = fn; }
	static void SetDeallocator(Functor1<void*> fn) { sm_Deallocator = fn; }
	static void SetIgnoreSoundOffCallback(bool ignore) { sm_IgnoreSoundOffCallback = ignore; }
	static bool ShouldIgnoreSoundOffCallback() { return sm_IgnoreSoundOffCallback; }

private:

	static void Register(HBINK bink, audReferencedRingBuffer *ringBuffer, const u32 numChannels, const u32 sampleRate);
	static void Unregister(HBINK bink);

	enum {kMaxBinkAudioStreams = 4};
	static atRangeArray<Info, kMaxBinkAudioStreams> sm_ActiveStreams;

	static Functor1<void*>				sm_Deallocator;
	static Functor2Ret<void*,u32,u32>	sm_Allocator;
	static bool							sm_IgnoreSoundOffCallback;
};
} // namespace rage
#endif // BNK_AUDIO_H
