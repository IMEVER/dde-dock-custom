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

#include "util/imageutil.h"

#include <QPainter>
#include <QProcess>
#include <QMouseEvent>
#include <DDBusSender>
#include <QApplication>

DCORE_USE_NAMESPACE

LauncherItem::LauncherItem(QWidget *parent)
    : DockItem(parent)
    , m_launcherInter(new LauncherInter("com.deepin.dde.Launcher", "/com/deepin/dde/Launcher", QDBusConnection::sessionBus(), this))
    , m_tips(new TipsWidget(this))
{
    m_launcherInter->setSync(true, false);

    m_tips->setVisible(false);
    m_tips->setObjectName("launcher");
    m_tips->setText("启动器");
}

void LauncherItem::refershIcon()
{
    const int iconSize = qMin(width(), height());
    m_icon = ImageUtil::getIcon("deepin-launcher", iconSize * 0.8, devicePixelRatioF());
    update();
}

void LauncherItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);

    if (!isVisible())
        return;

    QPainter painter(this);

    const auto ratio = devicePixelRatioF();
    const int iconX = rect().center().x() - m_icon.rect().center().x() / ratio;
    const int iconY = rect().center().y() - m_icon.rect().center().y() / ratio;

    painter.drawPixmap(iconX, iconY, m_icon);
}

void LauncherItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);

    refershIcon();
}

void LauncherItem::mousePressEvent(QMouseEvent *e)
{
    hidePopup();

    return QWidget::mousePressEvent(e);
}

void LauncherItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;

    if (!m_launcherInter->IsVisible()) {
        m_launcherInter->Show();
    }
}

QWidget *LauncherItem::popupTips()
{
    return m_tips;
}
