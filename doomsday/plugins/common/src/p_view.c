/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Ker�nen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "doomdef.h"
#  include "doomstat.h"
#  include "p_local.h"
#  include "d_config.h"
#  include "g_common.h"
#elif __JHERETIC__
#  include "../../jheretic/include/doomdef.h"
#  include "h_stat.h"
#  include "../../jheretic/include/p_local.h"
#  include "h_config.h"
#  include "g_common.h"
#elif __JHEXEN__
#  include "h2def.h"
#  include "p_local.h"
#  include "x_config.h"
#elif __JSTRIFE__
#  include "h2def.h"
#  include "p_local.h"
#  include "d_config.h"
#endif

// MACROS ------------------------------------------------------------------


#define VIEW_HEIGHT  (cfg.plrViewHeight << FRACBITS)

#define MAXBOB  0x100000        // 16 pixels of bob.

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Calculate the walking / running height adjustment.
 */
void P_CalcHeight(player_t *player)
{
    boolean setz = (player == &players[consoleplayer]);
    boolean airborne;
    boolean morphed = false;
    ddplayer_t *dplay = player->plr;
    mobj_t *pmo = dplay->mo;
    int     angle, bobmul, target, step;
    static int aircounter = 0;

    // Regular movement bobbing.
    // (needs to be calculated for gun swing even if not on ground)
    player->bob =
        FixedMul(pmo->momx, pmo->momx) + FixedMul(pmo->momy, pmo->momy);
    player->bob >>= 2;
    if(player->bob > MAXBOB)
        player->bob = MAXBOB;

    // When flying, don't bob the view.
    if(pmo->flags2 & MF2_FLY && pmo->pos[VZ] > pmo->floorz)
    {
        player->bob = FRACUNIT / 2;
    }

#if __JHERETIC__ || __JHEXEN__
    if(player->morphTics)
        morphed = true;
#endif

    // During demo playback the view is thought to be airborne
    // if viewheight is zero (Cl_MoveLocalPlayer).
    if(Get(DD_PLAYBACK))
        airborne = !dplay->viewheight;
    else
        airborne = pmo->pos[VZ] > pmo->floorz;    // Truly in the air?

    // Should view bobbing be done?
    if(setz)
    {
        if(P_IsCamera(dplay->mo)    // $ democam
           || player->cheats & CF_NOMOMENTUM || airborne || morphed)    // Morphed players don't bob their view.
        {
            // Reduce the bob offset to zero.
            target = 0;
        }
        else
        {
            angle = (FINEANGLES / 20 * leveltime) & FINEMASK;
            bobmul = FRACUNIT * cfg.bobView;
            target =
                FixedMul(bobmul, FixedMul(player->bob / 2, finesine[angle]));
        }
        // Do the change gradually.
        angle = Get(DD_VIEWZ_OFFSET);
        if(airborne || aircounter > 0)
            step = 0x40000 - (aircounter > 0 ? aircounter * 0x35C0 : 0x38000);
        else
            step = 0x40000;
        if(angle > target)
        {
            if(angle - target > step)
                angle -= step;
            else
                angle = target;
        }
        else if(angle < target)
        {
            if(target - angle > step)
                angle += step;
            else
                angle = target;
        }
        Set(DD_VIEWZ_OFFSET, angle);
        // The aircounter will soften the touchdown after a fall.
        aircounter--;
        if(airborne)
            aircounter = TICSPERSEC / 2;
    }

    // Should viewheight be moved? Not if camera or we're in demo.
    if(!(player->cheats & CF_NOMOMENTUM || P_IsCamera(pmo)  // $democam
         || Get(DD_PLAYBACK)))
    {
        // Move viewheight.
        if(player->playerstate == PST_LIVE)
        {
            dplay->viewheight += dplay->deltaviewheight;

            if(dplay->viewheight > VIEW_HEIGHT)
            {
                dplay->viewheight = VIEW_HEIGHT;
                dplay->deltaviewheight = 0;
            }
            if(dplay->viewheight < VIEW_HEIGHT / 2)
            {
                dplay->viewheight = VIEW_HEIGHT / 2;
                if(dplay->deltaviewheight <= 0)
                    dplay->deltaviewheight = 1;
            }
            if(dplay->deltaviewheight)
            {
                dplay->deltaviewheight += FRACUNIT / 4;
                if(!dplay->deltaviewheight)
                    dplay->deltaviewheight = 1;
            }
        }
    }

    // Set the player's eye-level Z coordinate.
    dplay->viewz = pmo->pos[VZ] + dplay->viewheight;

    // During demo playback (or camera mode) the viewz will not be
    // modified any further.
    if(!(Get(DD_PLAYBACK) || P_IsCamera(pmo)))
    {
        if(morphed)
        {
            // Chicken or pig.
            dplay->viewz -= 20 * FRACUNIT;
        }
        // Foot clipping is done for living players.
        if(player->playerstate != PST_DEAD)
        {
            // Foot clipping is done for living players.
            if(pmo->floorclip && pmo->pos[VZ] <= pmo->floorz)
            {
                dplay->viewz -= pmo->floorclip;
            }
        }
    }
}
