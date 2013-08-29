/** @file maputil.h World map utilities.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_WORLD_MAPUTIL_H
#define DENG_WORLD_MAPUTIL_H

#ifdef __CLIENT__

#include <de/binangle.h>

#include <de/Vector>

#include "Line" // LineSide

class Sector;
class LineOwner;

void R_SetRelativeHeights(Sector const *front, Sector const *back, int planeIndex,
    coord_t *fz = 0, coord_t *bz = 0, coord_t *bhz = 0);

/**
 * Determine the map space Z coordinates of a wall section.
 *
 * @param side            Map line side to determine Z heights for.
 * @param section         LineSide section to determine coordinates for.
 * @param skyClip         Perform sky plane clipping to line section.
 *
 * Return values:
 * @param bottom          Z map space coordinate at the bottom of the wall section. Can be @c 0.
 * @param top             Z map space coordinate at the top of the wall section. Can be @c 0.
 * @param materialOrigin  Surface space material coordinate offset. Can be @c 0.
 */
void R_SideSectionCoords(LineSide const &side, int section, bool skyClip = true,
    coord_t *bottom = 0, coord_t *top = 0, de::Vector2f *materialOrigin = 0);

/**
 * @param side  LineSide instance.
 * @param ignoreOpacity  @c true= do not consider Material opacity.
 *
 * @return  @c true if this side is considered "closed" (i.e., there is no opening
 * through which the relative back Sector can be seen). Tests consider all Planes
 * which interface with this and the "middle" Material used on the "this" side.
 */
bool R_SideBackClosed(LineSide const &side, bool ignoreOpacity = true);

/**
 * A neighbour is a line that shares a vertex with 'line', and faces the
 * specified sector.
 */
Line *R_FindLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff = 0);

Line *R_FindSolidLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff = 0);

#endif // __CLIENT__

#endif // DENG_WORLD_MAPUTIL_H
