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

AppItem::AppItem(const Entry *entry, QWidget *parent) : DockItem(parent)
    , m_itemEntry(const_cast<Entry*>(entry))
    , m_itemAnimation(nullptr)
    , m_updateIconGeometryTimer(nullptr)
    , m_dirItem(nullptr)
{
    setAcceptDrops(true);

    connect(m_itemEntry, &Entry::isActiveChanged, this, [this] { update(); });
    connect(m_itemEntry, &Entry::windowInfosChanged, this, &AppItem::updateWindowInfos, Qt::QueuedConnection);
    connect(m_itemEntry, &Entry::iconChanged, this, &AppItem::refreshIcon);
    connect(this, &AppItem::requestPresentWindows, m_itemEntry, &Entry::presentWindows);

    auto themeChanged = [this](DGuiApplicationHelper::ColorType type)
    {
        if (DGuiApplicationHelper::DarkType == type)
        {
            m_horizontalIndicator = QPixmap(":/indicator/resources/indicator_dark.svg");
            m_verticalIndicator = QPixmap(":/indicator/resources/indicator_dark_ver.svg");
        }
        else
        {
            m_horizontalIndicator = QPixmap(":/indicator/resources/indicator.svg");
            m_verticalIndicator = QPixmap(":/indicator/resources/indicator_ver.svg");
        }
    };
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, themeChanged);
    themeChanged(DGuiApplicationHelper::instance()->themeType());
    m_activeHorizontalIndicator = QPixmap(":/indicator/resources/indicator_active.svg");
    m_activeVerticalIndicator = QPixmap(":/indicator/resources/indicator_active_ver.svg");

    connect(DGuiApplicationHelper::instance()->systemTheme(), &DPlatformTheme::activeColorChanged, this, [this](const auto &color) { m_activeColor = color; });


    connect(DockItemManager::instance(), &DockItemManager::mergeModeChanged, this, &AppItem::mergeModeChanged);
    mergeModeChanged(DockItemManager::instance()->getDockMergeMode());

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
    mergeModeChanged(DockItemManager::instance()->getDockMergeMode());
}

void AppItem::removeDirItem()
{
    m_dirItem = nullptr;
    m_place = DockPlace;
    mergeModeChanged(DockItemManager::instance()->getDockMergeMode());
}

void AppItem::moveEvent(QMoveEvent *e)
{
    DockItem::moveEvent(e);

    if(m_updateIconGeometryTimer)
        m_updateIconGeometryTimer->start();
}

void AppItem::paintEvent(QPaintEvent *e)
{
    if (m_itemAnimation != nullptr || isScaling()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF itemRect = rect();

    if (m_itemEntry->hasWindow() && isMergeWindow())
    {
        QPoint p;
        QPixmap pixmap;
        QPixmap activePixmap;

        if (m_place == DockPlace)
        {
            switch (DockPosition)
            {
            case Top:
            case Bottom:
                pixmap = m_horizontalIndicator;
                activePixmap = m_activeHorizontalIndicator;
                p.setX((itemRect.width() - pixmap.width()) / 2);
                p.setY(itemRect.height() - pixmap.height() - 1);
                break;
            case Left:
                pixmap = m_verticalIndicator;
                activePixmap = m_activeVerticalIndicator;
                p.setX(1);
                p.setY((itemRect.height() - pixmap.height()) / 2);
                break;
            case Right:
                pixmap = m_verticalIndicator;
                activePixmap = m_activeVerticalIndicator;
                p.setX(itemRect.width() - pixmap.width() - 1);
                p.setY((itemRect.height() - pixmap.height()) / 2);
                break;
            }
        }
        else
        {
            pixmap = m_horizontalIndicator;
            activePixmap = m_activeHorizontalIndicator;
            p.setX((itemRect.width() - pixmap.width()) / 2);
            p.setY(itemRect.height() - pixmap.height() - 1);
        }

        painter.drawPixmap(p, m_itemEntry->getIsActive() ? activePixmap : pixmap);
    }

    painter.drawPixmap(appIconPosition(), m_icon.isNull() ? QPixmap(":/icons/resources/application-x-desktop.svg") : m_icon.pixmap(width() *.85).scaled(width() *.85, width() *.85));
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
    }
    else if (e->button() == Qt::LeftButton)
    {
        if(!isMergeWindow())
            m_itemEntry->launchApp(QX11Info::getTimestamp());
        else
            m_itemEntry->active(QX11Info::getTimestamp());

    }
    if ((!m_itemEntry->hasWindow() || !isMergeWindow()) && e->button() != Qt::RightButton)
        playSwingEffect();
        
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
    if(m_updateIconGeometryTimer) m_updateIconGeometryTimer->start();
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

bool AppItem::isMergeWindow() const {
    return m_place == DockItem::DockPlace and DockItemManager::instance()->getDockMergeMode() == MergeDock;
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

void AppItem::updateWindowInfos(const WindowInfoMap &info)
{
    if(m_updateIconGeometryTimer)
        m_updateIconGeometryTimer->start();

    // process attention effect
    if (hasAttention()) {
        if(m_place == DockItem::DockPlace)
            playSwingEffect();
    } else if(m_place == DockItem::DirPlace and m_itemAnimation)
        m_itemAnimation->stop();

    update();

    if (isMergeWindow()) return;

    for (auto it(m_windowMap.begin()); it != m_windowMap.end();)
    {
        if (!info.keys().contains(it.key()))
        {
            WindowItem *windowItem = it.value();
            it = m_windowMap.erase(it);

            emit windowItemRemoved(windowItem);
            // QTimer::singleShot(500, windowItem, &WindowItem::deleteLater);
        }
        else
            it++;
    }

    for (auto it(info.cbegin()); it != info.cend(); it++)
    {
        if (!m_windowMap.contains(it.key()))
        {
            WindowItem *windowItem = new WindowItem(this, it.key(), it.value(), m_itemEntry->getAllowedClosedWindowIds().contains(it.key()));
            m_windowMap.insert(it.key(), windowItem);
            emit windowItemInserted(windowItem);
            windowItem->fetchSnapshot();
        }
    }
}

void AppItem::mergeModeChanged(MergeMode mode)
{
    if (mode == MergeDock && m_place == DockPlace) {

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
            if (!m_windowMap.contains(it.key()))
            {
                WindowItem *windowItem = new WindowItem(this, it.key(), it.value(), m_itemEntry->getAllowedClosedWindowIds().contains(it.key()));
                m_windowMap.insert(it.key(), windowItem);
                emit windowItemInserted(windowItem);
                windowItem->fetchSnapshot();
            }
        }
    }
}

void AppItem::removeWindowItem(bool animation)
{
    while (!m_windowMap.isEmpty())
    {
        WindowItem *windowItem = m_windowMap.take(m_windowMap.firstKey());
        emit windowItemRemoved(windowItem, animation);
        // QTimer::singleShot(animation ? 500 : 0, windowItem, &WindowItem::deleteLater);
    }
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

void AppItem::showPreview()
{
    auto &infos = m_itemEntry->getExportWindowInfos();
    if (infos.isEmpty()) return;

    PreviewContainer *m_appPreviewTips = PreviewContainer::instance(infos, m_itemEntry->getAllowedClosedWindowIds(), DockPosition);

    connect(m_appPreviewTips, &PreviewContainer::requestActivateWindow, this, &AppItem::requestActivateWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestPreviewWindow, this, &AppItem::requestPreviewWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestCancelPreviewWindow, this, &AppItem::requestCancelPreview);
    connect(m_appPreviewTips, &PreviewContainer::requestCheckWindows, m_itemEntry, &Entry::check);
    connect(m_appPreviewTips, &PreviewContainer::requestHidePopup, this, &AppItem::hidePopup);

    showPopupWindow(m_appPreviewTips, true);
}

void AppItem::playSwingEffect()
{
    const DockItemManager::ActivateAnimationType type = DockItemManager::instance()->animationType();
    if (type == DockItemManager::No || m_itemAnimation) return;

    m_itemAnimation = type == DockItemManager::Swing ? AppEffect::SwingEffect(this, m_icon.pixmap(width() *.85))
        : AppEffect::JumpEffect(this, m_icon.pixmap(width() *.85), m_place == DirPlace ? Bottom : DockPosition);

    connect(m_itemAnimation, &QVariantAnimation::stateChanged, this, [this](const QVariantAnimation::State &newState, const QVariantAnimation::State &oldState) {
        if (newState == QVariantAnimation::Stopped) {
            m_itemAnimation = nullptr;
            update();

            if (m_place == DirPlace)
                m_dirItem->hideDirpopupWindow();
            else if(!m_itemEntry->hasWindow() || hasAttention())
                QTimer::singleShot(1000, this, [this] {
                    if (hasAttention()) playSwingEffect();
                });
        }
    });

    m_itemAnimation->start();
    update();
}

void AppItem::handleDragDrop(uint timestamp, const QStringList &uris)
{
    m_itemEntry->handleDragDrop(timestamp, uris);
}
