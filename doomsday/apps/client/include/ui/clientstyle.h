/** @file clientstyle.h  Client UI style.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_UI_CLIENTSTYLE_H
#define DENG_CLIENT_UI_CLIENTSTYLE_H

#include <de/Style>
#include <de/GuiWidget>
#include <de/ui/Stylist>

class ClientStyle : public de::Style
{
public:
    ClientStyle();

    de::GuiWidget *sharedBlurWidget() const override;
    de::ui::Stylist &emptyMenuLabelStylist() const;

    void performUpdate() override;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_CLIENTSTYLE_H
