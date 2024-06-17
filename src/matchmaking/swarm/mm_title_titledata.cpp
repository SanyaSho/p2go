//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "matchext_swarm.h"
#include "steam/isteamuserstats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static TitleDataFieldsDescription_t const* PrepareTitleDataStorageDescription()
{
#define TD_ENTRY( szName, nTD, eDataType, numBytesOffset ) \
	{ \
		TitleDataFieldsDescription_t aTDFD = { szName, TitleDataFieldsDescription_t::nTD, TitleDataFieldsDescription_t::eDataType, numBytesOffset }; \
		s_tdfd.AddToTail( aTDFD ); \
	}

	static CUtlVector< TitleDataFieldsDescription_t > s_tdfd;

	// Single player progress
	TD_ENTRY("SP.progress", DB_TD1, DT_uint32, offsetof(TitleData1, uiSinglePlayerProgressChapter));

		// COOP MAPS
#define CFG( fieldname, ctx, idx, num ) \
	TD_ENTRY( "MP.complete." #fieldname,DB_TD1,	DT_BITFIELD,	8*offsetof( TitleData1, coop.mapbits ) + TitleData1::CoopData_t::fieldname )
#define CFG_DISABLED( fieldname, ctx, idx, num )
#include "xlast_portal2/inc_coop_maps.inc"
#undef CFG_DISABLED
#undef CFG

		// GAME INSTRUCTOR LESSONS
#define CFG( fieldname ) \
	TD_ENTRY( "GI.lesson." #fieldname,	DB_TD1,	DT_uint8,	offsetof( TitleData1, gameinstructor.lessoninfo[ TitleData1::GameInstructorData_t::lesson_##fieldname ] ) )
#define CFG_DISABLED( fieldname )
#include "xlast_portal2/inc_gameinstructor_lessons.inc"
#undef CFG_DISABLED
#undef CFG

		// Storage for achievements components
	uint32 uiAchievementComponentBitsUsed = 0;
#define CFG( name, compcount, ... ) \
	{ \
		COMPILE_TIME_ASSERT( (compcount == 0) || (compcount > 1) ); \
		int numAchNameChars = Q_strlen( "ACH." #name ); \
		for ( int iComponent = 0; iComponent < compcount; ++ iComponent ) \
		{ \
			char *pszComponentName = new char[ numAchNameChars + 10 ]; \
			Q_snprintf( pszComponentName, numAchNameChars + 10, "%s[%d]", "ACH." #name, iComponent+1 ); \
			TD_ENTRY( pszComponentName,	DB_TD2,	DT_BITFIELD,	8*offsetof( TitleData2, bitsAchievementsComponents ) + uiAchievementComponentBitsUsed + iComponent ) \
		} \
		uiAchievementComponentBitsUsed += 32 * STORAGE_COUNT_FOR_BITS( uint32, compcount ); \
	}
//#include "inc_achievements.inc" // TODO - No idea how this is supposed to be laid out properly so it's mostly a guess and probably a very wrong one
#undef CFG

	// Custom achievements data
	{
		char const* szAch = "ACH.SPREAD_THE_LOVE";
		int numAchNameChars = Q_strlen(szAch);
		for (int iComponent = 0; iComponent < TitleData2::kAchievement_SpreadTheLove_FriendsHuggedCount; ++iComponent)
		{
			char* pszComponentName = new char[numAchNameChars + 10];
			Q_snprintf(pszComponentName, numAchNameChars + 10, "%s[%d]", szAch, iComponent + 1);
			TD_ENTRY(pszComponentName, DB_TD2, DT_uint64, offsetof(TitleData2, ach_SpreadTheLove_FriendsHugged[iComponent]));
		}
	}
	{
		char const* szAch = "ACH.SPEED_RUN_COOP";
		int numAchNameChars = Q_strlen(szAch);
		for (int iComponent = 0; iComponent < TitleData2::kAchievement_SpeedRunCoop_QualifiedRunsCount; ++iComponent)
		{
			char* pszComponentName = new char[numAchNameChars + 10];
			Q_snprintf(pszComponentName, numAchNameChars + 10, "%s[%d]", szAch, iComponent + 1);
			TD_ENTRY(pszComponentName, DB_TD2, DT_uint16, offsetof(TitleData2, ach_SpeedRunCoop_MapsQualified[iComponent]));
		}
	}

	// remaining fields to consider:
	// COOP TAUNTS
	// system convars
	// profile-specific convars
	// game stats

#ifdef _PS3
// DLC ownership bits
#define CFG( fieldname ) \
	TD_ENTRY( "DLC." #fieldname,DB_TD2,	DT_BITFIELD,	8*offsetof( TitleData2, dlcbits ) + fieldname )
	CFG(0x12)
		CFG(0x13)
		CFG(0x14)
#undef CFG
#endif

		// END MARKER
		TD_ENTRY(NULL, DB_TD1, DT_0, 0)

#undef TD_ENTRY

	COMPILE_TIME_ASSERT(TitleData1::CoopData_t::mapbits_total >= TitleData1::CoopData_t::mapbits_last_bit_used);
	//COMPILE_TIME_ASSERT(TitleData1::CoopData_t::tauntbits_total >= TitleData1::CoopData_t::tauntbits_last_bit_used);
	COMPILE_TIME_ASSERT(TitleData1::GameInstructorData_t::lessonbits_total >= TitleData1::GameInstructorData_t::lessonbits_last_bit_used);

	return s_tdfd.Base();
}

TitleDataFieldsDescription_t const * CMatchTitle::DescribeTitleDataStorage()
{
	static TitleDataFieldsDescription_t const* s_pTDFD = PrepareTitleDataStorageDescription();
	return s_pTDFD;
}

TitleAchievementsDescription_t const * CMatchTitle::DescribeTitleAchievements()
{
	static TitleAchievementsDescription_t tad[] =
	{
#define CFG( name, compcount, ... ) \
		{ "ACH." #name, ACHIEVEMENT_##name, compcount },
//#include "inc_achievements.inc"
#undef CFG
		// END MARKER
		{ NULL, 0 }
	};

	return tad;
}

TitleAvatarAwardsDescription_t const * CMatchTitle::DescribeTitleAvatarAwards()
{
	static TitleAvatarAwardsDescription_t taad[] =
	{
#define CFG( award, ... ) { "AV_" #award, AVATARASSETAWARD_##award, "AV_TD_" #award },
//#include "inc_asset_awards.inc"
#undef CFG
		// END MARKER
		{ NULL, 0 }
	};

	return taad;
}

// Title leaderboards
KeyValues* CMatchTitle::DescribeTitleLeaderboard(char const* szLeaderboardView)
{
	// Check if this is a challenge leaderboard
	if (char const* szChallenge = StringAfterPrefix(szLeaderboardView, "challenge_besttime_"))
	{
		// 		if ( IsX360() )
		// 		{
		// 			// Find the corresponding record in the mission script
		// 			KeyValues *pSettings = new KeyValues( "settings" );
		// 			KeyValues::AutoDelete autodelete_pSettings( pSettings );
		// 			pSettings->SetString( "game/mode", "survival" );
		// 
		// 			KeyValues *pMissionInfo = NULL;
		// 			KeyValues *pMapInfo = g_pMatchExtSwarm->GetMapInfoByBspName( pSettings, szSurvivalMap, &pMissionInfo );
		// 			if ( !pMapInfo || !pMissionInfo )
		// 				return NULL;
		// 
		// 			// Find the leaderboard description in the map info
		// 			KeyValues *pLbDesc = pMapInfo->FindKey( "x360leaderboard" );
		// 			if ( !pLbDesc )
		// 				return NULL;
		// 			
		// 			// Insert the required keys
		// 			pLbDesc = pLbDesc->MakeCopy();
		// 
		// 			static KeyValues *s_pRatingKey = KeyValues::FromString( ":rating",			// X360 leaderboards are rated
		// 				" name besttime "														// game name of the rating field is "besttime"
		// 				" type uint64 "															// type is uint64
		// 				" rule max"																// rated field must be greater than cached value so that it can be written
		// 				);
		// 			pLbDesc->AddSubKey( s_pRatingKey->MakeCopy() );
		// 			pLbDesc->SetString( "besttime/type", "uint64" );
		// 
		// 			return pLbDesc;
		// 		}
		// 
		if (!IsX360())
		{
			KeyValues* pSettings = KeyValues::FromString("SteamLeaderboard",
				" :score besttime "														// :score is the leaderboard value mapped to game name "besttime"
			);

			pSettings->SetInt(":sort", k_ELeaderboardSortMethodAscending);			// Sort order when fetching and displaying leaderboard data
			pSettings->SetInt(":format", k_ELeaderboardDisplayTypeTimeMilliSeconds);	// Note: this is actually 1/100th seconds type, Steam change pending
			pSettings->SetInt(":upload", k_ELeaderboardUploadScoreMethodKeepBest);	// Upload method when writing to leaderboard

			return pSettings;
		}
	}
	if (char const* szChallenge = StringAfterPrefix(szLeaderboardView, "challenge_distance_"))
	{
		if (!IsX360())
		{
			KeyValues* pSettings = KeyValues::FromString("SteamLeaderboard",
				" :score distance "														// :score is the leaderboard value mapped to game name "distance"
			);

			pSettings->SetInt(":sort", k_ELeaderboardSortMethodAscending);			// Sort order when fetching and displaying leaderboard data
			pSettings->SetInt(":format", k_ELeaderboardDisplayTypeNumeric);			// Note: this is actually 1/100th seconds type, Steam change pending
			pSettings->SetInt(":upload", k_ELeaderboardUploadScoreMethodKeepBest);	// Upload method when writing to leaderboard

			return pSettings;
		}
	}
	if (char const* szChallenge = StringAfterPrefix(szLeaderboardView, "challenge_portals_"))
	{
		if (!IsX360())
		{
			KeyValues* pSettings = KeyValues::FromString("SteamLeaderboard",
				" :score portals "														// :score is the leaderboard value mapped to game name "portals"
			);

			pSettings->SetInt(":sort", k_ELeaderboardSortMethodAscending);			// Sort order when fetching and displaying leaderboard data
			pSettings->SetInt(":format", k_ELeaderboardDisplayTypeNumeric);			// Note: this is actually 1/100th seconds type, Steam change pending
			pSettings->SetInt(":upload", k_ELeaderboardUploadScoreMethodKeepBest);	// Upload method when writing to leaderboard

			return pSettings;
		}
	}

	return NULL;
}
