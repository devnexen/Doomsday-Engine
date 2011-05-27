/**\file st_stuff.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Statusbar code jHeretic - specific.
 *
 * Does the face/direction indicator animatin.
 * Does palette indicators as well (red pain/berserk, bright pickup)
 */

#ifndef LIBHERETIC_STUFF_H
#define LIBHERETIC_STUFF_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "hu_chat.h"

// Size of statusbar, now sensitive for scaling.
#define ST_HEIGHT           (42 * SCREEN_MUL)
#define ST_WIDTH            (SCREENWIDTH)
#define ST_Y                (SCREENHEIGHT - ST_HEIGHT)

// Called by startup code.
void ST_Register(void);
void ST_Init(void);
void ST_Shutdown(void);

int ST_Responder(event_t* ev);
void ST_Ticker(timespan_t ticLength);
void ST_Drawer(int player);

void ST_Start(int player);
void ST_Stop(int player);

void ST_doPaletteStuff(int player);

boolean ST_ChatIsActive(int player);

/**
 * Post a message to the specified player's log.
 *
 * @param player  Player (local) number whose log to post to.
 * @param flags  @see logMessageFlags
 * @param text  Message Text to be posted. Messages may use the same
 *      paramater control blocks as with the engine's Text rendering API.
 */
void ST_LogPost(int player, byte flags, const char* text);

/**
 * Rewind the message log of the specified player, making the last few messages
 * visible once again.
 *
 * @param player  Local player number whose message log to refresh.
 */
void ST_LogRefresh(int player);

/**
 * Empty the message log of the specified player.
 *
 * @param player  Local player number whose message log to empty.
 */
void ST_LogEmpty(int player);

void ST_LogUpdateAlignment(void);
void ST_LogPostVisibilityChangeNotification(void);

/// Call when it might be neccessary for the hud to unhide.
void ST_HUDUnHide(int player, hueevent_t ev);

void ST_FlashCurrentItem(int player);

D_CMD(ChatOpen);
D_CMD(ChatAction);
D_CMD(ChatSendMacro);

#endif /* LIBHERETIC_STUFF_H */
