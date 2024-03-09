//  
// bink/movie.cpp 
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved. 
// 

#include "bink/movie.h"
#define BINK_VERIFY(x) { s32 ret=x; if (!ret) { Errorf("%s", BinkGetError()); } }

#include "system/timer.h"

#include "system/xtl.h"
#define _XBOX_VER 200
#define RAD_NO_LOWERCASE_TYPES

#if RSG_DURANGO
#define _SEKRIT
#elif RSG_ORBIS
#define _SEKRIT2
#endif

#include "bink_include.h"
#include "audio.h"
#include "audiohardware/driverutil.h"
#include "file/stream.h"
#include "file/device.h"		// for g_HadFatalError
#include "grcore/im.h"
#include "grcore/image.h"
#include "grcore/effect.h"
#include "system/cache.h"
#include "system/memops.h"
#include "system/memory.h"
#include "system/nelem.h"
#include "system/stack.h"
#include "system/hangdetect.h"

#include "audiohardware/driver.h"


#if RSG_DURANGO
#include "xaudio2.h"
#include "audiohardware/device_360.h"
#include "audiohardware/device_xboxone.h"
#include "grcore/wrapper_d3d.h"
#include "system/d3d11.h"
#elif RSG_ORBIS
#include <coredump.h>
#elif RSG_XENON
#include "xaudio2.h"
#include "audiohardware/device_360.h"
#include "grcore/texturexenon.h"
#elif RSG_PPU
#include <sysutil/sysutil_sysparam.h>
#include <sys/memory.h>
#include "radppu.h"
#endif

#if RSG_XENON
# ifdef	__USE_BINK_2
#  pragma comment(lib,"X:\\gta5\\src\\dev\\rage\\lib\\bink2\\binkxenon.lib")
# else
#  pragma comment(lib,"binkxenon.lib")
# endif
#elif RSG_PS3
# pragma comment(lib,"binkps3")
# pragma comment(lib,"binkppu_spurs")
#elif RSG_DURANGO
# ifdef	__USE_BINK_2
#	if RSG_BINK_2_7D
#		pragma comment(lib,"Bink2Durango_2_7d.lib")
#	else
#		pragma comment(lib,"Bink2Durango.lib")
#	endif
# else
#	pragma comment(lib,"binkdurango.lib")
# endif
#elif RSG_ORBIS
# ifdef	__USE_BINK_2
#  pragma comment(lib,"bink2orbis.a")
# else
#  pragma comment(lib,"libbinkorbis.a")
# endif
#elif RSG_CPU_X64
# ifdef	__USE_BINK_2
#  pragma comment(lib,"bink2w64.lib")
# else
#  pragma comment(lib,"binkw64.lib")
# endif
#elif RSG_CPU_X86
# ifdef	__USE_BINK_2
#  pragma comment(lib,"bink2w32.lib")
# else
#  pragma comment(lib,"binkw32.lib")
# endif
#endif

#if RSG_PC
#ifndef DBG
#if __DEV
#define DBG 1
#else
#define DBG 0
#endif
#endif
#else
#define DBG 0
#endif

#if RSG_XENON
#include <xgraphics.h>
#endif // RSG_XENON

#if RSG_DURANGO || RSG_XENON
#define XAUDIOSPEAKER_FRONTLEFT 0
#define XAUDIOSPEAKER_FRONTRIGHT 1
#define XAUDIOSPEAKER_FRONTCENTER 2
#define XAUDIOSPEAKER_LOWFREQUENCY 3
#define XAUDIOSPEAKER_BACKLEFT 4
#define XAUDIOSPEAKER_BACKRIGHT 5
#define XAUDIOSPEAKER_COUNT 6
#endif

#include "profile/timebars.h"


// DON'T COMMIT
//OPTIMISATIONS_OFF()

#define GOTO_FLAGS					(0)

#define USE_DEBUG_BINK_GOTO			(0)

RADDEFFUNC S32 RADLINK BinkFileOpenRfs( BINKIO * bio, const char * name, U32 flags );

using namespace rage;

#if RSG_DURANGO
namespace rage
{
	XPARAM(noWaitOnGpu);
}
#endif

#if RSG_XENON || RSG_DURANGO || RSG_ORBIS
const u32		bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_0		= bwMovie::THREAD_1;
const u32		bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_1		= bwMovie::THREAD_1;
#elif RSG_PC 
const u32		bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_0		= bwMovie::THREAD_1;
const u32		bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_1		= bwMovie::THREAD_1;
#elif RSG_PS3
const u32		bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_0		= bwMovie::THREAD_1;
const u32		bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_1		= bwMovie::THREAD_2;
#endif

grcEffect* bwMovie::sm_pEffect											= NULL;
grcEffectVar bwMovie::sm_nYPlaneId										= grcevNONE;
grcEffectVar bwMovie::sm_ncRPlaneId										= grcevNONE;
grcEffectVar bwMovie::sm_ncBPlaneId										= grcevNONE;
grcEffectVar  bwMovie::sm_nAPlaneId										= grcevNONE;
grcEffectVar  bwMovie::sm_nUVScalarId									= grcevNONE;
grcEffectTechnique bwMovie::sm_tOpaque									= grcetNONE;
grcEffectTechnique bwMovie::sm_tAlpha									= grcetNONE;
Functor3Ret<void*,u32,u32,void*> bwMovie::sm_TextureMemoryAllocator		= NULL;
Functor1<void*>				bwMovie::sm_TextureMemoryDeallocator		= NULL;
u32 bwMovie::sm_uNumSpeakers = 2;

bool	(*bwMovie::pControlPlayRateCB)() = NULL;

bool bwMovie::sm_UsingRAGEAudio = false;

#if BINK_ASYNC
bool bwMovie::sm_AsyncInit = false;
rage::sysIpcMutex bwMovie::sm_Mutex = NULL;
sysMessageQueue<bwMovie::sAsyncData, MAX_ASYNC_ACTIONS, true> bwMovie::sm_asyncQueue;
sysIpcThreadId bwMovie::sm_asyncThread;
char bwMovie::sm_pFileName[80];
int bwMovie::sm_currentGotoFrame = -1;
#endif

#if __BANK
bool bwMovie::m_gDumpTEX = false;
#endif	//__BANK

#if __ASSERT
extern atString g_lastBink;
#endif

bwMovie* bwMovie::Create()
{
	bwMovie* newMovie = rage_new bwMovie();
	return newMovie;
}

void bwMovie::Destroy(bwMovie* movie, bool async)
{
#if BINK_ASYNC
	if (async)
	{
		AUTO_HANG_DETECT_CRASH_ZONE;
		checkForRequestFlood();

		movie->m_bLoaded = false;
		sAsyncData newData = {movie, AA_DESTROY, sysTimer::GetSystemMsTime()};
		sm_asyncQueue.Push(newData);
	}
	else
	{
		Assert(!movie->IsRenderInProgress());
		Assert(!movie->IsPlaying());
		while (movie->AreFencesPending())
			sysIpcSleep(1);

		delete movie;
	}
#else
		delete movie;
#endif
}

void bwMovie::Init()
{
	const bool useRageAudio = (USE_BINK_RAGE_AUDIO_PROVIDER);
	if(useRageAudio && audDriver::GetMixer())
	{
		// RAGE Audio custom bink provider
		BinkSetSoundSystem(BinkOpenRAGEAudio, 0);
		sm_UsingRAGEAudio = true;
		Displayf("Attempt to play bink audio via game engine");
	}
	else
	{
		audDriver::SetAllocators(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL), sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL));
		if(!audDriver::InitMixer())
		{
#if RSG_ORBIS
			Errorf("Try reseting your PS4, turn it completely off");
#endif			
			Quitf(ERR_AUD_MIXER_INIT, "Error initializing audio mixer.");
		}
		// Bink built-in audio providers
		sm_UsingRAGEAudio = false;
		Displayf("Attempt to play bink audio using bink engine");

	#if RSG_PC
		// According to Jeff Roberts, if you pass in NULL they'll make the DSOUND object 
		BinkSoundUseDirectSound(NULL);
		sm_uNumSpeakers = 2;	// TODO: Multichannel on PC?

	#elif RSG_DURANGO
		// Essentially Xenon, but separate in-case we need to tailor.
		IXAudio2 * audio2 = ((audMixerDeviceXAudio2*)audDriver::GetMixer())->GetXAudio2();
		if (audio2)
		{
			#if RSG_BINK_2_7D
				BinkSoundUseXAudio2(audio2, nullptr);
			#else
				BinkSoundUseXAudio2(audio2);
			#endif
		}
		sm_uNumSpeakers = 6;
	#elif RSG_ORBIS
		sm_uNumSpeakers = 2;	// TODO: Multichannel for Orbis
		BinkSoundUseSonySound(sm_uNumSpeakers);

	#elif RSG_XENON
		BinkSoundUseXAudio2(((audMixerDeviceXAudio2*)audDriver::GetMixer())->GetXAudio2());
		sm_uNumSpeakers = 6;	// 360 always gets 6 speakers because it handles all its own downmixing
	#elif RSG_PPU
		sm_uNumSpeakers = 2;
		CellAudioOutConfiguration config;
		if (cellAudioOutGetConfiguration(CELL_AUDIO_OUT_PRIMARY,&config,NULL) == 0 /*CELL_OK*/) 
		{
			sm_uNumSpeakers = config.channel;
			printf("[BINK] Using %d speakers.\n",sm_uNumSpeakers);
		}
		else
		{
			printf("[BINK] cellAudioOutGetConfiguration failed, defaulting to two speakers.\n");
		}
		BinkSoundUseLibAudio(sm_uNumSpeakers);
	#endif
	}

#if BINK_ASYNC
	if ( !sm_AsyncInit )
	{
		sm_Mutex = rage::sysIpcCreateMutex();
		sm_asyncThread = sysIpcCreateThread(&bwMovie::AsyncBink, NULL, 64 << 10, PRIO_BELOW_NORMAL, "AsyncBink");
		sm_AsyncInit = true;
	}
#endif
}

void bwMovie::Shutdown()
{
#if BINK_ASYNC
	AUTO_HANG_DETECT_CRASH_ZONE;
	sAsyncData newData = {NULL, AA_GOTO, sysTimer::GetSystemMsTime()};
	sm_asyncQueue.Push(newData);
	sysIpcWaitThreadExit(sm_asyncThread);
	sysIpcDeleteMutex(sm_Mutex);
	sm_AsyncInit = false;
#endif

#if RSG_PS3
	// Undocumented function which shuts down IO/Audio threads, freeing up the 128k of stack space they used.
	BinkFreeGlobals();
#endif
}

void bwMovie::SetShouldControlPlaybackRateCB(bool (*fn)())
{
	pControlPlayRateCB = fn;
}

bool bwMovie::GetShouldControlPlaybackRate()
{
	if( pControlPlayRateCB )
	{
		return pControlPlayRateCB();
	}
	return false;
}

void bwMovie::InitShaders() 
{
	if (sm_pEffect != NULL) 
	{
		Warningf("[bwMovie::InitShaders] shaders already loaded");
		ShutdownShaders();
	}

	sysMemStartTemp();
	sm_pEffect = grcEffect::Create("rage_bink");
	sysMemEndTemp();

	Assertf(sm_pEffect != NULL, "[bwMovie::InitShaders] failed to load shaders");

	sm_nYPlaneId = sm_pEffect->LookupVar("YPlane");
	sm_ncRPlaneId = sm_pEffect->LookupVar("cRPlane");
	sm_ncBPlaneId = sm_pEffect->LookupVar("cBPlane");
	sm_nAPlaneId = sm_pEffect->LookupVar("APlane");
	sm_nUVScalarId = sm_pEffect->LookupVar("UVScalar");
	sm_tOpaque = sm_pEffect->LookupTechnique("bink_opaque");
	sm_tAlpha = sm_pEffect->LookupTechnique("bink_alpha");
}

void bwMovie::ShutdownShaders()
{
	sysMemStartTemp();
	delete sm_pEffect;
	sm_pEffect = NULL;
	sysMemEndTemp();
}

bwMovie::bwMovie()
{
	Assertf(sm_pEffect != NULL, "[bwMovie::bwMovie] Bink shaders haven't been preloaded - forgot to call bwMovie::InitShaders?");

	m_bReadyToRender = false;
	m_bDecodeInProgress = false;
	m_bRenderInProgress = false;
	m_bLoaded = false;
	m_bIsLoading = false;
	m_bPlaying = false;
	m_bStopRequested = false;
	m_bLooping = true;
	m_ShouldSkip = false;
	m_PlayedFirstFrame = false;
	m_bControlledByAnother = false;
	m_hBink = 0;
	m_vRect[0].Set(0.0f, 0.0f, 0.0f);
	m_vRect[1].Set(1.0f, 0.0f, 0.0f);
	m_vRect[2].Set(0.0f, 1.0f, 0.0f);
	m_vRect[3].Set(1.0f, 1.0f, 0.0f);
	m_TintColor.Set(1.0f, 1.0f, 1.0f, 1.0f);

	m_fPlaybackRate = 1.0f;
	m_uFrameRate = 2997;
	m_uFrameRateDiv = 100;
	m_uStartTime = 0;
	m_uCurrentTime = 0;
	m_uLastTime = 0;
	m_uPreviousFrameNum = 0;
	m_bControlPlaybackRate = false;

	m_BlackOutTime = 0;

	m_shouldGoToCounter = 0;
	m_targetGoToFrame = 1;

	m_bHasAudio = false;

	m_pFrameBuffers = rage_new BINKFRAMEBUFFERS;

#if !RSG_PC
    m_bFenceIndex = false;
	sysMemSet(m_textureFence, 0, sizeof(m_textureFence));
#endif
}

bwMovie::~bwMovie()
{
	if( m_hBink ) 
	{
		BinkClose(m_hBink);
	}

	Assertf(sm_TextureMemoryDeallocator, "bwMovie::~bwMovie: cannot deallocate texture memory");

	for( int i = 0; i < m_TexturePointers.GetCount(); i++ )
	{
		grcTexture* pTexture = m_TexturePointers[i];
		void* pTexMem = m_RawTextureMemPointers[i];

		if (sm_TextureMemoryDeallocator)
		{
			sm_TextureMemoryDeallocator(pTexMem);
		}

		if( pTexture )
		{
#if ENABLE_CHUNKY_ALLOCATOR
			ChunkyWrapper a(*sysMemManager::GetInstance().GetChunkyAllocator(), false, "Movie");
#endif // ENABLE_CHUNKY_ALLOCATOR
			pTexture->Release();
		}
	}

	delete m_pFrameBuffers;

#if !RSG_PC
	for (unsigned i=0; i<NELEM(m_textureFence); ++i)
	{
		grcFenceHandle f = m_textureFence[i];
		if (f)
		{
			GRCDEVICE.CpuFreeFence(f);
		}
	}
#endif
}


// allow game code to set the memory functions to be used for all bink allocations, so we can use
// our own special allocators instead of just pulling memory from where ever it feels like.
#if RSG_BINK_2_7D
	void bwMovie::SetMemoryFuncs( void* (a(u64)),  void (f(void*)))
#else
	void bwMovie::SetMemoryFuncs( void* (a(u32)),  void (f(void*)))
#endif
{
	BinkSetMemory(a, f);
}

u32 bwMovie::ImposeFrameRate()
{
	if( m_hBink )
	{
		BinkSetFrameRate( m_hBink->FrameRate, m_hBink->FrameRateDiv );
		return (m_hBink->FrameRate / m_hBink->FrameRateDiv);
	}
	return 0; 
}

void bwMovie::SetVolume(const f32 dbVolume)
{
	if (m_hBink)
	{
		sysCriticalSection lock(m_audioLock);

		// bink uses a linear integer scale; convenient
		const s32 vol = GetBinkVolume(dbVolume);

		const u32 numTracks = m_hBink->NumTracks;
		for (u32 i = 0; i < numTracks; ++i)
			BinkSetVolume(m_hBink, i, vol);

		// This is set in SetMovie() so not sure why it needs set again
		//m_ShouldSkip = true;
		//if( numTracks > 0 && vol > 0)
		//{
		//	m_ShouldSkip = true;
		//}
		//else
		//{
		//	m_ShouldSkip = false;
		//}
	}
}

s32 bwMovie::GetBinkVolume(const f32 dbVolume)
{
	return Min(65536, static_cast<s32>(audDriverUtil::ComputeLinearVolumeFromDb(dbVolume) * 32768.f));
}


u32	 bwMovie::GetWidth() const
{
	if (m_TexturePointers[0] != NULL)
		return m_TexturePointers[0]->GetWidth();

	return 0;
}

u32	 bwMovie::GetHeight() const
{
	if (m_TexturePointers[0] != NULL)
		return m_TexturePointers[0]->GetHeight();

	return 0;
}

s32 bwMovie::GetFramesRemaining() const
{
	s32 framesRemaining = 0;

	if (m_hBink)
	{
		framesRemaining = m_hBink->Frames - m_hBink->FrameNum;
	}

	return framesRemaining;
}

void bwMovie::UpdateMovieTime(u32 uCurrentTime)
{
	m_uLastTime = m_uCurrentTime;
	m_uCurrentTime = uCurrentTime;
	UpdateWithinBlackout();
}

void bwMovie::UpdateWithinBlackout()
{
	u32	dTime = m_uCurrentTime - m_uLastTime;
	if( m_BlackOutTime >= dTime )
	{
		m_BlackOutTime -= dTime;
	}
	else
	{
		m_BlackOutTime = 0;
	}
}

bool bwMovie::IsWithinBlackout() const
{
	return false;	// (m_BlackOutTime > 0) || !m_bReadyToRender;
}

//////////////////////////////////////////////////////////////////////////
// @RSNE @MTD Prepare textures to receive decoded frames
void bwMovie::LockBinkTextures(u32 nTextureBase)
{
	if (!Verifyf(m_TexturePointers.GetCount() > 0, "Trying to lock textures but no pointers in the array!"))
		return;

#if RSG_XENON	
	if (m_TexturePointers[nTextureBase] && m_TexturePointers[nTextureBase]->GetTexturePtr())
	{
		m_TexturePointers[nTextureBase]->GetTexturePtr()->BlockUntilNotBusy();
	}

	if (m_TexturePointers[nTextureBase + 1] && m_TexturePointers[nTextureBase + 1]->GetTexturePtr())
	{
		m_TexturePointers[nTextureBase + 1]->GetTexturePtr()->BlockUntilNotBusy();
	}

	if (m_TexturePointers[nTextureBase + 2] && m_TexturePointers[nTextureBase + 2]->GetTexturePtr())
	{
		m_TexturePointers[nTextureBase + 2]->GetTexturePtr()->BlockUntilNotBusy();
	}

	if (m_TexturePointers[nTextureBase + 3] && m_TexturePointers[nTextureBase + 3]->GetTexturePtr())
	{
		m_TexturePointers[nTextureBase + 3]->GetTexturePtr()->BlockUntilNotBusy();
	}
#elif RSG_PS3 || RSG_DURANGO || RSG_ORBIS || RSG_PC
	for( int i = 0; i < 4; i++ )
	{
		grcTexture* pTexture = m_TexturePointers[nTextureBase + i];
		if( pTexture )
		{	
			ASSERT_ONLY(bool lock = )pTexture->LockRect(0, 0, m_TextureLocks[i], grcsDiscard);
			AssertMsg(lock, "Cannot lock bink texture");
		}
	}
#endif // platforms
}

//////////////////////////////////////////////////////////////////////////
// @RSNE @MTD Unlock textures so they can be rendered
void bwMovie::UnlockBinkTextures(WIN32PC_ONLY(u32 nTextureBase) DURANGO_ONLY(u32 nTextureBase) ORBIS_ONLY(u32 nTextureBase) PS3_ONLY(u32 nTextureBase))
{
	if (!Verifyf(m_TexturePointers.GetCount() > 0, "Trying to unlock textures but no pointers in the array!"))
		return;

#if RSG_XENON 
	return;
#elif  RSG_PS3 || RSG_DURANGO || RSG_ORBIS || RSG_PC
	// Unlock the textures - This should flush them to main memory as well

	for( int i = 0; i < 4; i++ )
	{
		grcTexture* pTexture = m_TexturePointers[nTextureBase + i];
		if( pTexture )
		{
#if RSG_DURANGO || RSG_ORBIS
			// Flush the texture to main ram
			int nSize = m_TextureLocks[i].Pitch * m_TextureLocks[i].Height;
			WritebackDC(m_TextureLocks[i].Base, nSize);
#endif // RSG_DURANGO || RSG_ORBIS

#if RSG_DURANGO
			// Scarlett memory flush: BS#6405996 - [Scarlett] GTAV Back Compat - Bink issue;
			D3DFlushCpuCache(m_TextureLocks[i].Base, nSize);
#endif //RSG_DURANGO...

#if __BANK
			// DEBUG
			if(m_gDumpTEX)
			{
				// Convert to RGBA
				int nBufferSize = m_TextureLocks[i].Pitch * m_TextureLocks[i].Height;
				u32	*pRGBA = rage_new u32[nBufferSize];
				u32	*pRGBAWork = pRGBA;
				u8 *pSource = (u8*)m_TextureLocks[i].Base;

				for(int j=0;j<nBufferSize;j++)
				{
					u8	val = *pSource++;
					*pRGBAWork++ = val | val<<8 | val<<16 | (255<<24);
				}

				char	filename[256];
				sprintf(filename,"X:\\Test_%i", i);

				grcImage::SaveStreamToJPEG(filename,pRGBA, m_TextureLocks[i].Width, m_TextureLocks[i].Height, m_TextureLocks[i].Pitch * 4,75);

				delete []pRGBA;
			}
			// DEBUG
#endif	//__BANK

			// Unlock it
			pTexture->UnlockRect(m_TextureLocks[i]);
		}
	}

#if __BANK
	m_gDumpTEX = false;
#endif	//__BANK

#endif // platforms
}

float bwMovie::GetMovieDurationQuick(bwMovieParams& params)
{
	const char* pFileName		= params.pFileName;

#if __ASSERT
	g_lastBink = pFileName;
#endif

	u32	extraFlags = 0;

	// Open the bink handle
	extraFlags |= /*BINKNOTHREADEDIO |*/ BINKIOPROCESSOR | BINKNOFILLIOBUF;
	BinkSetIO(BinkFileOpenRfs);

	BINK *pBink = BinkOpen(pFileName, BINKNOFRAMEBUFFERS | extraFlags);
	if (!pBink)
	{
		return 0.0f;
	}

//	float rate = (static_cast<float>(params.frameRate) * params.playbackRate) / static_cast<float>(params.frameRateDiv);
	float rate = (static_cast<float>(pBink->FrameRate) * params.playbackRate) / static_cast<float>(pBink->FrameRateDiv);

	float duration = static_cast<float>(pBink->Frames) / rate;

	BinkClose(pBink);

	return duration;
}

bool bwMovie::SetMovieSync(bwMovieParams& params)
{
	const char* pFileName		= (params.pFileName != NULL ? params.pFileName : (const char*)(params.pStream->GetLocalHandle()) );
	u32			extraFlags		= params.extraFlags;
	float		playbackRate	= params.playbackRate;
	u32			frameRate		= params.frameRate;
	u32			frameRateDiv	= params.frameRateDiv;
	void*		owner			= params.owner;

	if( m_hBink )
		BinkClose(m_hBink);

	if (!sm_TextureMemoryAllocator) 
	{
		Displayf("bwMovie::SetMovie: Texture memory allocator hasn't been initialised");
		return false;
	}

#if __ASSERT
	g_lastBink = pFileName;
#endif

	U32 TrackIDsToPlay[ 4 ] = { 0, 1, 2, 3 };
	BinkSetSoundTrack( 4, TrackIDsToPlay );

#if RSG_PS3
	sys_memory_info_t mem_info;
	sys_memory_get_user_memory_size( &mem_info );
	if (mem_info.available_user_memory < 128 * 1024) {
		m_hBink = 0;
		printf("[BINK] Not enough OS memory to start callback theads.\n");
		return false;
	}
#endif   

	// FA: if adding BINKNOTHREADEDIO fixes any bink threading bugs we need to solve them
	// some other way. Running with synchronous IO is killing performance!
	extraFlags |= /*BINKNOTHREADEDIO |*/ BINKIOPROCESSOR | BINKNOFILLIOBUF;

	if (params.preloadedData)
	{
		extraFlags |= BINKFROMMEMORY;
	}

	BinkSetIO(BinkFileOpenRfs);

	// Set requested playback rate before opening the file
	if( playbackRate != 1.0f && playbackRate > 0.0f )
	{
		BinkSetFrameRate( rage::round(frameRate * playbackRate), frameRateDiv );
		extraFlags |= BINKFRAMERATE;
		m_fPlaybackRate = playbackRate;
	}
	else
	{
		m_fPlaybackRate = 1.0f;
	}

	// Open the bink handle
	m_hBink = BinkOpen((params.preloadedData) ? (const char *) params.preloadedData : pFileName, BINKNOFRAMEBUFFERS | BINKSNDTRACK | extraFlags);
	
	if( m_hBink )
	{
		// Set actual framerate from bink header
		m_uFrameRate = m_hBink->FrameRate;
		m_uFrameRateDiv = m_hBink->FrameRateDiv;

		if( m_hBink->NumTracks > 0 )
		{
			m_ShouldSkip = true;
			m_bHasAudio = true;
		}
		else
		{
			m_ShouldSkip = false;
		}

		// Get the frame buffer info
		BinkGetFrameBuffersInfo(m_hBink, m_pFrameBuffers);

		{


			// Free any existing textures
			for( int i = 0; i < m_TexturePointers.GetCount(); i++ )
			{
				if( m_TexturePointers[i] )
				{
					m_TexturePointers[i]->Release();
				}
			}

			int nTextures = m_pFrameBuffers->TotalFrames * 4;
			m_TexturePointers.Resize(nTextures);
			m_RawTextureMemPointers.Resize(nTextures);
			m_TextureLocks.Resize(nTextures);
			for( int i = 0; i < nTextures; i++ )
			{
				m_TexturePointers[i] = 0;
				m_RawTextureMemPointers[i] = 0;
			}

			grcTextureFactory::TextureCreateParams params(grcTextureFactory::TextureCreateParams::VIDEO, grcTextureFactory::TextureCreateParams::LINEAR, grcsRead | grcsWrite | grcsDiscard);

#if RSG_XENON 
			u32 format = D3DFMT_LIN_L8;
#elif RSG_PS3
			u32 format = grcImage::L8;
#else
			u32 format = grctfL8;
#endif

			for( int i = 0 ; i < m_pFrameBuffers->TotalFrames; i++ )
			{
				if( m_pFrameBuffers->Frames[i].YPlane.Allocate )
				{
					u32 allocSize = ComputeAllocSizeForL8Texture(m_pFrameBuffers->YABufferWidth, m_pFrameBuffers->YABufferHeight);
					void* pMem = sm_TextureMemoryAllocator(allocSize, 4096, owner);

					if (pMem == NULL)
					{
						Displayf("bwMovie::SetMovie: Failed to allocate memory for L buffer %d", i);
						return false;
					}

					sysMemSet(pMem, 0, allocSize);

					BANK_ONLY(grcTexture::SetCustomLoadName("Bink YA");)
					grcTexture* pTexture = nullptr;
					{
#if ENABLE_CHUNKY_ALLOCATOR
						ChunkyWrapper a(*sysMemManager::GetInstance().GetChunkyAllocator(), true, "Movie", pFileName);
#endif // ENABLE_CHUNKY_ALLOCATOR
						pTexture = grcTextureFactory::GetInstance().Create(m_pFrameBuffers->YABufferWidth, m_pFrameBuffers->YABufferHeight, format, pMem, 1, &params);
					}
					BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
					if (pTexture == NULL)
					{
						Displayf("bwMovie::SetMovie: Texture creation failed");
						return false;
					}

					m_RawTextureMemPointers[i*4] = pMem;
					m_TexturePointers[i * 4] = pTexture;

#if RSG_PC && BINK_ASYNC
					GRCDEVICE.LockContext();
#endif // RSG_PC && BINK_ASYNC

					int nTexIndex = (i * 4) + 0;
					if (pTexture->LockRect(0, 0, m_TextureLocks[nTexIndex]))
					{
						m_pFrameBuffers->Frames[i].YPlane.Buffer = m_TextureLocks[nTexIndex].Base;
						m_pFrameBuffers->Frames[i].YPlane.BufferPitch = m_TextureLocks[nTexIndex].Pitch;
				#if RSG_PC && BINK_ASYNC
						FillTexture(m_pFrameBuffers->YABufferWidth, m_pFrameBuffers->YABufferHeight, m_TextureLocks[nTexIndex].Base, m_TextureLocks[nTexIndex].Pitch, 0);
				#endif // RSG_PC && BINK_ASYNC
						pTexture->UnlockRect(m_TextureLocks[nTexIndex]);
					}

#if RSG_PC && BINK_ASYNC
					GRCDEVICE.UnlockContext();
#endif // RSG_PC && BINK_ASYNC
				}

				if( m_pFrameBuffers->Frames[i].cRPlane.Allocate )
				{
	
					int nTexIndex = (i * 4) + 1;

					u32 allocSize = ComputeAllocSizeForL8Texture(m_pFrameBuffers->cRcBBufferWidth, m_pFrameBuffers->cRcBBufferHeight);
					void* pMem = sm_TextureMemoryAllocator(allocSize, 4096, owner);

					if (pMem == NULL)
					{
						Displayf("bwMovie::SetMovie: Failed to allocate memory for CR buffer %d", i);
						return false;
					}

					sysMemSet(pMem, 0x80, allocSize);

					BANK_ONLY(grcTexture::SetCustomLoadName("Bink cRPlane");)
					grcTexture* pTexture = nullptr;
					{
#if ENABLE_CHUNKY_ALLOCATOR
						ChunkyWrapper a(*sysMemManager::GetInstance().GetChunkyAllocator(), true, "Movie", pFileName);
#endif // ENABLE_CHUNKY_ALLOCATOR
						pTexture = grcTextureFactory::GetInstance().Create(m_pFrameBuffers->cRcBBufferWidth, m_pFrameBuffers->cRcBBufferHeight, format, pMem, 1, &params);
					}
					BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
					if (pTexture == NULL)
					{
						Displayf("bwMovie::SetMovie: Texture creation failed");
						return false;
					}


					m_TexturePointers[nTexIndex] = pTexture;
					m_RawTextureMemPointers[nTexIndex] = pMem;

#if RSG_PC && BINK_ASYNC
					GRCDEVICE.LockContext();
#endif // RSG_PC && BINK_ASYNC
					if (pTexture->LockRect(0, 0, m_TextureLocks[nTexIndex]))
					{
						m_pFrameBuffers->Frames[i].cRPlane.Buffer = m_TextureLocks[nTexIndex].Base;
						m_pFrameBuffers->Frames[i].cRPlane.BufferPitch = m_TextureLocks[nTexIndex].Pitch;
				#if RSG_PC && BINK_ASYNC
						FillTexture(m_pFrameBuffers->cRcBBufferWidth, m_pFrameBuffers->cRcBBufferHeight, m_TextureLocks[nTexIndex].Base, m_TextureLocks[nTexIndex].Pitch, 0x80);
				#endif // RSG_PC && BINK_ASYNC
						pTexture->UnlockRect(m_TextureLocks[nTexIndex]);
					}
#if RSG_PC && BINK_ASYNC
					GRCDEVICE.UnlockContext();
#endif // RSG_PC && BINK_ASYNC
				}

				if( m_pFrameBuffers->Frames[i].cBPlane.Allocate )
				{
					int nTexIndex = (i * 4) + 2;


					u32 allocSize = ComputeAllocSizeForL8Texture(m_pFrameBuffers->cRcBBufferWidth, m_pFrameBuffers->cRcBBufferHeight);
					void* pMem = sm_TextureMemoryAllocator(allocSize, 4096, owner);

					if (pMem == NULL)
					{
						Displayf("bwMovie::SetMovie: Failed to allocate memory for CB buffer %d", i);
						return false;
					}

					sysMemSet(pMem, 0x80, allocSize);

					BANK_ONLY(grcTexture::SetCustomLoadName("Bink cBPlane");)
					grcTexture* pTexture = nullptr;
					{
#if ENABLE_CHUNKY_ALLOCATOR
						ChunkyWrapper a(*sysMemManager::GetInstance().GetChunkyAllocator(), true, "Movie", pFileName);
#endif // ENABLE_CHUNKY_ALLOCATOR
						pTexture = grcTextureFactory::GetInstance().Create(m_pFrameBuffers->cRcBBufferWidth, m_pFrameBuffers->cRcBBufferHeight, format, pMem, 1, &params);
					}
					BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
					if (pTexture == NULL)
					{
						Displayf("bwMovie::SetMovie: Texture creation failed");
						return false;
					}

					m_TexturePointers[nTexIndex] = pTexture;
					m_RawTextureMemPointers[nTexIndex] = pMem;

#if RSG_PC && BINK_ASYNC
					GRCDEVICE.LockContext();
#endif // RSG_PC && BINK_ASYNC
					if (pTexture->LockRect(0, 0, m_TextureLocks[nTexIndex]))
					{
						m_pFrameBuffers->Frames[i].cBPlane.Buffer = m_TextureLocks[nTexIndex].Base;
						m_pFrameBuffers->Frames[i].cBPlane.BufferPitch = m_TextureLocks[nTexIndex].Pitch;
				#if RSG_PC && BINK_ASYNC
						FillTexture(m_pFrameBuffers->cRcBBufferWidth, m_pFrameBuffers->cRcBBufferHeight, m_TextureLocks[nTexIndex].Base, m_TextureLocks[nTexIndex].Pitch, 0x80);
				#endif // RSG_PC && BINK_ASYNC
						pTexture->UnlockRect(m_TextureLocks[nTexIndex]);
					}
#if RSG_PC && BINK_ASYNC
					GRCDEVICE.UnlockContext();
#endif // RSG_PC && BINK_ASYNC
				}

				if( m_pFrameBuffers->Frames[i].APlane.Allocate )
				{
					int nTexIndex = (i * 4) + 3;

					u32 allocSize = ComputeAllocSizeForL8Texture(m_pFrameBuffers->YABufferWidth, m_pFrameBuffers->YABufferHeight);
					void* pMem = sm_TextureMemoryAllocator(allocSize, 4096, owner);

					if (pMem == NULL)
					{
						Displayf("bwMovie::SetMovie: Failed to allocate memory for A buffer %d", i);
						return false;
					}

					sysMemSet(pMem, 0, allocSize);

					BANK_ONLY(grcTexture::SetCustomLoadName("Bink APlane");)
					grcTexture* pTexture = nullptr;
					{
#if ENABLE_CHUNKY_ALLOCATOR
						ChunkyWrapper a(*sysMemManager::GetInstance().GetChunkyAllocator(), true, "Movie", pFileName);
#endif // ENABLE_CHUNKY_ALLOCATOR
						pTexture = grcTextureFactory::GetInstance().Create(m_pFrameBuffers->YABufferWidth, m_pFrameBuffers->YABufferHeight, format, pMem, 1, &params);
					}
					BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
					if (pTexture == NULL)
					{
						Displayf("bwMovie::SetMovie: Texture creation failed");
						return false;
					}

					m_TexturePointers[nTexIndex] = pTexture;
					m_RawTextureMemPointers[nTexIndex] = pMem;

#if RSG_PC && BINK_ASYNC
					GRCDEVICE.LockContext();
#endif // RSG_PC && BINK_ASYNC
					if (pTexture->LockRect(0, 0, m_TextureLocks[nTexIndex]))
					{
						m_pFrameBuffers->Frames[i].APlane.Buffer = m_TextureLocks[nTexIndex].Base;
						m_pFrameBuffers->Frames[i].APlane.BufferPitch = m_TextureLocks[nTexIndex].Pitch;
				#if RSG_PC && BINK_ASYNC
						FillTexture(m_pFrameBuffers->YABufferWidth, m_pFrameBuffers->YABufferHeight, m_TextureLocks[nTexIndex].Base, m_TextureLocks[nTexIndex].Pitch, 0);
				#endif // RSG_PC && BINK_ASYNC
						pTexture->UnlockRect(m_TextureLocks[nTexIndex]);
					}
#if RSG_PC && BINK_ASYNC
					GRCDEVICE.UnlockContext();
#endif // RSG_PC && BINK_ASYNC
				}
			}
		}

		// Register the frame buffer textures
		BinkRegisterFrameBuffers(m_hBink, m_pFrameBuffers);

		Assert(sm_uNumSpeakers >= 2);
		if (sm_UsingRAGEAudio)
		{
            sysCriticalSection lock(m_audioLock);
			BinkSetVolume(m_hBink, 0, 32767);
		}
		else
		{
            sysCriticalSection lock(m_audioLock);
#if RSG_DURANGO
			// Setup surround audio
			U32 bins[ 2 ];
			bins[ 0 ] = XAUDIOSPEAKER_FRONTLEFT;
			bins[ 1 ] = XAUDIOSPEAKER_FRONTRIGHT;
			BinkSetMixBins( m_hBink, 0, bins, 2 );
			bins[ 0 ] = XAUDIOSPEAKER_FRONTCENTER;
			BinkSetMixBins( m_hBink, 1, bins, 1 );
			bins[ 0 ] = XAUDIOSPEAKER_LOWFREQUENCY;
			BinkSetMixBins( m_hBink, 2, bins, 1  );
			bins[ 0 ] = XAUDIOSPEAKER_BACKLEFT;
			bins[ 1 ] = XAUDIOSPEAKER_BACKRIGHT;
			BinkSetMixBins( m_hBink, 3, bins, 2 );

#elif RSG_ORBIS
		// TODO: ORBIS

#elif RSG_XENON
			// Setup surround audio
			U32 bins[ 2 ];
			bins[ 0 ] = XAUDIOSPEAKER_FRONTLEFT;
			bins[ 1 ] = XAUDIOSPEAKER_FRONTRIGHT;
			BinkSetMixBins( m_hBink, 0, bins, 2 );
			bins[ 0 ] = XAUDIOSPEAKER_FRONTCENTER;
			BinkSetMixBins( m_hBink, 1, bins, 1 );
			bins[ 0 ] = XAUDIOSPEAKER_LOWFREQUENCY;
			BinkSetMixBins( m_hBink, 2, bins, 1  );
			bins[ 0 ] = XAUDIOSPEAKER_BACKLEFT;
			bins[ 1 ] = XAUDIOSPEAKER_BACKRIGHT;
			BinkSetMixBins( m_hBink, 3, bins, 2 );
#elif RSG_PPU
			S32 vols[ 8 ];

			memset( vols, 0, sizeof( vols ) );
			switch(sm_uNumSpeakers)
			{
			case 8:
				vols[ 0 ] = GetBinkVolume(0.0f);
				vols[ 1 ] = GetBinkVolume(0.0f);
				BinkSetMixBinVolumes( m_hBink, 0, 0, vols, 8 );
				memset( vols, 0, sizeof( vols ) );
				vols[ 2 ] = GetBinkVolume(0.0f);
				BinkSetMixBinVolumes( m_hBink, 1, 0, vols, 8 );
				memset( vols, 0, sizeof( vols ) );
				vols[ 3 ] = GetBinkVolume(0.0f);
				BinkSetMixBinVolumes( m_hBink, 2, 0, vols, 8 );
				memset( vols, 0, sizeof( vols ) );
				vols[ 4 ] = GetBinkVolume(0.0f);
				vols[ 5 ] = GetBinkVolume(0.0f);
				BinkSetMixBinVolumes( m_hBink, 3, 0, vols, 8 );
				memset( vols, 0, sizeof( vols ) );
				vols[ 6 ] = GetBinkVolume(0.0f);
				vols[ 7 ] = GetBinkVolume(0.0f);
				BinkSetMixBinVolumes( m_hBink, 4, 0, vols, 8 );
				break;
			case 6:
				vols[ 0 ] = GetBinkVolume(0.0f);
				vols[ 1 ] = GetBinkVolume(0.0f);
				BinkSetMixBinVolumes( m_hBink, 0, 0, vols, 8 );
				memset( vols, 0, sizeof( vols ) );
				vols[ 2 ] = GetBinkVolume(0.0f);
				BinkSetMixBinVolumes( m_hBink, 1, 0, vols, 8 );
				memset( vols, 0, sizeof( vols ) );
				vols[ 3 ] = GetBinkVolume(0.0f);
				BinkSetMixBinVolumes( m_hBink, 2, 0, vols, 8 );
				memset( vols, 0, sizeof( vols ) );
				vols[ 4 ] = GetBinkVolume(0.0f);
				vols[ 5 ] = GetBinkVolume(0.0f);
				BinkSetMixBinVolumes( m_hBink, 3, 0, vols, 8 );
				memset( vols, 0, sizeof( vols ) );

				// CMS - MC4 - If there are side channels add the signal 3 dB down to the front and rear speakers
				vols[ 0 ] = GetBinkVolume(-3.0f);
				vols[ 1 ] = GetBinkVolume(-3.0f);
				vols[ 4 ] = GetBinkVolume(-3.0f);
				vols[ 5 ] = GetBinkVolume(-3.0f);
				BinkSetMixBinVolumes( m_hBink, 4, 0, vols, 8 );
				break;
			case 2:
			default:
				vols[ 0 ] = GetBinkVolume(0.0f);
				vols[ 1 ] = GetBinkVolume(0.0f);
				BinkSetMixBinVolumes( m_hBink, 0, 0, vols, 8 );

				// CMS - MC4 - Lets just send everything else at -3dB to the 2 speakers we have, including center, LFE, rears, and sides
				vols[ 0 ] = GetBinkVolume(-3.0f);
				vols[ 1 ] = GetBinkVolume(-3.0f);
				BinkSetMixBinVolumes( m_hBink, 1, 0, vols, 8 );
				BinkSetMixBinVolumes( m_hBink, 2, 0, vols, 8 );
				BinkSetMixBinVolumes( m_hBink, 3, 0, vols, 8 );
				BinkSetMixBinVolumes( m_hBink, 4, 0, vols, 8 );
				break;
			}
#endif
		}
		m_PlayedFirstFrame = false;
	}
	else
	{
		Errorf("Unable to open movie: %s", BinkGetError());
		return false;
	}

#if !BINK_ASYNC
	m_bLoaded = true;
#endif

	return true;
}


#if RSG_PC && BINK_ASYNC

void bwMovie::FillTexture(u32 width, u32 height, void *pDest, u32 stride, u8 value)
{
	u8 *pLine = (u8 *)pDest;

	for(u32 y=0; y<height; y++)
	{
		sysMemSet(pLine, value, width);
		pLine += stride;
	}
}

#endif // RSG_PC && BINK_ASYNC

bool bwMovie::SetMovie(bwMovieParams& params)
{
#if BINK_ASYNC
	AUTO_HANG_DETECT_CRASH_ZONE;
	checkForRequestFlood();

	m_bReadyToRender = false;
	m_bDecodeInProgress = false;
	sAsyncData newData = {this, AA_OPEN, sysTimer::GetSystemMsTime(), params};
	m_bIsLoading = true;
	sm_asyncQueue.Push(newData);
	return true;
#else
	m_bLoaded = SetMovieSync(params);
	m_bReadyToRender = m_bLoaded;
	return (m_bLoaded);
#endif
}

void bwMovie::SetRect(const Vector3& vTopLeft, const Vector3& vTopRight, const Vector3& vBottomLeft, const Vector3& vBottomRight)
{
	m_vRect[0] = vTopLeft;
	m_vRect[1] = vTopRight;
	m_vRect[2] = vBottomLeft;
	m_vRect[3] = vBottomRight;
}

u32 bwMovie::NumberOfFramesToIncrement()
{
	u32 numFrames = 0;

	if (m_bControlPlaybackRate)
	{
		Assertf(GetShouldControlPlaybackRate(), "bwMovie::NumberOfFramesToIncrement() - Playback rate app controlled but not in Replay?");

		float fFrameTime = 1000.0f / (m_fPlaybackRate * m_uFrameRate / m_uFrameRateDiv);
		u32 uTimeElapsed = m_uCurrentTime - m_uStartTime;
		u32 uCurrentFrameNum = static_cast<u32>(uTimeElapsed / fFrameTime);

		//Displayf("======================================================");
		//Displayf("frameTime %f, m_uCurrentTime %u m_uStartTime %u uTimeElapsed %u, uCurrentFrameNum %u", fFrameTime, m_uCurrentTime, m_uStartTime, uTimeElapsed, uCurrentFrameNum);

		Assert(uCurrentFrameNum >= m_uPreviousFrameNum);

		if (uCurrentFrameNum > m_uPreviousFrameNum)
		{
			numFrames = uCurrentFrameNum - m_uPreviousFrameNum;
			m_uPreviousFrameNum = uCurrentFrameNum;
			//Displayf("numFrames %u, m_uPreviousFrameNum %u", numFrames, m_uPreviousFrameNum);
		}
	}
	else if (!BinkWait( m_hBink ))
	{
		numFrames = 1;
	}
	//Displayf("Frames skipped %u", numFrames);

	return numFrames;
}


#if BINK_ASYNC
//////////////////////////////////////////////////////////////////////////
// Handle dropping frames to catch up
void bwMovie::DoSkipFrames(u32 numFramesToIncrement)
{
	if (m_bControlPlaybackRate)
	{
		for (u32 i = 0; i < numFramesToIncrement - 1; ++i)
		{
			BinkNextFrame( m_hBink );
			BINK_VERIFY(BinkDoFrameAsync( m_hBink, BINK_GAME_BACKGROUND_THREAD_INDEX_0, BINK_GAME_BACKGROUND_THREAD_INDEX_1 ));		
			BinkDoFrameAsyncWait( m_hBink, -1 );
		}
	}
	else
	{
		while (BinkShouldSkip( m_hBink ))
		{
			BinkNextFrame( m_hBink );
			BINK_VERIFY(BinkDoFrameAsync( m_hBink, BINK_GAME_BACKGROUND_THREAD_INDEX_0, BINK_GAME_BACKGROUND_THREAD_INDEX_1 ));		
			BinkDoFrameAsyncWait( m_hBink, -1 );
		}
	}
}
#endif	//BINK_ASYNC


#if BINK_ASYNC
//////////////////////////////////////////////////////////////////////////
// Async playback state-machine
bool bwMovie::DoAsyncDecodePlayback(u32 numFramesToIncrement) 
{
#if RSG_XENON || RSG_PS3 || RSG_DURANGO || RSG_ORBIS || RSG_PC

	if ( m_shouldGoToCounter > 0 )
	{
		return false;
	}

	//If the first frame is not decompressed, lets do it now
	if(Unlikely(!m_PlayedFirstFrame))
	{

		// B*1557670 - The first decompress should be done only when the data is ready. Otherwise the SPU Task will stall the game
		// Once the bink starts to play then we have no issues with the rest of the playback
		// The assumption here is that if the bink decode needs the IO to be done and the current Buffer
		// used is empty, then the decode task will wait for more data to come, if it starts. There's a chance
		// that the data we get is very delayed, because it uses the streamer and gets queued up with other
		// requests. So if this condition is true, then skip the decode

		BINKIO& bio = m_hBink->bio;	
		bool bStalledBlockingRead = m_hBink->needio && (bio.CurBufUsed == 0);
		if(!bStalledBlockingRead)
		{
			//if the first frame was not decoded and the data is finally ready then do the first decode
			BINK_VERIFY(BinkDoFrameAsync( m_hBink, BINK_GAME_BACKGROUND_THREAD_INDEX_0, BINK_GAME_BACKGROUND_THREAD_INDEX_1 ));		
			m_PlayedFirstFrame = true;
			return true;
		}
		else
		{
			return false;
		}
	}
	else if ( BinkDoFrameAsyncWait( m_hBink, 1000 )) //Wait for the previous decompress to be completed
	{
        if (!m_bReadyToRender)
			m_BlackOutTime = 100;

		m_bReadyToRender = true;
		m_bDecodeInProgress = false;

		if (m_ShouldSkip)
		{
			DoSkipFrames(numFramesToIncrement);
		}

#if RSG_PC || RSG_DURANGO
		u32 nFrame = (m_pFrameBuffers->FrameNum + 1 >= (u32)(m_pFrameBuffers->TotalFrames)) ? 0 : m_pFrameBuffers->FrameNum + 1;
		u32 nTextureBase = nFrame * 4;

		LockBinkTextures(nTextureBase);
#endif //RSG_PC || RSG_DURANGO...

		BinkNextFrame(m_hBink);

#if (__DEV)
		static BINKSUMMARY binkSummary;
		BinkGetSummary( m_hBink, &binkSummary);
		static u32 frameWindowTime = 0;
		static BINKREALTIME binkRealTime;
		BinkGetRealtime( m_hBink, &binkRealTime, frameWindowTime );
#endif

#if RSG_PC || RSG_DURANGO
		UnlockBinkTextures(nTextureBase);
#endif // RSG_PC || RSG_DURANGO...

		Assertf(m_hBink->ReadError == 0, "bwMovie::Bink Read Error");

#if !RSG_PC
        m_bFenceIndex = !m_bFenceIndex;
        GRCDEVICE.CpuWaitOnFence(m_textureFence[m_bFenceIndex]);
#endif

		// Start decompressing the next frame asynchronously
		BINK_VERIFY(BinkDoFrameAsync( m_hBink, BINK_GAME_BACKGROUND_THREAD_INDEX_0, BINK_GAME_BACKGROUND_THREAD_INDEX_1 ));		

		return true;
	}
	else
	{
		m_bDecodeInProgress = true;
		return false;
	}
#endif
}
#endif	//#if BINK_ASYNC

// A replacement BinkGoto() function, which will jump to a keyframe, then decode to that point - Exactly like
// BinkGoto() does internally. Used for debugging url:bugstar:2079210.
#if USE_DEBUG_BINK_GOTO
static void DebugBinkGoto(HBINK bink, u32 frame_num)
{
	// Declare vars as volatile so we can still see them in the stack in the debugger
	volatile u32 debugVars[7];
	debugVars[0] = 0xbeefbeef;
	debugVars[1] = sysTimer::GetSystemMsTime();

	// Find previous keyframe
	u32 prev_key = BinkGetKeyFrame(bink, frame_num, BINKGETKEYPREVIOUS);

	debugVars[2] = frame_num;
	debugVars[3] = prev_key;

	// Jump to that frame
	BinkGoto(bink, prev_key, BINKGOTOQUICK);
	debugVars[4] = sysTimer::GetSystemMsTime();
	debugVars[6] = 0xdeaddead;

	// Decode up to the target frame
	for(u32 i=prev_key; i<frame_num; ++i)
	{
		debugVars[5] = i;
		BinkDoFrameAsync(bink, bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_0, bwMovie::BINK_GAME_BACKGROUND_THREAD_INDEX_1);
		BinkDoFrameAsyncWait(bink, -1);
		BinkNextFrame(bink);
	}
}
#endif // USE_DEBUG_BINK_GOTO

//////////////////////////////////////////////////////////////////////////
// Synchronous playback state-machine
bool bwMovie::DoPlayback(u32 UNUSED_PARAM(numFramesToIncrement) )
{
#if RSG_XENON || RSG_PS3 || RSG_DURANGO || RSG_ORBIS || RSG_PC

	u32 nFrame = (m_pFrameBuffers->FrameNum + 1 >= (u32)(m_pFrameBuffers->TotalFrames)) ? 0 : m_pFrameBuffers->FrameNum + 1;
	u32 nTextureBase = nFrame * 4;

	PF_PUSH_TIMEBAR_DETAIL("Movie Lock");
	LockBinkTextures(nTextureBase);
	PF_POP_TIMEBAR_DETAIL();

	PF_PUSH_TIMEBAR_DETAIL("Movie Update");
	if (m_shouldGoToCounter > 0)
	{
		sysCriticalSection lock(m_audioLock);
		BinkGoto(m_hBink, m_targetGoToFrame, GOTO_FLAGS);
		m_targetGoToFrame = 1;
		sysInterlockedDecrement(&m_shouldGoToCounter);
	}

	BinkDoFrame( m_hBink );
	PF_POP_TIMEBAR_DETAIL();

#if RSG_PS3 || RSG_DURANGO || RSG_ORBIS || RSG_PC
	UnlockBinkTextures(nTextureBase);
#endif // RSG_PS3 || RSG_DURANGO || RSG_ORBIS

	BinkNextFrame( m_hBink );

	return true;
#endif
}

bool bwMovie::Update(bool async)
{
	if (!m_hBink || !m_bPlaying ||  m_shouldGoToCounter > 0)
	{
		return false;
	}
#if BINK_ASYNC
	if(async)
		rage::sysIpcLockMutex(sm_Mutex);
#endif
	bool bFrameUpdated = false;
	// If the movie is over and looping is disabled, dont update anymore
	//printf( "MOVIE FRAME(%d of %d)  LAST(%d)\n", m_hBink->FrameNum, m_hBink->Frames, m_hBink->LastFrameNum );
	
	if (!m_bLooping && ((m_hBink->LastFrameNum > 0 && m_hBink->LastFrameNum != 0xffffffff) && ((m_hBink->FrameNum >= m_hBink->Frames) || (m_hBink->FrameNum < m_hBink->LastFrameNum) )))
	{
		if (IsPlaying() && !m_bStopRequested && !m_bControlledByAnother )
		{
			Stop();
		}
	}
	else
	{
		u32 numFramesToIncrement = NumberOfFramesToIncrement();

		if (numFramesToIncrement > 0)
		{
#if BINK_ASYNC
			if(async)
				bFrameUpdated = DoAsyncDecodePlayback(numFramesToIncrement);
			else
#endif // BINK_ASYNC
				bFrameUpdated = DoPlayback(numFramesToIncrement);
		}
	}
#if BINK_ASYNC
	if(async)
		rage::sysIpcUnlockMutex(sm_Mutex);
#endif // BINK_ASYNC
	return bFrameUpdated;
}

void bwMovie::Play()
{
#if BINK_ASYNC
	AUTO_HANG_DETECT_CRASH_ZONE;
	checkForRequestFlood();

	sAsyncData newData = {this, AA_PLAY, sysTimer::GetSystemMsTime()};
	sm_asyncQueue.Push(newData);
#else
	BinkPause(m_hBink, false);
	m_bPlaying = true;
#endif
}

void bwMovie::PlaySync()
{
	Play();
	while (!IsPlaying())
	{
		sysIpcSleep(10);
	}
}

void bwMovie::Play(u32 uStartTime)
{
	m_uStartTime = uStartTime;
	m_uCurrentTime = uStartTime;
	m_uLastTime = uStartTime;
	m_bControlPlaybackRate = GetShouldControlPlaybackRate();

	// Bink seemingly starts calling BINKSNDONOFF if the playback rate drops too low. If we're 
	// intentionally going to be throttling the playback, just ignore these calls to prevent the audio 
	// from dropping out.
	bwAudio::SetIgnoreSoundOffCallback(m_bControlPlaybackRate);	

	Play();
}

void bwMovie::Stop()
{
	// Don't stop if not playing

#if BINK_ASYNC
	AUTO_HANG_DETECT_CRASH_ZONE;
	checkForRequestFlood();

	m_bStopRequested = true;
	sAsyncData newData = {this, AA_STOP, sysTimer::GetSystemMsTime()};
	sm_asyncQueue.Push(newData);
#else
	BinkPause(m_hBink, true);

	m_bPlaying = false;
	
	m_uStartTime = 0;
	m_uCurrentTime = 0;
	m_uLastTime = 0;
	m_uPreviousFrameNum = 0;
	m_bControlPlaybackRate = false;

	if (m_pFrameBuffers){
		m_pFrameBuffers->FrameNum = 0;
	}
#endif
}

// --- need some extra functions to do useful stuff with this...

// Calculate UV scalars
Vec4V_Out bwMovie::GetUVScalar()
{
	Assert(m_hBink);
	Assert(m_pFrameBuffers);
	
	const ScalarV movieWidth((float)m_hBink->Width);
	const ScalarV movieHeight((float)m_hBink->Height);
	const ScalarV YABufferWidth((float)m_pFrameBuffers->YABufferWidth);
	const ScalarV YABufferHeight((float)m_pFrameBuffers->YABufferHeight);
	const ScalarV cRcBBufferWidth((float)m_pFrameBuffers->cRcBBufferWidth);
	const ScalarV cRcBBufferHeight((float)m_pFrameBuffers->cRcBBufferHeight);

	const Vec4V movieSize(movieWidth,movieHeight,movieWidth * ScalarV(V_HALF),movieHeight * ScalarV(V_HALF));
	const Vec4V bufferSize(YABufferWidth,YABufferHeight,cRcBBufferWidth,cRcBBufferHeight);
	const Vec4V UVScalar = movieSize/bufferSize;
	
	return UVScalar;
}

// set stuff up so that the movie texture will be used as the current texture
bool bwMovie::BeginDraw(bool async){

	if (m_hBink && m_bLoaded)
	{
		Update(async);

        m_bRenderInProgress = true;

		// Set the shader params
		int nFrame = m_pFrameBuffers->FrameNum;
		int nTexturePtr = nFrame * 4;
		sm_pEffect->SetVar(sm_nYPlaneId,	m_TexturePointers[nTexturePtr + 0]);
		sm_pEffect->SetVar(sm_ncRPlaneId,	m_TexturePointers[nTexturePtr + 1]);
		sm_pEffect->SetVar(sm_ncBPlaneId,	m_TexturePointers[nTexturePtr + 2]);
		sm_pEffect->SetVar(sm_nAPlaneId,	m_TexturePointers[nTexturePtr + 3]);
		sm_pEffect->SetVar(sm_nUVScalarId,	GetUVScalar());

		// Bind the shader
		grcEffectTechnique tech = ( m_TexturePointers[nTexturePtr + 3])? sm_tAlpha : sm_tOpaque;
		if (sm_pEffect->BeginDraw(tech, true)) {
			sm_pEffect->BeginPass(0);
			return(true);
		}
	}

	return(false);
}

// turn off using the movie as the current texture
void bwMovie::EndDraw(){
}

void bwMovie::ShaderEndDraw(bwMovie* movie){
	// Unbind the shader
	sm_pEffect->EndPass();
	sm_pEffect->EndDraw();

	sm_pEffect->SetVar(sm_nYPlaneId,	(grcTexture*)NULL);
	sm_pEffect->SetVar(sm_ncRPlaneId,	(grcTexture*)NULL);
	sm_pEffect->SetVar(sm_ncBPlaneId,	(grcTexture*)NULL);
	sm_pEffect->SetVar(sm_nAPlaneId,	(grcTexture*)NULL);
	
    if (movie)
	{
		movie->m_bRenderInProgress = false;
#if !RSG_PC
		if (movie->m_textureFence[!movie->m_bFenceIndex])
		{
			GRCDEVICE.GpuFreeFence(movie->m_textureFence[!movie->m_bFenceIndex]);
		}
		movie->m_textureFence[!movie->m_bFenceIndex] = GRCDEVICE.AllocFenceAndGpuWrite(grcDevice::GPU_WRITE_FENCE_AFTER_SHADER_READS);
#endif
	}
}

float bwMovie::GetPlaybackRate() const
{
	float rate = (static_cast<float>(m_hBink->FrameRate) * m_fPlaybackRate) / static_cast<float>(m_hBink->FrameRateDiv);
	return rate;
}

float bwMovie::GetCurrentFrame() const
{
	float frame = static_cast<float>(m_hBink->FrameNum);
	return frame;
}

float bwMovie::GetNextKeyFrame(float time) const
{
	s32 numFrames = m_hBink->Frames;
	s32	targetFrame = (s32)(time * (m_hBink->FrameRate / m_hBink->FrameRateDiv));
	targetFrame = rage::Clamp( targetFrame, 1, numFrames);
	targetFrame = BinkGetKeyFrame(m_hBink, targetFrame, BINKGETKEYNEXT) - 1;
	float frame = static_cast<float>(rage::Clamp(targetFrame, 1, numFrames));
	return frame;
}

float bwMovie::GetMovieTime() const 
{
	Assert(m_hBink);

	Assert(m_hBink->LastFrameNum != 0);
	float percent = static_cast<float>(m_hBink->FrameNum)/static_cast<float>(m_hBink->Frames)*100.0f;
	return percent;
}

// Returns time into the movie in seconds.
float bwMovie::GetMovieTimeReal() const
{
	float rate = GetPlaybackRate();
	float time = GetCurrentFrame() / rate;
	return time;
}

void bwMovie::SetMovieTime(float percent)
{
	Assert(m_hBink);

	if (!Verifyf(m_TexturePointers.GetCount() > 0, "Trying to set movie time but we have no texture pointers! Is the movie not created yet or did we fail with oom?"))
		return;

	if (percent < 0.0f) 
	{
		percent = 0.0f;
	} 
	else if (percent > 100.0f)
	{
		percent = 100.0f;
	}

	u32 numFrames = m_hBink->Frames;
	u32 targetFrame = static_cast<u32>(static_cast<float>(numFrames)*percent*0.01f);
	//u32	targetFrame = ((u32)(secs * m_hBink->FrameRate) / m_hBink->FrameRateDiv) + 1;

	m_targetGoToFrame = rage::Clamp(targetFrame, (u32)1, numFrames);
	sysInterlockedIncrement(&m_shouldGoToCounter);

#if BINK_ASYNC
	AUTO_HANG_DETECT_CRASH_ZONE;
	checkForRequestFlood();

	sAsyncData newData = {this, AA_GOTO, sysTimer::GetSystemMsTime()};
	sm_asyncQueue.Push(newData);
#endif

	if(percent >= 0.1f)
	{
		m_BlackOutTime = 1000;	// 1 second (for now, will eventually be calculated by using keyframe rate).
	}

//	BinkGoto(m_hBink, targetFrame, 0);
}


float bwMovie::SetMovieTimeReal(float time)
{
	if (!m_hBink)
		return 0.f;

	if (!Verifyf(m_TexturePointers.GetCount() > 0, "Trying to set movie time but we have no texture pointers! Is the movie not created yet or did we fail with oom?"))
		return 0.f;

	s32 numFrames = m_hBink->Frames;
	s32	targetFrame = (s32)(time * ((float)m_hBink->FrameRate / (float)m_hBink->FrameRateDiv));
	targetFrame = rage::Clamp( targetFrame, 1, numFrames);

	targetFrame = BinkGetKeyFrame(m_hBink, targetFrame, BINKGETKEYNEXT)-1;

	// DEBUG
//	{
//		float actualTime = (float)(targetFrame) * ((float)m_hBink->FrameRateDiv / (float)m_hBink->FrameRate);
//		Displayf("BINK - Keyframe for movie (%p) at time %f = %d. Actual Time = %f", m_hBink, time, targetFrame, actualTime);
//	}
	// DEBUG

	m_targetGoToFrame = rage::Clamp(targetFrame, 1, numFrames);
	sysInterlockedIncrement(&m_shouldGoToCounter);

#if BINK_ASYNC
	AUTO_HANG_DETECT_CRASH_ZONE;
	checkForRequestFlood();

	sAsyncData newData = {this, AA_GOTO, sysTimer::GetSystemMsTime()};
	sm_asyncQueue.Push(newData);
#endif

	// Black out time
	if(m_targetGoToFrame > 1)
	{
		m_BlackOutTime = 500;	// ms
	}

	// Calculate the time of the keyframe;
	float actualTime = (float)(m_targetGoToFrame) * ((float)m_hBink->FrameRateDiv / (float)m_hBink->FrameRate);
	float timeDiff = actualTime - time;

	//Displayf("BINK - Time Diff = %f", timeDiff);

	return timeDiff;
}


const bwAudio::Info *bwMovie::FindAudio() const
{
	return bwAudio::FindInfo(m_hBink);
}

#if BINK_ASYNC
void bwMovie::AsyncStopMovie()
{
	if ( IsPlaying())
	{
		BinkPause(m_hBink, true);
	}

	m_bPlaying = false;
	m_bStopRequested = false;

	m_uStartTime = 0;
	m_uCurrentTime = 0;
	m_uLastTime = 0;
	m_uPreviousFrameNum = 0;
	m_bControlPlaybackRate = false;

	if (m_pFrameBuffers){
		m_pFrameBuffers->FrameNum = 0;
	}
}


#if DEBUG_BINK_ASYNC
#include "system/timemgr.h"
PARAM(displayBinkCommands, "Display commands sent by the AsyncBink thread");
PARAM(displayBinkFloodDectect, "Display flood detection of the Bink command queue");

static const char *ops[] = {"AA_OPEN", "AA_GOTO", "AA_PLAY", "AA_STOP", "AA_DESTORY" };

void bwMovie::PrintData( const char *tag, const bwMovie::sAsyncData &dat )
{
	if( PARAM_displayBinkCommands.Get() )
	{
		Displayf("-=[ BINK ASYNC %12.12s %12.12s %6d]=- 0x%p %48.48s Playing: %c Loaded : %c Looping : %c\n", tag, ops[dat.action], TIME.GetFrameCount(), dat.movie, dat.openParams.pFileName,
			dat.movie->IsPlaying() ? 'T' : 'F', dat.movie->IsLoaded() ? 'T' : 'F', dat.movie->IsLooping() ? 'T' : 'F' );
	}
}

#endif // DEBUG_BINK_ASYNC

void bwMovie::checkForRequestFlood()
{
	// In non-final builds, check for code flooding requests. This can be a single tick with a lot of requests in it, or it can be
	// one or more requests per frame over an extended period. Either is Very Bad, as it'll fill up the async queue and cause a
	// stall, which could result in a deadlock (As has been seen before).
#if __DEV || __BANK

	if (PARAM_displayBinkFloodDectect.Get())
	{
#if __DEV
		Displayf("bwMovie request added, dumping stack:");
		sysStack::PrintStackTrace();
#endif

		// If, over a period of s_floodTime milliseconds, we receive more than s_maxRequestsInFlood requests, assume this is a flood.
		static const u32 s_floodTime = 1000;
		static const u32 s_maxRequestsInFlood = 24;
		static u32 floodStartT = 0;
		static u32 numRequestsInFlood = 0;
		static u32 peakFlood = 0;

		u32 currTime = sysTimer::GetSystemMsTime();
		if (currTime - floodStartT < s_floodTime)
		{
			++numRequestsInFlood;
			if (numRequestsInFlood > peakFlood)
			{
				peakFlood = numRequestsInFlood;
			}
			if (numRequestsInFlood > s_maxRequestsInFlood)
			{
				Displayf("WARNING: bwMovie flood detected! In the past %dms, there have been %d requests made to bwMovie! "
					"This will very likely cause the game to hang!", currTime - floodStartT, numRequestsInFlood);
				Assertf(false, "bwMovie flood detected see TTY for more info. Please update B*2079210 with a crash dump!");
			}
		}
		else
		{
			floodStartT = currTime;
			numRequestsInFlood = 1;
		}
	}

	if(!sm_asyncQueue.IsNotFull())
	{
		//Dev: If we hit this May need to look at the thread priority as it may not be poping commands or someone is queuing lots of commands on the same frame.
		Quitf("bwMovie command queue flood detected! The command queue is currently full! This will very likely cause the game to hang as it will start to skip commands, add -displayBinkFloodDectect to the commandline to gather more info.");
	}

#endif // __BANK
}

void bwMovie::AsyncBink(void* UNUSED_PARAM(ptr))
{
	sAsyncData		data;
	bwMovie *		movie;

	// Debug code for url:bugstar:2079210
	volatile u32	dequeueTimes[MAX_ASYNC_ACTIONS*2];
	u32				nextDequeueIdx = 0;

	while (1)
	{
		// Get the next
		data	= sm_asyncQueue.Pop();
		movie	= data.movie;
		if (!movie)
			break;

		rage::sysIpcLockMutex(sm_Mutex);

		// Debug code for url:bugstar:2079210
		dequeueTimes[nextDequeueIdx++] = sysTimer::GetSystemMsTime();
		if(nextDequeueIdx >= NELEM(dequeueTimes))
		{
			nextDequeueIdx = 0;
		}

		// Do any debugging
		PrintData( "EXECUTE", data );

		switch (data.action)
		{
		case AA_OPEN:
			{
				rage::sysIpcUnlockMutex(sm_Mutex);

				bool ret = movie->SetMovieSync(data.openParams);

				rage::sysIpcLockMutex(sm_Mutex);

				data.openParams.loadedCallback.Call(CallbackData(&ret));
				if (ret)
				{
					safecpy(sm_pFileName, data.openParams.pFileName);
					movie->SetVolume(0.f);
					movie->m_bLoaded = true;
				}
			}
			break;

		case AA_GOTO:
			{
				sm_currentGotoFrame = movie->m_targetGoToFrame;
				while (!BinkDoFrameAsyncWait(movie->m_hBink, 1000))
				{
					sysIpcSleep(10);
				};

				// We better be playing or this is going nowhere.
				if (movie->IsPlaying() != true)
				{
					Warningf( "movie->IsPlaying() != true : have a goto instruction, but movie not playing" );
				}

				// Debug code for url:bugstar:2079210
				// Insert another time marker after the async wait
				dequeueTimes[nextDequeueIdx++] = sysTimer::GetSystemMsTime();
				if(nextDequeueIdx >= NELEM(dequeueTimes))
				{
					nextDequeueIdx = 0;
				}

				{
					sysCriticalSection lock(movie->m_audioLock);
					#if USE_DEBUG_BINK_GOTO
						DebugBinkGoto(movie->m_hBink, movie->m_targetGoToFrame);
					#else
						BinkGoto(movie->m_hBink, movie->m_targetGoToFrame, GOTO_FLAGS);
					#endif
				}

				while (!BinkDoFrameAsyncWait(movie->m_hBink, 1000))
				{
					sysIpcSleep(10);
				};

				sysInterlockedDecrement(&movie->m_shouldGoToCounter);
				sm_currentGotoFrame = -1;

				// Because of the "have we looped test" in bwMovie::Update(), not resetting the LastFrameNum here could indicate a false loop there.
				movie->m_hBink->LastFrameNum = movie->m_targetGoToFrame;

				movie->m_targetGoToFrame = 1;

				// Black out time
				if (movie->m_targetGoToFrame > 1)
				{
					movie->m_BlackOutTime = 500;	// ms
				}

			}
			break;
		case AA_PLAY:
			BinkPause(movie->m_hBink, false);
			movie->m_bPlaying = true;
			
			break;
		case AA_STOP:
			movie->AsyncStopMovie();
			break;
		case AA_DESTROY:
			if (movie->IsRenderInProgress() || movie->AreFencesPending())
			{
				sm_asyncQueue.Push(data);
				PrintData("STALL-RENDER", data);
                sysIpcSleep(1);
			}
			else
			{
				movie->AsyncStopMovie();
				delete movie;
			}
			sm_pFileName[0] = 0;
			break;
		}

#if DEBUG_BINK_ASYNC
		if( PARAM_displayBinkCommands.Get() )
		{
			Displayf( "-=[ BINK ASYNC %12.12s %12.12s]=-", "COMPLETED", ops[data.action] );
		}
#endif	//DEBUG_BINK_ASYNC

		rage::sysIpcUnlockMutex(sm_Mutex);

	}
}

bool bwMovie::AreFencesPending()
{
	bool pending = false;
#if !RSG_PC
	if (m_textureFence[m_bFenceIndex] != 0) { pending |= GRCDEVICE.IsFencePending(m_textureFence[m_bFenceIndex]); }
	if (m_textureFence[!m_bFenceIndex] != 0) { pending |= GRCDEVICE.IsFencePending(m_textureFence[!m_bFenceIndex]); }
#endif
	return pending;
}

#endif // BINK_ASYNC

#if RSG_ORBIS || BACKTRACE_ENABLED
#if BINK_ASYNC

#if RSG_ORBIS
#define WRITE_COREDUMP_DATA(data,size) sceCoredumpWriteUserData(data,size)
#else
#define WRITE_COREDUMP_DATA(data,size) WriteFile(crashlogFileHandle, data, (DWORD)size, NULL, NULL)
#endif

void bwMovie::CoredumpHandler(BACKTRACE_ONLY(HANDLE crashlogFileHandle))
{
//	WRITE_COREDUMP_DATA("MovieAsyncQueue:", 16);
//	WRITE_COREDUMP_DATA(&sm_asyncQueue, sizeof(sm_asyncQueue));
	WRITE_COREDUMP_DATA("Filename:", 9);
	WRITE_COREDUMP_DATA(sm_pFileName, strlen(sm_pFileName));

	char buff[32];
	formatf(buff, "\nGotoFrame:%d\n", sm_currentGotoFrame);
	WRITE_COREDUMP_DATA(buff, strlen(buff));
}
#undef WRITE_COREDUMP_DATA
#else
	void bwMovie::coredumpHandler() {}
#endif // BINK_ASYNC
#endif // RSG_ORBIS
