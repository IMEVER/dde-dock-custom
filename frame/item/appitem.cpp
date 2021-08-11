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

#include "util/imageutil.h"
#include "xcb/xcb_misc.h"
#include "components/appswingeffectbuilder.h"
#include "components/appspreviewprovider.h"
#include "../window/dockitemmanager.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QHBoxLayout>
#include <QGraphicsScene>
#include <QX11Info>
#include <DGuiApplicationHelper>

AppItem::AppItem(const QDBusObjectPath &entry, QWidget *parent)
    : DockItem(parent)
    , m_appNameTips(new TipsWidget(this))
    , m_appPreviewTips(nullptr)
    , m_itemEntryInter(new DockEntryInter("com.deepin.dde.daemon.Dock", entry.path(), QDBusConnection::sessionBus(), this))
    , m_swingEffectView(nullptr)
    , m_itemAnimation(nullptr)
    , m_retryTimes(0)
    , m_lastclickTimes(0)
    , m_appIcon(QPixmap())
    , m_updateIconGeometryTimer(new QTimer(this))
    , m_retryObtainIconTimer(new QTimer(this))
    , m_smallWatcher(new QFutureWatcher<QPixmap>(this))
    , m_largeWatcher(new QFutureWatcher<QPixmap>(this))
    , m_dirItem(nullptr)
{
    // setWindowFlags(Qt::ToolTip);
    QHBoxLayout *centralLayout = new QHBoxLayout;
    centralLayout->setMargin(0);
    centralLayout->setSpacing(0);

    setObjectName(m_itemEntryInter->name());
    setAcceptDrops(true);
    setLayout(centralLayout);

    m_id = m_itemEntryInter->id();
    m_active = m_itemEntryInter->isActive();

    m_desktopFile = m_itemEntryInter->desktopFile();

    m_appNameTips->setObjectName(m_itemEntryInter->name());
    m_appNameTips->setVisible(false);
    m_appNameTips->installEventFilter(this);

    m_updateIconGeometryTimer->setInterval(500);
    m_updateIconGeometryTimer->setSingleShot(true);

    m_retryObtainIconTimer->setInterval(500);
    m_retryObtainIconTimer->setSingleShot(true);

    connect(m_itemEntryInter, &DockEntryInter::IsActiveChanged, this, &AppItem::activeChanged);
    connect(m_itemEntryInter, &DockEntryInter::IsActiveChanged, this, static_cast<void (AppItem::*)()>(&AppItem::update));
    connect(m_itemEntryInter, &DockEntryInter::WindowInfosChanged, this, &AppItem::updateWindowInfos, Qt::QueuedConnection);
    connect(m_itemEntryInter, &DockEntryInter::IconChanged, this, &AppItem::refershIcon);

    connect(m_updateIconGeometryTimer, &QTimer::timeout, this, &AppItem::updateWindowIconGeometries, Qt::QueuedConnection);
    connect(m_retryObtainIconTimer, &QTimer::timeout, this, &AppItem::refershIcon, Qt::QueuedConnection);

    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, [ = ](DGuiApplicationHelper::ColorType type){ m_themeType = type; });

    connect(DockItemManager::instance(), &DockItemManager::mergeModeChanged, this, &AppItem::mergeModeChanged);

    refershIcon();
}

AppItem::~AppItem()
{
    stopSwingEffect();

    m_appNameTips->deleteLater();
}

const QString AppItem::appId() const
{
    return m_id;
}

QString AppItem::getDesktopFile()
{
    return m_desktopFile;
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

    if(DockItemManager::instance()->getDockMergeMode() == MergeDock)
        mergeModeChanged(DockItemManager::instance()->getDockMergeMode());
}

void AppItem::removeDirItem()
{
    m_dirItem = nullptr;
    m_place = DockPlace;

    if(DockItemManager::instance()->getDockMergeMode() == MergeDock)
        mergeModeChanged(DockItemManager::instance()->getDockMergeMode());
}

// Update _NET_WM_ICON_GEOMETRY property for windows that every item
// that manages, so that WM can do proper animations for specific
// window behaviors like minimization.
void AppItem::updateWindowIconGeometries()
{
    const QRect r(mapToGlobal(QPoint(0, 0)),
                  mapToGlobal(QPoint(width(), height())));
    auto *xcb_misc = XcbMisc::instance();

    for (auto it(m_windowInfos.cbegin()); it != m_windowInfos.cend(); ++it)
        xcb_misc->set_window_icon_geometry(it.key(), r);
}

void AppItem::undock()
{
    m_itemEntryInter->RequestUndock();
}

void AppItem::moveEvent(QMoveEvent *e)
{
    DockItem::moveEvent(e);

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

        if (!m_windowInfos.isEmpty()) {
            QPoint p;
            QPixmap pixmap;
            QPixmap activePixmap;
            if (DGuiApplicationHelper::DarkType == m_themeType) {
                m_horizontalIndicator = QPixmap(":/indicator/resources/indicator_dark.svg");
                m_verticalIndicator = QPixmap(":/indicator/resources/indicator_dark_ver.svg");
            } else {
                m_horizontalIndicator = QPixmap(":/indicator/resources/indicator.svg");
                m_verticalIndicator = QPixmap(":/indicator/resources/indicator_ver.svg");
            }
            m_activeHorizontalIndicator = QPixmap(":/indicator/resources/indicator_active.svg");
            m_activeVerticalIndicator = QPixmap(":/indicator/resources/indicator_active_ver.svg");

            if(getPlace() == DockPlace)
            {
                switch (DockPosition) {
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


            if (m_active)
                painter.drawPixmap(p, activePixmap);
            else
                painter.drawPixmap(p, pixmap);
        }

    if (m_swingEffectView != nullptr)
        return;

    // icon
    if (m_appIcon.isNull())
        return;

    painter.drawPixmap(appIconPosition(), m_appIcon);
}

void AppItem::mouseReleaseEvent(QMouseEvent *e)
{
    int curTimestamp = QX11Info::getTimestamp();
    if ((curTimestamp - m_lastclickTimes) < 300)
        return;

    m_lastclickTimes = curTimestamp;

    if (e->button() == Qt::MiddleButton) {
        m_itemEntryInter->NewInstance(QX11Info::getTimestamp());

        // play launch effect
        if (m_windowInfos.isEmpty())
            playSwingEffect();

    } else if (e->button() == Qt::LeftButton) {
        m_itemEntryInter->Activate(QX11Info::getTimestamp());

        // play launch effect
        if (m_windowInfos.isEmpty())
            playSwingEffect();
    }

    hidePopup();
}

void AppItem::mousePressEvent(QMouseEvent *e)
{
    m_updateIconGeometryTimer->stop();

    if(getPlace() == DockPlace)
        hidePopup();

    // context menu will handle in DockItem
    DockItem::mousePressEvent(e);
}

void AppItem::wheelEvent(QWheelEvent *e)
{
    QWidget::wheelEvent(e);

    if (qAbs(e->angleDelta().y()) > 20) {
        m_itemEntryInter->PresentWindows();
    }
}

void AppItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);

    refershIcon();
}

void AppItem::dragEnterEvent(QDragEnterEvent *e)
{
    // ignore drag from panel
    if (e->source()) {
        return e->ignore();
    }

    // ignore request dock event
    QString draggingMimeKey = e->mimeData()->formats().contains("RequestDock") ? "RequestDock" : "text/plain";
    if (QMimeDatabase().mimeTypeForFile(e->mimeData()->data(draggingMimeKey)).name() == "application/x-desktop") {
        return e->ignore();
    }

    e->accept();
    hidePopup();
}

void AppItem::dragMoveEvent(QDragMoveEvent *e)
{
    DockItem::dragMoveEvent(e);

    if (m_windowInfos.isEmpty())
        return;

    if (!PopupWindow->isVisible() || !m_appPreviewTips)
        showPreview();
}

void AppItem::dropEvent(QDropEvent *e)
{
    QStringList uriList;
    for (auto uri : e->mimeData()->urls()) {
        uriList << uri.toEncoded();
    }

    qDebug() << "accept drop event with URIs: " << uriList;
    m_itemEntryInter->HandleDragDrop(QX11Info::getTimestamp(), uriList);
}

void AppItem::leaveEvent(QEvent *e)
{
    DockItem::leaveEvent(e);

    if (m_appPreviewTips) {
        if (m_appPreviewTips->isVisible()) {
            m_appPreviewTips->prepareHide();
        }
    }
}

void AppItem::showEvent(QShowEvent *e)
{
    DockItem::showEvent(e);
    refershIcon();
}

void AppItem::showHoverTips()
{
    if (!m_windowInfos.isEmpty())
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

QWidget *AppItem::popupTips()
{
    if (!m_windowInfos.isEmpty()) {
        const quint32 currentWindow = m_itemEntryInter->currentWindow();
        Q_ASSERT(m_windowInfos.contains(currentWindow));
        m_appNameTips->setText(m_windowInfos[currentWindow].title);
    } else {
        m_appNameTips->setText(m_itemEntryInter->name());
    }

    return m_appNameTips;
}

const QPoint AppItem::popupMarkPoint()
{
    if(getPlace() == DockItem::DirPlace)
    {
        return m_dirItem->popupDirMarkPoint();
    }
    else
    {
        return DockItem::popupMarkPoint();
    }
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
    if (m_appPreviewTips) m_appPreviewTips->setWindowInfos(m_windowInfos, m_itemEntryInter->GetAllowedCloseWindows().value());
    m_updateIconGeometryTimer->start();

    // process attention effect
    if (hasAttention()) {
        playSwingEffect();
    } else {
        stopSwingEffect();
    }

    update();

    if(DockItemManager::instance()->getDockMergeMode() == MergeAll)
        return;

    if(DockItemManager::instance()->getDockMergeMode() == MergeDock && m_place == DockPlace)
        return;

    for(auto it(m_windowMap.begin()); it != m_windowMap.end(); )
    {
        if (!m_windowInfos.keys().contains(it.key()))
        {
            WindowItem *windowItem = it.value();
            it = m_windowMap.erase(it);

            emit windowItemRemoved(windowItem);
            QTimer::singleShot(500, [ windowItem ]{
                windowItem->deleteLater();
            });
        }
        else
        {
            it++;
        }
    }

    for(auto it(m_windowInfos.cbegin()); it != m_windowInfos.cend(); it++)
    {
        if(!m_windowMap.contains(it.key()))
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
    if(mode == MergeAll || (mode == MergeDock && m_place == DockPlace))
    {
        while( !m_windowMap.isEmpty() )
        {
            WindowItem *windowItem = m_windowMap.take(m_windowMap.firstKey());

            emit windowItemRemoved(windowItem);
            QTimer::singleShot(500, [ windowItem ]{
                //Just wait for ease out animation complete
                windowItem->deleteLater();
            });
        }
    }
    else if(mode == MergeNone || (mode == MergeDock && m_place == DirPlace))
    {
        for(auto it(m_windowInfos.cbegin()); it != m_windowInfos.cend(); it++)
        {
            if(!m_windowMap.contains(it.key()))
            {
                WindowItem *windowItem = new WindowItem(this, it.key(), it.value(), m_itemEntryInter->GetAllowedCloseWindows().value().contains(it.key()));
                m_windowMap.insert(it.key(), windowItem);
                emit windowItemInserted(windowItem);
                windowItem->fetchSnapshot();
            }
        }
    }
}

void AppItem::refershIcon()
{
    if (!isVisible() && m_place != DockItem::DirPlace)
        return;

    const QString icon = m_itemEntryInter->icon();
    const int iconSize = qMin(width(), height());

    m_appIcon = ImageUtil::getIcon(icon, iconSize * 0.85, devicePixelRatioF());

    if (m_appIcon.isNull()) {
        if (m_retryTimes < 5) {
            m_retryTimes++;
            qDebug() << m_itemEntryInter->name() << "obtain app icon(" << icon << ")failed, retry times:" << m_retryTimes;
            m_retryObtainIconTimer->start();
        }
        return;
    } else if (m_retryTimes > 0) {
        // reset times
        m_retryTimes = 0;
    }

    update();

    m_updateIconGeometryTimer->start();
}

void AppItem::activeChanged()
{
    m_active = !m_active;
}

void AppItem::showPreview()
{
    if (m_windowInfos.isEmpty())
        return;

    m_appPreviewTips = PreviewWindow(m_windowInfos, m_itemEntryInter->GetAllowedCloseWindows().value(), DockPosition);

    connect(m_appPreviewTips, &PreviewContainer::requestActivateWindow, this, &AppItem::requestActivateWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestPreviewWindow, this, &AppItem::requestPreviewWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestCancelPreviewWindow, this, &AppItem::requestCancelPreview);
    connect(m_appPreviewTips, &PreviewContainer::requestCheckWindows, m_itemEntryInter, &DockEntryInter::Check);
    connect(m_appPreviewTips, &PreviewContainer::requestHidePopup, this, &AppItem::hidePopup);

    connect(m_appPreviewTips, &PreviewContainer::requestHidePopup, this, &AppItem::leavePreviewWindow);
    connect(m_appPreviewTips, &PreviewContainer::enterPreviewWindow, this, &AppItem::enterPreviewWindow);

    connect(m_appPreviewTips, &PreviewContainer::requestActivateWindow, [ = ]() { m_appPreviewTips = nullptr; });
    connect(m_appPreviewTips, &PreviewContainer::requestCancelPreviewWindow, [ = ]() { m_appPreviewTips = nullptr; });
    connect(m_appPreviewTips, &PreviewContainer::requestHidePopup, [ = ]() { m_appPreviewTips = nullptr; });

    showPopupWindow(m_appPreviewTips, true);
}

void AppItem::check()
{
    m_itemEntryInter->Check();
}

void AppItem::playSwingEffect()
{
    // NOTE(sbw): return if animation view already playing
    if (m_swingEffectView != nullptr)
        return;

    stopSwingEffect();

    QPair<QGraphicsView *, QGraphicsItemAnimation *> pair =  SwingEffect(
                                                                 this, m_appIcon, rect(), devicePixelRatioF());

    m_swingEffectView = pair.first;
    m_itemAnimation = pair.second;

    QTimeLine *tl = m_itemAnimation->timeLine();
    connect(tl, &QTimeLine::stateChanged, [ = ](QTimeLine::State newState) {
        if (newState == QTimeLine::NotRunning) {
            m_swingEffectView->hide();
            layout()->removeWidget(m_swingEffectView);
            m_swingEffectView = nullptr;
            m_itemAnimation = nullptr;
            checkAttentionEffect();
        }
    });

    layout()->addWidget(m_swingEffectView);
    tl->start();
}

void AppItem::stopSwingEffect()
{
    if (m_swingEffectView == nullptr || m_itemAnimation == nullptr)
        return;

    // stop swing effect
    m_swingEffectView->setVisible(false);

    if (m_itemAnimation->timeLine() && m_itemAnimation->timeLine()->state() != QTimeLine::NotRunning)
        m_itemAnimation->timeLine()->stop();
}

void AppItem::checkAttentionEffect()
{
    QTimer::singleShot(1000, this, [ = ] {
        if (hasAttention())
            playSwingEffect();
    });
}
