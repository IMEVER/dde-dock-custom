/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             listenerri <listenerri@gmail.com>
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

#include "appitem.h"

#include "util/utils.h"
#include "xcb/xcb_misc.h"
#include "components/appeffect.h"
#include "components/previewcontainer.h"
#include "../window/dockitemmanager.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QGraphicsScene>
#include <QX11Info>
#include <DGuiApplicationHelper>
#include <DPlatformTheme>

using namespace DTK_GUI_NAMESPACE;

AppItem::AppItem(const Entry *entry, QWidget *parent) : DockItem(parent)
    , m_itemEntry(const_cast<Entry*>(entry))
    , m_itemAnimation(nullptr)
    , m_updateIconGeometryTimer(nullptr)
    , m_dirItem(nullptr)
{
    setAcceptDrops(true);

    connect(this, &AppItem::requestPresentWindows, m_itemEntry, &Entry::presentWindows);
    connect(m_itemEntry, &Entry::windowInfoAdded, this, &AppItem::addWindowInfo);
    connect(m_itemEntry, &Entry::windowInfoRemoved, this, &AppItem::removeWindowInfo);
    connect(m_itemEntry, &Entry::iconChanged, this, &AppItem::refreshIcon);
    connect(m_itemEntry, &Entry::isActiveChanged, this, [this](const bool active){
        if(isMergeWindow())
            update(indicatorRect());
        else if(!m_windowMap.isEmpty()){
            auto wid = active ? m_itemEntry->getCurrentWindow() : -1;
            // for(auto it(m_windowMap.begin()); it != m_windowMap.end(); it++)
            //     it.value()->setActive(active &&  wid == it.key());
            emit windowActiveChanged(active, wid);
        }
    });
    connect(m_itemEntry, &Entry::currentWindowChanged, this, [this](XWindow wid){
        if(!m_windowMap.isEmpty() and wid>0) {
            // for(auto it(m_windowMap.begin()); it != m_windowMap.end(); it++)
            //     it.value()->setActive(wid == it.key());
            emit windowActiveChanged(true, wid);
        }
    });

    connect(m_itemEntry, &Entry::mprisChanged, this, [this]{
        if(isMergeWindow())
            update(QRect(width() - 20, 0, 20, 20));
        else
            for(auto window : m_windowMap.values())
                window->update(QRect(width() -20, 0, 20, 20));
    });

    connect(m_itemEntry, &Entry::titleChanged, [this](XWindow wid, const QString &title){
        if(auto windowItem = m_windowMap.value(wid))
            windowItem->updateTitle(title);
    });

    connect(DockSettings::instance(), &DockSettings::mergeModeChanged, this, &AppItem::mergeModeChanged);
    mergeModeChanged(DockSettings::instance()->getDockMergeMode());

    refreshIcon();
}

AppItem::~AppItem()
{
    if(m_itemAnimation) {
        disconnect(m_itemAnimation, &QVariantAnimation::stateChanged, this, nullptr);
        m_itemAnimation->stop();
    }
}

void AppItem::setDirItem(DirItem *dirItem)
{
    m_dirItem = dirItem;
    m_place = DirPlace;
    mergeModeChanged(DockSettings::instance()->getDockMergeMode());
}

void AppItem::removeDirItem()
{
    m_dirItem = nullptr;
    m_place = DockPlace;
    mergeModeChanged(DockSettings::instance()->getDockMergeMode());
}

void AppItem::moveEvent(QMoveEvent *e)
{
    DockItem::moveEvent(e);

    if(m_updateIconGeometryTimer and m_itemEntry->hasWindow())
        m_updateIconGeometryTimer->start();
}

void AppItem::paintEvent(QPaintEvent *e)
{
    if (m_itemAnimation != nullptr || isScaling()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QRect indicator = indicatorRect();
    if (m_itemEntry->hasWindow() && isMergeWindow() and e->rect().contains(indicator))
    {
        QRadialGradient radialGrad(indicator.center(), qMax(indicator.width(), indicator.height())/2);

        if(m_itemEntry->getIsActive()) {
            // radialGrad.setColorAt(0, QColor("#0b74dd"));
            // radialGrad.setColorAt(.8, QColor("#70209cff"));
            radialGrad.setColorAt(0, palette().highlight().color());
            radialGrad.setColorAt(.5, palette().highlight().color());
        } else {
            radialGrad.setColorAt(0, QColor(10, 10, 10));
            radialGrad.setColorAt(.6, QColor(50, 50, 50));
            radialGrad.setColorAt(.8, QColor(100,100,100, 100));
        }
        radialGrad.setColorAt(1, Qt::transparent);
        painter.fillRect(indicator, QBrush(radialGrad));
    }

    if(e->rect() == rect())
        painter.drawPixmap(appIconPosition(), m_icon.isNull() ? QPixmap(":/icons/resources/application-x-desktop.svg") : m_icon.pixmap(width() *.85).scaled(width() *.85, width() *.85));

    if(isMergeWindow() and m_itemEntry->hasMpris() and e->rect().contains(QRect(width() - 20, 0, 20, 20))) {
        painter.translate(width() - 20, 0);

        QPainterPath path;
        path.addRect(0, 6, 4, 8);
        path.addPolygon(QPolygon({QPoint(4, 6), QPoint(10, 0), QPoint(10, 20), QPoint(4, 14)}));

        path.moveTo(12, 6);
        path.quadTo(QPoint(14, 10), QPoint(12, 14));

        path.moveTo(14, 2);
        path.quadTo(QPoint(18, 10), QPoint(14, 18));

        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().highlight());
        painter.drawPath(path);

        painter.translate(QPoint(0, 0));
    }
}

void AppItem::mouseReleaseEvent(QMouseEvent *e)
{
    static unsigned long m_lastclickTimes = 0;
    unsigned long curTimestamp = QX11Info::getTimestamp();
    if ((curTimestamp - m_lastclickTimes) < 300)
        return;

    m_lastclickTimes = curTimestamp;

    if (e->button() == Qt::MiddleButton)
    {
        m_itemEntry->launchApp(QX11Info::getTimestamp());
        playSwingEffect();
    }
    else if (e->button() == Qt::LeftButton)
    {
        if(!m_itemEntry->hasWindow()) {
            m_itemEntry->launchApp(QX11Info::getTimestamp());
            playSwingEffect();
        } else
            m_itemEntry->active(QX11Info::getTimestamp());

    }

    if (m_place == DockItem::DirPlace && e->button() != Qt::RightButton)
        QTimer::singleShot(1000, m_dirItem, &DirItem::hideDirpopupWindow);
}

void AppItem::wheelEvent(QWheelEvent *e)
{
    QWidget::wheelEvent(e);

    if (isMergeWindow() and qAbs(e->angleDelta().y()) > 20)
        emit requestPresentWindows();
}

void AppItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);
    if(m_updateIconGeometryTimer and m_itemEntry->hasWindow()) m_updateIconGeometryTimer->start();
}

void AppItem::dragEnterEvent(QDragEnterEvent *e)
{
    // ignore drag from panel
    if (e->source()) return e->ignore();

    // ignore request dock event
    QString draggingMimeKey = e->mimeData()->formats().contains("RequestDock") ? "RequestDock" : "text/plain";
    if (QMimeDatabase().mimeTypeForFile(e->mimeData()->data(draggingMimeKey)).name() == "application/x-desktop")
        return e->ignore();

    if (e->mimeData()->hasUrls() == false)
        return e->ignore();

    e->accept();
    hidePopup();
}

void AppItem::dragMoveEvent(QDragMoveEvent *e)
{
    DockItem::dragMoveEvent(e);

    if (isMergeWindow() and m_itemEntry->hasWindow() and !popupVisible())
        showPreview();
}

void AppItem::dragLeaveEvent(QDragLeaveEvent *event) {
    DockItem::dragLeaveEvent(event);
    if (PreviewContainer::instance()->isVisible())
        PreviewContainer::instance()->prepareHide();
}

void AppItem::dropEvent(QDropEvent *e)
{
    QStringList uriList;
    for (auto uri : e->mimeData()->urls())
        uriList << uri.toEncoded();

    qDebug() << "accept drop event with URIs: " << uriList;
    handleDragDrop(QX11Info::getTimestamp(), uriList);
}

void AppItem::leaveEvent(QEvent *e)
{
    DockItem::leaveEvent(e);

    if (PreviewContainer::instance()->isVisible())
        PreviewContainer::instance()->prepareHide();
}

QPixmap AppItem::itemPixmap() {
    if(isMergeWindow() and m_itemEntry->hasWindow())
        return grab();

    return DockItem::itemPixmap();
}

bool AppItem::isMergeWindow() const {
    return m_place == DockItem::DockPlace and DockSettings::instance()->getDockMergeMode() == MergeDock;
}

void AppItem::showHoverTips()
{
    if (isMergeWindow() and m_itemEntry->hasWindow())
        return showPreview();

    DockItem::showHoverTips();
}

void AppItem::invokedMenuItem(const QString &itemId, const bool checked)
{
    Q_UNUSED(checked);

    m_itemEntry->handleMenuItem(QX11Info::getTimestamp(), itemId);
}

QString AppItem::popupTips()
{
    if (isMergeWindow())
        if(auto currentWindow = m_itemEntry->getCurrentWindowInfo())
            return currentWindow->getTitle();

    return m_itemEntry->getName();
}

const QPoint AppItem::popupMarkPoint()
{
    if (getPlace() == DockItem::DirPlace)
        return m_dirItem->popupDirMarkPoint();
    else
        return DockItem::popupMarkPoint();
}

bool AppItem::hasAttention() const
{
    for (const auto &info : m_itemEntry->getExportWindowInfos())
        if (info.attention) return true;
    return false;
}

QPoint AppItem::appIconPosition() const
{
    const auto ratio = devicePixelRatioF();
    const QRectF itemRect = rect();
    const QRectF iconRect = m_icon.pixmap(width()*.85).rect();
    const qreal iconX = itemRect.center().x() - iconRect.center().x() / ratio;
    const qreal iconY = itemRect.center().y() - iconRect.center().y() / ratio;

    return QPoint(iconX, iconY);
}

void AppItem::addWindowInfo(const WindowInfo &info)
{
    // process attention effect
    if (hasAttention()) {
        if(m_place == DockItem::DockPlace)
            playSwingEffect();
    } else if(m_place == DockItem::DirPlace and m_itemAnimation)
        m_itemAnimation->stop();

    if (isMergeWindow()) {
        if(m_updateIconGeometryTimer)
            m_updateIconGeometryTimer->start();
        update();
        return;
    }

    WindowItem *windowItem = new WindowItem(this, info);
    windowItem->setActive(m_itemEntry->getIsActive() and m_itemEntry->getCurrentWindow() == info.wid);
    m_windowMap.insert(info.wid, windowItem);
    emit windowItemInserted(windowItem);
}

void AppItem::removeWindowInfo(const WindowInfo &info) {
    if (isMergeWindow()) {
        update();
        return;
    }

    if(auto window = m_windowMap.take(info.wid))
        emit windowItemRemoved(window);
}

void AppItem::mergeModeChanged(MergeMode mode)
{
    if (isMergeWindow()) {
        if(!m_updateIconGeometryTimer) {
            m_updateIconGeometryTimer = new QTimer(this);
            m_updateIconGeometryTimer->setInterval(500);
            m_updateIconGeometryTimer->setSingleShot(true);
            connect(m_updateIconGeometryTimer, &QTimer::timeout, this, [this]{
                // Update _NET_WM_ICON_GEOMETRY property for windows that every item
                // that manages, so that WM can do proper animations for specific
                // window behaviors like minimization.
                auto &infos = m_itemEntry->getExportWindowInfos();
                if(!infos.isEmpty()) {
                    const QRect r(mapToGlobal(QPoint(0, 0)), mapToGlobal(QPoint(width(), height())));

                    for (auto it(infos.cbegin()); it != infos.cend(); ++it)
                        XcbMisc::instance()->set_window_icon_geometry(it.key(), r);
                }
            });
        }
        removeWindowItem();
    }
    else
    {
        if(m_updateIconGeometryTimer) {
            m_updateIconGeometryTimer->stop();
            m_updateIconGeometryTimer->deleteLater();
            m_updateIconGeometryTimer = nullptr;
        }

        auto &infos = m_itemEntry->getExportWindowInfos();
        for (auto it(infos.cbegin()); it != infos.cend(); it++)
        {
            WindowItem *windowItem = new WindowItem(this, it.value());
            windowItem->setActive(m_itemEntry->getIsActive() and m_itemEntry->getCurrentWindow() == it.key());
            m_windowMap.insert(it.key(), windowItem);
            emit windowItemInserted(windowItem);
        }
    }
}

void AppItem::removeWindowItem()
{
    for(auto window : m_windowMap)
        emit windowItemRemoved(window);
    m_windowMap.clear();
}

void AppItem::refreshIcon()
{
    const QString icon = m_itemEntry->getIcon();
    m_icon.addPixmap(Utils::getIcon(icon, 100 * 0.85, devicePixelRatioF()));
    update();
}

void AppItem::requestActivateWindow(const WId wid) {
    m_itemEntry->activeWindow(wid);
}

void AppItem::close(const WId wid) {
    m_itemEntry->close(wid);
}

void AppItem::showPreview()
{
    auto &infos = m_itemEntry->getExportWindowInfos();
    if (infos.isEmpty()) return;

    PreviewContainer *m_appPreviewTips = PreviewContainer::instance(infos, DockPosition);

    connect(m_appPreviewTips, &PreviewContainer::requestActivateWindow, this, &AppItem::requestActivateWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestPreviewWindow, this, &AppItem::requestPreviewWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestCancelPreviewWindow, this, &AppItem::requestCancelPreview);
    connect(m_appPreviewTips, &PreviewContainer::requestCheckWindows, m_itemEntry, &Entry::check);
    connect(m_appPreviewTips, &PreviewContainer::requestClose, this, &AppItem::close);
    connect(m_appPreviewTips, &PreviewContainer::requestHidePopup, this, [this] {
        emit requestCancelPreview();
        hidePopup();
    });

    showPopupWindow(m_appPreviewTips, true);
}

void AppItem::playSwingEffect()
{
    auto type = DockSettings::instance()->animationType();
    if (type == DockSettings::No || m_itemAnimation || !isVisible()) return;

    m_itemAnimation = type == DockSettings::Swing ? AppEffect::SwingEffect(this, m_icon.pixmap(width() *.85))
        : AppEffect::JumpEffect(this, m_icon.pixmap(width() *.85), m_place == DirPlace ? Bottom : DockPosition);

    connect(m_itemAnimation, &QVariantAnimation::stateChanged, this, [this](const QVariantAnimation::State &newState, const QVariantAnimation::State &oldState) {
        if (newState == QVariantAnimation::Stopped) {
            m_itemAnimation = nullptr;
            update();

            if (m_place == DirPlace)
                m_dirItem->hideDirpopupWindow();
            // else if(!m_itemEntry->hasWindow() || hasAttention())
            //     QTimer::singleShot(1000, this, [this] {
            //         if (hasAttention()) playSwingEffect();
            //     });
        }
    });

    m_itemAnimation->start();
    update();
}

void AppItem::handleDragDrop(uint timestamp, const QStringList &uris)
{
    if(!uris.isEmpty())
        m_itemEntry->handleDragDrop(timestamp, uris);
}