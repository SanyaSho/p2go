#ifndef __BINK_INCLUDE_H
#define __BINK_INCLUDE_H

#if RSG_DURANGO
#define _SEKRIT
#elif RSG_ORBIS
#define _SEKRIT2
#endif

#if RSG_DURANGO
	#define __USE_BINK_2		// DEFINE this to use bink2
#elif RSG_ORBIS
	#define __USE_BINK_2		// DEFINE this to use bink2
#elif RSG_PC
	#define __USE_BINK_2		// DEFINE this to use bink2
#endif


#ifdef	__USE_BINK_2
	#define RSG_BINK_2_7D				(0)				// use Bink 2.7d on Durango only (B*6701325 - Update Xbox Bink libraries to latest)
														// disabled because of B*6818853 (Crash in Rockstar Editor during two specific clips of the Arena Workshop);

	#if RSG_BINK_2_7D
		#include "bink2/bink_2_7d.h"					// Bink 2.7d Header
	#else
		#include "bink2/bink.h"							// Bink 2.4a Header
	#endif
#else	//__USE_BINK_2
	#define RSG_BINK_2_7D				(0)
	#include "bink.h"									// Bink 1 Header
#endif	//__USE_BINK_2

#endif	//__BINK_INCLUDE_H
