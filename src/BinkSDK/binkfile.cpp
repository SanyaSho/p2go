#include "bink_include.h"
#include "movie.h"

//#define BINK_USE_TELEMETRY  // comment out, if you don't have Telemetry

#if defined(__RADNT__) || defined(__RADXBOX__) || defined(__RADXENON__)

#pragma code_seg("BINK")
#pragma data_seg("BINKDATA")
#pragma const_seg("BINKCONST")
#pragma bss_seg("BINKBSS")

#endif

// If you intend to replace the BinkFile functions, then you should probably
//   replace the "rfile.h" below with your own defines and leave everything
//   else alone.  You should only try to replace all of the BinkFile functions
//   completely, if you are going to require more sophisticated streaming
//   (like off TCP/IP directly, for example).

#include "rfile.h"

#if USE_RAGE_PGSTREAMER
rage::binkRageHandle rage::binkRageHandle::theHandles[rage::binkRageHandle::maxBinkOpen];
#endif

// defined variables that are accessed from the Bink IO structure
typedef struct BINKFILE
{
#if RSG_BINK_2_7D
	U64 StartFile;
	UINTa FileHandle;
	U32 FileBufPos;
	U8* Buffer;
	U8* BufEnd;
	U32 DontClose;
	U32 FileSize;
	U64 volatile BufPos;
	U32 volatile FileIOPos;
	U64 volatile BufEmpty;
	U8* volatile BufBack;
	S32 volatile HaveSeekedBack;
#else
  U64 StartFile;
  UINTa FileHandle;
  U32 FileBufPos;
  U8* Buffer;
  U8* BufEnd;
  U32 DontClose;
  U32 FileSize;
  U32 volatile BufPos;
  U32 volatile FileIOPos;
  U32 volatile BufEmpty;
  U8* volatile BufBack;
  S32 volatile HaveSeekedBack;
#endif
} BINKFILE;

#ifdef __RADPSP__
  #define BASEREADSIZE ( 32 * 1024 )
  #define READALIGNMENT 2048
  #include <libvfpu.h> 
  #define radmemcpy sceVfpuMemcpy
#elif defined(__RADWII__)
  #define BASEREADSIZE ( 32 * 1024 )
  #define READALIGNMENT 2048
  #include <string.h>
  #define radmemcpy memcpy
#elif defined(__RADNDS__)
  #define BASEREADSIZE ( 8 * 1024 )
  #define READALIGNMENT 512
  #include <string.h>
  #define radmemcpy memcpy
#else
  #define BASEREADSIZE ( 128 * 1024 )
  #define READALIGNMENT 4096
  #include <string.h>
  #define radmemcpy memcpy
#endif

#ifdef BINK_USE_TELEMETRY
#include "binktm.h"
#endif

// functions that callback into the Bink library for service stuff
#define SUSPEND_CB( bio ) { if ( bio->suspend_callback ) bio->suspend_callback( bio );}
#define RESUME_CB( bio ) { if ( bio->resume_callback ) bio->resume_callback( bio );}
#define TRY_SUSPEND_CB( bio ) ( ( bio->try_suspend_callback == 0 ) ? 0 : bio->try_suspend_callback( bio ) )
#define IDLE_ON_CB( bio ) { if ( bio->idle_on_callback ) bio->idle_on_callback( bio );}
#define SIMULATE_CB( bio, amt, tmr ) { if ( bio->simulate_callback ) bio->simulate_callback( bio, amt, tmr );}
#define TIMER_CB( ) ( ( bio->timer_callback ) ? bio->timer_callback() : 0 )
#define FLIPENDIAN_CB( ptr, amt ) { if ( bio->flipendian_callback ) bio->flipendian_callback( ptr, amt );}
#define LOCKEDADD_CB( ptr, amt ) { if ( bio->lockedadd_callback ) bio->lockedadd_callback( ptr, amt ); else *(ptr)+=(amt); }

//reads from the header
#if RSG_BINK_2_7D
	static U64 RADLINK BinkFileReadHeader( BINKIO * bio, S64 offset, void* dest, U64 size )
#else
	static U32 RADLINK BinkFileReadHeader( BINKIO * bio, S32 offset, void* dest, U32 size )
#endif
{
  U32 amt, temp;
  BINKFILE * BF = (BINKFILE*) bio->iodata;

  SUSPEND_CB( bio );

  if ( offset != -1 )
  {
    if ( BF->FileIOPos != (U32) offset )
    {
      radseekbegin64( BF->FileHandle, offset + BF->StartFile );
      BF->FileIOPos = offset;
    }
  }

  #ifdef BINK_USE_TELEMETRY
    tmEnter( bink_context, TMZF_STALL, "%s %s", "Bink IO", "read header" );
  #endif
  
  radread( BF->FileHandle, dest, size, &amt );
  #ifdef  __RADNDS__
  raddone( BF->FileHandle, 1 );
  #endif

  #ifdef BINK_USE_TELEMETRY
      tmLeave( bink_context );
  #endif

  if ( amt !=size )
    bio->ReadError=1;

  BF->FileIOPos += amt;
  BF->FileBufPos = BF->FileIOPos;

  temp = ( BF->FileSize - BF->FileBufPos );
  bio->CurBufSize = ( temp < bio->BufSize ) ? temp : bio->BufSize;

  RESUME_CB( bio );

  FLIPENDIAN_CB( dest, amt );

  return( amt );
}


//reads a frame (BinkIdle might be running from another thread, so protect against it)
#if RSG_BINK_2_7D
	static U64  RADLINK BinkFileReadFrame( BINKIO * bio, U32 UNUSED_PARAM(framenum), S64 offset, void* dest, U64 size )
#else
	static U32  RADLINK BinkFileReadFrame( BINKIO * bio, U32 UNUSED_PARAM(framenum), S32 offset, void* dest, U32 size )
#endif
{
  S32 funcstart = 0;
  U32 amt, tamt = 0;
  U32 timer, timer2;
  U32 cpy;
  void* odest = dest;
  BINKFILE * BF = (BINKFILE*) bio->iodata;
  
  if ( bio->ReadError )
    return( 0 );

  timer = TIMER_CB();

  #ifdef BINK_USE_TELEMETRY
    if ( bink_context )
    {
      static U32 lf = 0xffffffff;
      if ( framenum < lf )
      {
        tmSetTimelineSectionName( bink_context, "Loop" );
      }
      lf = framenum;
      tmEnter( bink_context, TMZF_NONE, "%s %s", "Bink IO", "read frame" );
    }
  #endif

  if ( offset != -1 )
  {
    if ( BF->FileBufPos != (U32) offset )
    {
      funcstart = 1;

      #ifdef BINK_USE_TELEMETRY
        tmEnter( bink_context, TMZF_STALL, "%s %s", "Bink IO", "seek to new position" );
      #endif

      #ifdef  __RADNDS__
        raddone( BF->FileHandle, 1 );
      #endif

      SUSPEND_CB( bio );

      if ( ( (U32) offset > BF->FileBufPos ) && ( (U32) offset <= BF->FileIOPos ) )
      {
        amt = offset-BF->FileBufPos;

        BF->FileBufPos = offset;
        BF->BufEmpty += amt;
        bio->CurBufUsed -= amt;
        BF->BufPos += amt;
        if ( BF->BufPos >= bio->BufSize )
          BF->BufPos -= bio->BufSize;
      }
      else
      {
        radseekbegin64( BF->FileHandle, offset + BF->StartFile );
        BF->FileIOPos = offset;
        BF->FileBufPos = offset;

        BF->BufEmpty = bio->BufSize;
        bio->CurBufUsed = 0;
        BF->BufPos = 0;
        BF->BufBack = BF->Buffer;
      }

      #ifdef BINK_USE_TELEMETRY
        tmLeave( bink_context );
      #endif

    }
  }

 // copy from background buffer
 getrest:

  cpy = bio->CurBufUsed;

  if ( cpy )
  {
    U32 front;

    if ( cpy > size )
      cpy = size;

    size -= cpy;
    tamt += cpy;
    BF->FileBufPos += cpy;

    #ifdef  __RADNDS__
      if ( bio->CurBufUsed <= ( BASEREADSIZE * 2 ) )
        raddone( BF->FileHandle, 1 );
    #endif

    front = (U32) (bio->BufSize - BF->BufPos);
    if ( front <= cpy )
    {
      radmemcpy( dest, BF->Buffer + BF->BufPos, front );
      dest = ( (U8*) dest ) + front;
      BF->BufPos = 0;
      cpy -= front;

      LOCKEDADD_CB( &bio->CurBufUsed, -(S32)front );
      LOCKEDADD_CB( &BF->BufEmpty, front );

      if ( cpy == 0 )
        goto skipwrap;
    }
    
    radmemcpy( dest, BF->Buffer + BF->BufPos, cpy );
    dest = ( (U8*) dest ) + cpy;

	volatile U32 preBufPos = BF->BufPos;
	preBufPos = preBufPos;

    LOCKEDADD_CB( &BF->BufPos, cpy );
    LOCKEDADD_CB( &bio->CurBufUsed, -(S32)cpy );
    LOCKEDADD_CB( &BF->BufEmpty, cpy );
  }

 skipwrap:

  if ( size )
  {

    if ( funcstart == 0 )
    {
      funcstart = 1;

      #ifdef BINK_USE_TELEMETRY
        tmEnter( bink_context, TMZF_STALL, "%s %s", "Bink IO", "wait on background" );
      #endif

      SUSPEND_CB( bio );

      #ifdef BINK_USE_TELEMETRY
        tmLeave( bink_context );
      #endif

      // did we get more data while waiting on the lock?
      if ( bio->CurBufUsed )
        goto getrest;
    }

    #ifdef BINK_USE_TELEMETRY
      tmEnter( bink_context, TMZF_STALL, "%s %s", "Bink IO", "blocking read" );
    #endif

    timer2 = TIMER_CB();
    if ( BF->HaveSeekedBack )
    {
      bio->bink->OpenFlags &= ~BINKWILLLOOP;
      BF->HaveSeekedBack = 0;
    }

    radread( BF->FileHandle, dest, size, &amt );
    if ( amt < size )
      bio->ReadError = 1;

    // sleeps for the simulation amount (only necessary for debugging)
    SIMULATE_CB( bio, amt, timer2 );

    BF->FileIOPos += amt;
    BF->FileBufPos += amt;
    bio->BytesRead += amt;
    tamt += amt;

    amt = TIMER_CB();
    bio->TotalTime += ( amt - timer2 );
    bio->ForegroundTime += ( amt - timer );

    #ifdef BINK_USE_TELEMETRY
      tmLeave( bink_context );
    #endif

    #ifdef  __RADNDS__
      raddone( BF->FileHandle, 1 );
    #endif
  }
  else
  {
    bio->ForegroundTime += ( TIMER_CB() - timer );
  }

  amt = ( BF->FileSize - BF->FileBufPos );
  bio->CurBufSize = ( amt < bio->BufSize ) ? amt : bio->BufSize;
  if ( ( bio->CurBufUsed + BASEREADSIZE ) > bio->CurBufSize )
    bio->CurBufSize = bio->CurBufUsed;

  if ( funcstart )
    RESUME_CB( bio );

  FLIPENDIAN_CB( odest, tamt );

  #ifdef BINK_USE_TELEMETRY
    tmLeave( bink_context );
  #endif

  return( tamt );
}


//returns the size of the recommended buffer
#if RSG_BINK_2_7D
	static U64 RADLINK BinkFileGetBufferSize( BINKIO*  UNUSED_PARAM(bio), U64 size )
#else
	static U32 RADLINK BinkFileGetBufferSize( BINKIO*  UNUSED_PARAM(bio), U32 size )
#endif
{
  size = READALIGNMENT + ( ( size + ( BASEREADSIZE - 1 ) ) / BASEREADSIZE ) * BASEREADSIZE;
  if ( size < ( BASEREADSIZE * 2 + READALIGNMENT ) )
    size = BASEREADSIZE * 2 + READALIGNMENT;
  return( size );
}


//sets the address and size of the background buffer
#if RSG_BINK_2_7D
	static void RADLINK BinkFileSetInfo( BINKIO * bio, void * buff, U64 size, U64 filesize, U32  UNUSED_PARAM(simulate) )
#else
	static void RADLINK BinkFileSetInfo( BINKIO * bio, void * buff, U32 size, U32 filesize, U32  UNUSED_PARAM(simulate) )
#endif
{
  BINKFILE * BF = (BINKFILE*) bio->iodata;
  U8 * buf;

  SUSPEND_CB( bio );

  buf = (U8*) ( ( ( (UINTa) buff ) + (READALIGNMENT-1) ) & ~(READALIGNMENT-1) );
  size -= (U32) ( (U8*) buf - (U8*) buff );
  size = ( size / BASEREADSIZE ) * BASEREADSIZE;
  BF->Buffer = (U8*) buf;
  BF->BufPos = 0;
  BF->BufBack = (U8*) buf;
  BF->BufEnd =( (U8*) buf ) + size;
  bio->BufSize = size;
  BF->BufEmpty = size;
  bio->CurBufUsed = 0;
  BF->FileSize = filesize;

  RESUME_CB( bio );
}


//close the io structure
static void RADLINK BinkFileClose( BINKIO * bio )
{
  BINKFILE * BF = (BINKFILE*) bio->iodata;

  SUSPEND_CB( bio );

  if ( BF->DontClose == 0 )
    radclose( BF->FileHandle );

  RESUME_CB( bio );
}


//tells the io system that idle time is occurring (can be called from another thread)
#if RSG_BINK_2_7D
	static U64 RADLINK BinkFileIdle(BINKIO * bio)
#else
	static U32 RADLINK BinkFileIdle(BINKIO * bio)
#endif
{
  BINKFILE * BF = (BINKFILE*) bio->iodata;
  U32 amt = 0;
  U32 filesizeleft, timer;
  S32 Working = bio->Working;

  if ( bio->ReadError )
    return( 0 );

  if ( bio->Suspended )
    return( 0 );

  #ifdef  __RADNDS__
    if ( !raddone( BF->FileHandle, 0 ) ) 
      return 0;
  #endif

  if ( TRY_SUSPEND_CB( bio )  )
  {
    filesizeleft = ( BF->FileSize - BF->FileIOPos );

    // if we are out of data to read, and we are in loop mode, seek backwards with a dummy read
    if ( ( filesizeleft == 0 ) && ( !BF->HaveSeekedBack ) && ( bio->bink->OpenFlags & BINKWILLLOOP ) )
    {
      char dummy_buf[ 96 ]; // padded for wii/nds

      BF->HaveSeekedBack = 1;
      bio->bink->OpenFlags &= ~BINKWILLLOOP;

      bio->DoingARead = 1;

      #ifdef BINK_USE_TELEMETRY
        tmSetTimelineSectionName( bink_context, "Loop Seek Period" );
        tmEnter( bink_context, TMZF_NONE, "%s %s", "Bink IO", "seek to file start" );
      #endif

      radseekbegin64( BF->FileHandle, ( bio->bink->frameoffsets[ 0 ] & ~1 ) - 32 + BF->StartFile );
      radread( BF->FileHandle, dummy_buf, 32, &amt );

      #ifdef BINK_USE_TELEMETRY
        tmLeave( bink_context );
      #endif

      bio->DoingARead = 0;

      amt = 0;
    }

    // do a background IO, if we have the room
    if ( ( BF->BufEmpty >= BASEREADSIZE ) && ( filesizeleft ) )
    {
      U32 toreadamt;
      U32 align;

      toreadamt = BASEREADSIZE;
      if ( filesizeleft < toreadamt )
        toreadamt = filesizeleft;

      timer = TIMER_CB();

      align = BF->FileIOPos & ( BASEREADSIZE - 1 );
      if ( ( align > toreadamt ) || ( toreadamt == filesizeleft ) )
        align = 0;

#if RSG_BINK_2_7D
	  //rrassert( ( align == 0 ) || ( ( bio->CurBufUsed == 0 ) && ( BF->FileIOPos == BF->FileBufPos ) && ( (BF->Buffer+BF->BufPos)==BF->BufBack ) ) );
#else
	  rrassert( ( align == 0 ) || ( ( bio->CurBufUsed == 0 ) && ( BF->FileIOPos == BF->FileBufPos ) && ( (BF->Buffer+BF->BufPos)==BF->BufBack ) ) );
#endif

      if ( align )
	  {
        LOCKEDADD_CB( &BF->BufPos,align );
	  }

      toreadamt = toreadamt - align;

      if ( BF->HaveSeekedBack )
      {
        bio->bink->OpenFlags &= ~BINKWILLLOOP;
        BF->HaveSeekedBack = 0;
      }

      #ifdef BINK_USE_TELEMETRY
        tmEnter( bink_context, TMZF_NONE, "%s %s", "Bink IO", "doing background read" );
      #endif

      bio->DoingARead = 1;
      radread( BF->FileHandle, (void*) ( BF->BufBack + align ), toreadamt, &amt );
      bio->DoingARead = 0;

      // sleeps for the simulation time (only necessary for debugging)
      SIMULATE_CB( bio, amt, timer );

      if ( amt != toreadamt )
        bio->ReadError = 1;

      if ( amt )
      {
        bio->BytesRead += amt;
        BF->FileIOPos += amt;
        BF->BufBack += BASEREADSIZE;  // the background buffer ptr *ALWAYS* advances by a full base amount 
                                      //   because we will only read less than a buffer on the first read
                                      //   (when we need to align), and the last read (where we won't look
                                      //   at the extra data).  this pointer thereby always stays aligned
                                      
        if ( BF->BufBack >= BF->BufEnd )
          BF->BufBack = BF->Buffer;

        LOCKEDADD_CB( &BF->BufEmpty, -(S32) amt );
        LOCKEDADD_CB( &bio->CurBufUsed, amt );

        if ( bio->CurBufUsed > bio->BufHighUsed )
          bio->BufHighUsed = bio->CurBufUsed;

        timer = TIMER_CB() - timer;
        bio->TotalTime += timer;
        if ( ( Working ) || ( bio->Working ) )
          bio->ThreadTime += timer;
        else
          bio->IdleTime += timer;
      }
    
      #ifdef BINK_USE_TELEMETRY
        tmLeave( bink_context );
      #endif
    }
    else
    {
      // if we can't fill anymore, then set the max size to the current size
      bio->CurBufSize = bio->CurBufUsed;
    }

    RESUME_CB( bio );
  }
  else
  {
    // if we're in idle in the background thread, do a sleep to give it more time
    IDLE_ON_CB( bio ); // let the callback run
    amt = (U32)-1;
  }

  return( amt );
}


//close the io structure
static S32 RADLINK BinkFileBGControl( BINKIO * bio, U32 control )
{
  if ( control & BINKBGIOSUSPEND )
  {
    if ( bio->Suspended == 0 )
    {
      bio->Suspended = 1;
    }
    if ( control & BINKBGIOWAIT )
    {
      SUSPEND_CB( bio );
      RESUME_CB( bio );
    }
  }
  else if ( control & BINKBGIORESUME )
  {
    if ( bio->Suspended == 1 )
    {
      bio->Suspended = 0;
    }
    if ( control & BINKBGIOWAIT )
    {
      BinkFileIdle( bio );
    }
  }
  return( bio->Suspended );
}


//opens a normal filename into an io structure
RADDEFFUNC S32 RADLINK BinkFileOpenRfs( BINKIO * bio, const char * name, U32 flags );
RADDEFFUNC S32 RADLINK BinkFileOpenRfs( BINKIO * bio, const char * name, U32 flags )
{
  BINKFILE * BF = (BINKFILE*) bio->iodata;

  #if defined(__RADNDS__)
    // on nds, frommemory and filehandle together mean this is a FSFileID
    if ( ( flags & ( BINKFROMMEMORY | BINKFILEHANDLE ) ) == ( BINKFROMMEMORY | BINKFILEHANDLE ) )
    {
      BF->FileHandle = radopenfi( (void*)name );
    }
    else
  #endif
  
  if ( flags & BINKFILEHANDLE )
  {
    #if defined(__RADWII__) || defined(__RADNDS__)
      BF->FileHandle = radopenfh( (void*)name );
    #else
      BF->FileHandle = (UINTa) name;
      BF->DontClose = 1;
    #endif  
    if ( ( flags & BINKFILEOFFSET ) == 0 )
      BF->StartFile = radseekcur64( (UINTa) name, 0 );
  }
  else
  {
    BF->FileHandle = radopen( name, RADREAD );

    #ifdef __RADNT__
    if ( RADBADHANDLE( BF->FileHandle ) )
      BF->FileHandle = radopenwithwrite( name, RADREAD );
    #endif

    if ( RADBADHANDLE( BF->FileHandle ) )
      return( 0 );
  }

  if ( flags & BINKFILEOFFSET )
  {
    BF->StartFile = ( (HBINK) ( ( ( char * ) bio ) - ( (UINTa)&((HBINK)0)->bio ) ) )->FileOffset;
    if ( BF->StartFile )
      radseekbegin64( BF->FileHandle, BF->StartFile );
  }

  bio->ReadHeader = BinkFileReadHeader;
  bio->ReadFrame = BinkFileReadFrame;
  bio->GetBufferSize = BinkFileGetBufferSize;
  bio->SetInfo = BinkFileSetInfo;
  bio->Idle = BinkFileIdle;
  bio->Close = BinkFileClose;
  bio->BGControl = BinkFileBGControl;

  return( 1 );
}


#if defined(__RADNT__) || defined(__RADXBOX__) || defined(__RADXENON__)

#pragma code_seg()
#pragma data_seg()
#pragma const_seg()
#pragma bss_seg()

#endif
