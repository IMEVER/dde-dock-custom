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

#include "launcheritem.h"

#include "util/utils.h"

#include <QPainter>
#include <QProcess>
#include <QMouseEvent>
#include <DDBusSender>
#include <QApplication>

LauncherItem::LauncherItem(QWidget *parent)
    : DockItem(parent)
    , m_launcherInter(new LauncherInter("com.deepin.dde.Launcher", "/com/deepin/dde/Launcher", QDBusConnection::sessionBus(), this))
{
    m_launcherInter->setSync(true, false);
}

void LauncherItem::refershIcon()
{
    const int iconSize = qMin(width(), height());
    m_icon = Utils::getIcon("deepin-launcher", iconSize * 0.9, devicePixelRatioF());
    update();
}

void LauncherItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && !m_launcherInter->IsVisible())
        m_launcherInter->Show();
}
