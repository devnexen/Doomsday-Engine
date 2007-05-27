/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 * rend_shadow.c: Map Object Shadows
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int     useShadows = true;
static int     shadowMaxRad = 80;
static int     shadowMaxDist = 1000;
static float   shadowFactor = 0.5f;

// CODE --------------------------------------------------------------------

void Rend_ShadowRegister(void)
{
    // Cvars
    C_VAR_INT("rend-shadow", &useShadows, 0, 0, 1);
    C_VAR_FLOAT("rend-shadow-darkness", &shadowFactor, 0, 0, 1);
    C_VAR_INT("rend-shadow-far", &shadowMaxDist, CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-shadow-radius-max", &shadowMaxRad, CVF_NO_MAX, 0, 0);
}

/**
 * This is called for each sector a shadow-caster is touching.
 * The highest floor height will be searched and if larger than the
 * value of <code>data</code> it will be written back.
 *
 * @param sector    The sector to search the floor height of.
 * @param data      Ptr to a float containing the height to compare.
 *
 * @return          <code>true</code> if iteration should continue.
 */
static boolean Rend_ShadowIterator(sector_t *sector, void *data)
{
    float  *height = data;
    float   floor = sector->SP_floorvisheight;

    if(floor > *height)
        *height = floor;
    return true;                // Continue iteration.
}

static void Rend_ProcessThingShadow(mobj_t *mo)
{
    fixed_t     moz;
    float       height, moh, halfmoh, color, pos[2], floor;
    sector_t   *sec = mo->subsector->sector;
    int         radius;
    uint        i;
    rendpoly_t *poly;
    float       distance;

    // Is this too far?
    pos[VX] = FIX2FLT(mo->pos[VX]);
    pos[VY] = FIX2FLT(mo->pos[VY]);
    if((distance = Rend_PointDist2D(pos)) > shadowMaxDist)
        return;

    // Apply a Short Range Visual Offset?
    if(useSRVO && mo->state && mo->tics >= 0)
    {
        float   mul = mo->tics / (float) mo->state->tics;

        pos[VX] += FIX2FLT(mo->srvo[VX] << 8) * mul;
        pos[VY] += FIX2FLT(mo->srvo[VY] << 8) * mul;
    }

    // Check the height.
    moz = mo->pos[VZ] - FLT2FIX(mo->floorclip);
    if(mo->ddflags & DDMF_BOB)
        moz -= FLT2FIX(R_GetBobOffset(mo));

    height = FIX2FLT(moz) - mo->floorz;
    moh = mo->height;
    if(!moh)
        moh = 1;
    if(height > moh)
        return;                 // Too far.
    if(FIX2FLT(moz) + mo->height < mo->floorz)
        return;

    // Calculate the strength of the shadow.
    // Simplified version, no light diminishing or range compression.
    color =
        shadowFactor * sec->lightlevel *
        (1 - mo->translucency * reciprocal255);

    halfmoh = moh / 2;
    if(height > halfmoh)
        color *= 1 - (height - halfmoh) / (moh - halfmoh);
    if(usingFog)
        color /= 2;
    if(distance > 3 * shadowMaxDist / 4)
    {
        // Fade when nearing the maximum distance.
        color *= (shadowMaxDist - distance) / (shadowMaxDist / 4);
    }
    if(color <= 0)
        return;                 // Can't be seen.
    if(color > 1)
        color = 1;

    // Calculate the radius of the shadow.
    radius = R_VisualRadius(mo);
    if(!radius)
        return;
    if(radius > shadowMaxRad)
        radius = shadowMaxRad;

    // Figure out the visible floor height.
    floor = mo->subsector->sector->SP_floorvisheight;
    P_ThingSectorsIterator(mo, Rend_ShadowIterator, &floor);

    if(floor >= FIX2FLT(moz) + mo->height)
        return; // Can't have a shadow above the object!

    // View height might prevent us from seeing the shadow.
    if(vy < floor)
        return;

    // Prepare the poly.
    poly = R_AllocRendPoly(RP_FLAT, false, 4);
    poly->flags = RPF_SHADOW;
    poly->tex.id = GL_PrepareLSTexture(LST_DYNAMIC, NULL);
    poly->tex.width = poly->tex.height = radius * 2;
    poly->texoffx = -pos[VX] + radius;
    poly->texoffy = -pos[VY] - radius;
    floor += 0.2f;    // A bit above the floor.

    poly->vertices[0].pos[VX] = pos[VX] - radius;
    poly->vertices[0].pos[VY] = pos[VY] + radius;
    poly->vertices[0].pos[VZ] = floor;
    poly->vertices[1].pos[VX] = pos[VX] + radius;
    poly->vertices[1].pos[VY] = pos[VY] + radius;
    poly->vertices[1].pos[VZ] = floor;
    poly->vertices[2].pos[VX] = pos[VX] + radius;
    poly->vertices[2].pos[VY] = pos[VY] - radius;
    poly->vertices[2].pos[VZ] = floor;
    poly->vertices[3].pos[VX] = pos[VX] - radius;
    poly->vertices[3].pos[VY] = pos[VY] - radius;
    poly->vertices[3].pos[VZ] = floor;

    for(i = 0; i < 4; ++i)
    {
        // Shadows are black.
        memset(poly->vertices[i].color.rgba, 0, 3);
        poly->vertices[i].color.rgba[3] = color * 255;
    }

    RL_AddPoly(poly);
    R_FreeRendPoly(poly);
}

void Rend_RenderShadows(void)
{
    sector_t   *sec;
    mobj_t     *mo;
    uint        i;

    if(!useShadows)
        return;

    // Check all mobjs in all visible sectors.
    for(i = 0; i < numsectors; ++i)
    {
        sec = SECTOR_PTR(i);
        if(!(sec->frameflags & SIF_VISIBLE))
            continue;

        // Don't render mobj shadows on sky floors.
        if(R_IsSkySurface(&sec->SP_floorsurface))
            continue;

        for(mo = sec->thinglist; mo; mo = mo->snext)
        {
            if(!mo->state)
                continue;

            // Should this mobj have a shadow?
            if((mo->state->flags & STF_FULLBRIGHT) || (mo->ddflags & DDMF_DONTDRAW) ||
               (mo->ddflags & DDMF_ALWAYSLIT))
                continue;

            Rend_ProcessThingShadow(mo);
        }
    }
}
