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

#include "previewcontainer.h"

#include <QScreen>
#include <QApplication>
#include <QDragEnterEvent>

#define SPACING           0
#define MARGIN            0

PreviewContainer *PreviewContainer::instance() {
    static PreviewContainer *preview = new PreviewContainer;
    return preview;
}

PreviewContainer *PreviewContainer::instance(const WindowInfoMap &infos, const Dock::Position dockPos)
{
    auto preview = instance();
    preview->disconnect();
    preview->setWindowInfos(infos);
    preview->updateLayoutDirection(dockPos);
    return preview;
}

PreviewContainer::PreviewContainer() : QWidget(),
    m_mouseLeaveTimer(nullptr)
{
    m_windowListLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_windowListLayout->setSpacing(SPACING);
    m_windowListLayout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);

    setAcceptDrops(true);
    setLayout(m_windowListLayout);
    setFixedSize(SNAP_WIDTH, SNAP_HEIGHT);
}

void PreviewContainer::setWindowInfos(const WindowInfoMap &infos)
{
    // check removed window
    for (auto it(m_snapshots.begin()); it != m_snapshots.end();)
    {
        if (!infos.contains(it.key()))
        {
            m_windowListLayout->removeWidget(it.value());
            it.value()->deleteLater();
            it = m_snapshots.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it(infos.cbegin()); it != infos.cend(); ++it)
    {
        const WId key = it.key();
        if (!m_snapshots.contains(key))
            appendSnapWidget(key);
        m_snapshots[key]->setTitle(it.value().title);
        m_snapshots[key]->setCloseAble(it.value().closable);
    }

    if (m_snapshots.isEmpty())
        emit requestHidePopup();

    adjustSize();
}

void PreviewContainer::updateLayoutDirection(const Dock::Position dockPos)
{
    if (dockPos == Dock::Top || dockPos == Dock::Bottom)
        m_windowListLayout->setDirection(QBoxLayout::LeftToRight);
    else
        m_windowListLayout->setDirection(QBoxLayout::TopToBottom);

    adjustSize();
}

void PreviewContainer::checkMouseLeave()
{
    if (!underMouse())
        emit requestHidePopup();
}

void PreviewContainer::prepareHide()
{
    if(m_mouseLeaveTimer == nullptr) {
        m_mouseLeaveTimer = new QTimer(this);
        m_mouseLeaveTimer->setSingleShot(true);
        m_mouseLeaveTimer->setInterval(300);
        connect(m_mouseLeaveTimer, &QTimer::timeout, this, &PreviewContainer::checkMouseLeave, Qt::QueuedConnection);
    }
    m_mouseLeaveTimer->start();
}

void PreviewContainer::adjustSize()
{
    const int count = m_snapshots.size();

    const QRect r = qApp->primaryScreen()->geometry();
    const int padding = 20;

    if (m_windowListLayout->direction() == QBoxLayout::LeftToRight)
    {
        const int h = SNAP_HEIGHT + MARGIN * 2;
        const int w = SNAP_WIDTH * count + MARGIN * 2 + SPACING * (count - 1);

        setFixedHeight(h);
        setFixedWidth(std::min(w, r.width() - padding));
    } else {
        const int w = SNAP_WIDTH + MARGIN * 2;
        const int h = SNAP_HEIGHT * count + MARGIN * 2 + SPACING * (count - 1);

        setFixedWidth(w);
        setFixedHeight(std::min(h, r.height() - padding));
    }
}

void PreviewContainer::appendSnapWidget(const WId wid)
{
    AppSnapshot *snap = new AppSnapshot(wid);
    connect(snap, &AppSnapshot::clicked, this, &PreviewContainer::onSnapshotClicked, Qt::QueuedConnection);
    connect(snap, &AppSnapshot::entered, this, &PreviewContainer::requestPreviewWindow, Qt::QueuedConnection);
    connect(snap, &AppSnapshot::dragEntered, this, &PreviewContainer::requestActivateWindow, Qt::QueuedConnection);
    connect(snap, &AppSnapshot::requestCheckWindow, this, &PreviewContainer::requestCheckWindows, Qt::QueuedConnection);

    connect(snap, &AppSnapshot::requestClose, this, [this, snap](const WId wid){
        m_snapshots.remove(wid);
        m_windowListLayout->removeWidget(snap);
        snap->deleteLater();
        if(m_snapshots.isEmpty())
            emit requestHidePopup();
        else {
            adjustSize();
            emit requestCancelPreviewWindow();
        }
        emit requestClose(wid);
    });

    m_windowListLayout->addWidget(snap);
    m_snapshots.insert(wid, snap);
}

void PreviewContainer::enterEvent(QEvent *e)
{
    QWidget::enterEvent(e);
    if(m_mouseLeaveTimer) {
        m_mouseLeaveTimer->stop();
        m_mouseLeaveTimer->deleteLater();
        m_mouseLeaveTimer = nullptr;
    }
}

void PreviewContainer::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    checkMouseLeave();
}

void PreviewContainer::dragEnterEvent(QDragEnterEvent *e)
{
    e->accept();
    if(m_mouseLeaveTimer) {
        m_mouseLeaveTimer->stop();
        m_mouseLeaveTimer->deleteLater();
        m_mouseLeaveTimer = nullptr;
    }
}

void PreviewContainer::dragLeaveEvent(QDragLeaveEvent *e)
{
    e->ignore();
    checkMouseLeave();
}

void PreviewContainer::onSnapshotClicked(const WId wid)
{
    emit requestActivateWindow(wid);
    // the leaveEvent of this widget will be called after this signal
    Q_EMIT requestHidePopup();
}
