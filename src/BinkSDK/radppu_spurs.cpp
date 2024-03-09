#include "radppu.h"
//#include "spuimageutils.h"

#if __PPU

#include <stdio.h>
#include <sys/event.h>
#include <cell/spurs.h>
#include <sys/timer.h>

#define MAX_SPU_TASKS 6


static int loaded_on = 0;
static int waiting_on_stop = 0;

typedef struct QUEUE
{
  CellSpursEventFlag * event;
  volatile uint32_t to_spu_count;
  void * spu_reload_address;
  void * ppu_reload_address;
  uint32_t reload_size;
  volatile uint32_t to_spu[ 27 ]; // only use 16 entries (padded out to a cacheline)

  volatile uint32_t from_spu_count;
  volatile uint32_t from_spu[ 31 ];
} QUEUE;

static QUEUE __attribute__ ((aligned (128))) queues[ MAX_SPU_TASKS ];
static volatile uint32_t ppu_handled_count[ MAX_SPU_TASKS ];
static CellSpursTaskset sets[ MAX_SPU_TASKS ];
static CellSpursTaskId ids[ MAX_SPU_TASKS ];
static void * context[ MAX_SPU_TASKS ];
static CellSpursEventFlag events[ MAX_SPU_TASKS ];
static CellSpursTaskLsPattern patterns[ MAX_SPU_TASKS ];

#ifdef MSS_HOST_SPU_PROCESS
extern void * AIL_mem_alloc_lock_info( uint32_t bytes, char const * file, uint32_t line );
extern void AIL_mem_free_lock( void * ptr );
#define radmalloc(bytes) AIL_mem_alloc_lock_info(bytes,__FILE__,__LINE__)
#define radfree AIL_mem_free_lock
#else
extern void * radmalloc( uint32_t bytes );
extern void radfree( void * ptr );
#endif

typedef struct RAD_SPURS_INFO
{
  CellSpurs * spurs;      
  void const * elf_image;
} RAD_SPURS_INFO;

char * RAD_spu_error = 0;

int RAD_start_spu( int spu_num, void const * spurs_info )
{
  // 1 is the highest priority :
  uint8_t prios[8] = {0, 1, 1, 1, 1, 1, 1, 1};
  CellSpursTaskSaveConfig save;
  CellSpursTasksetAttribute tsattr;
  CellSpursTaskAttribute attr;
  RAD_SPURS_INFO * info;

  RAD_spu_error = 0;

  if ( spu_num >= MAX_SPU_TASKS )
  {
    RAD_spu_error = "Too many raw tasks.";
    return 0;
  }
  
  info = (RAD_SPURS_INFO*) spurs_info;

  if ( loaded_on & ( 1 << spu_num ) )
  {
    RAD_spu_error = "Already using this spu number.";
    return 0;
  }

  ppu_handled_count[ spu_num ] = 0;
  queues[ spu_num ].to_spu_count = 0;
  queues[ spu_num ].from_spu_count = 0;
  
  cellSpursTasksetAttributeInitialize ( &tsattr, (uint32_t) &queues[ spu_num ], prios, 1 );
  cellSpursTasksetAttributeSetName( &tsattr, "RAD Game Tools Task" );

  if ( cellSpursCreateTasksetWithAttribute( info->spurs, &sets[ spu_num ], &tsattr ) != CELL_OK )
  {
    RAD_spu_error = "cellSpursCreateTaskset failed.";
    return 0;
  }

  if ( cellSpursEventFlagInitialize( &sets[ spu_num ], &events[ spu_num ], CELL_SPURS_EVENT_FLAG_CLEAR_AUTO, CELL_SPURS_EVENT_FLAG_PPU2SPU ) != CELL_OK )
  {
    RAD_spu_error = "cellSpursEventFlagInitialize failed.";
   err: 
    cellSpursShutdownTaskset( &sets[ spu_num ] );
    return 0 ;
  }

  queues[ spu_num ].event = &events[ spu_num ];

  if ( get_spu_writable_ls_pattern( patterns[ spu_num ].u64, info->elf_image ) == 0 )
  {
    RAD_spu_error = "Bad SPU image. Make sure to specify ""--strip-mode=normal"" if using spu_elf-to-ppu_obj!";
    goto err;
  }
    
  or_ls_pattern( patterns[ spu_num ].u64, 254*1024, 256*1024 );

  get_spu_section_info( &queues[ spu_num ].spu_reload_address, &queues[ spu_num ].ppu_reload_address, &queues[ spu_num ].reload_size, info->elf_image, ".reload" );
  queues[ spu_num ].spu_reload_address = (void*) ( ( ( (unsigned int) queues[ spu_num ].spu_reload_address ) + 15 ) & ~15 );
  queues[ spu_num ].ppu_reload_address = (void*) ( ( ( (unsigned int) queues[ spu_num ].ppu_reload_address ) + 15 ) & ~15 );
  queues[ spu_num ].reload_size = ( queues[ spu_num ].reload_size - 16 ) & ~15;

  save.sizeContext = get_ls_pattern_size( patterns[ spu_num ].u64 );

  context[ spu_num ] = radmalloc( save.sizeContext + 128 );
  if ( context[ spu_num ] == 0 )
  {
    RAD_spu_error = "Memory allocation for task backing store failed.";
    goto err;
  }

  save.lsPattern = &patterns[ spu_num ];
  save.eaContext = (void*) ( ( ( (uint32_t) context[ spu_num ] ) + 127 ) & ~127 );

  if ( cellSpursTaskAttributeInitialize( &attr, info->elf_image, &save, 0 ) != CELL_OK )
  {
    radfree( context[ spu_num ] );
    context[ spu_num ] = 0;
    RAD_spu_error = "cellSpursTaskAttributeInitialize failed.";
    goto err;
  }

  __asm__ volatile ("lwsync");

  if ( cellSpursCreateTaskWithAttribute( &sets[ spu_num ], &ids[ spu_num ], &attr ) != CELL_OK )
  {
    radfree( context[ spu_num ] );
    context[ spu_num ] = 0;
    RAD_spu_error = "cellSpursCreateTaskWithAttribute failed.";
    goto err;
  } 

  loaded_on |= ( 1 << spu_num );

  return 1;
}

static int check_spu_num( int spu_num )
{
  if ( (unsigned int) spu_num >= (unsigned int) MAX_SPU_TASKS )
  {
    RAD_spu_error = "Bad spu task index.";
    return 0;
  }

  if ( ( loaded_on & ( 1 << spu_num ) ) == 0 )
  {
    RAD_spu_error = "Invalid spu number.";
    return 0;
  }

  return 1;
}  


int RAD_send_to_spu( int spu_num, unsigned int data1, unsigned int data2 )
{
  uint32_t count;

  RAD_spu_error = 0;

  if ( check_spu_num( spu_num ) == 0 )
    return 0;

  if ( data1 == 0 )  // data1 == 0 is reserved to shutdown thread
    return 0;

  count = queues[ spu_num ].to_spu_count & 15;

  queues[ spu_num ].to_spu[ count ] = data1;
  queues[ spu_num ].to_spu[ count + 1 ] = data2;
  __asm__ volatile ("lwsync");
  queues[ spu_num ].to_spu_count += 2;
  __asm__ volatile ("lwsync");

  cellSpursEventFlagSet( &events[ spu_num ], 0xffff );
  return 1;
}


int RAD_receive_from_spu( int spu_num, int us, unsigned int * data1, unsigned int * data2_24bits )
{
  //sys_event_t event;
  int time = 0;

  if ( check_spu_num( spu_num ) == 0 )
    return 0;

  for(;;)
  {
    if ( queues[ spu_num ].from_spu_count != ppu_handled_count[ spu_num ] )
    {
      uint32_t count;

      count = ppu_handled_count[ spu_num ] & 15;

      if ( data1 )
        *data1 = queues[ spu_num ].from_spu[ count ];
      
      if ( data2_24bits )
        *data2_24bits = queues[ spu_num ].from_spu[ count + 1 ];

      ppu_handled_count[ spu_num ] += 2;

      return 1;
    }

    if ( ( us == 0 ) || ( ( us != -1 ) && ( time >=us ) ) )
      break;

    // sleep for 400 us
    sys_timer_usleep( 400 );
    time += 400;
  }

  return 0;    
}


int RAD_stop_spu( int spu_num )
{
  uint32_t count;
  
  RAD_spu_error = 0;

  if ( check_spu_num( spu_num ) == 0 )
    return 0;

  count = queues[ spu_num ].to_spu_count & 15;

  // send a zero command down
  queues[ spu_num ].to_spu[ count ] = 0;
  queues[ spu_num ].to_spu[ count + 1 ] = 0;
  __asm__ volatile ("lwsync");
  queues[ spu_num ].to_spu_count += 2;
  __asm__ volatile ("lwsync");

  cellSpursEventFlagSet( &events[ spu_num ], 0xffff );

  loaded_on &= ~( 1 << spu_num );
  waiting_on_stop |= ( 1 << spu_num );

  return 1;
}


int RAD_wait_stop_spu( int spu_num )
{
  //int cause, status;

  RAD_spu_error = 0;

  if ( (unsigned int) spu_num >= (unsigned int) MAX_SPU_TASKS )
  {
    RAD_spu_error = "Bad spu task index.";
    return 0;
  }

  if ( ( waiting_on_stop & ( 1 << spu_num ) ) == 0 )
  {
    RAD_spu_error = "A stop command hasn't been queued on this spu number.";
    return 0;
  }

  // close spu
  cellSpursShutdownTaskset( &sets[ spu_num ] );

  // wait until the spurs task exits
  cellSpursJoinTaskset( &sets[ spu_num ] );

  radfree( context[ spu_num ] );
  context[ spu_num ] = 0;

  waiting_on_stop &= ~( 1 << spu_num );

  return 1;
}

#endif // __PPU

/**
@cdep pre
   
   $requires(spuimageutils.c)
   $requires(ansi/radmem.c)
   
**/
