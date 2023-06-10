/*
 * Copyright (C) 2018 Damir Porobic <damir.porobic@gmx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "ColorButton.h"

namespace kColorPicker {

ColorButton::ColorButton(const QIcon &icon, const QColor &color) : AbstractPopupMenuButton(icon)
{
    setCheckable(true);
    setToolTip(getColorName(color));
    mColor = color;
}

QColor ColorButton::color() const
{
    return mColor;
}

void ColorButton::buttonClicked()
{
    emit colorSelected(mColor);
}

QString ColorButton::getColorName(const QColor &color)
{
	auto format = color.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb;

	return color.name(format);
}

} // namespace kColorPicker