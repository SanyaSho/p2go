//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "../mm_title_main.h"

#include "matchext_swarm.h"
#include <matchmaking/portal2/imatchext_portal2.h>

#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


LINK_MATCHMAKING_LIB();


static CMatchTitle g_MatchTitle;
CMatchTitle *g_pMatchTitle = &g_MatchTitle;

IMatchTitle *g_pIMatchTitle = g_pMatchTitle;
IMatchEventsSink *g_pIMatchTitleEventsSink = g_pMatchTitle;



//
// Init / shutdown
//

InitReturnVal_t MM_Title_Init()
{
	return g_pMatchTitle->Init();
}

void MM_Title_Shutdown()
{
	if ( g_pMatchTitle )
		g_pMatchTitle->Shutdown();
}

uint64 CMatchTitle::GetTitleSettingsFlags()
{
	return MATCHTITLE_SETTING_MULTIPLAYER
		| MATCHTITLE_VOICE_INGAME
		| MATCHTITLE_PLAYERMGR_ALLFRIENDS
#if !defined( CSTRIKE15 )
		| MATCHTITLE_SETTING_NODEDICATED
		| MATCHTITLE_PLAYERMGR_DISABLED
		| MATCHTITLE_SERVERMGR_DISABLED
		| MATCHTITLE_INVITE_ONLY_SINGLE_USER
#else
		| MATCHTITLE_PLAYERMGR_FRIENDREQS
#endif // !CSTRIKE15
		;
}

TitleDlcDescription_t const* CMatchTitle::DescribeTitleDlcs()
{
	static TitleDlcDescription_t tdlcs[] =
	{
		{ PORTAL2_DLCID_COOP_BOT_SKINS,		PORTAL2_DLC_APPID_COOP_BOT_SKINS,	PORTAL2_DLC_PKGID_COOP_BOT_SKINS,	"DLC.0x12" },
		{ PORTAL2_DLCID_COOP_BOT_HELMETS,	PORTAL2_DLC_APPID_COOP_BOT_HELMETS,	PORTAL2_DLC_PKGID_COOP_BOT_HELMETS,	"DLC.0x13" },
		{ PORTAL2_DLCID_COOP_BOT_ANTENNA,	PORTAL2_DLC_APPID_COOP_BOT_ANTENNA,	PORTAL2_DLC_PKGID_COOP_BOT_ANTENNA,	"DLC.0x14" },
		// END MARKER
		{ 0, 0, 0 }
	};

	return tdlcs;
}
