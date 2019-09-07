/** @file bluemanawidget.cpp  GUI widget for -.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "hud/widgets/bluemanawidget.h"

#include "jhexen.h"
#include "hu_inventory.h"

using namespace de;

guidata_bluemana_t::guidata_bluemana_t(void (*updateGeometry) (HudWidget *wi),
                                       void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                                       dint player)
    : HudWidget(updateGeometry,
                drawer,
                player)
{}

guidata_bluemana_t::~guidata_bluemana_t()
{}

void guidata_bluemana_t::reset()
{
    _value = 1994;
}

void guidata_bluemana_t::tick(timespan_t /*tickLength*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    const player_t &plr = ::players[player()];
    _value = plr.ammo[AT_BLUEMANA].owned;
}

void BlueManaWidget_Draw(guidata_bluemana_t *mana, const Point2Raw *offset)
{
#define TRACKING                ( 1 )

    DE_ASSERT(mana);
    dfloat const textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

    if(mana->_value == 1994) return;

    if(!::cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsOpen(mana->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[mana->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    auto const valueAsText = String::asText(mana->_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(mana->font());
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
    FR_DrawTextXY3(valueAsText, 0, 0, ALIGN_TOPLEFT, DTF_NO_EFFECTS);
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void SBarBlueManaWidget_Draw(guidata_bluemana_t *mana, const Point2Raw *offset)
{
#define X_OFFSET                ( 91 )
#define Y_OFFSET                ( 19 )
#define TRACKING                (  0 )

    DE_ASSERT(mana);
    static Vec2i const origin(-ST_WIDTH / 2, -ST_HEIGHT);

    dint const activeHud     = ST_ActiveHud(mana->player());
    dfloat const yOffset     = ST_HEIGHT * (1 - ST_StatusBarShown(mana->player()));
    dfloat const textOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(mana->_value == 1994) return;
    if(mana->_value <= 0) return;

    if(Hu_InventoryIsOpen(mana->player())) return;
    if(ST_AutomapIsOpen(mana->player())) return;
    if(P_MobjIsCamera(::players[mana->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    auto const valueAsText = String::asText(mana->_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(mana->font());
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
    FR_DrawTextXY3(valueAsText, origin.x + X_OFFSET, origin.y + Y_OFFSET, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
#undef Y_OFFSET
#undef X_OFFSET
}

void BlueMana_UpdateGeometry(guidata_bluemana_t *mana)
{
#define TRACKING                ( 1 )

    DE_ASSERT(mana);
    Rect_SetWidthHeight(&mana->geometry(), 0, 0);

    if(mana->_value == 1994) return;

    if(!::cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsOpen(mana->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[mana->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    auto const valueAsText = String::asText(mana->_value);

    FR_SetFont(mana->font());
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
    Rect_SetWidthHeight(&mana->geometry(), textSize.width  * cfg.common.hudScale,
                                           textSize.height * cfg.common.hudScale);

#undef TRACKING
}

void SBarBlueManaWidget_UpdateGeometry(guidata_bluemana_t *mana)
{
#define TRACKING                ( 0 )

    DE_ASSERT(mana);
    Rect_SetWidthHeight(&mana->geometry(), 0, 0);

    if(mana->_value == 1994) return;
    if(mana->_value <= 0) return;

    if(Hu_InventoryIsOpen(mana->player())) return;
    if(ST_AutomapIsOpen(mana->player())) return;
    if(P_MobjIsCamera(::players[mana->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    auto const valueAsText = String::asText(mana->_value);

    FR_SetFont(mana->font());
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
    Rect_SetWidthHeight(&mana->geometry(), textSize.width  * ::cfg.common.statusbarScale,
                                           textSize.height * ::cfg.common.statusbarScale);

#undef TRACKING
}
