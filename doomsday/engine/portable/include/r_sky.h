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
 * r_sky.h: Sky Management
 */

#ifndef __DOOMSDAY_REFRESH_SKY_H__
#define __DOOMSDAY_REFRESH_SKY_H__

#include "r_model.h"

typedef struct skymodel_s {
    ded_skymodel_t *def;
    modeldef_t     *model;
    int             frame;
    int             timer;
    int             maxTimer;
    float           yaw;
} skymodel_t;

extern skymodel_t skyModels[NUM_SKY_MODELS];
extern boolean  skyModelsInited;
extern boolean  alwaysDrawSphere;

void            R_SetupSkyModels(ded_mapinfo_t *info);
void            R_PrecacheSky(void);
void            R_SkyTicker(void);

#endif
