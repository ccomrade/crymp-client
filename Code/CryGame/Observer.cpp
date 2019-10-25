/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 9:6:2004: Created by Filippo De Luca
  - 15:8:2005: Renamed CDrone to CObserver by Mikko Mononen

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "Observer.h"
#include "CryCommon/GameUtils.h"

#include "CryAction/IViewSystem.h"
#include "CryAction/IItemSystem.h"
#include "CryCommon/IPhysics.h"
#include "CryCommon/ICryAnimation.h"
#include "CryCommon/ISerialize.h"
#include "CryCommon/IRenderAuxGeom.h"
