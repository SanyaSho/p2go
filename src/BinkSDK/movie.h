//
// bink/movie.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#ifndef BINK_MOVIE_H
#define BINK_MOVIE_H

#if !__FINAL
#define __RFS__ 1
#else
#define __RFS__ 0
#endif

#include "atl/array.h"
#include "data/callback.h"
#include "grcore/texture.h"
#include "vector/vector3.h"
#include "vector/vector4.h"
#include "bink/audio.h"
#include "system/criticalsection.h"
#include "system/exception.h"
#include "system/messagequeue.h"

#define USE_BINK_RAGE_AUDIO_PROVIDER 1

#define MAX_ASYNC_ACTIONS	(32)
#define DEBUG_BINK_ASYNC	(1 && !__FINAL)

#if RSG_XENON || RSG_DURANGO || RSG_ORBIS || RSG_PS3 || RSG_PC

#define			BINK_ASYNC											(1)

#endif

struct BINK;
struct BINKFRAMEBUFFERS;
struct ID3D11Query;

namespace rage
{

class fiStream;
class grcEffect;
class audRingBuffer;

class bwMovie
{
	bwMovie();
	~bwMovie();

public:

	struct bwMovieParams
	{
		datCallback		loadedCallback;
		char			pFileName[80];
		fiStream*		pStream;
		u32				extraFlags;
		u32				ioSize;
		float			playbackRate;
		u32				frameRate;
		u32				frameRateDiv;
		void*			owner;
		void*			preloadedData;

		bwMovieParams()
		{
			pFileName[0]	= '\0';
			pStream			= NULL;
			extraFlags		= 0;
			ioSize			= 0;
			playbackRate	= 1.0f;
			frameRate		= 2997;
			frameRateDiv	= 100;
			owner			= NULL;
			preloadedData	= NULL;
		}
	};

	// These threads must be created before they can be used.
	enum BackgroundThread
	{
		THREAD_0				= 0,
		THREAD_1				= 1,
		THREAD_2				= 2,
		THREAD_5				= 5
	};

	static const u32		BINK_GAME_BACKGROUND_THREAD_INDEX_0;
	static const u32		BINK_GAME_BACKGROUND_THREAD_INDEX_1;

    static void     Init();
    static void     Shutdown();

	static void		SetShouldControlPlaybackRateCB(bool (*fn)());
	static bool		GetShouldControlPlaybackRate();		// Accessor for callback

	// before instancing any bwMovie, InitShaders should be called once 
	static void		InitShaders();
	static void		ShutdownShaders();

	static bwMovie* Create();
	static void Destroy(bwMovie* movie, bool async = true);

	static bool IsUsingRAGEAudio() { return sm_UsingRAGEAudio; }

	inline static u32 ComputeAllocSizeForL8Texture(u32 width, u32 height)
	{
		u32 w = width;
		u32 h = height;
#if RSG_XENON || RSG_DURANGO || RSG_ORBIS
		// pitch size needs to be multiple of 256 (not sure if this is applicable to NG?)
		w = ((w+255) & (~255));
#endif
		return 	(w*h);
	}

	// allow game code to set the memory functions to be used for all bink allocations, so we can use
	// our own special allocators instead of just pulling memory from where ever it feels like.
	#if RSG_BINK_2_7D
		static void SetMemoryFuncs( void* (a(u64)),  void (f(void*)));
	#else
		static void SetMemoryFuncs( void* (a(u32)),  void (f(void*)));
	#endif
	
	bool SetMovie(bwMovieParams& params);
	bool SetMovieSync(bwMovieParams& params);
#if RSG_PC && BINK_ASYNC
	static void FillTexture(u32 width, u32 height, void *pDest, u32 Stride, u8 value);
#endif // RSG_PC && BINK_ASYNC
    u32 ImposeFrameRate();

	void Play();
	void Play(u32 uStartTime);
	void PlaySync();			// Yuck - intro screens in async land (IsPlaying takes time)
	void Stop();

	void SetRect(const Vector3& vTopLeft, const Vector3& vTopRight, const Vector3& vBottomLeft, const Vector3& vBottomRight);

	static float GetMovieDurationQuick(bwMovieParams& params);

	float	GetPlaybackRate() const;
	float	GetCurrentFrame() const;
	float	GetNextKeyFrame(float time) const;

	float GetMovieTime() const;
	float GetMovieTimeReal() const;

	void SetMovieTime(float percent);
	float SetMovieTimeReal(float time);		// Actual time

	void SetLooping(bool bLooping)			{ m_bLooping = bLooping; }
	void SetTintColor(const Vector4 &vColor){ m_TintColor = vColor; }

	bool IsPlaying() const					{ return m_bPlaying; }
	u32	 GetWidth() const;
	u32	 GetHeight() const;
	s32	 GetFramesRemaining() const;

	bool HasAudio() const { return m_bHasAudio; }

	bool Update(bool async);
	bool BeginDraw(bool async=true);					// use the movie texture as the current source texture
	void EndDraw();						// reset the current source texture
	static void ShaderEndDraw(bwMovie* movie);

	void UpdateMovieTime(u32 uCurrentTime);

	void UpdateWithinBlackout();
	bool IsWithinBlackout() const;
	
	bool IsLoaded() const { return m_bLoaded; }
    void SetLoaded(bool val) { m_bLoaded = val; }

	bool IsDecodeInProgress() const { return m_bDecodeInProgress; }
	bool IsRenderInProgress() const { return m_bRenderInProgress; }

	// PURPOSE
	//	Enable frame skipping so that video keeps in sync with audio regardless of rendering framerate
	void SetShouldSkip(const bool shouldSkip)
	{
		m_ShouldSkip = shouldSkip;
	}
	void SetVolume(const f32 dBVolume);
	static s32 GetBinkVolume(const f32 dbVolume);

	const bwAudio::Info *FindAudio() const;

	void SetControlledByAnother() { m_bControlledByAnother = true; }

	static void SetTextureMemoryAllocator(Functor3Ret<void*, u32, u32, void*> fn) { sm_TextureMemoryAllocator = fn; }
	static void SetTextureMemoryDeallocator(Functor1<void*> fn) { sm_TextureMemoryDeallocator = fn; }
	
	void AsyncStopMovie();
	static void AsyncBink(void* ptr);

#if BINK_ASYNC
	bool AreFencesPending();
#endif

#if RSG_ORBIS
	static void CoredumpHandler();
#elif BACKTRACE_ENABLED
	typedef void* HANDLE;
	static void CoredumpHandler(HANDLE crashlogFileHandle);
#endif

private:

	bool IsLooping() const { return m_bLooping; }

	u32 NumberOfFramesToIncrement();

	void LockBinkTextures(u32 nTextureBase);

	// Had to use WIN32PC_ONLY(), which *is* defined for RSG_PC (x64)
	void UnlockBinkTextures(WIN32PC_ONLY(u32 nTextureBase) DURANGO_ONLY(u32 nTextureBase) ORBIS_ONLY(u32 nTextureBase) PS3_ONLY(u32 nTextureBase));
	void DoSkipFrames(u32 numFramesToIncrement);
	bool DoPlayback(u32 numFramesToIncrement);
	bool DoAsyncDecodePlayback(u32 numFramesToIncrement);
	Vec4V_Out GetUVScalar();

	BINK*						m_hBink;
	BINKFRAMEBUFFERS*			m_pFrameBuffers;
	atArray<grcTexture*>		m_TexturePointers;
	atArray<void*>				m_RawTextureMemPointers;
	atArray<grcTextureLock>		m_TextureLocks;
	Vector3						m_vRect[4];

#if !RSG_PC
    grcFenceHandle              m_textureFence[2];
#endif

    sysCriticalSectionToken     m_audioLock;

	bool						m_bDecodeInProgress;
    bool                        m_bRenderInProgress;
	bool						m_bReadyToRender;
	bool						m_bLoaded;
	volatile bool				m_bPlaying;
	volatile bool				m_bStopRequested;
	bool						m_bLooping;
	bool						m_ShouldSkip;
	bool						m_PlayedFirstFrame;
	bool						m_bControlledByAnother;
	volatile u32				m_shouldGoToCounter;
	Vector4						m_TintColor;
	u32							m_targetGoToFrame;

	// playback Rate Controls
	float						m_fPlaybackRate;
	u32							m_uFrameRate;
	u32							m_uFrameRateDiv;
	u32							m_uStartTime;
	u32							m_uLastTime;
	u32							m_uCurrentTime;
	u32							m_uPreviousFrameNum;
	bool						m_bControlPlaybackRate;
	bool						m_bHasAudio;
	bool						m_bIsLoading;

#if !RSG_PC
    bool                        m_bFenceIndex;
#endif

	u32							m_BlackOutTime;

	// u16							m_rtPoolId;

	static grcEffect*			sm_pEffect;
	static grcEffectVar			sm_nYPlaneId;
	static grcEffectVar			sm_ncRPlaneId;
	static grcEffectVar			sm_ncBPlaneId;
	static grcEffectVar			sm_nAPlaneId;
	static grcEffectVar			sm_nUVScalarId;
	static grcEffectTechnique	sm_tOpaque;
	static grcEffectTechnique	sm_tAlpha;

	static Functor1<void*>				sm_TextureMemoryDeallocator;
	static Functor3Ret<void*,u32,u32, void*> sm_TextureMemoryAllocator;

	static u32					sm_uNumSpeakers;

	static bool					(*pControlPlayRateCB)();

#if BINK_ASYNC

	// Async command buffer functionality
	enum eAsyncAction
	{
		AA_OPEN = 0,
		AA_GOTO,
		AA_PLAY,
		AA_STOP,
		AA_DESTROY
	};
	struct sAsyncData
	{
		bwMovie* movie;
		eAsyncAction action;
		u32 issueTime;
		bwMovieParams openParams;
	};

#if DEBUG_BINK_ASYNC
	static void PrintData( const char *tag, const sAsyncData &dat );
#else
	static inline void PrintData( const char *, const sAsyncData & ) {}
#endif

	static void checkForRequestFlood();

	static sysMessageQueue<sAsyncData, MAX_ASYNC_ACTIONS, true> sm_asyncQueue;
	static sysIpcThreadId		sm_asyncThread;

	static bool sm_AsyncInit;
	static rage::sysIpcMutex sm_Mutex;

	static char sm_pFileName[80];
	static int sm_currentGotoFrame;

#endif	//BINK_ASYNC

	static bool sm_UsingRAGEAudio;

#if __BANK
	static bool m_gDumpTEX;
#endif	//__BANK

};

} // namespace rage

#endif // BINK_MOVIE_H
