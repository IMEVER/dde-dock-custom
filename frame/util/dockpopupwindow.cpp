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

#include "org_deepin_dde_xeventmonitor1.h"
#include "displaymanager.h"
#include "dockscreen.h"

#include <QApplication>
#include <QScreen>

DWIDGET_USE_NAMESPACE

#define DOCK_SCREEN DockScreen::instance()
#define DIS_INS DisplayManager::instance()

DockPopupWindow::DockPopupWindow(QWidget *parent) : DArrowRectangle(ArrowBottom, parent),
    m_enableMouseRelease(true),
    m_model(false),
    m_eventInter(new XEventMonitorInter("org.deepin.dde.XEventMonitor1", "/org/deepin/dde/XEventMonitor1", QDBusConnection::sessionBus())),
    m_extendWidget(nullptr)
{

    setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_InputMethodEnabled, false);

    connect(m_eventInter, &XEventMonitorInter::ButtonPress, this, &DockPopupWindow::onButtonPress);
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

void DockPopupWindow::setExtendWidget(QWidget *widget)
{
    if(m_extendWidget) disconnect(m_extendWidget, &QWidget::destroyed, this, 0);

    m_extendWidget = widget;
    connect(widget, &QWidget::destroyed, this, [ this ] { m_extendWidget = nullptr; }, Qt::UniqueConnection);
}

QWidget *DockPopupWindow::extendWidget() const
{
    return m_extendWidget;
}


void DockPopupWindow::show(const QPoint &pos, const bool model)
{
    m_model = model;

    show(pos.x(), pos.y());

    if (m_registerKey.isEmpty() == false) {
        m_eventInter->UnregisterArea(m_registerKey);
        m_registerKey.clear();
    }

    if (m_model)
        m_registerKey = m_eventInter->RegisterFullScreen();
}

void DockPopupWindow::show(const int x, const int y)
{
    m_lastPoint = QPoint(x, y);

    DArrowRectangle::show(x, y);
}

void DockPopupWindow::hide()
{
    if (m_registerKey.isEmpty() == false) {
        m_eventInter->UnregisterArea(m_registerKey);
        m_registerKey.clear();
    }

    DArrowRectangle::hide();
}

bool DockPopupWindow::eventFilter(QObject *o, QEvent *e)
{
    if(o != getContent()) return false;

    // FIXME: ensure position move after global mouse release event
    if (e->type() == QEvent::Resize && isVisible())
    {
        QTimer::singleShot(10, this, [this] {
            // NOTE(sbw): double check is necessary, in this time, the popup maybe already hided.
            if (isVisible())
                show(m_lastPoint, m_model);
        });
    } else if(e->type() == QEvent::Hide)
        hide();

    return false;
}

void DockPopupWindow::onButtonPress(int type, int x, int y, const QString &key)
{
    if (!m_enableMouseRelease)
        return;
    QScreen *screen = DIS_INS->screen(DOCK_SCREEN->current());
    if (!screen)
        return;
    QRect screenRect = screen->geometry();
    QRect popupRect(((pos() - screenRect.topLeft()) * qApp->devicePixelRatio() + screenRect.topLeft()), size() * qApp->devicePixelRatio());
    if (popupRect.contains(QPoint(x, y)))
        return;

    if (m_extendWidget) {
        // 计算额外添加的区域，如果鼠标的点击点在额外的区域内，也无需隐藏
        QPoint extendPoint = m_extendWidget->mapToGlobal(QPoint(0, 0));
        QRect extendRect(((extendPoint - screenRect.topLeft()) * qApp->devicePixelRatio() + screenRect.topLeft()), m_extendWidget->size() * qApp->devicePixelRatio());
        if (extendRect.contains(QPoint(x, y)))
            return;
    }

    // if there is something focus on widget, return
    if (auto focus = qApp->focusWidget()) {
        auto className = QString(focus->metaObject()->className());
        qDebug() << "Find focused widget, focus className is" << className;
        if (className == "QLineEdit") {
            qDebug() << "PopupWindow window will not be hidden";
            return;
        }
    }

    emit accept();
    hide();
}
