/**\file p_setup.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * Common map setup routines.
 *
 * Management of extended map data objects (eg xlines) is done here
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <ctype.h>  // has isspace
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "hu_stuff.h"
#elif __JHEXEN__
#  include "jhexen.h"
#  include "p_start.h"
#endif

#include "p_actor.h"
#include "dmu_lib.h"
#include "r_common.h"
#include "p_mapsetup.h"
#include "am_map.h"
#include "p_tick.h"
#include "p_start.h"
#include "hu_pspr.h"

// MACROS ------------------------------------------------------------------

#if __JDOOM64__
# define TOLIGHTIDX(c) (!((c) >> 8)? 0 : ((c) - 0x100) + 1)
#endif

// TYPES -------------------------------------------------------------------

typedef struct {
    int             gameModeBits;
    mobjtype_t      type;
} mobjtype_precachedata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void     P_ResetWorldState(void);
static void     P_FinalizeMap(void);
static void     P_PrintMapBanner(uint episode, uint map);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Our private map data structures
xsector_t* xsectors;
xline_t* xlines;

// If true we are in the process of setting up a map
boolean mapSetup;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Converts a line to an xline.
 */
xline_t* P_ToXLine(linedef_t* line)
{
    if(!line)
        return NULL;

    // Is it a dummy?
    if(P_IsDummy(line))
    {
        return P_DummyExtraData(line);
    }
    else
    {
        return &xlines[P_ToIndex(line)];
    }
}

xline_t* P_GetXLine(uint idx)
{
    if(idx >= numlines) return NULL;
    return &xlines[idx];
}

void P_SetLinedefAutomapVisibility(int player, uint lineIdx, boolean visible)
{
    linedef_t* line = P_ToPtr(DMU_LINEDEF, lineIdx);
    xline_t* xline;
    if(NULL == line || P_IsDummy(line)) return;

    // Will we need to rebuild one or more display lists?
    xline = P_ToXLine(line);
    if(xline->mapped[player] != visible)
        ST_RebuildAutomap(player);
    xline->mapped[player] = visible;
}

/**
 * Converts a sector to an xsector.
 */
xsector_t* P_ToXSector(sector_t* sector)
{
    if(!sector)
        return NULL;

    // Is it a dummy?
    if(P_IsDummy(sector))
    {
        return P_DummyExtraData(sector);
    }
    else
    {
        return &xsectors[P_ToIndex(sector)];
    }
}

/**
 * Given a subsector - find its parent xsector.
 */
xsector_t* P_ToXSectorOfSubsector(subsector_t* sub)
{
    sector_t*           sec;

    if(!sub)
        return NULL;

    sec = P_GetPtrp(sub, DMU_SECTOR);

    // Is it a dummy?
    if(P_IsDummy(sec))
    {
        return P_DummyExtraData(sec);
    }
    else
    {
        return &xsectors[P_ToIndex(sec)];
    }
}

xsector_t* P_GetXSector(uint index)
{
    if(index >= numsectors)
        return NULL;

    return &xsectors[index];
}

/**
 * Doomsday calls this (before any data is read) for each type of map object
 * at the start of the map load process. This is to allow us (the game) to
 * do any initialization we need. For example if we maintain our own data
 * for lines (the xlines) we'll do all allocation and init here.
 *
 * @param type          (DMU object type) The id of the data type being setup.
 * @param num           The number of elements of "type" Doomsday is creating.
 */
void P_SetupForMapData(int type, uint num)
{
    switch(type)
    {
    case DMU_SECTOR:
        if(num > 0)
            xsectors = Z_Calloc(num * sizeof(xsector_t), PU_MAP, 0);
        else
            xsectors = NULL;
        break;

    case DMU_LINEDEF:
        if(num > 0)
            xlines = Z_Calloc(num * sizeof(xline_t), PU_MAP, 0);
        else
            xlines = NULL;
        break;

    default:
        break;
    }
}

#if __JDOOM64__
static void getSurfaceColor(uint idx, float rgba[4])
{
    if(!idx)
    {
        rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1;
    }
    else
    {
        rgba[0] = P_GetGMOFloat(MO_LIGHT, idx-1, MO_COLORR);
        rgba[1] = P_GetGMOFloat(MO_LIGHT, idx-1, MO_COLORG);
        rgba[2] = P_GetGMOFloat(MO_LIGHT, idx-1, MO_COLORB);
        rgba[3] = 1;
    }
}

typedef struct applysurfacecolorparams_s {
    sector_t*       frontSec;
    float           topColor[4];
    float           bottomColor[4];
} applysurfacecolorparams_t;

int applySurfaceColor(void* obj, void* context)
{
#define LDF_NOBLENDTOP      32
#define LDF_NOBLENDBOTTOM   64
#define LDF_BLEND           128

#define LTF_SWAPCOLORS      4

    linedef_t*          li = (linedef_t*) obj;
    applysurfacecolorparams_t* params =
        (applysurfacecolorparams_t*) context;
    byte                dFlags =
        P_GetGMOByte(MO_XLINEDEF, P_ToIndex(li), MO_DRAWFLAGS);
    byte                tFlags =
        P_GetGMOByte(MO_XLINEDEF, P_ToIndex(li), MO_TEXFLAGS);

    if((dFlags & LDF_BLEND) &&
       params->frontSec == P_GetPtrp(li, DMU_FRONT_SECTOR))
    {
        sidedef_t*          side = P_GetPtrp(li, DMU_SIDEDEF0);

        if(side)
        {
            int                 flags;
            float*              top, *bottom;

            top = (tFlags & LTF_SWAPCOLORS)? params->bottomColor :
                params->topColor;
            bottom = (tFlags & LTF_SWAPCOLORS)? params->topColor :
                params->bottomColor;

            P_SetFloatpv(side, DMU_TOP_COLOR, top);
            P_SetFloatpv(side, DMU_BOTTOM_COLOR, bottom);

            flags = P_GetIntp(side, DMU_FLAGS);
            if(!(dFlags & LDF_NOBLENDTOP))
                flags |= SDF_BLENDTOPTOMID;
            if(!(dFlags & LDF_NOBLENDBOTTOM))
                flags |= SDF_BLENDBOTTOMTOMID;

            P_SetIntp(side, DMU_FLAGS, flags);
        }
    }

    if((dFlags & LDF_BLEND) &&
       params->frontSec == P_GetPtrp(li, DMU_BACK_SECTOR))
    {
        sidedef_t*          side = P_GetPtrp(li, DMU_SIDEDEF1);

        if(side)
        {
            int                 flags;
            float*              top, *bottom;

            top = /*(tFlags & LTF_SWAPCOLORS)? params->bottomColor :*/
                params->topColor;
            bottom = /*(tFlags & LTF_SWAPCOLORS)? params->topColor :*/
                params->bottomColor;

            P_SetFloatpv(side, DMU_TOP_COLOR, top);
            P_SetFloatpv(side, DMU_BOTTOM_COLOR, bottom);

            flags = P_GetIntp(side, DMU_FLAGS);
            if(!(dFlags & LDF_NOBLENDTOP))
                flags |= SDF_BLENDTOPTOMID;
            if(!(dFlags & LDF_NOBLENDBOTTOM))
                flags |= SDF_BLENDBOTTOMTOMID;

            P_SetIntp(side, DMU_FLAGS, flags);
        }
    }

    return 1; // Continue iteration
}
#endif

static boolean checkMapSpotSpawnFlags(const mapspot_t* spot)
{
#if __JHEXEN__
    static unsigned int classFlags[] = {
        MSF_FIGHTER,
        MSF_CLERIC,
        MSF_MAGE
    };
#endif

    // Don't spawn things flagged for Multiplayer if we're not in a netgame.
    if(!IS_NETGAME && (spot->flags & MSF_NOTSINGLE))
        return false;

    // Don't spawn things flagged for Not Deathmatch if we're deathmatching.
    if(deathmatch && (spot->flags & MSF_NOTDM))
        return false;

    // Don't spawn things flagged for Not Coop if we're coop'in.
    if(IS_NETGAME && !deathmatch && (spot->flags & MSF_NOTCOOP))
        return false;

    // Check for appropriate skill level.
    if(!(spot->skillModes & (1 << gameSkill)))
        return false;

#if __JHEXEN__
    // Check current character classes with spawn flags.
    if(IS_NETGAME == false)
    {   // Single player.
        if((spot->flags & classFlags[cfg.playerClass[0]]) == 0)
        {   // Not for current class.
            return false;
        }
    }
    else if(deathmatch == false)
    {   // Cooperative.
        int i, spawnMask = 0;

        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                spawnMask |= classFlags[cfg.playerClass[i]];
            }
        }

        // No players are in the game when a dedicated server is started.
        // In this case, we'll be generous and spawn stuff for all the
        // classes.
        if(!spawnMask)
        {
            spawnMask |= MSF_FIGHTER | MSF_CLERIC | MSF_MAGE;
        }

        if((spot->flags & spawnMask) == 0)
        {
            return false;
        }
    }
#endif

    return true;
}

/**
 * Should we auto-spawn one or more mobjs from the specified map spot?
 */
static boolean checkMapSpotAutoSpawn(const mapspot_t* spot)
{
#if __JHERETIC__
    // Ambient sound sequence activator?
    if(spot->doomEdNum >= 1200 && spot->doomEdNum < 1300)
        return false;
#elif __JHEXEN__
    // Sound sequence origin?
    if(spot->doomEdNum >= 1400 && spot->doomEdNum < 1410)
        return false;
#endif

    // The following are currently handled by special-case spawn logic elsewhere.
    switch(spot->doomEdNum)
    {
    case 1: // Player starts 1 through 4.
    case 2:
    case 3:
    case 4:
    case 11: // Player start (deathmatch).
#if __JHERETIC__
    case 56: // Boss spot.
    case 2002: // Mace spot.
#endif
#if __JHEXEN__
    case 3000: // Polyobj origins.
    case 3001:
    case 3002:
    case 9100: // Player starts 5 through 8.
    case 9101:
    case 9102:
    case 9103:
#endif
        return false;
    default:
        break;
    }

    // So far so good. Now check the flags to make the final decision.
    return checkMapSpotSpawnFlags(spot);
}

static void initXLineDefs(void)
{
    uint i;

    for(i = 0; i < numlines; ++i)
    {
        xline_t* xl = &xlines[i];

        xl->flags = P_GetGMOShort(MO_XLINEDEF, i, MO_FLAGS) & ML_VALID_MASK;
#if __JHEXEN__
        xl->special = P_GetGMOByte(MO_XLINEDEF, i, MO_TYPE);
        xl->arg1 = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG0);
        xl->arg2 = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG1);
        xl->arg3 = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG2);
        xl->arg4 = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG3);
        xl->arg5 = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG4);
#else
# if __JDOOM64__
        xl->special = P_GetGMOByte(MO_XLINEDEF, i, MO_TYPE);
# else
        xl->special = P_GetGMOShort(MO_XLINEDEF, i, MO_TYPE);
# endif
        xl->tag = P_GetGMOShort(MO_XLINEDEF, i, MO_TAG);
#endif
    }
}

static void initXSectors(void)
{
    uint i;

    for(i = 0; i < numsectors; ++i)
    {
        xsector_t* xsec = &xsectors[i];

        xsec->special = P_GetGMOShort(MO_XSECTOR, i, MO_TYPE);
        xsec->tag = P_GetGMOShort(MO_XSECTOR, i, MO_TAG);

#if __JDOOM64__
        {
        applysurfacecolorparams_t params;
        float rgba[4];
        sector_t* sec = P_ToPtr(DMU_SECTOR, i);

        getSurfaceColor(TOLIGHTIDX(
            P_GetGMOShort(MO_XSECTOR, i, MO_FLOORCOLOR)), rgba);
        P_SetFloatpv(sec, DMU_FLOOR_COLOR, rgba);

        getSurfaceColor(TOLIGHTIDX(
            P_GetGMOShort(MO_XSECTOR, i, MO_CEILINGCOLOR)), rgba);
        P_SetFloatpv(sec, DMU_CEILING_COLOR, rgba);

        // Now set the side surface colors.
        params.frontSec = sec;
        getSurfaceColor(TOLIGHTIDX(
            P_GetGMOShort(MO_XSECTOR, i, MO_WALLTOPCOLOR)),
                          params.topColor);
        getSurfaceColor(TOLIGHTIDX(
            P_GetGMOShort(MO_XSECTOR, i, MO_WALLBOTTOMCOLOR)),
                          params.bottomColor);

        P_Iteratep(sec, DMU_LINEDEF, &params, applySurfaceColor);
        }
#endif
    }
}

static void loadMapSpots(void)
{
    uint i;

    numMapSpots = P_CountGameMapObjs(MO_THING);
    if(numMapSpots > 0)
        mapSpots = Z_Malloc(numMapSpots * sizeof(mapspot_t), PU_MAP, 0);
    else
        mapSpots = NULL;

    for(i = 0; i < numMapSpots; ++i)
    {
        mapspot_t* spot = &mapSpots[i];

        spot->pos[VX] = P_GetGMOFloat(MO_THING, i, MO_X);
        spot->pos[VY] = P_GetGMOFloat(MO_THING, i, MO_Y);
        spot->pos[VZ] = P_GetGMOFloat(MO_THING, i, MO_Z);

        spot->doomEdNum = P_GetGMOInt(MO_THING, i, MO_DOOMEDNUM);
        spot->skillModes = P_GetGMOInt(MO_THING, i, MO_SKILLMODES);
        spot->flags = P_GetGMOInt(MO_THING, i, MO_FLAGS);
        spot->angle = P_GetGMOAngle(MO_THING, i, MO_ANGLE);

#if __JHEXEN__
        spot->tid = P_GetGMOShort(MO_THING, i, MO_ID);
        spot->special = P_GetGMOByte(MO_THING, i, MO_SPECIAL);
        spot->arg1 = P_GetGMOByte(MO_THING, i, MO_ARG0);
        spot->arg2 = P_GetGMOByte(MO_THING, i, MO_ARG1);
        spot->arg3 = P_GetGMOByte(MO_THING, i, MO_ARG2);
        spot->arg4 = P_GetGMOByte(MO_THING, i, MO_ARG3);
        spot->arg5 = P_GetGMOByte(MO_THING, i, MO_ARG4);
#endif

#if __JHERETIC__
        // Ambient sound sequence activator?
        if(spot->doomEdNum >= 1200 && spot->doomEdNum < 1300)
        {
            P_AddAmbientSfx(spot->doomEdNum - 1200);
            continue;
        }
#elif __JHEXEN__
        // Sound sequence origin?
        if(spot->doomEdNum >= 1400 && spot->doomEdNum < 1410)
        {
            subsector_t* ssec = R_PointInSubsector(spot->pos[VX], spot->pos[VY]);
            xsector_t* xsector = P_ToXSector(P_GetPtrp(ssec, DMU_SECTOR));

            xsector->seqType = spot->doomEdNum - 1400;
            continue;
        }
#endif

        switch(spot->doomEdNum)
        {
        case 11: // Player start (deathmatch).
            P_CreatePlayerStart(0, 0, true, i);
            break;

        case 1: // Player starts 1 through 4.
        case 2:
        case 3:
        case 4:
            {
#if __JHEXEN__
            uint entryPoint = spot->arg1;
#else
            uint entryPoint = 0;
#endif

            P_CreatePlayerStart(spot->doomEdNum, entryPoint, false, i);
            break;
            }

#if __JHERETIC__
        case 56: // Boss spot.
            P_AddBossSpot(i);
            break;

        case 2002:
            if(gameMode != heretic_shareware)
                P_AddMaceSpot(i);
            break;
#endif

#if __JHEXEN__
        case 3000: // Polyobj origins.
        case 3001:
        case 3002:
            break;

        case 9100: // Player starts 5 through 8.
        case 9101:
        case 9102:
        case 9103:
            P_CreatePlayerStart(5 + spot->doomEdNum - 9100, spot->arg1, false, i);
            break;
#endif
        default: // No special handling.
            break;
        }
    }

    P_DealPlayerStarts(0);
}

static void spawnMapObjects(void)
{
    uint i;

    for(i = 0; i < numMapSpots; ++i)
    {
        const mapspot_t* spot = &mapSpots[i];
        mobjtype_t type;

        // Not all map spots spawn mobjs on map load.
        if(!checkMapSpotAutoSpawn(spot))
            continue;

        // A spot that should auto-spawn one (or more) mobjs.

        // Find which type to spawn.
        if((type = P_DoomEdNumToMobjType(spot->doomEdNum)) != MT_NONE)
        {   // A known type; spawn it!
            mobj_t* mo;
/*#if _DEBUG
Con_Message("spawning x:[%g, %g, %g] angle:%i ednum:%i flags:%i\n",
        spot->pos[VX], spot->pos[VY], spot->pos[VZ], spot->angle,
        spot->doomedNum, spot->flags);
#endif*/

            if((mo = P_SpawnMobj3fv(type, spot->pos, spot->angle,
                                    spot->flags)))
            {
                if(mo->tics > 0)
                    mo->tics = 1 + (P_Random() % mo->tics);

#if __JHEXEN__
                mo->tid = spot->tid;
                mo->special = spot->special;
                mo->args[0] = spot->arg1;
                mo->args[1] = spot->arg2;
                mo->args[2] = spot->arg3;
                mo->args[3] = spot->arg4;
                mo->args[4] = spot->arg5;
#endif

#if __JHEXEN__
                if(mo->flags2 & MF2_FLOATBOB)
                    mo->special1 = FLT2FIX(spot->pos[VZ]);
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
                if(mo->flags & MF_COUNTKILL)
                    totalKills++;
                if(mo->flags & MF_COUNTITEM)
                    totalItems++;
#endif
            }
        }
        else
        {
            Con_Message("spawnMapThing: Warning, unknown thing num %i "
                        "at [%g, %g, %g].\n", spot->doomEdNum,
                        spot->pos[VX], spot->pos[VY], spot->pos[VZ]);
        }
    }

#if __JHERETIC__
    if(maceSpotCount)
    {
        // Sometimes doesn't show up if not in deathmatch.
        if(!(!deathmatch && P_Random() < 64))
        {
            const mapspot_t* spot = &mapSpots[maceSpots[P_Random() % maceSpotCount]];

            P_SpawnMobj3f(MT_WMACE, spot->pos[VX], spot->pos[VY], 0,
                          spot->angle, MSF_Z_FLOOR);
        }
    }
#endif

#if __JHEXEN__
    P_CreateTIDList();
#endif

    P_SpawnPlayers();
}

typedef struct setupmapparams_s {
    uint            episode;
    uint            map;
    int             playerMask; // Unused?
    skillmode_t     skill;
} setupmapparams_t;

int P_SetupMapWorker(void* ptr)
{
    setupmapparams_t*   param = ptr;
    char                mapID[9];

    // It begins...
    mapSetup = true;

    // The engine manages polyobjects, so reset the count.
    DD_SetInteger(DD_POLYOBJ_COUNT, 0);
    P_ResetWorldState();

    // Let the engine know that we are about to start setting up a map.
    R_SetupMap(DDSMM_INITIALIZE, 0);

    // Initialize The Logical Sound Manager.
    S_MapChange();

    Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);

    P_MapId(param->episode, param->map, mapID);
    if(!P_LoadMap(mapID))
    {
        Con_Error("P_SetupMap: Failed loading map \"%s\".\n", mapID);
    }

    DD_InitThinkers();
#if __JHERETIC__
    P_InitAmbientSound();
#endif
#if __JHEXEN__
    P_InitCorpseQueue();
#endif

    initXLineDefs();
    initXSectors();
    loadMapSpots();

    spawnMapObjects();

#if __JHEXEN__
    PO_InitForMap();

    Con_Message("Load ACS scripts\n");
    // \fixme Should be interpreted by the map converter.
    P_LoadACScripts(W_GetLumpNumForName(mapID) + 11 /*ML_BEHAVIOR*/); // ACS object code
#endif

    HU_UpdatePsprites();

    // Set up world state.
    P_SpawnSpecials();

#if __JHEXEN__
    // Initialize the sky.
    P_InitSky(param->map);
#endif

    // Preload graphics.
    if(precache)
    {
#if __JDOOM__
        static const mobjtype_precachedata_t types[] = {
            { GM_ANY,   MT_SKULL },
            // Missiles:
            { GM_ANY,   MT_BRUISERSHOT },
            { GM_ANY,   MT_TROOPSHOT },
            { GM_ANY,   MT_HEADSHOT },
            { GM_ANY,   MT_ROCKET },
            { GM_ANY^GM_DOOM_SHAREWARE, MT_PLASMA },
            { GM_ANY^GM_DOOM_SHAREWARE, MT_BFG },
            { GM_DOOM2, MT_ARACHPLAZ },
            { GM_DOOM2, MT_FATSHOT },
            // Potentially dropped weapons:
            { GM_ANY,   MT_CLIP },
            { GM_ANY,   MT_SHOTGUN },
            { GM_ANY,   MT_CHAINGUN },
            // Misc effects:
            { GM_DOOM2, MT_FIRE },
            { GM_ANY,   MT_TRACER },
            { GM_ANY,   MT_SMOKE },
            { GM_DOOM2, MT_FATSHOT },
            { GM_ANY,   MT_BLOOD },
            { GM_ANY,   MT_PUFF },
            { GM_ANY,   MT_TFOG }, // Teleport FX.
            { GM_ANY,   MT_EXTRABFG },
            { GM_ANY,   MT_ROCKETPUFF },
            { 0,        0}
        };
        uint                i;
#endif

        R_PrecachePSprites();

#if __JDOOM__
        for(i = 0; types[i].type != 0; ++i)
            if(types[i].gameModeBits & gameModeBits)
                R_PrecacheMobjNum(types[i].type);

        if(IS_NETGAME)
            R_PrecacheMobjNum(MT_IFOG);
#endif
    }

    if(IS_SERVER)
        R_SetAllDoomsdayFlags();

    P_FinalizeMap();

    // Someone may want to do something special now that the map has been
    // fully set up.
    R_SetupMap(DDSMM_FINALIZE, 0);

    P_PrintMapBanner(param->episode, param->map);

    // It ends.
    mapSetup = false;

    Con_BusyWorkerEnd();
    return 0;
}

/**
 * Loads map and glnode data for the requested episode and map.
 */
void P_SetupMap(uint episode, uint map, int playerMask, skillmode_t skill)
{
    setupmapparams_t  param;

    param.episode = episode;
    param.map = map;
    param.playerMask = playerMask; // Unused?
    param.skill = skill;

    DD_Executef(true, "texreset raw"); // Delete raw images to save memory.

    // \todo Use progress bar mode and update progress during the setup.
    Con_Busy(BUSYF_ACTIVITY | /*BUSYF_PROGRESS_BAR |*/ BUSYF_TRANSITION | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
             "Loading map...", P_SetupMapWorker, &param);

    R_SetupMap(DDSMM_AFTER_BUSY, 0);

#if __JHEXEN__
    {
    int i;
    // Load colormap and set the fullbright flag
    i = P_GetMapFadeTable(map);
    if(i == W_GetLumpNumForName("COLORMAP"))
    {
        // We don't want fog in this case.
        GL_UseFog(false);
    }
    else
    {
        // Probably fog ... don't use fullbright sprites
        if(i == W_GetLumpNumForName("FOGMAP"))
        {
            // Tell the renderer to turn on the fog.
            GL_UseFog(true);
        }
    }
    }
#endif
}

/**
 * Called during map setup when beginning to load a new map.
 */
static void P_ResetWorldState(void)
{
    int                 i, parm;

#if __JDOOM__ || __JDOOM64__
    wmInfo.maxFrags = 0;
    wmInfo.parTime = -1;

    // Only used with 666/7 specials
    bossKilled = false;
#endif

#if __JDOOM__
    P_BrainInitForMap();
#endif

#if __JHERETIC__
    maceSpotCount = 0;
    maceSpots = NULL;
    bossSpotCount = 0;
    bossSpots = NULL;
#endif

    P_PurgeDeferredSpawns();

#if !__JHEXEN__
    totalKills = totalItems = totalSecret = 0;
#endif

    timerGame = 0;
    if(deathmatch)
    {
        parm = ArgCheck("-timer");
        if(parm && parm < Argc() - 1)
        {
            timerGame = atoi(Argv(parm + 1)) * 35 * 60;
        }
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];
        plr->killCount = plr->secretCount = plr->itemCount = 0;
    }

#if __JDOOM__ || __JDOOM64__
    bodyQueueSlot = 0;
#endif

    P_DestroyPlayerStarts();

    mapTime = actualMapTime = 0;
}

/**
 * Do any map finalization including any game-specific stuff.
 */
static void P_FinalizeMap(void)
{
#if __JDOOM__
    // Adjust slime lower wall textures (a hack!).
    // This will hide the ugly green bright line that would otherwise be
    // visible due to texture repeating and interpolation.
    if(!(gameModeBits & (GM_DOOM2_HACX|GM_DOOM_CHEX)))
    {
        uint i, k;
        material_t* mat = P_ToPtr(DMU_MATERIAL, Materials_IndexForName(MN_TEXTURES_NAME":NUKE24"));
        material_t* bottomMat, *midMat;
        float yoff;
        sidedef_t* sidedef;
        linedef_t* line;

        for(i = 0; i < numlines; ++i)
        {
            line = P_ToPtr(DMU_LINEDEF, i);

            for(k = 0; k < 2; ++k)
            {
                sidedef = P_GetPtrp(line, k == 0? DMU_SIDEDEF0 : DMU_SIDEDEF1);
                if(sidedef)
                {
                    bottomMat = P_GetPtrp(sidedef, DMU_BOTTOM_MATERIAL);
                    midMat = P_GetPtrp(sidedef, DMU_MIDDLE_MATERIAL);

                    if(bottomMat == mat && midMat == NULL)
                    {
                        yoff = P_GetFloatp(sidedef, DMU_BOTTOM_MATERIAL_OFFSET_Y);
                        P_SetFloatp(sidedef, DMU_BOTTOM_MATERIAL_OFFSET_Y, yoff + 1.0f);
                    }
                }
            }
        }
    }

#elif __JHEXEN__
    // Check if the map should have lightening.
    P_InitLightning();

    SN_StopAllSequences();
#endif

    // Do some fine tuning with mobj placement and orientation.
    P_MoveThingsOutOfWalls();
#if __JHERETIC__
    P_TurnGizmosAwayFromDoors();
#endif
}

const char* P_GetMapNiceName(void)
{
    const char* lname, *ptr;

    lname = (char*) DD_GetVariable(DD_MAP_NAME);
#if __JHEXEN__
    // In jHexen we can look in the MAPINFO for the map name.
    if(!lname)
        lname = P_GetMapName(gameMap);
#endif

    if(!lname || !lname[0])
        return NULL;

    // Skip the ExMx part.
    ptr = strchr(lname, ':');
    if(ptr)
    {
        lname = ptr + 1;
        while(*lname && isspace(*lname))
            lname++;
    }

    return lname;
}

patchid_t P_FindMapTitlePatch(uint episode, uint map)
{
#if __JDOOM__ || __JDOOM64__
#  if __JDOOM__
    if(!(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX)))
        map = (episode * 9) + map;
#  endif
    return pMapNames[map];
#endif
    return 0;
}

boolean P_IsMapFromIWAD(uint episode, uint map)
{
    char lumpName[9];
    P_MapId(episode, map, lumpName);
    return W_LumpIsFromIWAD(W_GetLumpNumForName(lumpName));
}

const char* P_GetMapAuthor(boolean supressGameAuthor)
{
    const char* author = (const char*) DD_GetVariable(DD_MAP_AUTHOR);

    if(!author || !author[0])
        return 0;

    /// \kludge We need DED Reader 2.0 to handle this the Right Way...
    /// \todo dj: this logic can now be handled engine-side.
    if(supressGameAuthor)
    {
        if(P_IsMapFromIWAD(gameEpisode, gameMap))
            return 0;

        { ddgameinfo_t gameInfo;
        if(DD_GetGameInfo(&gameInfo) && !stricmp(author, gameInfo.author))
            return 0;
        }
    }
    /// << kludge end.

    return author;
}

/**
 * Prints a banner to the console containing information pertinent to the
 * current map (e.g. map name, author...).
 */
static void P_PrintMapBanner(uint episode, uint map)
{
    const char* lname;

    Con_Printf("\n");
    lname = P_GetMapNiceName();
    if(lname)
    {
        char name[64];

#if __JHEXEN__
        dd_snprintf(name, 64, "Map %u (%u): %s", P_GetMapWarpTrans(map)+1,
                    map+1, lname);
#else
        dd_snprintf(name, 64, "Map %u: %s", map+1, lname);
#endif

        Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "%s\n", name);
    }

#if !__JHEXEN__
    {
    static const char* unknownAuthorStr = "Unknown";
    const char* lauthor;

    lauthor = P_GetMapAuthor(!P_IsMapFromIWAD(episode, map));
    if(!lauthor)
        lauthor = unknownAuthorStr;

    Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "Author: %s\n", lauthor);
    }
#endif

    Con_Printf("\n");
}
