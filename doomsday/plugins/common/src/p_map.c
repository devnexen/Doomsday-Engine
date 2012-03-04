/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

/**
 * p_map.c: Common map routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "doomsday.h"
#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "d_net.h"
#include "g_common.h"
#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_terraintype.h"
#include "p_tick.h"
#include "p_actor.h"
#include "p_player.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#if __JHEXEN__
#define USE_PUZZLE_ITEM_SPECIAL     129
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

#if __JDOOM64__
static void CheckMissileImpact(mobj_t* mobj);
#endif

#if __JHERETIC__
static void  CheckMissileImpact(mobj_t* mobj);
#elif __JHEXEN__
static void  P_FakeZMovement(mobj_t* mo);
static void  checkForPushSpecial(linedef_t* line, int side, mobj_t* mobj);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

AABoxf tmBox;
mobj_t* tmThing;

// If "floatOk" true, move would be ok if within "tmFloorZ - tmCeilingZ".
boolean floatOk;

float tmFloorZ;
float tmCeilingZ;
#if __JHEXEN__
material_t* tmFloorMaterial;
#endif

boolean fellDown; // $dropoff_fix

// The following is used to keep track of the linedefs that clip the open
// height range e.g. PIT_CheckLine. They in turn are used with the &unstuck
// logic and to prevent missiles from exploding against sky hack walls.
linedef_t* ceilingLine;
linedef_t* floorLine;

mobj_t* lineTarget; // Who got hit (or NULL).
linedef_t* blockLine; // $unstuck: blocking linedef

float attackRange;

#if __JHEXEN__
mobj_t* puffSpawned;
mobj_t* blockingMobj;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static float tm[3];
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
static float tmHeight;
static linedef_t* tmHitLine;
#endif
static float tmDropoffZ;
static float bestSlideDistance, secondSlideDistance;
static linedef_t* bestSlideLine, *secondSlideLine;

static mobj_t* slideMo;

static float tmMove[3];
static mobj_t* shootThing;

// Height if not aiming up or down.
static float shootZ;

static int lineAttackDamage;
static float aimSlope;

// slopes to top and bottom of target
static float topSlope, bottomSlope;

static mobj_t* useThing;

static mobj_t* bombSource, *bombSpot;
static int bombDamage;
static int bombDistance;

static boolean crushChange;
static boolean noFit;

static float startPos[3]; // start position for trajectory line checks
static float endPos[3]; // end position for trajectory checks

#if __JHEXEN__
static mobj_t* tsThing;
static boolean damageSource;
static mobj_t* onMobj; // generic global onMobj...used for landing on pods/players

static mobj_t* puzzleItemUser;
static int puzzleItemType;
static boolean puzzleActivated;
#endif

#if !__JHEXEN__
static int tmUnstuck; // $unstuck: used to check unsticking
#endif

static byte* rejectMatrix = NULL; // For fast sight rejection.

// CODE --------------------------------------------------------------------

float P_GetGravity(void)
{
    if(cfg.netGravity != -1)
        return (float) cfg.netGravity / 100;

    return *((float*) DD_GetVariable(DD_GRAVITY));
}

/**
 * Checks the reject matrix to find out if the two sectors are visible
 * from each other.
 */
static boolean checkReject(subsector_t* a, subsector_t* b)
{
    if(rejectMatrix != NULL)
    {
        uint                s1, s2, pnum, bytenum, bitnum;
        sector_t*           sec1 = P_GetPtrp(a, DMU_SECTOR);
        sector_t*           sec2 = P_GetPtrp(b, DMU_SECTOR);

        // Determine subsector entries in REJECT table.
        s1 = P_ToIndex(sec1);
        s2 = P_ToIndex(sec2);
        pnum = s1 * numsectors + s2;
        bytenum = pnum >> 3;
        bitnum = 1 << (pnum & 7);

        // Check in REJECT table.
        if(rejectMatrix[bytenum] & bitnum)
        {
            // Can't possibly be connected.
            return false;
        }
    }

    return true;
}

/**
 * Look from eyes of t1 to any part of t2 (start from middle of t1).
 *
 * @param from          The mobj doing the looking.
 * @param to            The mobj being looked at.
 *
 * @return              @c true if a straight line between t1 and t2 is
 *                      unobstructed.
 */
boolean P_CheckSight(const mobj_t* from, const mobj_t* to)
{
    float               fPos[3];

    // If either is unlinked, they can't see each other.
    if(!from->subsector || !to->subsector)
        return false;

    if(to->dPlayer && (to->dPlayer->flags & DDPF_CAMERA))
        return false; // Cameramen don't exist!

    // Check for trivial rejection.
    if(!checkReject(from->subsector, to->subsector))
        return false;

    fPos[VX] = from->pos[VX];
    fPos[VY] = from->pos[VY];
    fPos[VZ] = from->pos[VZ];

    if(!P_MobjIsCamera(from))
        fPos[VZ] += from->height + -(from->height / 4);

    return P_CheckLineSight(fPos, to->pos, 0, to->height, 0);
}

int PIT_StompThing(mobj_t* mo, void* data)
{
    int                 stompAnyway;
    float               blockdist;

    if(!(mo->flags & MF_SHOOTABLE))
        return false;

    blockdist = mo->radius + tmThing->radius;
    if(fabs(mo->pos[VX] - tm[VX]) >= blockdist ||
       fabs(mo->pos[VY] - tm[VY]) >= blockdist)
        return false; // Didn't hit it.

    if(mo == tmThing)
        return false; // Don't clip against self.

    stompAnyway = *(int*) data;

    // Should we stomp anyway? unless self.
    if(mo != tmThing && stompAnyway)
    {
        P_DamageMobj(mo, tmThing, tmThing, 10000, true);
        return false;
    }

#if __JDOOM64__
    // monsters don't stomp things
    if(!tmThing->player)
        return true;
#elif __JDOOM__
    // Monsters don't stomp things except on a boss map.
    if(!tmThing->player && gameMap != 29)
        return true;
#endif

    if(!(tmThing->flags2 & MF2_TELESTOMP))
        return true; // Not allowed to stomp things.

    // Do stomp damage (unless self).
    if(mo != tmThing)
        P_DamageMobj(mo, tmThing, tmThing, 10000, true);

    return false;
}

boolean P_TeleportMove(mobj_t* thing, float x, float y, boolean alwaysStomp)
{
    int stomping;
    subsector_t* newSSec;
    AABoxf tmBoxExpanded;

    // Kill anything occupying the position.
    tmThing = thing;
    stomping = alwaysStomp;

    tm[VX] = x;
    tm[VY] = y;

    tmBox.minX = tm[VX] - tmThing->radius;
    tmBox.minY = tm[VY] - tmThing->radius;
    tmBox.maxX = tm[VX] + tmThing->radius;
    tmBox.maxY = tm[VY] + tmThing->radius;

    newSSec = P_SubsectorAtPointXY(tm[VX], tm[VY]);

    ceilingLine = floorLine = NULL;
#if !__JHEXEN__
    blockLine = NULL;
    tmUnstuck = thing->dPlayer && thing->dPlayer->mo == thing;
#endif

    // The base floor / ceiling is from the subsector that contains the
    // point. Any contacted lines the step closer together will adjust them.
    tmFloorZ = tmDropoffZ = P_GetFloatp(newSSec, DMU_FLOOR_HEIGHT);
    tmCeilingZ = P_GetFloatp(newSSec, DMU_CEILING_HEIGHT);
#if __JHEXEN__
    tmFloorMaterial = P_GetPtrp(newSSec, DMU_FLOOR_MATERIAL);
#endif

    IterList_Empty(spechit);

    tmBoxExpanded.minX = tmBox.minX - MAXRADIUS;
    tmBoxExpanded.minY = tmBox.minY - MAXRADIUS;
    tmBoxExpanded.maxX = tmBox.maxX + MAXRADIUS;
    tmBoxExpanded.maxY = tmBox.maxY + MAXRADIUS;

    // Stomp on any things contacted.
    VALIDCOUNT++;
    if(P_MobjsBoxIterator(&tmBoxExpanded, PIT_StompThing, &stomping))
        return false;

    // The move is ok, so link the thing into its new position.
    P_MobjUnsetPosition(thing);

    thing->floorZ = tmFloorZ;
    thing->ceilingZ = tmCeilingZ;
#if !__JHEXEN__
    thing->dropOffZ = tmDropoffZ;
#endif
    thing->pos[VX] = x;
    thing->pos[VY] = y;

    P_MobjSetPosition(thing);
    P_MobjClearSRVO(thing);

    return true;
}

/**
 * Checks to see if a start->end trajectory line crosses a blocking line.
 * Returns false if it does.
 *
 * tmBox holds the bounding box of the trajectory. If that box does not
 * touch the bounding box of the line in question, then the trajectory is
 * not blocked. If the start is on one side of the line and the end is on
 * the other side, then the trajectory is blocked.
 *
 * Currently this assumes an infinite line, which is not quite correct.
 * A more correct solution would be to check for an intersection of the
 * trajectory and the line, but that takes longer and probably really isn't
 * worth the effort.
 *
 * @param data          Unused.
 */
int PIT_CrossLine(linedef_t* ld, void* data)
{
    int                 flags = P_GetIntp(ld, DMU_FLAGS);

    if((flags & DDLF_BLOCKING) ||
       (P_ToXLine(ld)->flags & ML_BLOCKMONSTERS) ||
       (!P_GetPtrp(ld, DMU_FRONT_SECTOR) || !P_GetPtrp(ld, DMU_BACK_SECTOR)))
    {
        AABoxf* aaBox = P_GetPtrp(ld, DMU_BOUNDING_BOX);

        if(!(tmBox.minX > aaBox->maxX ||
             tmBox.maxX < aaBox->minX ||
             tmBox.maxY < aaBox->minY ||
             tmBox.minY > aaBox->maxY))
        {
            if(P_PointOnLinedefSideXY(startPos[VX], startPos[VY], ld) !=
               P_PointOnLinedefSideXY(endPos[VX], endPos[VY], ld))
                // Line blocks trajectory.
                return true;
        }
    }

    // Line doesn't block trajectory.
    return false;
}

/**
 * This routine checks for Lost Souls trying to be spawned across 1-sided
 * lines, impassible lines, or "monsters can't cross" lines.
 *
 * Draw an imaginary line between the PE and the new Lost Soul spawn spot.
 * If that line crosses a 'blocking' line, then disallow the spawn. Only
 * search lines in the blocks of the blockmap where the bounding box of the
 * trajectory line resides. Then check bounding box of the trajectory vs
 * the bounding box of each blocking line to see if the trajectory and the
 * blocking line cross. Then check the PE and LS to see if they are on
 * different sides of the blocking line. If so, return true otherwise
 * false.
 */
boolean P_CheckSides(mobj_t* actor, float x, float y)
{
    startPos[VX] = actor->pos[VX];
    startPos[VY] = actor->pos[VY];
    startPos[VZ] = actor->pos[VZ];

    endPos[VX] = x;
    endPos[VY] = y;
    endPos[VZ] = DDMINFLOAT; // Initialize with *something*.

    // The bounding box of the trajectory
    tmBox.minX = (startPos[VX] < endPos[VX]? startPos[VX] : endPos[VX]);
    tmBox.minY = (startPos[VY] < endPos[VY]? startPos[VY] : endPos[VY]);
    tmBox.maxX = (startPos[VX] > endPos[VX]? startPos[VX] : endPos[VX]);
    tmBox.maxY = (startPos[VY] > endPos[VY]? startPos[VY] : endPos[VY]);

    VALIDCOUNT++;
    return P_AllLinesBoxIterator(&tmBox, PIT_CrossLine, 0);
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
/**
 * $unstuck: used to test intersection between thing and line assuming NO
 * movement occurs -- used to avoid sticky situations.
 */
static int untouched(linedef_t* ld)
{
    const float x = tmThing->pos[VX];
    const float y = tmThing->pos[VY];
    const float radius = tmThing->radius;
    AABoxf* ldBox = P_GetPtrp(ld, DMU_BOUNDING_BOX);
    AABoxf moBox;

    if(((moBox.minX = x - radius) >= ldBox->maxX) ||
       ((moBox.minY = y - radius) >= ldBox->maxY) ||
       ((moBox.maxX = x + radius) <= ldBox->minX) ||
       ((moBox.maxY = y + radius) <= ldBox->minY) ||
       P_BoxOnLineSide(&moBox, ld) != -1)
        return true;

    return false;
}
#endif

int PIT_CheckThing(mobj_t* thing, void* data)
{
    int                 damage;
    float               blockdist;
    boolean             solid;
#if !__JHEXEN__
    boolean             overlap = false;
#endif

    // Don't clip against self.
    if(thing == tmThing)
        return false;

#if __JHEXEN__
    // Don't clip on something we are stood on.
    if(thing == tmThing->onMobj)
        return false;
#endif

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)) ||
       P_MobjIsCamera(thing) || P_MobjIsCamera(tmThing))
        return false;

#if !__JHEXEN__
    // Player only.
    if(tmThing->player && tm[VZ] != DDMAXFLOAT &&
       (cfg.moveCheckZ || (tmThing->flags2 & MF2_PASSMOBJ)))
    {
        if((thing->pos[VZ] > tm[VZ] + tmHeight) ||
           (thing->pos[VZ] + thing->height < tm[VZ]))
            return false; // Under or over it.

        overlap = true;
    }
#endif

    blockdist = thing->radius + tmThing->radius;
    if(fabs(thing->pos[VX] - tm[VX]) >= blockdist ||
       fabs(thing->pos[VY] - tm[VY]) >= blockdist)
        return false; // Didn't hit thing.

    if(IS_CLIENT)
    {
        // On clientside, missiles don't collide with mobjs.
        if(tmThing->ddFlags & DDMF_MISSILE)
        {
            return false;
        }

        // Players can't hit their own clmobjs.
        if(tmThing->player && ClPlayer_ClMobj(tmThing->player - players) == thing)
        {
            return false;
        }
    }

/*
#if __JHEXEN__
    // Stop here if we are a client.
    if(IS_CLIENT)
        return true;
#endif
*/

#if !__JHEXEN__
    if(!tmThing->player && (tmThing->flags2 & MF2_PASSMOBJ))
#else
    blockingMobj = thing;
    if(tmThing->flags2 & MF2_PASSMOBJ)
#endif
    {   // Check if a mobj passed over/under another object.
#if __JHERETIC__
        if((tmThing->type == MT_IMP || tmThing->type == MT_WIZARD) &&
           (thing->type == MT_IMP || thing->type == MT_WIZARD))
        {
            return true; // Don't let imps/wizards fly over other imps/wizards.
        }
#elif __JHEXEN__
        if(tmThing->type == MT_BISHOP && thing->type == MT_BISHOP)
            return true; // Don't let bishops fly over other bishops.
#endif

        if(!(thing->flags & MF_SPECIAL))
        {
            if(tmThing->pos[VZ] > thing->pos[VZ] + thing->height ||
               tmThing->pos[VZ] + tmThing->height < thing->pos[VZ])
                return false; // Over/under thing.
        }
    }

    // Check for skulls slamming into things.
    if((tmThing->flags & MF_SKULLFLY) && (thing->flags & MF_SOLID))
    {
#if __JHEXEN__
        blockingMobj = NULL;
        if(tmThing->type == MT_MINOTAUR)
        {
            // Slamming minotaurs shouldn't move non-creatures.
            if(!(thing->flags & MF_COUNTKILL))
            {
                return true;
            }
        }
        else if(tmThing->type == MT_HOLY_FX)
        {
            if((thing->flags & MF_SHOOTABLE) && thing != tmThing->target)
            {
                if(IS_NETGAME && !deathmatch && thing->player)
                    return false; // don't attack other co-op players

                if((thing->flags2 & MF2_REFLECTIVE) &&
                   (thing->player || (thing->flags2 & MF2_BOSS)))
                {
                    tmThing->tracer = tmThing->target;
                    tmThing->target = thing;
                    return false;
                }

                if(thing->flags & MF_COUNTKILL || thing->player)
                {
                    tmThing->tracer = thing;
                }

                if(P_Random() < 96)
                {
                    damage = 12;
                    if(thing->player || (thing->flags2 & MF2_BOSS))
                    {
                        damage = 3;
                        // Ghost burns out faster when attacking players/bosses.
                        tmThing->health -= 6;
                    }

                    P_DamageMobj(thing, tmThing, tmThing->target, damage, false);
                    if(P_Random() < 128)
                    {
                        P_SpawnMobj3fv(MT_HOLY_PUFF, tmThing->pos,
                                       P_Random() << 24, 0);
                        S_StartSound(SFX_SPIRIT_ATTACK, tmThing);
                        if((thing->flags & MF_COUNTKILL) && P_Random() < 128 &&
                           !S_IsPlaying(SFX_PUPPYBEAT, thing))
                        {
                            if((thing->type == MT_CENTAUR) ||
                               (thing->type == MT_CENTAURLEADER) ||
                               (thing->type == MT_ETTIN))
                            {
                                S_StartSound(SFX_PUPPYBEAT, thing);
                            }
                        }
                    }
                }

                if(thing->health <= 0)
                    tmThing->tracer = NULL;
            }
            return false;
        }
#endif
#if __JDOOM__
        if(tmThing->damage == DDMAXINT) //// \kludge to support old save games.
            damage = tmThing->info->damage;
        else
#endif
            damage = tmThing->damage;

        damage *= (P_Random() % 8) + 1;
        P_DamageMobj(thing, tmThing, tmThing, damage, false);

        tmThing->flags &= ~MF_SKULLFLY;
        tmThing->mom[MX] = tmThing->mom[MY] = tmThing->mom[MZ] = 0;

#if __JHERETIC__ || __JHEXEN__
        P_MobjChangeState(tmThing, P_GetState(tmThing->type, SN_SEE));
#else
        P_MobjChangeState(tmThing, P_GetState(tmThing->type, SN_SPAWN));
#endif

        return true; // Stop moving.
    }

#if __JHEXEN__
    // Check for blasted thing running into another
    if((tmThing->flags2 & MF2_BLASTED) && (thing->flags & MF_SHOOTABLE))
    {
        if(!(thing->flags2 & MF2_BOSS) && (thing->flags & MF_COUNTKILL))
        {
            thing->mom[MX] += tmThing->mom[MX];
            thing->mom[MY] += tmThing->mom[MY];

            NetSv_PlayerMobjImpulse(thing, tmThing->mom[MX], tmThing->mom[VY], 0);

            if((thing->mom[MX] + thing->mom[MY]) > 3)
            {
                damage = (tmThing->info->mass / 100) + 1;
                P_DamageMobj(thing, tmThing, tmThing, damage, false);

                damage = (thing->info->mass / 100) + 1;
                P_DamageMobj(tmThing, thing, thing, damage >> 2, false);
            }

            return true;
        }
    }
#endif

    // Missiles can hit other things.
    if(tmThing->flags & MF_MISSILE)
    {
#if __JHEXEN__
        // Check for a non-shootable mobj.
        if(thing->flags2 & MF2_NONSHOOTABLE)
            return false;
#else
        // Check for passing through a ghost.
        if((thing->flags & MF_SHADOW) && (tmThing->flags2 & MF2_THRUGHOST))
            return false;
#endif

        // See if it went over / under.
        if(tmThing->pos[VZ] > thing->pos[VZ] + thing->height)
            return false; // Overhead.

        if(tmThing->pos[VZ] + tmThing->height < thing->pos[VZ])
            return false; // Underneath.

#if __JHEXEN__
        if(tmThing->flags2 & MF2_FLOORBOUNCE)
        {
            if(tmThing->target == thing || !(thing->flags & MF_SOLID))
                return false;
            else
                return true;
        }

        if(tmThing->type == MT_LIGHTNING_FLOOR ||
           tmThing->type == MT_LIGHTNING_CEILING)
        {
            if((thing->flags & MF_SHOOTABLE) && thing != tmThing->target)
            {
                if(thing->info->mass != DDMAXINT)
                {
                    thing->mom[MX] += tmThing->mom[MX] / 16;
                    thing->mom[MY] += tmThing->mom[MY] / 16;

                    NetSv_PlayerMobjImpulse(thing, tmThing->mom[MX] / 16, tmThing->mom[MY] / 16, 0);
                }

                if((!thing->player && !(thing->flags2 & MF2_BOSS)) ||
                   !(mapTime & 1))
                {
                    if(thing->type == MT_CENTAUR ||
                       thing->type == MT_CENTAURLEADER)
                    {   // Lightning does more damage to centaurs.
                        P_DamageMobj(thing, tmThing, tmThing->target, 9, false);
                    }
                    else
                    {
                        P_DamageMobj(thing, tmThing, tmThing->target, 3, false);
                    }

                    if(!(S_IsPlaying(SFX_MAGE_LIGHTNING_ZAP, tmThing)))
                    {
                        S_StartSound(SFX_MAGE_LIGHTNING_ZAP, tmThing);
                    }

                    if((thing->flags & MF_COUNTKILL) && P_Random() < 64 &&
                       !S_IsPlaying(SFX_PUPPYBEAT, thing))
                    {
                        if((thing->type == MT_CENTAUR) ||
                           (thing->type == MT_CENTAURLEADER) ||
                           (thing->type == MT_ETTIN))
                        {
                            S_StartSound(SFX_PUPPYBEAT, thing);
                        }
                    }
                }

                tmThing->health--;
                if(tmThing->health <= 0 || thing->health <= 0)
                {
                    return true;
                }

                if(tmThing->type == MT_LIGHTNING_FLOOR)
                {
                    if(tmThing->lastEnemy &&
                       !tmThing->lastEnemy->tracer)
                    {
                        tmThing->lastEnemy->tracer = thing;
                    }
                }
                else if(!tmThing->tracer)
                {
                    tmThing->tracer = thing;
                }
            }

            return false; // Lightning zaps through all sprites.
        }
        else if(tmThing->type == MT_LIGHTNING_ZAP)
        {
            mobj_t *lmo;

            if((thing->flags & MF_SHOOTABLE) && thing != tmThing->target)
            {
                lmo = tmThing->lastEnemy;
                if(lmo)
                {
                    if(lmo->type == MT_LIGHTNING_FLOOR)
                    {
                        if(lmo->lastEnemy &&
                           !lmo->lastEnemy->tracer)
                        {
                            lmo->lastEnemy->tracer = thing;
                        }
                    }
                    else if(!lmo->tracer)
                    {
                        lmo->tracer = thing;
                    }

                    if(!(mapTime & 3))
                    {
                        lmo->health--;
                    }
                }
            }
        }
        else if(tmThing->type == MT_MSTAFF_FX2 && thing != tmThing->target)
        {
            if(!thing->player && !(thing->flags2 & MF2_BOSS))
            {
                switch(thing->type)
                {
                case MT_FIGHTER_BOSS:   // these not flagged boss
                case MT_CLERIC_BOSS:    // so they can be blasted
                case MT_MAGE_BOSS:
                    break;
                default:
                    P_DamageMobj(thing, tmThing, tmThing->target, 10, false);
                    return false;
                    break;
                }
            }
        }
#endif

        // Don't hit same species as originator.
#if __JDOOM__ || __JDOOM64__
        if(tmThing->target &&
           (tmThing->target->type == thing->type ||
           (tmThing->target->type == MT_KNIGHT && thing->type == MT_BRUISER) ||
           (tmThing->target->type == MT_BRUISER && thing->type == MT_KNIGHT)))
#else
        if(tmThing->target && tmThing->target->type == thing->type)
#endif
        {
            if(thing == tmThing->target)
                return false;

#if __JHEXEN__
            if(!thing->player)
                return true; // Hit same species as originator, explode, no damage
#else
            if(!monsterInfight && thing->type != MT_PLAYER)
            {
                // Explode, but do no damage.
                // Let players missile other players.
                return true;
            }
#endif
        }

        if(!(thing->flags & MF_SHOOTABLE))
        {
            return !!(thing->flags & MF_SOLID); // Didn't do any damage.
        }

        if(tmThing->flags2 & MF2_RIP)
        {
#if __JHEXEN__
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE))
#else
            if(!(thing->flags & MF_NOBLOOD))
#endif
            {   // Ok to spawn some blood.
                P_RipperBlood(tmThing);
            }
#if __JHERETIC__
            S_StartSound(SFX_RIPSLOP, tmThing);
#endif
#if __JDOOM__
            if(tmThing->damage == DDMAXINT) //// \kludge to support old save games.
                damage = tmThing->info->damage;
            else
#endif
                damage = tmThing->damage;

            damage *= (P_Random() & 3) + 2;

            P_DamageMobj(thing, tmThing, tmThing->target, damage, false);

            if((thing->flags2 & MF2_PUSHABLE) &&
               !(tmThing->flags2 & MF2_CANNOTPUSH))
            {   // Push thing
                thing->mom[MX] += tmThing->mom[MX] / 4;
                thing->mom[MY] += tmThing->mom[MY] / 4;
                NetSv_PlayerMobjImpulse(thing, tmThing->mom[MX]/4, tmThing->mom[MY]/4, 0);
            }
            IterList_Empty(spechit);
            return false;
        }

        // Do damage
#if __JDOOM__
        if(tmThing->damage == DDMAXINT) //// \kludge to support old save games.
            damage = tmThing->info->damage;
        else
#endif
            damage = tmThing->damage;

        damage *= (P_Random() % 8) + 1;
#if __JDOOM__ || __JDOOM64__
        P_DamageMobj(thing, tmThing, tmThing->target, damage, false);
#else
        if(damage)
        {
# if __JHERETIC__
            if(!(thing->flags & MF_NOBLOOD) && P_Random() < 192)
# else //__JHEXEN__
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE) &&
               !(tmThing->type == MT_TELOTHER_FX1) &&
               !(tmThing->type == MT_TELOTHER_FX2) &&
               !(tmThing->type == MT_TELOTHER_FX3) &&
               !(tmThing->type == MT_TELOTHER_FX4) &&
               !(tmThing->type == MT_TELOTHER_FX5) && (P_Random() < 192))
# endif
                P_SpawnBloodSplatter(tmThing->pos[VX], tmThing->pos[VY], tmThing->pos[VZ], thing);

            P_DamageMobj(thing, tmThing, tmThing->target, damage, false);
        }
#endif
        // Don't traverse anymore.
        return true;
    }

    if((thing->flags2 & MF2_PUSHABLE) && !(tmThing->flags2 & MF2_CANNOTPUSH))
    {   // Push thing
        thing->mom[MX] += tmThing->mom[MX] / 4;
        thing->mom[MY] += tmThing->mom[MY] / 4;
        NetSv_PlayerMobjImpulse(thing, tmThing->mom[MX]/4, tmThing->mom[MY]/4, 0);
    }

    // \kludge: Always treat blood as a solid.
    if(tmThing->type == MT_BLOOD)
        solid = true;
    else
        solid = (thing->flags & MF_SOLID) && !(thing->flags & MF_NOCLIP) &&
                (tmThing->flags & MF_SOLID);
    // \kludge: end.

#if __JHEXEN__
    if(tmThing->player && tmThing->onMobj && solid)
    {
        /// @todo Unify Hexen's onMobj logic with the other games.

        // We may be standing on more than one thing.
        if(tmThing->pos[VZ] > thing->pos[VZ] + thing->height - 24)
        {
            // Stepping up on this is possible.
            tmFloorZ = MAX_OF(tmFloorZ, thing->pos[VZ] + thing->height);
            solid = false;
        }
    }
#endif

    // Check for special pickup.
    if((thing->flags & MF_SPECIAL) && (tmThing->flags & MF_PICKUP))
    {
        P_TouchSpecialMobj(thing, tmThing); // Can remove thing.
    }
#if !__JHEXEN__
    else if(overlap && solid)
    {
        // How are we positioned, allow step up?
        if(!(thing->flags & MF_CORPSE) && tm[VZ] > thing->pos[VZ] + thing->height - 24)
        {
            tmThing->onMobj = thing;
            if(thing->pos[VZ] + thing->height > tmFloorZ)
                tmFloorZ = thing->pos[VZ] + thing->height;
            return false;
        }
    }
#endif

    return solid;
}

/**
 * Adjusts tmFloorZ and tmCeilingZ as lines are contacted.
 */
int PIT_CheckLine(linedef_t* ld, void* data)
{
    AABoxf* aaBox = P_GetPtrp(ld, DMU_BOUNDING_BOX);
    xline_t* xline;

    if(tmBox.minX >= aaBox->maxX ||
       tmBox.minY >= aaBox->maxY ||
       tmBox.maxX <= aaBox->minX ||
       tmBox.maxY <= aaBox->minY)
        return false;

    if(P_BoxOnLineSide(&tmBox, ld) != -1)
        return false;

    /*
    if(IS_CLIENT)
    {
        // On clientside, missiles don't collide with anything.
        if(tmThing->ddFlags & DDMF_MISSILE)
        {
            return false;
        }
    }*/

    // A line has been hit
    xline = P_ToXLine(ld);
#if !__JHEXEN__
    tmThing->wallHit = true;

    // A Hit event will be sent to special lines.
    if(xline->special)
        tmHitLine = ld;
#endif

    if(!P_GetPtrp(ld, DMU_BACK_SECTOR)) // One sided line.
    {
#if __JHEXEN__
        if(tmThing->flags2 & MF2_BLASTED)
            P_DamageMobj(tmThing, NULL, NULL, tmThing->info->mass >> 5, false);
        checkForPushSpecial(ld, 0, tmThing);
        return true;
#else
        float               d1[2];

        P_GetFloatpv(ld, DMU_DXY, d1);

        /**
         * $unstuck: allow player to move out of 1s wall, to prevent
         * sticking. The moving thing's destination position will cross the
         * given line.
         * If this should not be allowed, return false.
         * If the line is special, keep track of it to process later if the
         * move is proven ok.
         *
         * \note Specials are NOT sorted by order, so two special lines that
         * are only 8 units apart could be crossed in either order.
         */

        blockLine = ld;
        return !(tmUnstuck && !untouched(ld) &&
            ((tm[VX] - tmThing->pos[VX]) * d1[1]) >
            ((tm[VY] - tmThing->pos[VY]) * d1[0]));
#endif
    }

    /// @todo Will never pass this test due to above. Is the previous check
    /// supposed to qualify player mobjs only?
#if __JHERETIC__
    if(!P_GetPtrp(ld, DMU_BACK_SECTOR)) // one sided line
    {   // One sided line
        if(tmThing->flags & MF_MISSILE)
        {   // Missiles can trigger impact specials
            if(xline->special)
                IterList_Push(spechit, ld);
        }
        return true;
    }
#endif

    if(!(tmThing->flags & MF_MISSILE))
    {
        // Explicitly blocking everything?
        if(P_GetIntp(ld, DMU_FLAGS) & DDLF_BLOCKING)
        {
#if __JHEXEN__
            if(tmThing->flags2 & MF2_BLASTED)
                P_DamageMobj(tmThing, NULL, NULL, tmThing->info->mass >> 5, false);
            checkForPushSpecial(ld, 0, tmThing);
            return true;
#else
            // $unstuck: allow escape.
            return !(tmUnstuck && !untouched(ld));
#endif
        }

        // Block monsters only?
#if __JHEXEN__
        if(!tmThing->player && tmThing->type != MT_CAMERA &&
           (xline->flags & ML_BLOCKMONSTERS))
#elif __JHERETIC__
        if(!tmThing->player && tmThing->type != MT_POD &&
           (xline->flags & ML_BLOCKMONSTERS))
#else
        if(!tmThing->player &&
            (xline->flags & ML_BLOCKMONSTERS))
#endif
        {
#if __JHEXEN__
            if(tmThing->flags2 & MF2_BLASTED)
                P_DamageMobj(tmThing, NULL, NULL, tmThing->info->mass >> 5, false);
#endif
            return true;
        }
    }

#if __JDOOM64__
    if((tmThing->flags & MF_MISSILE))
    {
        if(xline->flags & ML_BLOCKALL) // Explicitly blocking everything.
            return !(tmUnstuck && !untouched(ld));  // $unstuck: allow escape.
    }
#endif

    // Set OPENRANGE, OPENTOP, OPENBOTTOM.
    P_LineOpening(ld);

    // Adjust floor / ceiling heights.
    if(OPENTOP < tmCeilingZ)
    {
        tmCeilingZ = OPENTOP;
        ceilingLine = ld;
#if !__JHEXEN__
        blockLine = ld;
#endif
    }

    if(OPENBOTTOM > tmFloorZ)
    {
        tmFloorZ = OPENBOTTOM;
        floorLine = ld;
#if !__JHEXEN__
        blockLine = ld;
#endif
    }

    if(LOWFLOOR < tmDropoffZ)
        tmDropoffZ = LOWFLOOR;

    // If contacted a special line, add it to the list.
    if(P_ToXLine(ld)->special)
        IterList_Push(spechit, ld);

#if !__JHEXEN__
    tmThing->wallHit = false;
#endif
    return false; // Continue iteration.
}

/**
 * This is purely informative, nothing is modified (except things picked up).
 *
 * in:
 *  a mobj_t (can be valid or invalid)
 *  a position to be checked
 *   (doesn't need to be related to the mobj_t->x,y)
 *
 * during:
 *  special things are touched if MF_PICKUP early out on solid lines?
 *
 * out:
 *  newsubsec
 *  floorz
 *  ceilingz
 *  tmDropoffZ
 *   the lowest point contacted
 *   (monsters won't move to a drop off)
 *  speciallines[]
 *  numspeciallines
 */
boolean P_CheckPosition3f(mobj_t* thing, float x, float y, float z)
{
    sector_t* newSec;
    AABoxf tmBoxExpanded;

    tmThing = thing;

#if !__JHEXEN__
    thing->onMobj = NULL;
#endif
    thing->wallHit = false;

#if !__JHEXEN__
    tmHitLine = NULL;
    tmHeight = thing->height;
#endif

    tm[VX] = x;
    tm[VY] = y;
    tm[VZ] = z;

    tmBox.minX = tm[VX] - tmThing->radius;
    tmBox.minY = tm[VY] - tmThing->radius;
    tmBox.maxX = tm[VX] + tmThing->radius;
    tmBox.maxY = tm[VY] + tmThing->radius;

    newSec = P_GetPtrp(P_SubsectorAtPointXY(tm[VX], tm[VY]), DMU_SECTOR);

    ceilingLine = floorLine = NULL;
#if !__JHEXEN__
    blockLine = NULL;
    tmUnstuck = ((thing->dPlayer && thing->dPlayer->mo == thing)? true : false);
#endif

    // The base floor/ceiling is from the subsector that contains the point.
    // Any contacted lines the step closer together will adjust them.
    tmFloorZ = tmDropoffZ = P_GetFloatp(newSec, DMU_FLOOR_HEIGHT);
    tmCeilingZ = P_GetFloatp(newSec, DMU_CEILING_HEIGHT);
#if __JHEXEN__
    tmFloorMaterial = P_GetPtrp(newSec, DMU_FLOOR_MATERIAL);
#endif

    IterList_Empty(spechit);

#if __JHEXEN__
    if((tmThing->flags & MF_NOCLIP) && !(tmThing->flags & MF_SKULLFLY))
        return true;
#else
    if((tmThing->flags & MF_NOCLIP))
        return true;
#endif

    // Check things first, possibly picking things up the bounding box is
    // extended by MAXRADIUS because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap into adjacent blocks by
    // up to MAXRADIUS units.
    tmBoxExpanded.minX = tmBox.minX - MAXRADIUS;
    tmBoxExpanded.minY = tmBox.minY - MAXRADIUS;
    tmBoxExpanded.maxX = tmBox.maxX + MAXRADIUS;
    tmBoxExpanded.maxY = tmBox.maxY + MAXRADIUS;

    VALIDCOUNT++;

    // The camera goes through all objects.
    if(!P_MobjIsCamera(thing))
    {
#if __JHEXEN__
        blockingMobj = NULL;
#endif
        if(P_MobjsBoxIterator(&tmBoxExpanded, PIT_CheckThing, 0))
        {
            return false;
        }

#if _DEBUG
        VERBOSE2(
            if(thing->onMobj)
                Con_Message("thing->onMobj = %p/%i (solid:%i) [thing:%p/%i]\n", thing->onMobj,
                            thing->onMobj->thinker.id,
                            (thing->onMobj->flags & MF_SOLID)!=0,
                            thing, thing->thinker.id)
        );
#endif
    }

     // Check lines.
#if __JHEXEN__
    if(tmThing->flags & MF_NOCLIP)
        return true;

    blockingMobj = NULL;
#endif

    tmBoxExpanded.minX = tmBox.minX;
    tmBoxExpanded.minY = tmBox.minY;
    tmBoxExpanded.maxX = tmBox.maxX;
    tmBoxExpanded.maxY = tmBox.maxY;

    return !P_AllLinesBoxIterator(&tmBoxExpanded, PIT_CheckLine, 0);
}

boolean P_CheckPosition3fv(mobj_t* thing, const float pos[3])
{
    return P_CheckPosition3f(thing, pos[VX], pos[VY], pos[VZ]);
}

boolean P_CheckPosition2f(mobj_t* thing, float x, float y)
{
    return P_CheckPosition3f(thing, x, y, DDMAXFLOAT);
}

/**
 * Attempt to move to a new position, crossing special lines unless
 * MF_TELEPORT is set. $dropoff_fix
 */
#if __JHEXEN__
static boolean P_TryMove2(mobj_t* thing, float x, float y)
#else
static boolean P_TryMove2(mobj_t* thing, float x, float y, boolean dropoff)
#endif
{
    float               oldpos[3];
    int                 side, oldSide;
    linedef_t*          ld;
    boolean             isRemotePlayer = ((IS_DEDICATED && thing->dPlayer) ||
                                          (IS_CLIENT && thing->player && thing->player - players != CONSOLEPLAYER));

    // $dropoff_fix: fellDown.
    floatOk = false;
#if !__JHEXEN__
    fellDown = false;
#endif

#if __JHEXEN__
    if(!P_CheckPosition2f(thing, x, y))
#else
    if(!P_CheckPosition3f(thing, x, y, thing->pos[VZ]))
#endif
    {
#if __JHEXEN__
        if(!blockingMobj || blockingMobj->player || !thing->player)
        {
            goto pushline;
        }
        else if(blockingMobj->pos[VZ] + blockingMobj->height - thing->pos[VZ] > 24 ||
                (P_GetFloatp(blockingMobj->subsector, DMU_CEILING_HEIGHT) -
                 (blockingMobj->pos[VZ] + blockingMobj->height) < thing->height) ||
                (tmCeilingZ - (blockingMobj->pos[VZ] + blockingMobj->height) <
                 thing->height))
        {
            goto pushline;
        }
#else
#  if __JHERETIC__
        CheckMissileImpact(thing);
#  endif
        // Would we hit another thing or a solid wall?
        if(!thing->onMobj || thing->wallHit)
            return false;
#endif
    }

    if(!(thing->flags & MF_NOCLIP))
    {
#if __JHEXEN__
        if(tmCeilingZ - tmFloorZ < thing->height)
        {   // Doesn't fit.
            goto pushline;
        }

        floatOk = true;

        if(!(thing->flags & MF_TELEPORT) &&
           tmCeilingZ - thing->pos[VZ] < thing->height &&
           thing->type != MT_LIGHTNING_CEILING && !(thing->flags2 & MF2_FLY))
        {   // Mobj must lower itself to fit.
            goto pushline;
        }
#else
        // Possibly allow escape if otherwise stuck.
        boolean             ret = (tmUnstuck &&
            !(ceilingLine && untouched(ceilingLine)) &&
            !(floorLine   && untouched(floorLine)));

        if(tmCeilingZ - tmFloorZ < thing->height)
            return ret; // Doesn't fit.

        // Mobj must lower to fit.
        floatOk = true;
        if(!(thing->flags & MF_TELEPORT) && !(thing->flags2 & MF2_FLY) &&
           tmCeilingZ - thing->pos[VZ] < thing->height)
        {
            return ret;
        }

        // Too big a step up.
        if(!(thing->flags & MF_TELEPORT) &&
            !(thing->flags2 & MF2_FLY)
# if __JHERETIC__
            && thing->type != MT_MNTRFX2 // The Minotaur floor fire (MT_MNTRFX2) can step up any amount
# endif
            )
        {
            if(!isRemotePlayer && tmFloorZ - thing->pos[VZ] > 24)
            {
# if __JHERETIC__
                CheckMissileImpact(thing);
# endif
                return ret;
            }
        }
# if __JHERETIC__
        if((thing->flags & MF_MISSILE) && tmFloorZ > thing->pos[VZ])
        {
            CheckMissileImpact(thing);
        }
# endif
#endif
        if(thing->flags2 & MF2_FLY)
        {
            if(thing->pos[VZ] + thing->height > tmCeilingZ)
            {
                thing->mom[MZ] = -8;
#if __JHEXEN__
                goto pushline;
#else
                return false;
#endif
            }
            else if(thing->pos[VZ] < tmFloorZ &&
                    tmFloorZ - tmDropoffZ > 24)
            {
                thing->mom[MZ] = 8;
#if __JHEXEN__
                goto pushline;
#else
                return false;
#endif
            }
        }

#if __JHEXEN__
        if(!(thing->flags & MF_TELEPORT)
           // The Minotaur floor fire (MT_MNTRFX2) can step up any amount
           && thing->type != MT_MNTRFX2 && thing->type != MT_LIGHTNING_FLOOR
           && !isRemotePlayer
           && tmFloorZ - thing->pos[VZ] > 24)
        {
            goto pushline;
        }
#endif

#if __JHEXEN__
        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)) &&
           (tmFloorZ - tmDropoffZ > 24) &&
           !(thing->flags2 & MF2_BLASTED))
        {   // Can't move over a dropoff unless it's been blasted.
            return false;
        }
#else

        /**
         * Allow certain objects to drop off.
         * Prevent monsters from getting stuck hanging off ledges.
         * Allow dropoffs in controlled circumstances.
         * Improve symmetry of clipping on stairs.
         */
        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)))
        {
            // Dropoff height limit.
            if(cfg.avoidDropoffs)
            {
                if(tmFloorZ - tmDropoffZ > 24)
                    return false; // Don't stand over dropoff.
            }
            else
            {
                float floorZ = tmFloorZ;

                if(thing->onMobj)
                {
                    // Thing is stood on something so use our z position
                    // as the floor.
                    floorZ = (thing->pos[VZ] > tmFloorZ ?
                        thing->pos[VZ] : tmFloorZ);
                }

                if(!dropoff)
                {
                    if(thing->floorZ - floorZ > 24 || thing->dropOffZ - tmDropoffZ > 24)
                        return false;
                }
                else
                {
                    // Set fellDown if drop > 24.
                    fellDown = !(thing->flags & MF_NOGRAVITY) &&
                        thing->pos[VZ] - floorZ > 24;
                }
            }
        }
#endif

#if __JDOOM64__
        //// \fixme DJS - FIXME! Mother demon fire attack.
        if(!(thing->flags & MF_TELEPORT) /*&& thing->type != MT_SPAWNFIRE*/
            && !isRemotePlayer
            && tmFloorZ - thing->pos[VZ] > 24)
        { // Too big a step up
            CheckMissileImpact(thing);
            return false;
        }
#endif

#if __JHEXEN__
        // Must stay within a sector of a certain floor type?
        if((thing->flags2 & MF2_CANTLEAVEFLOORPIC) &&
           (tmFloorMaterial != P_GetPtrp(thing->subsector, DMU_FLOOR_MATERIAL) ||
            !FEQUAL(tmFloorZ, thing->pos[VZ])))
        {
            return false;
        }
#endif

#if !__JHEXEN__
        // $dropoff: prevent falling objects from going up too many steps.
        if(!thing->player && (thing->intFlags & MIF_FALLING) &&
           tmFloorZ - thing->pos[VZ] > (thing->mom[MX] * thing->mom[MX]) +
                                       (thing->mom[MY] * thing->mom[MY]))
        {
            return false;
        }
#endif
    }

    // The move is ok, so link the thing into its new position.
    P_MobjUnsetPosition(thing);

    oldpos[VX] = thing->pos[VX];
    oldpos[VY] = thing->pos[VY];
    oldpos[VZ] = thing->pos[VZ];

    thing->floorZ = tmFloorZ;
    thing->ceilingZ = tmCeilingZ;
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    thing->dropOffZ = tmDropoffZ; // $dropoff_fix: keep track of dropoffs.
#endif

    thing->pos[VX] = x;
    thing->pos[VY] = y;

    P_MobjSetPosition(thing);

    if(thing->flags2 & MF2_FLOORCLIP)
    {
        thing->floorClip = 0;

        if(thing->pos[VZ] == P_GetFloatp(thing->subsector, DMU_FLOOR_HEIGHT))
        {
            const terraintype_t* tt = P_MobjGetFloorTerrainType(thing);

            if(tt->flags & TTF_FLOORCLIP)
            {
                thing->floorClip = 10;
            }
        }
    }

    // If any special lines were hit, do the effect.
    if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        while((ld = IterList_Pop(spechit)) != NULL)
        {
            // See if the line was crossed.
            if(P_ToXLine(ld)->special)
            {
                side = P_PointOnLinedefSideXY(thing->pos[VX], thing->pos[VY], ld);
                oldSide = P_PointOnLinedefSideXY(oldpos[VX], oldpos[VY], ld);
                if(side != oldSide)
                {
#if __JHEXEN__
                    if(thing->player)
                    {
                        P_ActivateLine(ld, thing, oldSide, SPAC_CROSS);
                    }
                    else if(thing->flags2 & MF2_MCROSS)
                    {
                        P_ActivateLine(ld, thing, oldSide, SPAC_MCROSS);
                    }
                    else if(thing->flags2 & MF2_PCROSS)
                    {
                        P_ActivateLine(ld, thing, oldSide, SPAC_PCROSS);
                    }
#else
#ifdef _DEBUG
                    if(!IS_CLIENT && thing->player)
                    {
                        Con_Message("P_TryMove2: Mobj %i crossing line %i from %f,%f to %f,%f\n",
                                    thing->thinker.id, P_ToIndex(ld),
                                    oldpos[VX], oldpos[VY],
                                    thing->pos[VX], thing->pos[VY]);
                    }
#endif
                    P_ActivateLine(ld, thing, oldSide, SPAC_CROSS);
#endif
                }
            }
        }
    }

    return true;

#if __JHEXEN__
  pushline:
    if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        if(tmThing->flags2 & MF2_BLASTED)
        {
            P_DamageMobj(tmThing, NULL, NULL, tmThing->info->mass >> 5, false);
        }

        IterList_SetIteratorDirection(spechit, ITERLIST_BACKWARD);
        IterList_RewindIterator(spechit);
        while((ld = IterList_MoveIterator(spechit)) != NULL)
        {
            // See if the line was crossed.
            side = P_PointOnLinedefSideXY(thing->pos[VX], thing->pos[VY], ld);
            checkForPushSpecial(ld, side, thing);
        }
    }
    return false;
#endif
}

#if __JHEXEN__
boolean P_TryMove(mobj_t* thing, float x, float y)
#else
boolean P_TryMove(mobj_t* thing, float x, float y, boolean dropoff,
                  boolean slide)
#endif
{
#if __JHEXEN__
    return P_TryMove2(thing, x, y);
#else
    // $dropoff_fix
    boolean             res = P_TryMove2(thing, x, y, dropoff);

    if(!res && tmHitLine)
    {
        // Move not possible, see if the thing hit a line and send a Hit
        // event to it.
        XL_HitLine(tmHitLine, P_PointOnLinedefSideXY(thing->pos[VX], thing->pos[VY], tmHitLine),
                   thing);
    }

    if(res && slide)
        thing->wallRun = true;

    return res;
#endif
}

/**
 * Attempts to move a mobj to a new 3D position, crossing special lines
 * and picking up things.
 *
 * @note  This function is exported from the game plugin.
 *
 * @param thing  Mobj to move.
 * @param x      New X coordinate.
 * @param y      New Y coordinate.
 * @param z      New Z coordinate.
 *
 * @return  @c true, if the move was successful. Otherwise, @c false.
 */
boolean P_TryMove3f(mobj_t* thing, float x, float y, float z)
{
    float oldZ = thing->pos[VZ];

    // Go to the new Z height.
    thing->pos[VZ] = z;

#ifdef __JHEXEN__
    if(P_TryMove(thing, x, y))
#else
    if(P_TryMove(thing, x, y, false, false))
#endif
    {
        // The move was successful.
        return true;
    }

    // The move failed, so restore the original position.
    thing->pos[VZ] = oldZ;
    return false;
}

/**
 * \fixme DJS - This routine has gotten way too big, split if(in->isaline)
 * to a seperate routine?
 */
int PTR_ShootTraverse(const intercept_t* in, void* paramaters)
{
#if __JHEXEN__
    extern mobj_t lavaInflictor;
#endif

    int                 divisor;
    float               pos[3], frac, slope, dist, thingTopSlope,
                        thingBottomSlope, cTop, cBottom, d[3], step,
                        stepv[3], tracePos[3], cFloor, cCeil;
    linedef_t*          li;
    mobj_t*             th;
    divline_t*          trace =
        (divline_t *) DD_GetVariable(DD_TRACE_ADDRESS);
    sector_t*           frontSec = NULL, *backSec = NULL;
    subsector_t* contact, *originSub;
    xline_t*            xline;
    boolean             lineWasHit;

    tracePos[VX] = FIX2FLT(trace->pos[VX]);
    tracePos[VY] = FIX2FLT(trace->pos[VY]);
    tracePos[VZ] = shootZ;

    if(in->type == ICPT_LINE)
    {
        li = in->d.lineDef;
        xline = P_ToXLine(li);

        frontSec = P_GetPtrp(li, DMU_FRONT_SECTOR);
        backSec = P_GetPtrp(li, DMU_BACK_SECTOR);

        if(!backSec && P_PointOnLinedefSideXY(tracePos[VX], tracePos[VY], li))
            return false; // Continue traversal.

        if(xline->special)
            P_ActivateLine(li, shootThing, 0, SPAC_IMPACT);

        if(!backSec)
            goto hitline;

#if __JDOOM64__
        if(xline->flags & ML_BLOCKALL) // jd64
            goto hitline;
#endif

        // Crosses a two sided line.
        P_LineOpening(li);
        dist = attackRange * in->distance;

        if(P_GetFloatp(frontSec, DMU_FLOOR_HEIGHT) !=
           P_GetFloatp(backSec, DMU_FLOOR_HEIGHT))
        {
            slope = (OPENBOTTOM - tracePos[VZ]) / dist;
            if(slope > aimSlope)
                goto hitline;
        }

        if(P_GetFloatp(frontSec, DMU_CEILING_HEIGHT) !=
           P_GetFloatp(backSec, DMU_CEILING_HEIGHT))
        {
            slope = (OPENTOP - tracePos[VZ]) / dist;
            if(slope < aimSlope)
                goto hitline;
        }

        // Shot continues...
        return false;

        // Hit a line.
      hitline:

        // Position a bit closer.
        frac = in->distance - (4 / attackRange);
        pos[VX] = tracePos[VX] + (FIX2FLT(trace->dX) * frac);
        pos[VY] = tracePos[VY] + (FIX2FLT(trace->dY) * frac);
        pos[VZ] = tracePos[VZ] + (aimSlope * (frac * attackRange));

        if(backSec)
        {
            // Is it a sky hack wall? If the hitpoint is beyond the visible
            // surface, no puff must be shown.
            if((P_GetIntp(P_GetPtrp(frontSec, DMU_CEILING_MATERIAL),
                            DMU_FLAGS) & MATF_SKYMASK) &&
               (pos[VZ] > P_GetFloatp(frontSec, DMU_CEILING_HEIGHT) ||
                pos[VZ] > P_GetFloatp(backSec, DMU_CEILING_HEIGHT)))
                return true;

            if((P_GetIntp(P_GetPtrp(backSec, DMU_FLOOR_MATERIAL),
                            DMU_FLAGS) & MATF_SKYMASK) &&
               (pos[VZ] < P_GetFloatp(frontSec, DMU_FLOOR_HEIGHT) ||
                pos[VZ] < P_GetFloatp(backSec, DMU_FLOOR_HEIGHT)))
                return true;
        }

        lineWasHit = true;

        // This is the subsector where the trace originates.
        originSub = P_SubsectorAtPointXY(tracePos[VX], tracePos[VY]);

        d[VX] = pos[VX] - tracePos[VX];
        d[VY] = pos[VY] - tracePos[VY];
        d[VZ] = pos[VZ] - tracePos[VZ];

        if(!INRANGE_OF(d[VZ], 0, .0001f)) // Epsilon
        {
            contact = P_SubsectorAtPointXY(pos[VX], pos[VY]);
            step = P_ApproxDistance3(d[VX], d[VY], d[VZ]);
            stepv[VX] = d[VX] / step;
            stepv[VY] = d[VY] / step;
            stepv[VZ] = d[VZ] / step;

            cFloor = P_GetFloatp(contact, DMU_FLOOR_HEIGHT);
            cCeil = P_GetFloatp(contact, DMU_CEILING_HEIGHT);
            // Backtrack until we find a non-empty sector.
            while(cCeil <= cFloor && contact != originSub)
            {
                d[VX] -= 8 * stepv[VX];
                d[VY] -= 8 * stepv[VY];
                d[VZ] -= 8 * stepv[VZ];
                pos[VX] = tracePos[VX] + d[VX];
                pos[VY] = tracePos[VY] + d[VY];
                pos[VZ] = tracePos[VZ] + d[VZ];
                contact = P_SubsectorAtPointXY(pos[VX], pos[VY]);
            }

            // Should we backtrack to hit a plane instead?
            cTop = cCeil - 4;
            cBottom = cFloor + 4;
            divisor = 2;

            // We must not hit a sky plane.
            if(pos[VZ] > cTop &&
               (P_GetIntp(P_GetPtrp(contact, DMU_CEILING_MATERIAL),
                            DMU_FLAGS) & MATF_SKYMASK))
                return true;

            if(pos[VZ] < cBottom &&
               (P_GetIntp(P_GetPtrp(contact, DMU_FLOOR_MATERIAL),
                            DMU_FLAGS) & MATF_SKYMASK))
                return true;

            // Find the approximate hitpoint by stepping back and
            // forth using smaller and smaller steps.
            while((pos[VZ] > cTop || pos[VZ] < cBottom) && divisor <= 128)
            {
                // We aren't going to hit a line any more.
                lineWasHit = false;

                // Take a step backwards.
                pos[VX] -= d[VX] / divisor;
                pos[VY] -= d[VY] / divisor;
                pos[VZ] -= d[VZ] / divisor;

                // Divisor grows.
                divisor *= 2;

                // Can we get any closer?
                if(FEQUAL(d[VZ] / divisor, 0)) break; // No.

                // Move forward until limits breached.
                while((d[VZ] > 0 && pos[VZ] <= cTop) ||
                      (d[VZ] < 0 && pos[VZ] >= cBottom))
                {
                    pos[VX] += d[VX] / divisor;
                    pos[VY] += d[VY] / divisor;
                    pos[VZ] += d[VZ] / divisor;
                }
            }
        }

        // Spawn bullet puffs.
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ], P_Random() << 24);

#if !__JHEXEN__
        if(lineWasHit && xline->special)
        {
            // Extended shoot events only happen when the bullet actually
            // hits the line.
            XL_ShootLine(li, 0, shootThing);
        }
/*
if(lineWasHit)
    Con_Message("Hit line [%i,%i]\n", P_GetIntp(li, DMU_SIDEDEF0), P_GetIntp(li, DMU_SIDEDEF1));
*/
#endif
        // Don't go any farther.
        return true;
    }

    // Shot a mobj.
    th = in->d.mobj;
    if(th == shootThing)
        return false; // Can't shoot self.

    if(!(th->flags & MF_SHOOTABLE))
        return false; // Corpse or something.

#if __JHERETIC__
    // Check for physical attacks on a ghost.
    if((th->flags & MF_SHADOW) && shootThing->player->readyWeapon == WT_FIRST)
        return false;
#endif

    // Check angles to see if the thing can be aimed at
    dist = attackRange * in->distance;
    {
    float               dz = th->pos[VZ];

    if(!(th->player && (th->player->plr->flags & DDPF_CAMERA)))
        dz += th->height;
    dz -= tracePos[VZ];

    thingTopSlope = dz / dist;
    }

    if(thingTopSlope < aimSlope)
        return false; // Shot over the thing.

    thingBottomSlope = (th->pos[VZ] - tracePos[VZ]) / dist;
    if(thingBottomSlope > aimSlope)
        return false; // Shot under the thing.

    // Hit thing.

    // Position a bit closer.
    frac = in->distance - (10 / attackRange);

    pos[VX] = tracePos[VX] + (FIX2FLT(trace->dX) * frac);
    pos[VY] = tracePos[VY] + (FIX2FLT(trace->dY) * frac);
    pos[VZ] = tracePos[VZ] + (aimSlope * (frac * attackRange));

    // Spawn bullet puffs or blood spots, depending on target type.
#if __JHERETIC__
    if(puffType == MT_BLASTERPUFF1)
    {   // Make blaster big puff.
        mobj_t*             mo;

        if((mo = P_SpawnMobj3fv(MT_BLASTERPUFF2, pos, P_Random() << 24, 0)))
            S_StartSound(SFX_BLSHIT, mo);
    }
    else
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ], P_Random() << 24);
#elif __JHEXEN__
    P_SpawnPuff(pos[VX], pos[VY], pos[VZ], P_Random() << 24);
#endif

    if(lineAttackDamage)
    {
        int                 damageDone;
#if __JDOOM__ || __JDOOM64__
        angle_t             attackAngle =
            R_PointToAngle2(shootThing->pos[VX], shootThing->pos[VY],
                            pos[VX], pos[VY]);
#endif

#if __JHEXEN__
        if(PuffType == MT_FLAMEPUFF2)
        {   // Cleric FlameStrike does fire damage.
            damageDone = P_DamageMobj(th, &lavaInflictor, shootThing,
                                      lineAttackDamage, false);
        }
        else
#endif
        {
            damageDone = P_DamageMobj(th, shootThing, shootThing,
                                      lineAttackDamage, false);
        }

#if __JHEXEN__
        if(!(in->d.mobj->flags2 & MF2_INVULNERABLE))
#endif
        {
            if(!(in->d.mobj->flags & MF_NOBLOOD))
            {
                if(damageDone > 0)
                {   // Damage was inflicted, so shed some blood.
#if __JDOOM__ || __JDOOM64__
                    P_SpawnBlood(pos[VX], pos[VY], pos[VZ], lineAttackDamage,
                                 attackAngle + ANG180);
#else
# if __JHEXEN__
                    if(PuffType == MT_AXEPUFF || PuffType == MT_AXEPUFF_GLOW)
                    {
                        P_SpawnBloodSplatter2(pos[VX], pos[VY], pos[VZ], in->d.mobj);
                    }
                    else
# endif
                    if(P_Random() < 192)
                        P_SpawnBloodSplatter(pos[VX], pos[VY], pos[VZ], in->d.mobj);
#endif
                }
            }
#if __JDOOM__ || __JDOOM64__
            else
                P_SpawnPuff(pos[VX], pos[VY], pos[VZ], P_Random() << 24);
#endif
        }
    }

    // Don't go any farther.
    return true;
}

/**
 * Sets linetarget and aimSlope when a target is aimed at.
 */
int PTR_AimTraverse(const intercept_t* in, void* paramaters)
{
    float               slope, thingTopSlope, thingBottomSlope, dist;
    mobj_t*             th;
    linedef_t*          li;
    sector_t*           backSec, *frontSec;

    if(in->type == ICPT_LINE)
    {
        float               fFloor, bFloor;
        float               fCeil, bCeil;

        li = in->d.lineDef;

        if(!(frontSec = P_GetPtrp(li, DMU_FRONT_SECTOR)) ||
           !(backSec  = P_GetPtrp(li, DMU_BACK_SECTOR)))
        {
            float               tracePos[3];
            divline_t*          trace =
                (divline_t *) DD_GetVariable(DD_TRACE_ADDRESS);

            tracePos[VX] = FIX2FLT(trace->pos[VX]);
            tracePos[VY] = FIX2FLT(trace->pos[VY]);
            tracePos[VZ] = shootZ;

            return !P_PointOnLinedefSideXY(tracePos[VX], tracePos[VY], li);
        }

        // Crosses a two sided line.
        // A two sided line will restrict the possible target ranges.
        P_LineOpening(li);

        if(OPENBOTTOM >= OPENTOP)
            return true; // Stop.

        dist = attackRange * in->distance;

        fFloor = P_GetFloatp(frontSec, DMU_FLOOR_HEIGHT);
        fCeil = P_GetFloatp(frontSec, DMU_CEILING_HEIGHT);

        bFloor = P_GetFloatp(backSec, DMU_FLOOR_HEIGHT);
        bCeil = P_GetFloatp(backSec, DMU_CEILING_HEIGHT);

        if(fFloor != bFloor)
        {
            slope = (OPENBOTTOM - shootZ) / dist;
            if(slope > bottomSlope)
                bottomSlope = slope;
        }

        if(fCeil != bCeil)
        {
            slope = (OPENTOP - shootZ) / dist;
            if(slope < topSlope)
                topSlope = slope;
        }

        if(topSlope <= bottomSlope)
            return true; // Stop.

        return false; // Shot continues...
    }

    // Shot a mobj.
    th = in->d.mobj;
    if(th == shootThing)
        return false; // Can't shoot self.

    if(!(th->flags & MF_SHOOTABLE))
        return false; // Corpse or something?

#if __JHERETIC__
    if(th->type == MT_POD)
        return false; // Can't auto-aim at pods.
#endif

#if __JDOOM__ || __JHEXEN__ || __JDOOM64__
    if(th->player && IS_NETGAME && !deathmatch)
        return false; // Don't aim at fellow co-op players.
#endif

    // Check angles to see if the thing can be aimed at.
    dist = attackRange * in->distance;
    {
    float               posZ = th->pos[VZ];

    if(!(th->player && (th->player->plr->flags & DDPF_CAMERA)))
        posZ += th->height;

    thingTopSlope = (posZ - shootZ) / dist;

    if(thingTopSlope < bottomSlope)
        return false; // Shot over the thing.

    // Too far below?
    // $addtocfg $limitautoaimZ:
#if __JHEXEN__
    if(posZ < shootZ - attackRange / 1.2f)
        return false;
#endif
    }

    thingBottomSlope = (th->pos[VZ] - shootZ) / dist;
    if(thingBottomSlope > topSlope)
        return false; // Shot under the thing.

    // Too far above?
    // $addtocfg $limitautoaimZ:
#if __JHEXEN__
    if(th->pos[VZ] > shootZ + attackRange / 1.2f)
        return false;
#endif

    // This thing can be hit!
    if(thingTopSlope > topSlope)
        thingTopSlope = topSlope;

    if(thingBottomSlope < bottomSlope)
        thingBottomSlope = bottomSlope;

    aimSlope = (thingTopSlope + thingBottomSlope) / 2;
    lineTarget = th;

    return true; // Don't go any farther.
}

float P_AimLineAttack(mobj_t* t1, angle_t angle, float distance)
{
    float pos[2];
    uint an = angle >> ANGLETOFINESHIFT;

    pos[VX] = t1->pos[VX] + distance * FIX2FLT(finecosine[an]);
    pos[VY] = t1->pos[VY] + distance * FIX2FLT(finesine[an]);

    // Determine the z trace origin.
    shootZ = t1->pos[VZ];
#if __JHEXEN__
    if(t1->player &&
      (t1->player->class_ == PCLASS_FIGHTER ||
       t1->player->class_ == PCLASS_CLERIC ||
       t1->player->class_ == PCLASS_MAGE))
#else
    if(t1->player && t1->type == MT_PLAYER)
#endif
    {
        if(!(t1->player->plr->flags & DDPF_CAMERA))
            shootZ += (cfg.plrViewHeight - 5);
    }
    else
        shootZ += (t1->height / 2) + 8;

    topSlope = 100.0/160;
    bottomSlope = -100.0/160;
    attackRange = distance;
    lineTarget = NULL;
    shootThing = t1;

    P_PathXYTraverse(t1->pos[VX], t1->pos[VY], pos[VX], pos[VY],
                   PT_ADDLINES | PT_ADDMOBJS, PTR_AimTraverse);

    if(lineTarget)
    {   // While autoaiming, we accept this slope.
        if(!t1->player || !cfg.noAutoAim)
            return aimSlope;
    }

    if(t1->player && cfg.noAutoAim)
    {   // The slope is determined by lookdir.
        return tan(LOOKDIR2RAD(t1->dPlayer->lookDir)) / 1.2;
    }

    return 0;
}

/**
 * If damage == 0, it is just a test trace that will leave lineTarget set.
 */
void P_LineAttack(mobj_t* t1, angle_t angle, float distance, float slope,
                  int damage)
{
    uint                an;
    float               targetPos[2];

    an = angle >> ANGLETOFINESHIFT;
    shootThing = t1;
    lineAttackDamage = damage;

    targetPos[VX] = t1->pos[VX] + distance * FIX2FLT(finecosine[an]);
    targetPos[VY] = t1->pos[VY] + distance * FIX2FLT(finesine[an]);

    // Determine the z trace origin.
    shootZ = t1->pos[VZ];
#if __JHEXEN__
    if(t1->player &&
      (t1->player->class_ == PCLASS_FIGHTER ||
       t1->player->class_ == PCLASS_CLERIC ||
       t1->player->class_ == PCLASS_MAGE))
#else
    if(t1->player && t1->type == MT_PLAYER)
#endif
    {
        if(!(t1->player->plr->flags & DDPF_CAMERA))
            shootZ += cfg.plrViewHeight - 5;
    }
    else
        shootZ += (t1->height / 2) + 8;

    shootZ -= t1->floorClip;
    attackRange = distance;
    aimSlope = slope;

    if(!P_PathXYTraverse(t1->pos[VX], t1->pos[VY], targetPos[VX], targetPos[VY],
                      PT_ADDLINES | PT_ADDMOBJS, PTR_ShootTraverse))
    {
#if __JHEXEN__
        switch(PuffType)
        {
        case MT_PUNCHPUFF:
            S_StartSound(SFX_FIGHTER_PUNCH_MISS, t1);
            break;

        case MT_HAMMERPUFF:
        case MT_AXEPUFF:
        case MT_AXEPUFF_GLOW:
            S_StartSound(SFX_FIGHTER_HAMMER_MISS, t1);
            break;

        case MT_FLAMEPUFF:
            P_SpawnPuff(targetPos[VX], targetPos[VY],
                        shootZ + (slope * distance), P_Random() << 24);
            break;

        default:
            break;
        }
#endif
    }
}

/**
 * "bombSource" is the creature that caused the explosion at "bombSpot".
 */
int PIT_RadiusAttack(mobj_t* thing, void* data)
{
    float dx, dy, dz, dist;

    if(!(thing->flags & MF_SHOOTABLE))
        return false;

    // Boss spider and cyborg take no damage from concussion.
#if __JHERETIC__
    if(thing->type == MT_MINOTAUR || thing->type == MT_SORCERER1 ||
       thing->type == MT_SORCERER2)
        return false;
#elif __JDOOM__ || __JDOOM64__
    if(thing->type == MT_CYBORG)
        return false;
# if __JDOOM__
    if(thing->type == MT_SPIDER)
        return false;
# endif
#endif

#if __JHEXEN__
    if(!damageSource && thing == bombSource) // Don't damage the source of the explosion.
        return false;
#endif

    dx = fabs(thing->pos[VX] - bombSpot->pos[VX]);
    dy = fabs(thing->pos[VY] - bombSpot->pos[VY]);
    dz = fabs((thing->pos[VZ] + thing->height / 2) - bombSpot->pos[VZ]);

    dist = (dx > dy? dx : dy);

#if __JHEXEN__
    if(!cfg.netNoMaxZRadiusAttack)
        dist = (dz > dist? dz : dist);
#else
    if(!(cfg.netNoMaxZRadiusAttack || (thing->info->flags2 & MF2_INFZBOMBDAMAGE)))
        dist = (dz > dist? dz : dist);
#endif

    dist = (dist - thing->radius);

    if(dist < 0)
        dist = 0;

    if(dist >= bombDistance)
        return false; // Out of range.

    // Must be in direct path.
    if(P_CheckSight(thing, bombSpot))
    {
        int damage;

        damage = (bombDamage * (bombDistance - dist) / bombDistance) + 1;
#if __JHEXEN__
        if(thing->player)
            damage /= 4;
#endif
        P_DamageMobj(thing, bombSpot, bombSource, damage, false);
    }

    return false;
}

/**
 * Source is the creature that caused the explosion at spot.
 */
#if __JHEXEN__
void P_RadiusAttack(mobj_t* spot, mobj_t* source, int damage, int distance,
                    boolean canDamageSource)
#else
void P_RadiusAttack(mobj_t* spot, mobj_t* source, int damage, int distance)
#endif
{
    float               dist;
    AABoxf box;

    dist = distance + MAXRADIUS;

    box.minX = spot->pos[VX] - dist;
    box.minY = spot->pos[VY] - dist;
    box.maxX = spot->pos[VX] + dist;
    box.maxY = spot->pos[VY] + dist;

    bombSpot = spot;
    bombDamage = damage;
    bombDistance = distance;

#if __JHERETIC__
    if(spot->type == MT_POD && spot->target)
        bombSource = spot->target;
    else
#endif
        bombSource = source;

#if __JHEXEN__
    damageSource = canDamageSource;
#endif
    VALIDCOUNT++;
    P_MobjsBoxIterator(&box, PIT_RadiusAttack, 0);
}

int PTR_UseTraverse(const intercept_t* in, void* paramaters)
{
    int                 side;
    xline_t*            xline;

    if(in->type != ICPT_LINE)
        return false; // Continue iteration.

    xline = P_ToXLine(in->d.lineDef);

    if(!xline->special)
    {
        P_LineOpening(in->d.lineDef);
        if(OPENRANGE <= 0)
        {
            if(useThing->player)
                S_StartSound(PCLASS_INFO(useThing->player->class_)->failUseSound,
                             useThing);

            return true; // Can't use through a wall.
        }

#if __JHEXEN__
        if(useThing->player)
        {
            float       pheight = useThing->pos[VZ] + useThing->height/2;

            if((OPENTOP < pheight) || (OPENBOTTOM > pheight))
                S_StartSound(PCLASS_INFO(useThing->player->class_)->failUseSound,
                             useThing);
        }
#endif
        // Not a special line, but keep checking.
        return false;
    }

    side = 0;
    if(1 == P_PointOnLinedefSideXY(useThing->pos[VX], useThing->pos[VY],
                              in->d.lineDef))
        side = 1;

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    if(side == 1)
        return true;       // don't use back side
#endif

    P_ActivateLine(in->d.lineDef, useThing, side, SPAC_USE);

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    // Can use multiple line specials in a row with the PassThru flag.
    if(xline->flags & ML_PASSUSE)
        return false;
#endif
    // Can't use more than one special line in a row.
    return true;
}

/**
 * Looks for special lines in front of the player to activate.
 *
 * @param player        The player to test.
 */
void P_UseLines(player_t* player)
{
    uint                an;
    float               pos[3];
    mobj_t*             mo;

    if(IS_CLIENT)
    {
#ifdef _DEBUG
        Con_Message("P_UseLines: Sending a use request for player %i.\n", (int) (player - players));
#endif
        NetCl_PlayerActionRequest(player, GPA_USE, 0);
        return;
    }

    useThing = mo = player->plr->mo;

    an = mo->angle >> ANGLETOFINESHIFT;

    pos[VX] = mo->pos[VX];
    pos[VY] = mo->pos[VY];
    pos[VZ] = mo->pos[VZ];

    pos[VX] += USERANGE * FIX2FLT(finecosine[an]);
    pos[VY] += USERANGE * FIX2FLT(finesine[an]);

    P_PathXYTraverse(mo->pos[VX], mo->pos[VY], pos[VX], pos[VY],
                   PT_ADDLINES, PTR_UseTraverse);
}

/**
 * Takes a valid thing and adjusts the thing->floorZ, thing->ceilingZ,
 * and possibly thing->pos[VZ].
 *
 * This is called for all nearby monsters whenever a sector changes height
 * If the thing doesn't fit, the z will be set to the lowest value and
 * false will be returned
 *
 * @param thing         The mobj whoose position to adjust.
 * @return              @c true, if the thing did fit.
 */
static boolean P_ThingHeightClip(mobj_t* thing)
{
    boolean             onfloor;

    if(P_MobjIsCamera(thing))
        return false; // Don't height clip cameras.

    onfloor = (thing->pos[VZ] == thing->floorZ)? true : false;
    P_CheckPosition3fv(thing, thing->pos);

    thing->floorZ = tmFloorZ;
    thing->ceilingZ = tmCeilingZ;
#if !__JHEXEN__
    thing->dropOffZ = tmDropoffZ; // $dropoff_fix: remember dropoffs.
#endif

    if(onfloor)
    {
#if __JHEXEN__
        if((thing->pos[VZ] - thing->floorZ < 9) ||
           (thing->flags & MF_NOGRAVITY))
        {
            thing->pos[VZ] = thing->floorZ;
        }
#else
        // Update view offset of real players $voodoodolls.
        if(thing->player && thing->player->plr->mo == thing)
            thing->player->viewZ += thing->floorZ - thing->pos[VZ];

        // Walking monsters rise and fall with the floor.
        thing->pos[VZ] = thing->floorZ;

        // $dropoff_fix: Possibly upset balance of objects hanging off ledges.
        if((thing->intFlags & MIF_FALLING) && thing->gear >= MAXGEAR)
            thing->gear = 0;
#endif
    }
    else
    {
        // Don't adjust a floating monster unless forced to.
        if(thing->pos[VZ] + thing->height > thing->ceilingZ)
            thing->pos[VZ] = thing->ceilingZ - thing->height;
    }

    if(thing->ceilingZ - thing->floorZ >= thing->height)
        return true;

    return false;
}

/**
 * Allows the player to slide along any angled walls by adjusting the
 * xmove / ymove so that the NEXT move will slide along the wall.
 *
 * @param ld            The line being slid along.
 */
static void P_HitSlideLine(linedef_t* ld)
{
    int                 side;
    unsigned int        an;
    angle_t             lineAngle, moveAngle, deltaAngle;
    float               moveLen, newLen, d1[2];
    slopetype_t         slopeType = P_GetIntp(ld, DMU_SLOPE_TYPE);

    if(slopeType == ST_HORIZONTAL)
    {
        tmMove[MY] = 0;
        return;
    }
    else if(slopeType == ST_VERTICAL)
    {
        tmMove[MX] = 0;
        return;
    }

    side = P_PointOnLinedefSideXY(slideMo->pos[VX], slideMo->pos[VY], ld);
    P_GetFloatpv(ld, DMU_DXY, d1);
    lineAngle = R_PointToAngle2(0, 0, d1[0], d1[1]);
    moveAngle = R_PointToAngle2(0, 0, tmMove[MX], tmMove[MY]);

    if(side == 1)
        lineAngle += ANG180;
    deltaAngle = moveAngle - lineAngle;
    if(deltaAngle > ANG180)
        deltaAngle += ANG180;

    moveLen = P_ApproxDistance(tmMove[MX], tmMove[MY]);
    an = deltaAngle >> ANGLETOFINESHIFT;
    newLen = moveLen * FIX2FLT(finecosine[an]);

    an = lineAngle >> ANGLETOFINESHIFT;
    tmMove[MX] = newLen * FIX2FLT(finecosine[an]);
    tmMove[MY] = newLen * FIX2FLT(finesine[an]);
}

int PTR_SlideTraverse(const intercept_t* in, void* paramaters)
{
    linedef_t*          li;

    if(in->type != ICPT_LINE)
        Con_Error("PTR_SlideTraverse: Not a line?");

    li = in->d.lineDef;

    if(!P_GetPtrp(li, DMU_FRONT_SECTOR) || !P_GetPtrp(li, DMU_BACK_SECTOR))
    {
        if(P_PointOnLinedefSideXY(slideMo->pos[VX], slideMo->pos[VY], li))
            return false; // Don't hit the back side.

        goto isblocking;
    }

#if __JDOOM64__
    if(P_ToXLine(li)->flags & ML_BLOCKALL) // jd64
        goto isblocking;
#endif

    P_LineOpening(li); // Set OPENRANGE, OPENTOP, OPENBOTTOM...

    if(OPENRANGE < slideMo->height)
        goto isblocking; // Doesn't fit.

    if(OPENTOP - slideMo->pos[VZ] < slideMo->height)
        goto isblocking; // mobj is too high.

    if(OPENBOTTOM - slideMo->pos[VZ] > 24)
        goto isblocking; // Too big a step up.

    // This line doesn't block movement.
    return false;

    // The line does block movement, see if it is closer than best so far.
  isblocking:
    if(in->distance < bestSlideDistance)
    {
        secondSlideDistance = bestSlideDistance;
        secondSlideLine = bestSlideLine;
        bestSlideDistance = in->distance;
        bestSlideLine = li;
    }

    return true; // Stop.
}

/**
 * \fixme The momx / momy move is bad, so try to slide along a wall.
 * Find the first line hit, move flush to it, and slide along it
 *
 * This is a kludgy mess.
 *
 * @param mo            The mobj to attempt the slide move.
 */
void P_SlideMove(mobj_t* mo)
{
    float oldPos[2] = { mo->pos[VX], mo->pos[VY] };
    int hitcount = 3;

    slideMo = mo;

    do
    {
        float leadpos[3], trailpos[3], newPos[3];

        if(--hitcount == 0)
            goto stairstep; // Don't loop forever.

        // Trace along the three leading corners.
        leadpos[VX] = trailpos[VX] = mo->pos[VX];
        leadpos[VY] = trailpos[VY] = mo->pos[VY];
        leadpos[VZ] = trailpos[VZ] = mo->pos[VZ];

        if(mo->mom[MX] > 0)
        {
            leadpos[VX] += mo->radius;
            trailpos[VX] -= mo->radius;
        }
        else
        {
            leadpos[VX] -= mo->radius;
            trailpos[VX] += mo->radius;
        }

        if(mo->mom[MY] > 0)
        {
            leadpos[VY] += mo->radius;
            trailpos[VY] -= mo->radius;
        }
        else
        {
            leadpos[VY] -= mo->radius;
            trailpos[VY] += mo->radius;
        }

        bestSlideDistance = 1;

        P_PathXYTraverse(leadpos[VX], leadpos[VY],
                       leadpos[VX] + mo->mom[MX], leadpos[VY] + mo->mom[MY],
                       PT_ADDLINES, PTR_SlideTraverse);
        P_PathXYTraverse(trailpos[VX], leadpos[VY],
                       trailpos[VX] + mo->mom[MX], leadpos[VY] + mo->mom[MY],
                       PT_ADDLINES, PTR_SlideTraverse);
        P_PathXYTraverse(leadpos[VX], trailpos[VY],
                       leadpos[VX] + mo->mom[MX], trailpos[VY] + mo->mom[MY],
                       PT_ADDLINES, PTR_SlideTraverse);

        // Move up to the wall.
        if(bestSlideDistance == 1)
        {   // The move must have hit the middle, so stairstep. $dropoff_fix
          stairstep:
            /**
             * Ideally we would set the directional momentum of the mobj to zero
             * here should a move fail (to prevent noticeable stuttering against
             * the blocking surface/thing). However due to the mechanics of the
             * wall side algorithm this is not possible as it results in highly
             * unpredictable behaviour and resulting in the player sling-shoting
             * away from the wall.
             */
#if __JHEXEN__
            if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY] + mo->mom[MY]))
                P_TryMove(mo, mo->pos[VX] + mo->mom[MX], mo->pos[VY]);
#else
            if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY] + mo->mom[MY], true, true))
                P_TryMove(mo, mo->pos[VX] + mo->mom[MX], mo->pos[VY], true, true);
#endif
            break;
        }

        // Fudge a bit to make sure it doesn't hit.
        bestSlideDistance -= (1.0f / 32);
        if(bestSlideDistance > 0)
        {
            newPos[VX] = mo->mom[MX] * bestSlideDistance;
            newPos[VY] = mo->mom[MY] * bestSlideDistance;
            newPos[VZ] = DDMAXFLOAT; // Just initialize with *something*.

            // $dropoff_fix: Allow objects to drop off ledges
#if __JHEXEN__
            if(!P_TryMove(mo, mo->pos[VX] + newPos[VX], mo->pos[VY] + newPos[VY]))
                goto stairstep;
#else
            if(!P_TryMove(mo, mo->pos[VX] + newPos[VX], mo->pos[VY] + newPos[VY],
                          true, true))
                goto stairstep;
#endif
        }

        // Now continue along the wall.
        // First calculate remainder.
        bestSlideDistance = 1 - (bestSlideDistance + (1.0f / 32));
        if(bestSlideDistance > 1)
            bestSlideDistance = 1;
        if(bestSlideDistance <= 0)
            break;

        tmMove[MX] = mo->mom[MX] * bestSlideDistance;
        tmMove[MY] = mo->mom[MY] * bestSlideDistance;

        P_HitSlideLine(bestSlideLine); // Clip the move.

        mo->mom[MX] = tmMove[MX];
        mo->mom[MY] = tmMove[MY];

    // $dropoff_fix: Allow objects to drop off ledges:
#if __JHEXEN__
    } while(!P_TryMove(mo, mo->pos[VX] + tmMove[MX],
                           mo->pos[VY] + tmMove[MY]));
#else
    } while(!P_TryMove(mo, mo->pos[VX] + tmMove[MX],
                           mo->pos[VY] + tmMove[MY], true, true));
#endif

#ifdef _DEBUG
    // Didn't move?
    if(mo->player && mo->pos[VX] == oldPos[VX] && mo->pos[VY] == oldPos[VY])
    {
        Con_Message("P_SlideMove: Mobj pos stays the same.\n");
    }
#endif
}

/**
 * SECTOR HEIGHT CHANGING
 * After modifying a sectors floor or ceiling height, call this routine
 * to adjust the positions of all things that touch the sector.
 *
 * If anything doesn't fit anymore, true will be returned.
 * If crunch is true, they will take damage as they are being crushed.
 * If Crunch is false, you should set the sector height back the way it
 * was and call P_ChangeSector again to undo the changes.
 */

/**
 * @param thing         The thing to check against height changes.
 * @param data          Unused.
 */
int PIT_ChangeSector(mobj_t* thing, void* data)
{
    mobj_t*             mo;

    if(!thing->info)
        return false; // Invalid thing?

    // Don't check things that aren't blocklinked (supposedly immaterial).
    if(thing->info->flags & MF_NOBLOCKMAP)
        return false;

    if(P_ThingHeightClip(thing))
        return false; // Keep checking...

    // Crunch bodies to giblets.
#if __JDOOM__ || __JDOOM64__
    if(thing->health <= 0 && !(thing->flags & MF_NOBLOOD))
#elif __JHEXEN__
    if(thing->health <= 0 && (thing->flags & MF_CORPSE))
#else
    if(thing->health <= 0)
#endif
    {
#if __JHEXEN__
        if(thing->flags & MF_NOBLOOD)
        {
            P_MobjRemove(thing, false);
        }
        else
        {
            if(thing->state != &STATES[S_GIBS1])
            {
                P_MobjChangeState(thing, S_GIBS1);
                thing->height = 0;
                thing->radius = 0;
                S_StartSound(SFX_PLAYER_FALLING_SPLAT, thing);
            }
        }
#else
# if __JDOOM64__
        S_StartSound(SFX_SLOP, thing);
# endif

# if __JDOOM__ || __JDOOM64__
        P_MobjChangeState(thing, S_GIBS);
# endif
        thing->flags &= ~MF_SOLID;
        thing->height = 0;
        thing->radius = 0;
#endif
        return false; // Keep checking...
    }

    // Crunch dropped items.
#if __JHEXEN__
    if(thing->flags2 & MF2_DROPPED)
#else
    if(thing->flags & MF_DROPPED)
#endif
    {
        P_MobjRemove(thing, false);
        return false; // Keep checking...
    }

    if(!(thing->flags & MF_SHOOTABLE))
        return false; // Keep checking...

    noFit = true;
    if(crushChange > 0 && !(mapTime & 3))
    {
#if __JHEXEN__
        P_DamageMobj(thing, NULL, NULL, crushChange, false);
#else
        P_DamageMobj(thing, NULL, NULL, 10, false);
#endif

#if __JDOOM__ || __JDOOM64__
        if(!(thing->flags & MF_NOBLOOD))
#elif __JHEXEN__
        if(!(thing->flags & MF_NOBLOOD) &&
           !(thing->flags2 & MF2_INVULNERABLE))
#endif
        {
            // Spray blood in a random direction.
            if((mo = P_SpawnMobj3f(MT_BLOOD, thing->pos[VX], thing->pos[VY],
                                   thing->pos[VZ] + (thing->height /2),
                                   P_Random() << 24, 0)))
            {
                mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 12);
                mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 12);
            }
        }
    }

    return false; // Keep checking (crush other things)...
}

/**
 * @param sector        The sector to check.
 * @param crunch        @c true = crush any things in the sector.
 */
boolean P_ChangeSector(sector_t* sector, boolean crunch)
{
    noFit = false;
    crushChange = crunch;

    VALIDCOUNT++;
    P_SectorTouchingMobjsIterator(sector, PIT_ChangeSector, 0);

    return noFit;
}

/**
 * This is called by the engine when it needs to change sector heights without
 * consulting game logic first. Most commonly this occurs on clientside, where
 * the client needs to apply plane height changes as per the deltas.
 *
 * @param sectorIdx  Index of the sector to update.
 */
void P_HandleSectorHeightChange(int sectorIdx)
{
    P_ChangeSector(P_ToPtr(DMU_SECTOR, sectorIdx), false);
}

/**
 * The following routines originate from the Heretic src!
 */

#if __JHERETIC__ || __JHEXEN__
/**
 * @param mobj          The mobj whoose position to test.
 * @return boolean      @c true, if the mobj is not blocked by anything.
 */
boolean P_TestMobjLocation(mobj_t* mobj)
{
    int                 flags;

    flags = mobj->flags;
    mobj->flags &= ~MF_PICKUP;

    if(P_CheckPosition2f(mobj, mobj->pos[VX], mobj->pos[VY]))
    {
        // XY is ok, now check Z
        mobj->flags = flags;
        if((mobj->pos[VZ] < mobj->floorZ) ||
           (mobj->pos[VZ] + mobj->height > mobj->ceilingZ))
        {
            return false; // Bad Z
        }

        return true;
    }

    mobj->flags = flags;
    return false;
}
#endif

#if __JDOOM64__ || __JHERETIC__
static void CheckMissileImpact(mobj_t* mobj)
{
    int                 size;
    linedef_t*          ld;

    if(IS_CLIENT || !mobj->target || !mobj->target->player || !(mobj->flags & MF_MISSILE))
        return;

    if(!(size = IterList_Size(spechit)))
        return;

    IterList_SetIteratorDirection(spechit, ITERLIST_BACKWARD);
    IterList_RewindIterator(spechit);
    while((ld = IterList_MoveIterator(spechit)) != NULL)
        P_ActivateLine(ld, mobj->target, 0, SPAC_IMPACT);
}
#endif

#if __JHEXEN__
int PIT_ThrustStompThing(mobj_t* thing, void* data)
{
    float               blockdist;

    if(!(thing->flags & MF_SHOOTABLE))
        return false;

    blockdist = thing->radius + tsThing->radius;
    if(fabs(thing->pos[VX] - tsThing->pos[VX]) >= blockdist ||
       fabs(thing->pos[VY] - tsThing->pos[VY]) >= blockdist ||
       (thing->pos[VZ] > tsThing->pos[VZ] + tsThing->height))
        return false; // Didn't hit it.

    if(thing == tsThing)
        return false; // Don't clip against self.

    P_DamageMobj(thing, tsThing, tsThing, 10001, false);
    tsThing->args[1] = 1; // Mark thrust thing as bloody.

    return false;
}

void PIT_ThrustSpike(mobj_t* actor)
{
    float radius;
    AABoxf box;

    tsThing = actor;
    radius = actor->info->radius + MAXRADIUS;

    box.minX = actor->pos[VX] - radius;
    box.minY = actor->pos[VY] - radius;
    box.maxX = actor->pos[VX] + radius;
    box.maxY = actor->pos[VY] + radius;

    // Stomp on any things contacted.
    VALIDCOUNT++;
    P_MobjsBoxIterator(&box, PIT_ThrustStompThing, 0);
}

int PIT_CheckOnmobjZ(mobj_t* thing, void* data)
{
    float               blockdist;

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)))
        return false; // Can't hit thing.

    blockdist = thing->radius + tmThing->radius;
    if(fabs(thing->pos[VX] - tm[VX]) >= blockdist ||
       fabs(thing->pos[VY] - tm[VY]) >= blockdist)
        return false; // Didn't hit thing.

    if(thing == tmThing)
        return false; // Don't clip against self.

    if(tmThing->pos[VZ] > thing->pos[VZ] + thing->height)
        return false;
    else if(tmThing->pos[VZ] + tmThing->height < thing->pos[VZ])
        return false; // Under thing.

    // Players cannot hit their clmobjs.
    if(tmThing->player)
    {
        if(thing == ClPlayer_ClMobj(tmThing->player - players))
            return false;
    }

    if(thing->flags & MF_SOLID)
        onMobj = thing;

    return !!(thing->flags & MF_SOLID);
}

mobj_t* P_CheckOnMobj(mobj_t* thing)
{
    subsector_t*        newSSec;
    float               pos[3];
    mobj_t              oldMo;
    AABoxf tmBoxExpanded;

    if(Mobj_IsPlayerClMobj(thing))
    {
        // Players' clmobjs shouldn't do any on-mobj logic; the real player mobj
        // will interact with (cl)mobjs.
        return NULL;
    }

    //// \fixme Do this properly! Consolidate with how jDoom/jHeretic do on-mobj checks?

    tmThing = thing;
    oldMo = *thing; // Save the old mobj before the fake z movement.

    P_FakeZMovement(tmThing);

    pos[VX] = thing->pos[VX];
    pos[VY] = thing->pos[VY];
    pos[VZ] = thing->pos[VZ];
    tm[VX] = pos[VX];
    tm[VY] = pos[VY];
    tm[VZ] = pos[VZ];

    tmBox.minX = pos[VX] - tmThing->radius;
    tmBox.minY = pos[VY] - tmThing->radius;
    tmBox.maxX = pos[VX] + tmThing->radius;
    tmBox.maxY = pos[VY] + tmThing->radius;

    newSSec = P_SubsectorAtPointXY(pos[VX], pos[VY]);
    ceilingLine = floorLine = NULL;

    // The base floor/ceiling is from the subsector that contains the
    // point. Any contacted lines the step closer together will adjust them.

    tmFloorZ = tmDropoffZ = P_GetFloatp(newSSec, DMU_FLOOR_HEIGHT);
    tmCeilingZ = P_GetFloatp(newSSec, DMU_CEILING_HEIGHT);
    tmFloorMaterial = P_GetPtrp(newSSec, DMU_FLOOR_MATERIAL);

    IterList_Empty(spechit);

    if(tmThing->flags & MF_NOCLIP)
        goto nothingUnderneath;

    // Check things first, possibly picking things up the bounding box is
    // extended by MAXRADIUS because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap into adjacent blocks by
    // up to MAXRADIUS.

    tmBoxExpanded.minX = tmBox.minX - MAXRADIUS;
    tmBoxExpanded.minY = tmBox.minY - MAXRADIUS;
    tmBoxExpanded.maxX = tmBox.maxX + MAXRADIUS;
    tmBoxExpanded.maxY = tmBox.maxY + MAXRADIUS;

    VALIDCOUNT++;
    if(P_MobjsBoxIterator(&tmBoxExpanded, PIT_CheckOnmobjZ, 0))
    {
        *tmThing = oldMo;
        return onMobj;
    }

nothingUnderneath:
    *tmThing = oldMo;
    return NULL;
}

/**
 * Fake the zmovement so that we can check if a move is legal.
 */
static void P_FakeZMovement(mobj_t* mo)
{
    float               dist;
    float               delta;

    if(P_MobjIsCamera(mo))
        return;

    // Adjust height.
    mo->pos[VZ] += mo->mom[MZ];
    if((mo->flags & MF_FLOAT) && mo->target)
    {   // Float down towards target if too close.
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                    mo->pos[VY] - mo->target->pos[VY]);

            delta = mo->target->pos[VZ] + (mo->height / 2) - mo->pos[VZ];

            if(delta < 0 && dist < -(delta * 3))
                mo->pos[VZ] -= FLOATSPEED;
            else if(delta > 0 && dist < (delta * 3))
                mo->pos[VZ] += FLOATSPEED;
        }
    }

    if(mo->player && (mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) &&
       (mapTime & 2))
    {
        mo->pos[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
    }

    // Clip movement.
    if(mo->pos[VZ] <= mo->floorZ) // Hit the floor.
    {
        mo->pos[VZ] = mo->floorZ;
        if(mo->mom[MZ] < 0)
            mo->mom[MZ] = 0;

        if(mo->flags & MF_SKULLFLY)
            mo->mom[MZ] = -mo->mom[MZ]; // The skull slammed into something

        if(P_GetState(mo->type, SN_CRASH) && (mo->flags & MF_CORPSE))
            return;
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(FEQUAL(mo->mom[MZ], 0))
            mo->mom[MZ] = -(P_GetGravity() / 32) * 2;
        else
            mo->mom[MZ] -= P_GetGravity() / 32;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(FEQUAL(mo->mom[MZ], 0))
            mo->mom[MZ] = -P_GetGravity() * 2;
        else
            mo->mom[MZ] -= P_GetGravity();
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingZ) // Hit the ceiling.
    {
        mo->pos[VZ] = mo->ceilingZ - mo->height;

        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        if(mo->flags & MF_SKULLFLY)
            mo->mom[MZ] = -mo->mom[MZ]; // The skull slammed into something.
    }
}

static void checkForPushSpecial(linedef_t* line, int side, mobj_t* mobj)
{
    if(P_ToXLine(line)->special)
    {
        if(mobj->flags2 & MF2_PUSHWALL)
        {
            P_ActivateLine(line, mobj, side, SPAC_PUSH);
        }
        else if(mobj->flags2 & MF2_IMPACT)
        {
            P_ActivateLine(line, mobj, side, SPAC_IMPACT);
        }
    }
}

int PTR_BounceTraverse(const intercept_t* in, void* paramaters)
{
    linedef_t*          li;

    if(in->type != ICPT_LINE)
        Con_Error("PTR_BounceTraverse: Not a line?");

    li = in->d.lineDef;

    if(!P_GetPtrp(li, DMU_FRONT_SECTOR) || !P_GetPtrp(li, DMU_BACK_SECTOR))
    {
        if(P_PointOnLinedefSideXY(slideMo->pos[VX], slideMo->pos[VY], li))
            return false; // Don't hit the back side.

        goto bounceblocking;
    }

    P_LineOpening(li); // Set OPENRANGE, OPENTOP, OPENBOTTOM...

    if(OPENRANGE < slideMo->height)
        goto bounceblocking; // Doesn't fit.

    if(OPENTOP - slideMo->pos[VZ] < slideMo->height)
        goto bounceblocking; // Mobj is too high...

    return false; // This line doesn't block movement...

    // the line does block movement, see if it is closer than best so far.
  bounceblocking:
    if(in->distance < bestSlideDistance)
    {
        secondSlideDistance = bestSlideDistance;
        secondSlideLine = bestSlideLine;
        bestSlideDistance = in->distance;
        bestSlideLine = li;
    }

    return true; // Stop.
}

void P_BounceWall(mobj_t* mo)
{
    int                 side;
    unsigned int        an;
    float               moveLen, leadPos[3], d1[2];
    angle_t             lineAngle, moveAngle, deltaAngle;

    slideMo = mo;

    // Trace along the three leading corners.
    leadPos[VX] = mo->pos[VX];
    leadPos[VY] = mo->pos[VY];
    leadPos[VZ] = mo->pos[VZ];

    if(mo->mom[MX] > 0)
        leadPos[VX] += mo->radius;
    else
        leadPos[VX] -= mo->radius;

    if(mo->mom[MY] > 0)
        leadPos[VY] += mo->radius;
    else
        leadPos[VY] -= mo->radius;

    bestSlideLine = NULL;
    bestSlideDistance = 1;
    P_PathXYTraverse(leadPos[VX], leadPos[VY],
                   leadPos[VX] + mo->mom[MX], leadPos[VY] + mo->mom[MY],
                   PT_ADDLINES, PTR_BounceTraverse);

    if(!bestSlideLine)
        return; // We don't want to crash.

    side = P_PointOnLinedefSideXY(mo->pos[VX], mo->pos[VY], bestSlideLine);
    P_GetFloatpv(bestSlideLine, DMU_DXY, d1);
    lineAngle = R_PointToAngle2(0, 0, d1[0], d1[1]);
    if(side == 1)
        lineAngle += ANG180;

    moveAngle = R_PointToAngle2(0, 0, mo->mom[MX], mo->mom[MY]);
    deltaAngle = (2 * lineAngle) - moveAngle;

    moveLen = P_ApproxDistance(mo->mom[MX], mo->mom[MY]);
    moveLen *= 0.75f; // Friction.

    if(moveLen < 1)
        moveLen = 2;

    an = deltaAngle >> ANGLETOFINESHIFT;
    mo->mom[MX] = moveLen * FIX2FLT(finecosine[an]);
    mo->mom[MY] = moveLen * FIX2FLT(finesine[an]);
}

int PTR_PuzzleItemTraverse(const intercept_t* in, void* paramaters)
{
    switch(in->type)
    {
    case ICPT_LINE: // Linedef.
        {
        linedef_t*          line = in->d.lineDef;
        xline_t*            xline = P_ToXLine(line);

        if(xline->special != USE_PUZZLE_ITEM_SPECIAL)
        {
            P_LineOpening(line);

            if(OPENRANGE <= 0)
            {
                sfxenum_t           sound = SFX_NONE;

                if(puzzleItemUser->player)
                {
                    switch(puzzleItemUser->player->class_)
                    {
                    case PCLASS_FIGHTER:
                        sound = SFX_PUZZLE_FAIL_FIGHTER;
                        break;

                    case PCLASS_CLERIC:
                        sound = SFX_PUZZLE_FAIL_CLERIC;
                        break;

                    case PCLASS_MAGE:
                        sound = SFX_PUZZLE_FAIL_MAGE;
                        break;

                    default:
                        sound = SFX_NONE;
                        break;
                    }
                }

                S_StartSound(sound, puzzleItemUser);
                return true; // Can't use through a wall.
            }

            return false; // Continue searching...
        }

        if(P_PointOnLinedefSideXY(puzzleItemUser->pos[VX],
                                puzzleItemUser->pos[VY], line) == 1)
            return true; // Don't use back sides.

        if(puzzleItemType != xline->arg1)
            return true; // Item type doesn't match.

        P_StartACS(xline->arg2, 0, &xline->arg3, puzzleItemUser, line, 0);
        xline->special = 0;
        puzzleActivated = true;

        return true; // Stop searching.
        }

    case ICPT_MOBJ: { // Mobj.
        mobj_t* mo = in->d.mobj;

        if(mo->special != USE_PUZZLE_ITEM_SPECIAL)
            return false; // Wrong special...

        if(puzzleItemType != mo->args[0])
            return false; // Item type doesn't match...

        P_StartACS(mo->args[1], 0, &mo->args[2], puzzleItemUser, NULL, 0);
        mo->special = 0;
        puzzleActivated = true;

        return true; // Stop searching.
      }
    default:
        Con_Error("PTR_PuzzleItemTraverse: Unknown intercept type %i.", in->type);
        exit(1); // Unreachable.
    }
}

/**
 * See if the specified player can use the specified puzzle item on a
 * thing or line(s) at their current world location.
 *
 * @param player        The player using the puzzle item.
 * @param itemType      The type of item to try to use.
 * @return boolean      true if the puzzle item was used.
 */
boolean P_UsePuzzleItem(player_t* player, int itemType)
{
    int                 angle;
    float               pos1[3], pos2[3];

    puzzleItemType = itemType;
    puzzleItemUser = player->plr->mo;
    puzzleActivated = false;

    angle = player->plr->mo->angle >> ANGLETOFINESHIFT;

    memcpy(pos1, player->plr->mo->pos, sizeof(pos1));
    memcpy(pos2, player->plr->mo->pos, sizeof(pos2));

    pos2[VX] += FIX2FLT(USERANGE * finecosine[angle]);
    pos2[VY] += FIX2FLT(USERANGE * finesine[angle]);

    P_PathXYTraverse(pos1[VX], pos1[VY], pos2[VX], pos2[VY],
                   PT_ADDLINES | PT_ADDMOBJS, PTR_PuzzleItemTraverse);

    if(!puzzleActivated)
    {
        P_SetYellowMessage(player, TXT_USEPUZZLEFAILED, false);
    }

    return puzzleActivated;
}
#endif
