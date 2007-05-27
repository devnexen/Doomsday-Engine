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
 * dd_help.h: Help Text Strings
 */

#ifndef __DOOMSDAY_HELP_H__
#define __DOOMSDAY_HELP_H__

// Help string types.
enum {
    HST_DESCRIPTION,
    HST_CONSOLE_VARIABLE,
    HST_DEFAULT_VALUE,
    HST_INFO,
    NUM_HELPSTRING_TYPES
};

void            DH_Register(void);
void            DD_InitHelp(void);
void            DD_ShutdownHelp(void);
void           *DH_Find(const char *id);
char           *DH_GetString(void *found, int type);

#endif
