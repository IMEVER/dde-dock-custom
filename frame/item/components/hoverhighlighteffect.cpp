/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hoverhighlighteffect.h"
#include "util/utils.h"

#include <QPainter>
#include <QEvent>
#include <QDebug>

HoverHighlightEffect::HoverHighlightEffect(QObject *parent)
    : QGraphicsEffect(parent)
    , m_highlighting(false)
{
    parent->installEventFilter(this);
}

HoverHighlightEffect::~HoverHighlightEffect() {
    parent()->removeEventFilter(this);
}

bool HoverHighlightEffect::eventFilter(QObject *object, QEvent *event) {
    if(object == parent()) {
        if(event->type() == QEvent::Enter) {
            m_highlighting = true;
            update();
        } else if(event->type() == QEvent::Leave) {
            m_highlighting = false;
            update();
        }
    }

    return QGraphicsEffect::eventFilter(object, event);
}

void HoverHighlightEffect::draw(QPainter *painter)
{
    const QPixmap pix = sourcePixmap(Qt::DeviceCoordinates);

    if (m_highlighting)
        painter->drawPixmap(0, 0, Utils::lighterEffect(pix));
    else
        painter->drawPixmap(0, 0, pix);
}
