// This is the interface that Bink and Miles uses to start and stop SPU execution.

// We supply example code for SPU threads, but as long as you supply these functions,
//   we should run with anything.

#ifdef __cplusplus
extern "C" {
#endif  

// This function takes an spu number the image to load on the spus (this is a pointer
//   to a sys_spu_image_t structure.  It is up to the spu interface library to decide 
//   how to interpret the spu number - on spu threads, this is just lets you refer 
//   to one of 6 spu thread groups. On RAW, this could be the low level SPU processor 
//   numbers.

int RAD_start_spu( int spu_num, void const * spu_elf_image );


// This function sticks a command into a queue that the spu reads from.  This function
//   takes an specific spu index - not a mask (so if you passed 3 for the spu_mask,
//   then you can pass in 0 or 1 for this function).  data1 can only be non-zero values,
//   but data2 can be any value.

int RAD_send_to_spu( int spu_num, unsigned int data1, unsigned int data2 );


// This function waits for the specified period of time for data returning from the
//   spu. "us" is a microsecond count, or zero for no wait (just try/fail), or a negative
//   number which means wait forever.  The data parameters return the spu data, but 
//   the pointers can be null if you don't want the values.  Only 24-bits of data2 are 
//   received with SPU threads - other interfaces may not have this restriction.

int RAD_receive_from_spu( int spu_num, int us, unsigned int * data1, unsigned int * data2_24bits );


// This function tells the specified spu to stop.  This happens asynchronously, so
//   you should call RAD_wait_stop_spu to pause until the spu exits.  This command
//   lets you begin multiple spus stopping, and then wait for them after.

int RAD_stop_spu( int spu_num );


// This function waits for a previous spu stop command to complete (call RAD_stop_spu
//   first).

int RAD_wait_stop_spu( int spu_num );


#if __PPU // only used to link in some symbols on ps3
int get_spu_writable_ls_pattern( rage::u64[2], void const* ); 
void or_ls_pattern( rage::u64[2], int, int );

void get_spu_section_info( void**, void**, rage::u32*, void const*, const char* );

rage::u32 get_ls_pattern_size(rage::u64[2]);

void* radmalloc( rage::u32 bytes );
void radfree( void * ptr );
#endif // __PPU

#ifdef __cplusplus
}
#endif  
