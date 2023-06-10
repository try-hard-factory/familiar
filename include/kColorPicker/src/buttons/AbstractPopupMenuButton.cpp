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

#include "AbstractPopupMenuButton.h"

namespace kColorPicker {

AbstractPopupMenuButton::AbstractPopupMenuButton(const QIcon &icon) :
	mHoverColor(QColor(QLatin1String("#add8e6")))
{
	setIcon(icon);
	setFixedSize(iconSize() + QSize(8, 8));
    connect(this, &QToolButton::clicked, this, &AbstractPopupMenuButton::buttonClicked);
}

void AbstractPopupMenuButton::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	QStyleOption styleOption;
	styleOption.initFrom(this);
	auto rect = event->rect();
	auto scaleRatio = devicePixelRatioF();
	auto buttonRect = QRectF(rect.x() + (2 / scaleRatio), rect.y() + (2 / scaleRatio), rect.width() - 5, rect.height() - 5);

	if(styleOption.state & QStyle::State_MouseOver)
	{
		auto defaultPen = painter.pen();
		auto defaultBrush = painter.brush();
		painter.setPen(mHoverColor);
		painter.setBrush(mHoverColor);
		painter.drawRect(buttonRect);
		painter.setPen(defaultPen);
		painter.setBrush(defaultBrush);
	}

	painter.drawPixmap(buttonRect.topLeft() + QPointF(2, 2), icon().pixmap(iconSize()));

	if(isChecked()) {
		painter.drawRect(buttonRect);
	}
}

} // namespace kColorPicker
