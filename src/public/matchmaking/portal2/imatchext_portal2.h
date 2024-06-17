//===== Copyright c 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IMATCHEXT_PORTAL2_H
#define IMATCHEXT_PORTAL2_H

#ifdef _WIN32
#pragma once
#pragma warning( push )
#pragma warning( disable : 4201 )
#endif

#define STORAGE_COUNT_FOR_BITS( aStorageType, numBits ) ( ( (numBits) + 8*sizeof( aStorageType ) - 1 ) / ( 8* sizeof( aStorageType ) ) )

#define MATCHMAKINGDATA_FIELD(name) short name [MMDATA_TYPE_COUNT][MMDATA_SCOPE_COUNT];

// MatchmakingData
// This is the data structure used for matchmaking.  Any fields addsed to this structure
// need to be added to all of the appropriate areas where we calculate averages and 
// serialize this data.
struct MatchmakingData
{
};

#undef MATCHMAKINGDATA_FIELD

// Disabled these for p2asw because we don't have the include files
// and most of the code using these is console only anyway

//
//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData1 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
struct TitleData1
{
	uint32 uiSinglePlayerProgressChapter;

	struct CoopData_t
	{
		enum MapBits_t
		{
#define CFG( fieldname, ctx, idx, num ) fieldname,
#define CFG_DISABLED( fieldname, ctx, idx, num ) CFG( fieldname )
#include "xlast_portal2/inc_coop_maps.inc"
#undef CFG_DISABLED
#undef CFG
			mapbits_last_bit_used,
			mapbits_total_basegame = 42,
			mapbits_total = 160 // leave room for total 160 maps
		};
		uint32 mapbits[ STORAGE_COUNT_FOR_BITS( uint32, mapbits_total ) ];

//		enum TauntBits_t
//		{
//#define CFG( fieldname ) taunt_##fieldname,
//#define CFG_DISABLED( fieldname ) CFG( fieldname )
//#include "xlast_portal2/inc_coop_taunts.inc"
//#undef CFG_DISABLED
//#undef CFG
//			tauntbits_last_bit_used,
//			tauntbits_total = 48 // leave room for total 48 taunts
//		};
//		uint32 tauntbitsOwned[ STORAGE_COUNT_FOR_BITS( uint32, tauntbits_total ) ];
//		uint32 tauntbitsUsed[ STORAGE_COUNT_FOR_BITS( uint32, tauntbits_total ) ];
//		enum TauntEquipSlots_t
//		{
//#define CFG( fieldname ) taunt_equipslot_##fieldname,
//#define CFG_DISABLED( fieldname ) CFG( fieldname )
//#include "xlast_portal2/inc_coop_taunts_equipslots.inc"
//#undef CFG_DISABLED
//#undef CFG
//			taunt_equipslots_total
//		};
//		uint8 tauntsEquipSlots[taunt_equipslots_total];
	};
	CoopData_t coop;

	struct GameInstructorData_t
	{
		enum LessonsBits_t
		{
#define CFG( fieldname ) lesson_##fieldname,
#define CFG_DISABLED( fieldname ) CFG( fieldname )
#include "xlast_portal2/inc_gameinstructor_lessons.inc"
#undef CFG_DISABLED
#undef CFG
			lessonbits_last_bit_used,
			lessonbits_total = 48 // leave room for total 48 lessons
		};

		union LessonInfo_t
		{
			uint8 u8dummy;
			struct
			{
				uint8 display : 4;
				uint8 success : 4;
			};
		} lessoninfo[ lessonbits_total ];
	};
	GameInstructorData_t gameinstructor;
};

//
//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData2 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
struct TitleData2
{
	// Achievement component bits
//	enum AchievementBits_t
//	{
//		kAchievementComponentTotalCount = 0
//#define CFG( name, compcount, ... ) \
//		+ STORAGE_COUNT_FOR_BITS( uint32, compcount )
//#include "xlast_portal2/inc_achievements.inc" TODO!
//#undef CFG
//	};
//	uint32 bitsAchievementsComponents[ kAchievementComponentTotalCount ];

	// Add a padding for future progress achievement bits
	uint32 bitsAchievementFuture[ 64 ];

	// Awards bitfields
//	enum AwardBits_t
//	{
//#define CFG( award, ... ) bitAward##award,
//#include "xlast_portal2/inc_asset_awards.inc"
//#undef CFG
//		bitAward_last_bit_used,
//		bitAwards_total = 32 // leave room for total 32 awards
//	};
//	uint32 awardbits[ STORAGE_COUNT_FOR_BITS( uint32, bitAwards_total ) ];

	// Custom achievements data
	enum { kAchievement_SpreadTheLove_FriendsHuggedCount = 3 };
	uint64 ach_SpreadTheLove_FriendsHugged[ kAchievement_SpreadTheLove_FriendsHuggedCount ];
	enum { kAchievement_SpeedRunCoop_QualifiedRunsCount = 3 };
	uint16 ach_SpeedRunCoop_MapsQualified[ kAchievement_SpeedRunCoop_QualifiedRunsCount ];

	// Add a padding for future custom achievements data
	uint32 bitsAchievementDataFuture[ 64 ];

	// DLC ownership bits
	uint32 dlcbits[2];

	// imatchext_cstrike15.h probably not needed
	//uint16 versionNumber;
};

//
//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData3 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
// struct TitleData3
// {
// 	uint64 unused; // unused, free for taking
// };
//
//
//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData3 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
//struct TitleData3
//{
//	uint32 version;
//
//	struct ConVarsSystem_t
//	{
//#define CFG( name, scfgType, cppType, ... ) cppType name;
//#include "xlast_portal2/inc_gameconsole_settings_sys.inc"
//#undef CFG
//		uint8  unused_uint8[2];
//		uint32 unused_values[9];
//
//		enum Bits_bitfields_t
//		{
//#define CFG( name ) name,
//#include "xlast_portal2/inc_gameconsole_settings_sys_bits.inc"
//#undef CFG
//			bit_last
//		};
//		uint32 bitfields[ STORAGE_COUNT_FOR_BITS( uint32, bit_last ) ];
//
//		uint32 unused[10];
//	};
//	ConVarsSystem_t cvSystem;
//
//	struct ConVarsUser_t
//	{
//#define CFG( name, scfgType, cppType ) cppType name;
//#include "xlast_portal2/inc_gameconsole_settings_usr.inc"
//#undef CFG
//		uint32 unused_values[8];
//
//		enum Bits_bitfields_t
//		{
//#define CFG( name ) name,
//#include "xlast_portal2/inc_gameconsole_settings_usr_bits.inc"
//#undef CFG
//			bit_last
//		};
//		uint32 bitfields[ STORAGE_COUNT_FOR_BITS( uint32, bit_last ) ];
//
//		uint32 unused[10];
//	};
//	ConVarsUser_t cvUser;
//	ConVarsUser_t cvUserSS;
//
//	struct GameStats_t
//	{
//#define CFG( name, scfgType, cppType ) cppType name;
//#include "xlast_portal2/inc_gamestats.inc"
//#undef CFG
//		uint32 unused_values[50];
//	};
//	GameStats_t gamestats;
//
//};

//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData3 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
struct TitleData3 // CSGO's version copied here and cleaned up so imatchext_cstrike15 doesn't need to be included anymore.
{
	uint32 version;

	uint16 versionNumber;

	struct ConVarsSystem_t
	{
		uint32 unused_values[10];
	};
	ConVarsSystem_t cvSystem;

	struct ConVarsUser_t
	{
	};
	ConVarsUser_t cvUser;
	ConVarsUser_t cvUserSS;

	struct JoystickBindings_t
	{
#undef BINDING
#undef ACTION
	};
	JoystickBindings_t JoystickBindings;

	struct usrMMData_t
	{
	};
	usrMMData_t usrMMData;

};

// FIXME: Shouldn't hardcode mp_coop_lobby_3 here, need to handle mp_coop_start
// too once co-op progress saving is reimplemented in a different way.
#define PORTAL2_LOBBY_CONFIG_COOP( szNetwork, szAccess ) \
		" system { " \
			" network " szNetwork " " \
			" access " szAccess " " \
		" } " \
		" game { " \
			" mode coop " \
			" mission mp_coop_lobby_3 " \
		" } "


#define PORTAL2_DLCID_RETAIL_DLC1			( 1ull << 0x01 )
#define PORTAL2_DLCID_RETAIL_DLC2			( 1ull << 0x02 )
#define PORTAL2_DLCID_COOP_BOT_SKINS		( 1ull << 0x12 )
#define PORTAL2_DLCID_COOP_BOT_HELMETS		( 1ull << 0x13 )
#define PORTAL2_DLCID_COOP_BOT_ANTENNA		( 1ull << 0x14 )
#define PORTAL2_DLC_ALLMASK ( PORTAL2_DLCID_RETAIL_DLC1 | PORTAL2_DLCID_RETAIL_DLC2 | PORTAL2_DLCID_COOP_BOT_SKINS | PORTAL2_DLCID_COOP_BOT_HELMETS | PORTAL2_DLCID_COOP_BOT_ANTENNA )

#define PORTAL2_DLC_APPID_COOP_BOT_SKINS		651
#define PORTAL2_DLC_APPID_COOP_BOT_HELMETS		652
#define PORTAL2_DLC_APPID_COOP_BOT_ANTENNA		653

#define PORTAL2_DLC_PKGID_COOP_BOT_SKINS		7364
#define PORTAL2_DLC_PKGID_COOP_BOT_HELMETS		7365
#define PORTAL2_DLC_PKGID_COOP_BOT_ANTENNA		7366

#define PORTAL2_DLC_PKGID_PCSTEAMPLAY			7397


#ifdef _WIN32
#pragma warning( pop )
#endif
#endif // IMATCHEXT_PORTAL2_H