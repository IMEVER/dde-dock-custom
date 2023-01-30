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
#include "components/appswingeffectbuilder.h"
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

AppItem::AppItem(const QString path, QWidget *parent)
    : DockItem(parent)
    , m_itemEntryInter(new DockEntryInter("com.deepin.dde.daemon.Dock", path, QDBusConnection::sessionBus(), this))
    , m_swingEffectView(nullptr)
    , m_itemAnimation(nullptr)
    , m_lastclickTimes(0)
    , m_appIcon(QPixmap())
    , m_updateIconGeometryTimer(nullptr)
    , m_dirItem(nullptr)
{
    setAcceptDrops(true);

    connect(m_itemEntryInter, &DockEntryInter::IsActiveChanged, this, [this] { update(); });
    connect(m_itemEntryInter, &DockEntryInter::WindowInfosChanged, this, &AppItem::updateWindowInfos, Qt::QueuedConnection);
    connect(m_itemEntryInter, &DockEntryInter::IconChanged, this, &AppItem::refershIcon);
    connect(this, &AppItem::requestPresentWindows, m_itemEntryInter, &DockEntryInter::PresentWindows);

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

    connect(DockItemManager::instance(), &DockItemManager::mergeModeChanged, this, &AppItem::mergeModeChanged);
    mergeModeChanged(DockItemManager::instance()->getDockMergeMode());
    refershIcon();
}

AppItem::~AppItem()
{
    stopSwingEffect();
}

const QString AppItem::appId() const
{
    return m_itemEntryInter->id();
}

QString AppItem::getDesktopFile()
{
    return m_itemEntryInter->desktopFile();
}

bool AppItem::isValid() const
{
    return m_itemEntryInter->isValid() && !m_itemEntryInter->id().isEmpty();
}

DirItem *AppItem::getDirItem()
{
    return m_dirItem;
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

void AppItem::undock()
{
    m_itemEntryInter->RequestUndock();
}

void AppItem::moveEvent(QMoveEvent *e)
{
    DockItem::moveEvent(e);

    if(m_updateIconGeometryTimer)
        m_updateIconGeometryTimer->start();
}

void AppItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);

    if (m_swingEffectView != nullptr)
        return;

    QPainter painter(this);
    if (!painter.isActive())
        return;
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF itemRect = rect();

    if (!m_windowInfos.isEmpty())
    {
        QPoint p;
        QPixmap pixmap;
        QPixmap activePixmap;

        if (m_place == DockPlace)
        {
            switch (DockPosition)
            {
            case Top:
                pixmap = m_horizontalIndicator;
                activePixmap = m_activeHorizontalIndicator;
                p.setX((itemRect.width() - pixmap.width()) / 2);
                p.setY(1);
                break;
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

        if (m_itemEntryInter->isActive())
            painter.drawPixmap(p, activePixmap);
        else
            painter.drawPixmap(p, pixmap);
    }

    painter.drawPixmap(appIconPosition(), m_appIcon.isNull() ? QPixmap(":/icons/resources/application-x-desktop.svg") : m_appIcon);
}

void AppItem::mouseReleaseEvent(QMouseEvent *e)
{
    int curTimestamp = QX11Info::getTimestamp();
    if ((curTimestamp - m_lastclickTimes) < 300)
        return;

    m_lastclickTimes = curTimestamp;

    if (e->button() == Qt::MiddleButton)
    {
        m_itemEntryInter->NewInstance(QX11Info::getTimestamp());

        if (m_windowInfos.isEmpty())
            playSwingEffect();
        else if (m_place == DockItem::DirPlace)
            QTimer::singleShot(1000,  m_dirItem, &DirItem::hideDirpopupWindow);
    }
    else if (e->button() == Qt::LeftButton)
    {
        m_itemEntryInter->Activate(QX11Info::getTimestamp());

        if (m_windowInfos.isEmpty())
            playSwingEffect();
        else if (m_place == DockItem::DirPlace)
            QTimer::singleShot(1000, m_dirItem, &DirItem::hideDirpopupWindow);
    }
}

void AppItem::mousePressEvent(QMouseEvent *e)
{
    if(m_updateIconGeometryTimer)
        m_updateIconGeometryTimer->stop();

    if (m_place == DockPlace)
        hidePopup();

    // context menu will handle in DockItem
    DockItem::mousePressEvent(e);
}

void AppItem::wheelEvent(QWheelEvent *e)
{
    QWidget::wheelEvent(e);

    if (m_place == DockItem::DockPlace and DockItemManager::instance()->getDockMergeMode() == MergeDock and qAbs(e->angleDelta().y()) > 20)
        emit requestPresentWindows();
}

void AppItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);
    if(m_updateIconGeometryTimer) m_updateIconGeometryTimer->start();
    refershIcon();
}

void AppItem::dragEnterEvent(QDragEnterEvent *e)
{
    // ignore drag from panel
    if (e->source())
        return e->ignore();

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

    if (m_place == DockItem::DockPlace and DockItemManager::instance()->getDockMergeMode() == MergeDock and !m_windowInfos.isEmpty() and !popupVisible())
        showPreview();
}

void AppItem::dropEvent(QDropEvent *e)
{
    QStringList uriList;
    for (auto uri : e->mimeData()->urls())
        uriList << uri.toEncoded();

    qDebug() << "accept drop event with URIs: " << uriList;
    m_itemEntryInter->HandleDragDrop(QX11Info::getTimestamp(), uriList);
}

void AppItem::leaveEvent(QEvent *e)
{
    DockItem::leaveEvent(e);

    if (PreviewContainer::instance()->isVisible())
        PreviewContainer::instance()->prepareHide();
}

void AppItem::showHoverTips()
{
    if (m_place == DockItem::DockPlace and DockItemManager::instance()->getDockMergeMode() == MergeDock and !m_windowInfos.isEmpty())
        return showPreview();

    DockItem::showHoverTips();
}

void AppItem::invokedMenuItem(const QString &itemId, const bool checked)
{
    Q_UNUSED(checked);

    m_itemEntryInter->HandleMenuItem(QX11Info::getTimestamp(), itemId);
}

const QString AppItem::contextMenu() const
{
    return m_itemEntryInter->menu();
}

QString AppItem::popupTips()
{
    if (!m_windowInfos.isEmpty())
    {
        const quint32 currentWindow = m_itemEntryInter->currentWindow();
        Q_ASSERT(m_windowInfos.contains(currentWindow));
        return m_windowInfos[currentWindow].title;
    }
    else
        return m_itemEntryInter->name();
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
    for (const auto &info : m_windowInfos)
        if (info.attention)
            return true;
    return false;
}

QPoint AppItem::appIconPosition() const
{
    const auto ratio = devicePixelRatioF();
    const QRectF itemRect = rect();
    const QRectF iconRect = m_appIcon.rect();
    const qreal iconX = itemRect.center().x() - iconRect.center().x() / ratio;
    const qreal iconY = itemRect.center().y() - iconRect.center().y() / ratio;

    return QPoint(iconX, iconY);
}

void AppItem::fetchWindowInfos()
{
    updateWindowInfos(m_itemEntryInter->windowInfos());
}

void AppItem::updateWindowInfos(const WindowInfoMap &info)
{
    m_windowInfos = info;

    if(m_updateIconGeometryTimer)
        m_updateIconGeometryTimer->start();

    // process attention effect
    if (hasAttention()) {
        if(m_place == DockItem::DockPlace)
            playSwingEffect();
    } else if(m_place == DockItem::DirPlace)
        stopSwingEffect();

    update();

    if (DockItemManager::instance()->getDockMergeMode() == MergeDock && m_place == DockPlace)
        return;

    for (auto it(m_windowMap.begin()); it != m_windowMap.end();)
    {
        if (!m_windowInfos.keys().contains(it.key()))
        {
            WindowItem *windowItem = it.value();
            it = m_windowMap.erase(it);

            emit windowItemRemoved(windowItem);
            QTimer::singleShot(500, windowItem, &WindowItem::deleteLater);
        }
        else
            it++;
    }

    for (auto it(m_windowInfos.cbegin()); it != m_windowInfos.cend(); it++)
    {
        if (!m_windowMap.contains(it.key()))
        {
            WindowItem *windowItem = new WindowItem(this, it.key(), it.value(), m_itemEntryInter->GetAllowedCloseWindows().value().contains(it.key()));
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
                if(!m_windowInfos.isEmpty()) {
                    const QRect r(mapToGlobal(QPoint(0, 0)), mapToGlobal(QPoint(width(), height())));
                    auto *xcb_misc = XcbMisc::instance();

                    for (auto it(m_windowInfos.cbegin()); it != m_windowInfos.cend(); ++it)
                        xcb_misc->set_window_icon_geometry(it.key(), r);
                }
            });
        }
        removeWindowItem();
    }
    else if (mode == MergeNone || (mode == MergeDock && m_place == DirPlace))
    {
        if(m_updateIconGeometryTimer) {
            m_updateIconGeometryTimer->stop();
            m_updateIconGeometryTimer->deleteLater();
            m_updateIconGeometryTimer = nullptr;
        }
        for (auto it(m_windowInfos.cbegin()); it != m_windowInfos.cend(); it++)
        {
            if (!m_windowMap.contains(it.key()))
            {
                WindowItem *windowItem = new WindowItem(this, it.key(), it.value(), m_itemEntryInter->GetAllowedCloseWindows().value().contains(it.key()));
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
        QTimer::singleShot(animation ? 500 : 0, windowItem, &WindowItem::deleteLater);
    }
}

void AppItem::refershIcon()
{
    const QString icon = m_itemEntryInter->icon();
    const int iconSize = qMin(width(), height());

    m_appIcon = Utils::getIcon(icon, iconSize * 0.85, devicePixelRatioF());

    update();
}

void AppItem::showPreview()
{
    if (m_windowInfos.isEmpty()) return;

    PreviewContainer *m_appPreviewTips = PreviewContainer::instance(m_windowInfos, m_itemEntryInter->GetAllowedCloseWindows().value(), DockPosition);

    connect(m_appPreviewTips, &PreviewContainer::requestActivateWindow, this, &AppItem::requestActivateWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestPreviewWindow, this, &AppItem::requestPreviewWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestCancelPreviewWindow, this, &AppItem::requestCancelPreview);
    connect(m_appPreviewTips, &PreviewContainer::requestCheckWindows, m_itemEntryInter, &DockEntryInter::Check);
    connect(m_appPreviewTips, &PreviewContainer::requestHidePopup, this, &AppItem::hidePopup);

    showPopupWindow(m_appPreviewTips, true);
}

void AppItem::check()
{
    m_itemEntryInter->Check();
}

void AppItem::playSwingEffect()
{
    DockItemManager::ActivateAnimationType type = DockItemManager::instance()->animationType();
    // NOTE(sbw): return if animation view already playing
    if (type == DockItemManager::No || m_swingEffectView != nullptr)
        return;

    QPair<QGraphicsView *, QGraphicsItemAnimation *> pair = type == DockItemManager::Swing ? SwingEffect(this, m_appIcon, rect(), devicePixelRatioF())
        : JumpEffect(this, m_appIcon, rect(), devicePixelRatioF(), m_place == DirPlace ? Bottom : DockPosition);

    m_swingEffectView = pair.first;
    m_itemAnimation = pair.second;

    QTimeLine *tl = m_itemAnimation->timeLine();
    connect(tl, &QTimeLine::stateChanged, [this, type](QTimeLine::State newState) {
        if (newState == QTimeLine::NotRunning) {
            m_swingEffectView->hide();
            if(type == DockItemManager::Swing)
                layout()->removeWidget(m_swingEffectView);
            else
                update();
            m_swingEffectView = nullptr;
            m_itemAnimation = nullptr;

            if (m_place == DockItem::DirPlace)
                m_dirItem->hideDirpopupWindow();
            else if(m_windowInfos.isEmpty() || hasAttention())
                QTimer::singleShot(1000, this, [this] {
                    if (hasAttention()) playSwingEffect();
                });
        }
    });

    if(type == DockItemManager::Swing)
        layout()->addWidget(m_swingEffectView);
    else {
        m_swingEffectView->show();
        if (DockPosition == Bottom || m_place == DirPlace)
            m_swingEffectView->move(qobject_cast<QWidget*>(parent())->mapToGlobal(pos()) - QPoint{0, m_swingEffectView->height()-height()});
        else if (DockPosition == Right)
            m_swingEffectView->move(qobject_cast<QWidget*>(parent())->mapToGlobal(pos()) - QPoint{m_swingEffectView->width()-width(), 0});
        else if(DockPosition == Left)
            m_swingEffectView->move(qobject_cast<QWidget*>(parent())->mapToGlobal(pos()));
    }
    tl->start();
}

void AppItem::stopSwingEffect()
{
    if (m_swingEffectView == nullptr || m_itemAnimation == nullptr)
        return;

    // stop swing effect
    m_swingEffectView->setVisible(false);

    if (m_itemAnimation->timeLine())
        m_itemAnimation->timeLine()->stop();
}

void AppItem::handleDragDrop(uint timestamp, const QStringList &uris)
{
    m_itemEntryInter->HandleDragDrop(timestamp, uris);
}
