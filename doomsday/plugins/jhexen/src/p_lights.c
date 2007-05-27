/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "dmu_lib.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static float phaseTable[64] = {
    .5, .4375, .375, .3125, .25, .1875, .125, .125,
    .0625, .0625, .0625, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, .0625, .0625, .0625,
    .125, .125, .1875, .25, .3125, .375, .4375, .5
};

// CODE --------------------------------------------------------------------

void T_Light(light_t *light)
{
    if(light->count)
    {
        light->count--;
        return;
    }

    switch(light->type)
    {
    case LITE_FADE:
        P_SectorModifyLight(light->sector, light->value2);

        if(light->tics2 == 1)
        {
            if(P_SectorLight(light->sector) >= light->value1)
            {
                P_SectorSetLight(light->sector, light->value1);
                P_RemoveThinker(&light->thinker);
            }
        }
        else if(P_SectorLight(light->sector) <= light->value1)
        {
            P_SectorSetLight(light->sector, light->value1);
            P_RemoveThinker(&light->thinker);
        }
        break;

    case LITE_GLOW:
        P_SectorModifyLightx(light->sector, light->tics1);
        if(light->tics2 == 1)
        {
            if(P_SectorLight(light->sector) >= light->value1)
            {
                P_SectorSetLight(light->sector, light->value1);
                light->tics1 = -light->tics1;
                light->tics2 = -1;  // reverse direction
            }
        }
        else if(P_SectorLight(light->sector) <= light->value2)
        {
            P_SectorSetLight(light->sector, light->value2);
            light->tics1 = -light->tics1;
            light->tics2 = 1;   // reverse direction
        }
        break;

    case LITE_FLICKER:
        if(P_SectorLight(light->sector) == light->value1)
        {
            P_SectorSetLight(light->sector, light->value2);
            light->count = (P_Random() & 7) + 1;
        }
        else
        {
            P_SectorSetLight(light->sector, light->value1);
            light->count = (P_Random() & 31) + 1;
        }
        break;

    case LITE_STROBE:
        if(P_SectorLight(light->sector) == light->value1)
        {
            P_SectorSetLight(light->sector, light->value2);
            light->count = light->tics2;
        }
        else
        {
            P_SectorSetLight(light->sector, light->value1);
            light->count = light->tics1;
        }
        break;

    default:
        break;
    }
}

boolean EV_SpawnLight(line_t *line, byte *arg, lighttype_t type)
{
    int         arg1, arg2, arg3, arg4;
    boolean     think = false;
    boolean     rtn = false;
    light_t    *light;
    sector_t   *sec = NULL;
    iterlist_t *list;

    arg1 = (int) arg[1];
    arg2 = (int) arg[2];
    arg3 = (int) arg[3];
    arg4 = (int) arg[4];

    list = P_GetSectorIterListForTag((int) arg[0], false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        think = false;
        rtn = true;

        light = Z_Malloc(sizeof(light_t), PU_LEVSPEC, 0);
        light->type = type;
        light->sector = sec;
        light->count = 0;

        switch (type)
        {
        case LITE_RAISEBYVALUE:
            P_SectorModifyLight(light->sector, (float) arg1 / 255.0f);
            break;

        case LITE_LOWERBYVALUE:
            P_SectorModifyLight(light->sector, -((float) arg1 / 255.0f));
            break;

        case LITE_CHANGETOVALUE:
            P_SectorSetLight(light->sector, (float) arg1 / 255.0f);
            break;

        case LITE_FADE:
            think = true;
            light->value1 = (float) arg1 / 255.0f;   // destination lightlevel
            light->value2 =
                FIX2FLT(FixedDiv((arg1 - (int) (255.0f * P_SectorLight(light->sector))) << FRACBITS,
                         arg2 << FRACBITS)) / 255.0f;  // delta lightlevel
            if(P_SectorLight(light->sector) <= (float) arg1 / 255.0f)
            {
                light->tics2 = 1;   // get brighter
            }
            else
            {
                light->tics2 = -1;
            }
            break;

        case LITE_GLOW:
            think = true;
            light->value1 = (float) arg1 / 255.0f;   // upper lightlevel
            light->value2 = (float) arg2 / 255.0f;   // lower lightlevel
            light->tics1 =
                FixedDiv((arg1 - (int) (255.0f * P_SectorLight(light->sector))) << FRACBITS,
                         arg3 << FRACBITS);   // lightlevel delta
            if(P_SectorLight(light->sector) <= (float) arg1 / 255.0f)
            {
                light->tics2 = 1;   // get brighter
            }
            else
            {
                light->tics2 = -1;
            }
            break;

        case LITE_FLICKER:
            think = true;
            light->value1 = (float) arg1 / 255.0f;   // upper lightlevel
            light->value2 = (float) arg2 / 255.0f;   // lower lightlevel
            P_SectorSetLight(light->sector, light->value1);
            light->count = (P_Random() & 64) + 1;
            break;

        case LITE_STROBE:
            think = true;
            light->value1 = (float) arg1 / 255.0f;   // upper lightlevel
            light->value2 = (float) arg2 / 255.0f;   // lower lightlevel
            light->tics1 = arg3;    // upper tics
            light->tics2 = arg4;    // lower tics
            light->count = arg3;
            P_SectorSetLight(light->sector, light->value1);
            break;

        default:
            rtn = false;
            break;
        }

        if(think)
        {
            P_AddThinker(&light->thinker);
            light->thinker.function = T_Light;
        }
        else
        {
            Z_Free(light);
        }
    }
    return rtn;
}

void T_Phase(phase_t *phase)
{
    phase->index = (phase->index + 1) & 63;
    P_SectorSetLight(phase->sector,
                     phase->baseValue + phaseTable[phase->index]);
}

void P_SpawnPhasedLight(sector_t *sector, float base, int index)
{
    phase_t    *phase;

    phase = Z_Malloc(sizeof(*phase), PU_LEVSPEC, 0);
    P_AddThinker(&phase->thinker);
    phase->sector = sector;
    if(index == -1)
    {                           // sector->lightlevel as the index
        phase->index = (int) (255.0f * P_SectorLight(sector)) & 63;
    }
    else
    {
        phase->index = index & 63;
    }
    phase->baseValue = base;
    P_SectorSetLight(phase->sector,
                     phase->baseValue + phaseTable[phase->index]);
    phase->thinker.function = T_Phase;

    P_XSector(sector)->special = 0;
}

void P_SpawnLightSequence(sector_t *sector, int indexStep)
{
    int         i, seqSpecial, count;
    float       base;
    fixed_t     index, indexDelta;
    sector_t   *sec, *nextSec, *tempSec;

    seqSpecial = LIGHT_SEQUENCE;    // look for Light_Sequence, first
    sec = sector;
    count = 1;
    do
    {
        nextSec = NULL;
        // make sure that the search doesn't back up.
        P_XSector(sec)->special = LIGHT_SEQUENCE_START;
        for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); ++i)
        {
            tempSec = P_GetNextSector(P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i), sec);
            if(!tempSec)
                continue;

            if(P_XSector(tempSec)->special == seqSpecial)
            {
                if(seqSpecial == LIGHT_SEQUENCE)
                {
                    seqSpecial = LIGHT_SEQUENCE_ALT;
                }
                else
                {
                    seqSpecial = LIGHT_SEQUENCE;
                }
                nextSec = tempSec;
                count++;
            }
        }
        sec = nextSec;
    } while(sec);

    sec = sector;
    count *= indexStep;
    index = 0;
    indexDelta = FixedDiv(64 * FRACUNIT, count * FRACUNIT);
    base = P_SectorLight(sector);
    do
    {
        nextSec = NULL;
        if(P_SectorLight(sec))
        {
            base = P_SectorLight(sec);
        }
        P_SpawnPhasedLight(sec, base, index >> FRACBITS);
        index += indexDelta;

        for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); ++i)
        {
            tempSec = P_GetNextSector(P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i), sec);
            if(!tempSec)
                continue;

            if(P_XSector(tempSec)->special == LIGHT_SEQUENCE_START)
            {
                nextSec = tempSec;
            }
        }
        sec = nextSec;
    } while(sec);
}
