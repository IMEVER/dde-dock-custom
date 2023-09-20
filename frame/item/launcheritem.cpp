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

#include "window/dockitemmanager.h"

#include <QMouseEvent>

LauncherItem::LauncherItem(QWidget *parent) : DockItem(parent)
    , m_launcherInter(new LauncherInter("org.deepin.dde.Launcher1", "/org/deepin/dde/Launcher1", QDBusConnection::sessionBus(), this))
{
    setFixedSize(DockItemManager::instance()->itemSize()-2, DockItemManager::instance()->itemSize()-2);
    m_icon = QIcon::fromTheme("deepin-launcher");
}

void LauncherItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        m_launcherInter->Toggle();
}
