// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//  Sprite animation.
//
//-----------------------------------------------------------------------------


#ifndef __P_PSPR__
#define __P_PSPR__

// Basic data types.
// Needs fixed point, and BAM angles.
#include "tables.h"


//
// Needs to include the precompiled
//  sprite animation tables.
// Header generated by multigen utility.
// This includes all the data for thing animation,
// i.e. the Thing Atrributes table
// and the Frame Sequence table.
#include "info.h"

#ifdef __GNUG__
#pragma interface
#endif


//
// Frame flags:
// handles maximum brightness (torches, muzzle flare, light sources)
//
#define FF_FULLBRIGHT	0x8000	// flag in thing->frame
#define FF_FRAMEMASK	0x7fff



//
// Overlay psprites are scaled shapes
// drawn directly on the view screen,
// coordinates are given for a 320*200 view screen.
//
typedef enum
{
    ps_weapon,
    ps_flash,
    NUMPSPRITES

} psprnum_t;

typedef struct
{
    state_t*	state;	// a NULL state means not active
    int		tics;
    fixed_t	sx;
    fixed_t	sy;

} pspdef_t;

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.3  2004/01/07 20:44:40  skyjake
// Merged from branch-nix
//
// Revision 1.2.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2  2003/02/27 23:14:31  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:18:36  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:13  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------

