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

#include "dockpopupwindow.h"

DWIDGET_USE_NAMESPACE

DockPopupWindow::DockPopupWindow(QWidget *parent) : DArrowRectangle(ArrowBottom, parent),
    m_model(false),
    m_regionInter(new DRegionMonitor(this))
{
    m_wmHelper = DWindowManagerHelper::instance();

    compositeChanged();

    setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_InputMethodEnabled, false);

    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &DockPopupWindow::compositeChanged);
    connect(m_regionInter, &DRegionMonitor::buttonPress, this, &DockPopupWindow::onGlobMouseRelease);
}

DockPopupWindow::~DockPopupWindow()
{
}

bool DockPopupWindow::model() const
{
    return isVisible() && m_model;
}

void DockPopupWindow::setContent(QWidget *content)
{
    if(QWidget *lastWidget = getContent()) {
        if(lastWidget == content) {
            resize(content->sizeHint());
            return;
        }
        lastWidget->removeEventFilter(this);
        lastWidget->setVisible(false);
        disconnect(this, &DockPopupWindow::accept, nullptr, nullptr);
    }
    content->installEventFilter(this);

    resize(content->sizeHint());
    DArrowRectangle::setContent(content);
}

void DockPopupWindow::show(const QPoint &pos, const bool model)
{
    m_model = model;

    show(pos.x(), pos.y());

    if (m_regionInter->registered())
        m_regionInter->unregisterRegion();

    if (m_model)
        m_regionInter->registerRegion();
}

void DockPopupWindow::show(const int x, const int y)
{
    m_lastPoint = QPoint(x, y);

    DArrowRectangle::show(x, y);
}

void DockPopupWindow::hide()
{
    if (m_regionInter->registered())
        m_regionInter->unregisterRegion();

    DArrowRectangle::hide();
}

bool DockPopupWindow::eventFilter(QObject *o, QEvent *e)
{
    // FIXME: ensure position move after global mouse release event
    if (o == getContent() && e->type() == QEvent::Resize && isVisible())
    {
        QTimer::singleShot(10, this, [this] {
            // NOTE(sbw): double check is necessary, in this time, the popup maybe already hided.
            if (isVisible())
                show(m_lastPoint, m_model);
        });
    }

    return false;
}

void DockPopupWindow::onGlobMouseRelease(const QPoint &mousePos, const int flag)
{
    Q_ASSERT(m_model);

    if (!((flag == DRegionMonitor::WatchedFlags::Button_Left) || (flag == DRegionMonitor::WatchedFlags::Button_Right)))
        return;

    const QRect rect = QRect(pos(), size());
    if (rect.contains(mousePos))
        return;

    emit accept();

    m_regionInter->unregisterRegion();
}

void DockPopupWindow::compositeChanged()
{
    if (m_wmHelper->hasComposite())
        setBorderColor(QColor(255, 255, 255, 255 * 0.05));
    else
        setBorderColor(QColor("#2C3238"));
}
