#ifndef RFILEH
#define RFILEH

#define USE_RAGE_PGSTREAMER 1

// @cdep pre $set(INCs,$INCs -I$clipfilename($file))
#if USE_RAGE_PGSTREAMER
	#include "paging/streamer.h"

	#define RADBADHANDLE(h)		((h) == rage::pgStreamer::Error)

	#define RADREAD  1
	#define RADWRITE 0

	namespace rage { 
		struct binkRageHandle {
			static const int maxBinkOpen = 4;
			static binkRageHandle theHandles[maxBinkOpen];

			u64 seekPos;
			u32 fileSize;
			pgStreamer::Handle handle;
			sysIpcSema sema;
			
			binkRageHandle() : handle(pgStreamer::Error) { }

			static UINTa Open(const char* name,bool /*ro*/) {
				int i;
				for (i=0; i<maxBinkOpen; i++)
					if (theHandles[i].handle == pgStreamer::Error)
						break;
				if (i == maxBinkOpen) {
					Errorf("Can only have %d bink files open at a time in current impl", maxBinkOpen);
					return pgStreamer::Error;
				}
				char buf[128];
				if ((theHandles[i].handle = pgStreamer::Open(formatf(buf,"%s.bik",name),&theHandles[i].fileSize,0)) == pgStreamer::Error)
					return pgStreamer::Error;
				theHandles[i].seekPos = 0;
				theHandles[i].sema = sysIpcCreateSema(false);
				return (UINTa)(uintptr_t)&theHandles[i];
			}
			U32 Read(void *dest,U32 toRead) {
				if (seekPos + toRead > fileSize) {
					Warningf("Read past end of file, truncated from %u to %" SIZETFMT "u",toRead,(size_t)(fileSize - seekPos));
					toRead = (u32)(fileSize - seekPos);
				}
				datResourceChunk list = { 0, dest, toRead };
				// The read can fail if the queue is full, so keep retrying periodically.
				while (pgStreamer::Read(handle,&list,1,(u32)seekPos,sema,pgStreamer::CRITICAL) == NULL)
					sysIpcSleep(10);
				seekPos += toRead;
				sysIpcWaitSema(sema);
				return toRead;
			}
			void Close() {
				pgStreamer::Close(handle);
				sysIpcDeleteSema(sema);
				handle = pgStreamer::Error;	// this also marks the handle slot free
			}
		};
	}
	#define radopen(name, acc)				(rage::binkRageHandle::Open((name),(bool)(acc)))
	#define radclose(handle)				(((rage::binkRageHandle*)(handle))->Close())
	#define radseekbegin64(handle, off)		((((rage::binkRageHandle*)handle))->seekPos = (off))
	#define radseekcur64(handle, off)		((((rage::binkRageHandle*)handle))->seekPos += (off))
	#define radread(handle, dest, size, ar)	((*(ar)) = ((rage::binkRageHandle*)(handle))->Read((dest), (size)))
	#define radopenwithwrite radopen

#elif __RFS__

	#include "file/asset.h"

	#define RADBADHANDLE(h) ( (h) == NULL )

	#define RADREAD  1
	#define RADWRITE 0

	#define RFS_SEEK_BEG 0
	#define RFS_SEEK_CUR 1
	#define RSF_SEEK_END 2

	#define radopen(name, acc)					((UINTa)(rage::ASSET.Open(name, "bik", false, (bool)acc)))
	#define radcreate(name)						((UINTa)(rage::fiStream::Create(name)))
	#define radclose(stream)            		((rage::fiStream*)stream)->Close();

	#define radseekbegin64(stream, offset) 		rfsSeek64((rage::fiStream*)(stream), offset, RFS_SEEK_BEG)
	#define radseekcur64(stream, offset)   		rfsSeek64((rage::fiStream*)(stream), offset, RFS_SEEK_CUR)
	#define radseekend64(stream, offset)   		rfsSeek64((rage::fiStream*)(stream), offset, RSF_SEEK_END)

	#define radseekbegin(stream, offset)		rfsSeek((rage::fiStream*)(stream), offset, RFS_SEEK_BEG)
	#define radseekcur(stream, offset)			rfsSeek((rage::fiStream*)(stream), offset, RFS_SEEK_CUR)
	#define radseekend(stream, offset)			rfsSeek((rage::fiStream*)(stream), offset, RSF_SEEK_END)

	#define radread(stream, dest, size, ar)		*(ar) = ((rage::fiStream*)stream)->Read(dest, size)
	#define radwrite(stream, src, size, ar)		*(ar) = ((rage::fiStream*)stream)->Write(src, size)

	#define radcreatewithread radcreate
	#define radopenwithwrite radopen

	static U32 RADINLINE rfsSeek(rage::fiStream* stream, U32 offset, U32 method)
	{
		U32 destPos = (U32)(S32)-1;

		switch (method)
		{
			case RFS_SEEK_BEG:
				destPos = (U32)offset;
				break;
			case RFS_SEEK_CUR:
				destPos = (U32)(stream->Tell() + offset);
				break;
			case RSF_SEEK_END:
				destPos = (U32)(stream->Size() + offset);
				break;
			default:
				destPos = (U32)stream->Tell();
		}

		return (U32)stream->Seek(destPos);
	}

	static U64 RADINLINE rfsSeek64(rage::fiStream* stream, U64 offset, U32 method)
	{
		U64 destPos = (U64)(S64)(S32)-1;

		switch (method)
		{
		case RFS_SEEK_BEG:
			destPos = (U64)offset;
			break;
		case RFS_SEEK_CUR:
			destPos = (U64)(stream->Tell() + offset);
			break;
		case RSF_SEEK_END:
			destPos = (U64)(stream->Size() + offset);
			break;
		default:
			destPos = (U64)stream->Tell();
		}

		return (U64)stream->Seek64(destPos);
	}
	
#elif defined(__RAD3DS__) || defined(__RADWII__) || defined(__RADNDS__)

  #define RADBADHANDLE(h) ( h == 0 )
  #define RADREAD 0
  #define RADWRITE 2

  RADDEFSTART
    U32  radopen(const char* fn,U32 acc);
    #if defined(__RADWII__) || defined(__RADNDS__)
      U32  radopenfh(void* fh);
      #ifdef __RADNDS__
        U32 radopenfi( void * fi ); //FSFileID
        S32 raddone( U32 hand, S32 until_done ); // only nds is async
      #endif
    #endif
    U32  radread(U32 hand,void* dest,U32 bytes,U32* amtread);
    U32  radseekbegin(U32 hand,U32 pos);
    U32  radseekcur(U32 hand,S32 pos);
    U32  radseekend(U32 hand,U32 pos);
    U64  radseekbegin64(U32 hand,U64 pos);
    U64  radseekcur64(U32 hand,S64 pos);
    U64  radseekend64(U32 hand,U64 pos);
    void radclose(U32 hand);
  RADDEFEND

#elif ( defined(__RADNT__) || defined(__RADXBOX__) || defined(__RADXENON__) ) && !defined(__RADNTBUILDLINUX__)

#if defined(__RADXBOX__) || defined(__RADXENON__)
  #undef S8
  //#include <xtl.h>
  #include "system/xtl.h"
  #define S8 signed char
#else
	#include "system/xtl.h"
/*
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #include <windows.h>
*/
#endif

#define RADBADHANDLE(h) ( (HANDLE)(h) == INVALID_HANDLE_VALUE )

#define RADREAD GENERIC_READ
#define RADWRITE (GENERIC_READ|GENERIC_WRITE)

#define radopen(name,acc) ((UINTa)CreateFile(name,acc,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,0))
#define radopenwithwrite(name,acc) ((UINTa)CreateFile(name,acc,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,0))
#define radcreate(name)   ((UINTa)CreateFile(name,RADWRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,0))
#define radcreatewithread(name) ((UINTa)CreateFile(name,RADWRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,0))
#define radread(hand,buf,toread,amtread) ReadFile((HANDLE)hand,buf,toread,(DWORD*)amtread,0)
#define radwrite(hand,buf,toread,amtread) WriteFile((HANDLE)hand,buf,toread,(DWORD*)amtread,0)
#define radclose(hand) CloseHandle((HANDLE)hand)
#define radseekbegin(hand,pos) SetFilePointer((HANDLE)hand,pos,0,FILE_BEGIN)
#define radseekcur(hand,pos) SetFilePointer((HANDLE)hand,pos,0,FILE_CURRENT)
#define radseekend(hand,pos) SetFilePointer((HANDLE)hand,pos,0,FILE_END)
#define radseekbegin64(hand,pos) radseek64((HANDLE)hand,pos,FILE_BEGIN)
#define radseekcur64(hand,pos) radseek64((HANDLE)hand,pos,FILE_CURRENT)
#define radseekend64(hand,pos) radseek64((HANDLE)hand,pos,FILE_END)
#define raderase DeleteFile

#define radfiledone()
#define radreadopen(dummy)
#define radreadclose()

static U64 RADINLINE radseek64( HANDLE h, U64 offset, DWORD method )
{
  U32 posb,post;

  // this api is nuts
  post = (U32) ( offset >> 32 );
  posb = SetFilePointer( h, (U32) offset, (PLONG)&post, method );

  if ( ( posb == 0xffffffff ) && ( GetLastError() != NO_ERROR ) )
    return( (U64)(S64)(S32) -1 );

  return( ( (U64) posb ) | ( ( (U64) post ) << 32 ) );
}

#elif defined(__RADPS3__) || defined(__RADMACH__)

  #include <fcntl.h>
  #include <unistd.h>

  #define RADBADHANDLE(h) ( (int)(h) == -1 )

  #define FARx86
  #define RADREAD  O_RDONLY
  #define RADWRITE O_RDWR

  #define radopen(name,acc)           ((UINTa)open(name,acc))
  #define radcreate(name)             ((UINTa)open(name,O_CREAT|O_WRONLY|O_TRUNC))
  #define radclose(file)              close((int)(file))
  
  #ifdef __RADMACH__
    //macos and freebsd lseeks are natively 64-bit
    #define radseekbegin64 radseekbegin
    #define radseekcur64 radseekcur
    #define radseekend64 radseekend
  #else
    #define radseekbegin64(file,offset) lseek64((int)(file),offset,SEEK_SET)
    #define radseekcur64(file,offset)   lseek64((int)(file),offset,SEEK_CUR)
    #define radseekend64(file,offset)   lseek64((int)(file),offset,SEEK_END)
  #endif
  
  #define radseekbegin(file,offset)   lseek((int)(file),offset,SEEK_SET)
  #define radseekcur(file,offset)     lseek((int)(file),offset,SEEK_CUR)
  #define radseekend(file,offset)     lseek((int)(file),offset,SEEK_END)
  #define radread(file,addr,size,ar)  *(ar)=read((int)(file),addr,size)
  #define radwrite(file,addr,size,ar) *(ar)=write((int)(file),addr,size)

#elif defined(__RADPSP__)

  #include <iofilemgr.h>

  #define RADBADHANDLE(h) ( (SceUID)(h) < 0 )

  #define RADREAD  SCE_O_RDONLY
  #define RADWRITE SCE_O_RDWR

  #define radopen(name,acc)           ((UINTa)sceIoOpen( name,acc,0 ) )
  #define radclose(hand)              sceIoClose( (SceUID) hand )
  #define radseekbegin64(hand,offset) sceIoLseek( (SceUID) hand,offset,SCE_SEEK_SET)
  #define radseekcur64(hand,offset)   sceIoLseek( (SceUID) hand,offset,SCE_SEEK_CUR)
  #define radseekend64(hand,offset)   sceIoLseek( (SceUID) hand,offset,SCE_SEEK_END)
  #define radseekbegin(hand,offset)   sceIoLseek( (SceUID) hand,offset,SCE_SEEK_SET)
  #define radseekcur(hand,offset)     sceIoLseek( (SceUID) hand,offset,SCE_SEEK_CUR)
  #define radseekend(hand,offset)     sceIoLseek( (SceUID) hand,offset,SCE_SEEK_END)
  #define radread(hand,addr,size,ar)  *(ar)=sceIoRead((SceUID)(hand),addr,size)

#else

  #define radcreatewithread radcreate
  #define radopenwithwrite radopen

  #define RADBADHANDLE(h) ( (int)(h) == -1 )

  #ifdef __RADMAC__  // old school mac non-mach
    #include <OpenTransport.h>
    #include <files.h>
    #include <string.h>
    #include <gestalt.h>

    #define FARx86
    #define RADREAD fsRdPerm
    #define RADWRITE fsRdWrPerm

    U32 radfsopen(const FSSpec* fsp, U32 acc);
    U32 RADASMLINK radreadasyncsize( void );
    U32 RADASMLINK radreadasync(U32 hand,void* dest,U32 bytes,void* backinfo);
    U32 RADASMLINK radreadasyncdone( void* backinfo,U32 * amtread );

  #else

    #ifdef __RADDOS__
      #define FARx86 far
    #else
      #define FARx86
    #endif

    #define RADREAD 0
    #define RADWRITE 2

  #endif

  RADDEFSTART
    U32  RADASMLINK radopen(const char FARx86* fn,U32 acc);
    U32  RADASMLINK radcreate(const char FARx86* fn);
    U32  RADASMLINK radread(U32 hand,void FARx86* dest,U32 bytes,U32* amtread);
    U32  RADASMLINK radwrite(U32 hand,void FARx86* dest,U32 bytes,U32* amtread);
    U32  RADASMLINK radseekbegin(U32 hand,U32 pos);
    U32  RADASMLINK radseekcur(U32 hand,U32 pos);
    U32  RADASMLINK radseekend(U32 hand,U32 pos);
    U64  RADASMLINK radseekbegin64(U32 hand,U64 pos);
    U64  RADASMLINK radseekcur64(U32 hand,U64 pos);
    U64  RADASMLINK radseekend64(U32 hand,U64 pos);
    void RADASMLINK radclose(U32 hand);
    U32  RADASMLINK radgetfiletime(U32 hand);
    void RADASMLINK radsetfiletime(U32 hand,U32 time);
    U8   RADASMLINK IsRemote(U32 file);
    S32  RADASMLINK raderase(const char FARx86* delfile);
    S32  RADASMLINK radrename(const char FARx86* destfile,const char FARx86* srcfile);

    #ifdef __RADWINEXT__
      void RADASMLINK radfiledone();
      void RADASMLINK radreadopen(char FARx86* dummy);
      void RADASMLINK radreadclose();
    #else
      #define radfiledone()
      #define radreadopen(dummy)
      #define radreadclose()
    #endif
  RADDEFEND

  #endif

#endif
