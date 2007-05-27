/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * Handle Sector base lighting effects.
 * Muzzle flash?
 *  2006/01/17 DJS - Recreated using jDoom's p_lights.c as a base.
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Broken light flashing.
 */
void T_LightFlash(lightflash_t *flash)
{
    float       lightlevel = P_GetFloatp(flash->sector, DMU_LIGHT_LEVEL);

    if(--flash->count)
        return;

    if(lightlevel == flash->maxlight)
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->minlight);
        flash->count = (P_Random() & flash->mintime) + 1;
    }
    else
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->maxlight);
        flash->count = (P_Random() & flash->maxtime) + 1;
    }
}

/**
 * After the map has been loaded, scan each sector
 * for specials that spawn thinkers
 */
void P_SpawnLightFlash(sector_t *sector)
{
    float       lightlevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);
    lightflash_t *flash;

    // nothing special about it during gameplay
    P_XSector(sector)->special = 0;

    flash = Z_Malloc(sizeof(*flash), PU_LEVSPEC, 0);

    P_AddThinker(&flash->thinker);

    flash->thinker.function = T_LightFlash;
    flash->sector = sector;
    flash->maxlight = lightlevel;

    flash->minlight = P_FindMinSurroundingLight(sector, lightlevel);
    flash->maxtime = 64;
    flash->mintime = 7;
    flash->count = (P_Random() & flash->maxtime) + 1;
}

/**
 * Strobe light flashing.
 */
void T_StrobeFlash(strobe_t *flash)
{
    float       lightlevel = P_GetFloatp(flash->sector, DMU_LIGHT_LEVEL);

    if(--flash->count)
        return;

    if(lightlevel == flash->minlight)
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->maxlight);
        flash->count = flash->brighttime;
    }
    else
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->minlight);
        flash->count = flash->darktime;
    }
}

/**
 * After the map has been loaded, scan each sector
 * for specials that spawn thinkers
 */
void P_SpawnStrobeFlash(sector_t *sector, int fastOrSlow, int inSync)
{
    strobe_t   *flash;
    float       lightlevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);

    flash = Z_Malloc(sizeof(*flash), PU_LEVSPEC, 0);

    P_AddThinker(&flash->thinker);

    flash->sector = sector;
    flash->darktime = fastOrSlow;
    flash->brighttime = STROBEBRIGHT;
    flash->thinker.function = T_StrobeFlash;
    flash->maxlight = lightlevel;
    flash->minlight = P_FindMinSurroundingLight(sector, lightlevel);

    if(flash->minlight == flash->maxlight)
        flash->minlight = 0;

    // nothing special about it during gameplay
    P_XSector(sector)->special = 0;

    if(!inSync)
        flash->count = (P_Random() & 7) + 1;
    else
        flash->count = 1;
}

/*
 * Start strobing lights (usually from a trigger)
 */
void EV_StartLightStrobing(line_t *line)
{
    sector_t   *sec = NULL;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
    if(!list)
        return;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_XSector(sec)->specialdata)
            continue;

        P_SpawnStrobeFlash(sec, SLOWDARK, 0);
    }
}

void EV_TurnTagLightsOff(line_t *line)
{
    int         j;
    float       min;
    float       lightlevel;
    sector_t   *sec = NULL, *tsec;
    line_t     *other;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
    if(!list)
        return;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        min = P_GetFloatp(sec, DMU_LIGHT_LEVEL);
        for(j = 0; j < P_GetIntp(sec, DMU_LINE_COUNT); ++j)
        {
            other = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | j);
            tsec = P_GetNextSector(other, sec);

            if(!tsec)
                continue;

            lightlevel = P_GetFloatp(tsec, DMU_LIGHT_LEVEL);

            if(lightlevel < min)
                min = lightlevel;
        }

        P_SetFloatp(sec, DMU_LIGHT_LEVEL, min);
    }
}

void EV_LightTurnOn(line_t *line, float max)
{
    int         j;
    float       lightlevel;
    sector_t   *sec = NULL, *tsec;
    line_t     *tline;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
    if(!list)
        return;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        // max = 0 means to search
        // for highest light level
        // surrounding sector
        if(!max)
        {
            for(j = 0; j < P_GetIntp(sec, DMU_LINE_COUNT); j++)
            {
                tline = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | j);
                tsec = P_GetNextSector(tline, sec);

                if(!tsec)
                    continue;

                lightlevel = P_GetFloatp(tsec, DMU_LIGHT_LEVEL);

                if(lightlevel > max)
                    max = lightlevel;
            }
        }

        P_SetFloatp(sec, DMU_LIGHT_LEVEL, max);
    }
}

void T_Glow(glow_t * g)
{
    float       lightlevel = P_GetFloatp(g->sector, DMU_LIGHT_LEVEL);
    float       glowdelta = (1.0f / 255.0f) * (float) GLOWSPEED;

    switch(g->direction)
    {
    case -1:
        // DOWN
        lightlevel -= glowdelta;
        if(lightlevel <= g->minlight)
        {
            lightlevel += glowdelta;
            g->direction = 1;
        }
        break;

    case 1:
        // UP
        lightlevel += glowdelta;
        if(lightlevel >= g->maxlight)
        {
            lightlevel -= glowdelta;
            g->direction = -1;
        }
        break;
    }

    P_SetFloatp(g->sector, DMU_LIGHT_LEVEL, lightlevel);
}

void P_SpawnGlowingLight(sector_t *sector)
{
    float      lightlevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);
    glow_t     *g;

    g = Z_Malloc(sizeof(*g), PU_LEVSPEC, 0);

    P_AddThinker(&g->thinker);

    g->sector = sector;
    g->minlight = P_FindMinSurroundingLight(sector, lightlevel);
    g->maxlight = lightlevel;
    g->thinker.function = T_Glow;
    g->direction = -1;

    P_XSector(sector)->special = 0;
}
