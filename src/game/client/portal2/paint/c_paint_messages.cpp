//===== Copyright 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "igamesystem.h"
#include "hud_macros.h" //HOOK_MESSAGE
#include "paint/paint_color_manager.h"
#include "c_world.h"
#include "paint/paint_sprayer_shared.h"
#include "paint/paintable_entity.h"
#include "cdll_int.h"
#include "portal2_usermessages.pb.h"
#ifdef PORTAL2
#include "c_weapon_paintgun.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#ifdef GAME_DLL
#include "../../../server/util.h"
#endif

bool __MsgFunc_PaintWorld( const CUsrMsg_PaintWorld &msg )
{
	// if client is local to active server, don't do paint in client
	// because the paintmap is shared in the same engine
	if( !engine->IsClientLocalToActiveServer() )
	{
		// Get the color index and number of paint locations
		PaintPowerType power = static_cast< PaintPowerType >( msg.paintpower() );
		C_BaseEntity* pBrushEntity = C_BaseEntity::Instance(msg.brushentity());
		float flPaintRadius = msg.paintradius();
		float flAlphaPercent = msg.alphapercent();

		int nPaintCount = msg.paintcount();

		// Get the center point
		Vector vCenter;
		vCenter.x = msg.centerx();
		vCenter.y = msg.centery();
		vCenter.z = msg.centerz();

		// For each offset
		Vector vContactPoint;
		for( int i = 0; i < nPaintCount; ++i )
		{
			// Compute the position
			vContactPoint.x = vCenter.x + msg.contactpointx();
			vContactPoint.y = vCenter.y + msg.contactpointy();
			vContactPoint.z = vCenter.z + msg.contactpointz();

			UTIL_PaintBrushEntity( pBrushEntity, vContactPoint, power, flPaintRadius, flAlphaPercent, NULL );
		}
	}
	return true;
}


bool __MsgFunc_PaintEntity( const CUsrMsg_PaintEntity &msg )
{
	IPaintableEntity* pPaintableEnt = dynamic_cast<IPaintableEntity*>(C_BaseEntity::Instance(msg.paintableent()));

	const PaintPowerType power = static_cast< PaintPowerType >( msg.paintpower() );
	Vector pos;
	pos.x = msg.posx();
	pos.y = msg.posy();
	pos.z = msg.posz();

	pPaintableEnt->Paint( power, pos );
	return true;
}


bool __MsgFunc_ChangePaintColor( const CUsrMsg_ChangePaintColor &msg )
{
#ifdef PORTAL2
	// get entity
	C_BaseEntity* pEntity = C_BaseEntity::Instance(msg.entity());

	if ( pEntity == NULL )
	{
		DevMsg("Failed to change paint color");
		return true;
	}

	C_WeaponPaintGun *pPaintGun = dynamic_cast< C_WeaponPaintGun* >( pEntity );
	if ( pPaintGun )
	{
		int power = msg.paintpower();

		pPaintGun->SetSubType( power );
		pPaintGun->ChangeRenderColor( true );
	}
#endif
	return true;
}


bool __MsgFunc_RemoveAllPaint( const CUsrMsg_RemoveAllPaint &msg )
{
	if( !engine->IsClientLocalToActiveServer() )
	{
		engine->RemoveAllPaint();
	}
	return true;
}


bool __MsgFunc_PaintAllSurfaces( const CUsrMsg_PaintAllSurfaces &msg )
{
	
	if( !engine->IsClientLocalToActiveServer() )
	{
		BYTE power = msg.paintpower();
		engine->PaintAllSurfaces( power );
	}
	return true;
}


bool __MsgFunc_RemovePaint( const CUsrMsg_RemovePaint &msg )
{
	
	if( engine->IsClientLocalToActiveServer() )
		return true;

//	C_BaseEntity *pEntity = UTIL_EntityFromUserMessageEHandle( msg.entity() );
	C_BaseEntity* pEntity = C_BaseEntity::Instance(msg.entity());


	if ( pEntity && pEntity->IsBSPModel() )
	{
		engine->RemovePaint( pEntity->GetModel() );
	}
	return true;
}


// This class is to hook message for all usermessages in Paint
class C_PaintInitHelper : public CAutoGameSystem
{
	CUserMessageBinder m_UMCMsgPaintWorld;
	CUserMessageBinder m_UMCMsgPaintEntity;
	CUserMessageBinder m_UMCMsgChangePaintColor;
	CUserMessageBinder m_UMCMsgRemoveAllPaint;
	CUserMessageBinder m_UMCMsgPaintAllSurfaces;
	CUserMessageBinder m_UMCMsgRemovePaint;
	virtual bool Init()
	{
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
			HOOK_MESSAGE( PaintWorld );
			HOOK_MESSAGE( PaintEntity );
			HOOK_MESSAGE( ChangePaintColor );
			HOOK_MESSAGE( RemoveAllPaint );
			HOOK_MESSAGE( PaintAllSurfaces );
			HOOK_MESSAGE( RemovePaint );
		}

		return true;
	}
};
static C_PaintInitHelper s_PaintInitHelper;
