#ifndef BINKGPU_H
#define BINKGPU_H

// All this stuff is part of the internal BinkGPU API,
// and some of it may end up getting compiled into shaders!

#define BINKGPUMAXEDGE      16384 // Max width or height of a BinkGPU video. Must be a multiple of 32.
#define BINKGPUMAXBLOCKS32  (BINKGPUMAXEDGE/32)
#define BINKGPUMAXBLOCKS8   (BINKGPUMAXEDGE/8)

// "Flags" field for BINKGPUDATABUFFERS
#define BINKGPUDEBLOCKUD    0x0001
#define BINKGPUDEBLOCKLR    0x0002
#define BINKGPUNEWCLAMP     0x0004

#define BINKGPUPLANE_Y      0
#define BINKGPUPLANE_CR     1
#define BINKGPUPLANE_CB     2
#define BINKGPUPLANE_A      3

typedef struct BINKGPUMOVEC
{
  S16 rel_x;
  S16 rel_y;
} BINKGPUMOVEC;

typedef struct BINKGPURANGE
{
  U32 Offset;   // in bytes
  U32 Size;     // in bytes
} BINKGPURANGE;

// typedef is already in bink.h
struct BINKGPUDATABUFFERS
{
  // Set up by BinkGetGPUDataBuffersInfo
  U32 BufferWidth;
  U32 BufferHeight;
  U8 const * ScanOrderTable;
  U32 AcBufSize[ BINKMAXPLANES ]; // in bytes

  // Needs to be set up before call to DoFrame
  void * Dc[ BINKMAXPLANES ];
  void * Ac[ BINKMAXPLANES ];
  BINKGPURANGE * AcRanges[ BINKMAXPLANES ]; // One AC range per slice, per plane.
  BINKGPUMOVEC * Motion;
  U8 * TopFlag;
  U8 * DeblockUdFlag;
  U8 * DeblockLrFlag;

  // Filled out by DoFrame.
  U32 DcThreshold;
  U32 Flags;

  // Old clamp: (tl_fullpel,wh_fullpel,tl_halfpel,wh_halfpel)
  // New clamp: (min_fullpel,max_fullpel,min_halfpel,max_halfpel)
  S32 MotionXClamp[4];
  S32 MotionYClamp[4];
};

#endif
