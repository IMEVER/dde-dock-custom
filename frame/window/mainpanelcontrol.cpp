/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     xuwenw <xuwenw@xuwenw.so>
 *
 * Maintainer: xuwenw <xuwenw@xuwenw.so>
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

#include "mainpanelcontrol.h"
#include "../item/dockitem.h"
#include "../util/docksettings.h"
#include "../item/placeholderitem.h"
#include "../item/components/appdrag.h"
#include "../item/appitem.h"
#include "dockitemmanager.h"

#include "../item/diritem.h"

#include <QDrag>
#include <QTimer>
#include <QStandardPaths>
#include <QString>
#include <QApplication>
#include <QPointer>

#include <DGuiApplicationHelper>
#include <DWindowManagerHelper>

DWIDGET_USE_NAMESPACE

MainPanelControl::MainPanelControl(QWidget *parent) : QWidget(parent)
    , m_mainPanelLayout(new QBoxLayout(QBoxLayout::LeftToRight, this))
    , m_fixedAreaWidget(new QWidget(this))
    , m_appAreaWidget(new QWidget(this))
    , m_splitter(new QLabel(this))
    , m_fixedAreaLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_appAreaLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_position(Position::Bottom)
    , m_placeholderItem(nullptr)
    , m_appDragWidget(nullptr)
{
    init();
    updateMainPanelLayout();
    setAcceptDrops(true);
    setMouseTracking(true);

    m_appAreaWidget->installEventFilter(this);
}

MainPanelControl::~MainPanelControl()
{
}

void MainPanelControl::init()
{
    m_mainPanelLayout->setAlignment(Qt::AlignCenter);

    m_mainPanelLayout->setSpacing(MODE_PADDING);
    m_mainPanelLayout->addStretch(1);
    m_mainPanelLayout->addWidget(m_fixedAreaWidget);
    m_mainPanelLayout->addWidget(m_appAreaWidget);
    m_mainPanelLayout->addStretch(1);

    m_mainPanelLayout->setMargin(0);
    m_mainPanelLayout->setContentsMargins(0, 0, 0, 0);
    m_mainPanelLayout->setSpacing(MODE_PADDING);

    // 固定区域
    m_fixedAreaWidget->setLayout(m_fixedAreaLayout);
    m_fixedAreaLayout->setMargin(0);
    m_fixedAreaLayout->setContentsMargins(0, 0, 0, 0);
    m_fixedAreaLayout->setSpacing(MODE_PADDING);
    m_fixedAreaLayout->setAlignment(Qt::AlignCenter);
    // 应用程序
    m_appAreaWidget->setLayout(m_appAreaLayout);
    m_appAreaLayout->setMargin(0);
    m_appAreaLayout->setContentsMargins(0, 0, 0, 0);
    m_appAreaLayout->setSpacing(MODE_PADDING);
    m_appAreaLayout->setAlignment(Qt::AlignCenter);
}

void MainPanelControl::updateMainPanelLayout()
{
    switch (m_position) {
        case Position::Top:
        case Position::Bottom:
            m_fixedAreaWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            m_appAreaWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_mainPanelLayout->setDirection(QBoxLayout::LeftToRight);
            m_fixedAreaLayout->setDirection(QBoxLayout::LeftToRight);
            m_appAreaLayout->setDirection(QBoxLayout::LeftToRight);
            break;
        case Position::Right:
        case Position::Left:
            m_fixedAreaWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            m_appAreaWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_mainPanelLayout->setDirection(QBoxLayout::TopToBottom);
            m_fixedAreaLayout->setDirection(QBoxLayout::TopToBottom);
            m_appAreaLayout->setDirection(QBoxLayout::TopToBottom);
            break;
    }
    resizeDockIcon();
}

void MainPanelControl::addFixedAreaItem(int index, QWidget *wdg)
{
    wdg->setFixedSize(DockSettings::Instance().itemSize(), DockSettings::Instance().itemSize());
    m_fixedAreaLayout->insertWidget(index, wdg);
}

void MainPanelControl::addAppAreaItem(int index, QWidget *wdg)
{
    // wdg->setFixedSize(DockSettings::Instance().itemSize(), DockSettings::Instance().itemSize());
    m_appAreaLayout->insertWidget(index, wdg, 0, Qt::AlignCenter);
}

void MainPanelControl::removeAppAreaItem(QWidget *wdg)
{
    m_appAreaLayout->removeWidget(wdg);
}

void MainPanelControl::resizeEvent(QResizeEvent *event)
{
    resizeDockIcon();
    m_fixedAreaWidget->adjustSize();
    m_appAreaWidget->adjustSize();
    return QWidget::resizeEvent(event);
}

void MainPanelControl::setPositonValue(Dock::Position position)
{
    if (m_position == position)
        return;

    m_position = position;
    updateMainPanelLayout();
}

void MainPanelControl::insertItem(int index, DockItem *item)
{
    item->installEventFilter(this);
    item->setFixedSize(QSize(5, 5));
    item->easeIn();

    switch (item->itemType()) {
        case DockItem::Launcher:
            addFixedAreaItem(0, item);
            break;
        case DockItem::App:
        case DockItem::Placeholder:
        case DockItem::DirApp:
            addAppAreaItem(index, item);
            break;
        default:
            break;
    }
}

void MainPanelControl::removeItem(DockItem *item)
{
    item->removeEventFilter(this);
    item->easeOut();

    switch (item->itemType()) {
        case DockItem::App:
        case DockItem::Placeholder:
        case DockItem::DirApp:
            QTimer::singleShot(310, [ this, item ]{
                removeAppAreaItem(item);
            });
            break;
        default:
            break;
    }
}

void MainPanelControl::moveItem(DockItem *sourceItem, DockItem *targetItem)
{
    if (targetItem->itemType() != DockItem::App && targetItem->itemType() == DockItem::DirApp)
        return;

    // get target index
    int idx = m_appAreaLayout->indexOf(targetItem);

    // remove old item
    removeItem(sourceItem);

    // insert new position
    insertItem(idx, sourceItem);
}

void MainPanelControl::dragEnterEvent(QDragEnterEvent *e)
{
    e->accept();
}

void MainPanelControl::dragLeaveEvent(QDragLeaveEvent *e)
{
    Q_UNUSED(e);
    if (m_placeholderItem) {
        const QRect r(static_cast<QWidget *>(parent())->pos(), size());
        const QPoint p(QCursor::pos());

        // remove margins to fix a touch screen bug:
        // the mouse point position will stay on this rect's margins after
        // drag move to the edge of screen
        if (r.marginsRemoved(QMargins(1, 10, 1, 1)).contains(p))
            return;

        removeAppAreaItem(m_placeholderItem);
        m_placeholderItem->deleteLater();
    }
}

void MainPanelControl::dropEvent(QDropEvent *e)
{
    if (m_placeholderItem) {
        emit itemAdded(e->mimeData()->data(m_draggingMimeKey), m_appAreaLayout->indexOf(m_placeholderItem));

        emit dirAppChanged();
        removeAppAreaItem(m_placeholderItem);
        m_placeholderItem->deleteLater();
    }
    else
    {
        DockItem *source = qobject_cast<DockItem *>(e->source());
        handleDragDrop(source, m_appAreaWidget->mapFromParent(e->pos()));
    }
}

void MainPanelControl::handleDragMove(QDragMoveEvent *e, bool isFilter)
{
    if (!e->source()) {
        // 应用程序拖到dock上
        e->accept();

        if (m_placeholderItem.isNull()) {
            m_placeholderItem = new PlaceholderItem;
        }

        dropTargetItem(m_placeholderItem, e->pos());
        return;
    }

    DockItem *sourceItem = qobject_cast<DockItem *>(e->source());

    if (!sourceItem) {
        e->ignore();
        return;
    }

    if (isFilter) {
        // appItem调整顺序或者移除驻留
        dropTargetItem(sourceItem, mapFromGlobal(m_appDragWidget->mapToGlobal(e->pos())));
    } else {
        // other dockItem调整顺序
        dropTargetItem(sourceItem, e->pos());
    }

    e->accept();
}

void MainPanelControl::dragMoveEvent(QDragMoveEvent *e)
{
    DockItem *sourceItem = qobject_cast<DockItem *>(e->source());
    if (sourceItem) {
        handleDragMove(e, false);
        return;
    }

    // 拖app到dock上
    const char *RequestDockKey = "RequestDock";
    const char *RequestDockKeyFallback = "text/plain";
    const char *DesktopMimeType = "application/x-desktop";
    auto DragmineData = e->mimeData();

    m_draggingMimeKey = DragmineData->formats().contains(RequestDockKey) ? RequestDockKey : RequestDockKeyFallback;

    // dragging item is NOT a desktop file
    if (QMimeDatabase().mimeTypeForFile(DragmineData->data(m_draggingMimeKey)).name() != DesktopMimeType) {
        m_draggingMimeKey.clear();
        e->setAccepted(false);
        qDebug() << "dragging item is NOT a desktop file";
        return;
    }

    //如果当前从桌面拖拽的的app是trash，则不能放入app任务栏中
    QString str = "file://";
    //启动器
    QString str_t = "";

    str.append(QStandardPaths::locate(QStandardPaths::DesktopLocation, "dde-trash.desktop"));
    str_t.append(QStandardPaths::locate(QStandardPaths::ApplicationsLocation, "dde-trash.desktop"));

    if ((str == DragmineData->data(m_draggingMimeKey)) || (str_t == DragmineData->data(m_draggingMimeKey))) {
        e->setAccepted(false);
        return;
    }

    if (DockItemManager::instance()->appIsOnDock(DragmineData->data(m_draggingMimeKey))) {
        e->setAccepted(false);
        return;
    }

    handleDragMove(e, false);
}

bool MainPanelControl::eventFilter(QObject *watched, QEvent *event)
{
    if (m_appDragWidget && watched == static_cast<QGraphicsView *>(m_appDragWidget)->viewport()) {
        QDropEvent *e = static_cast<QDropEvent *>(event);
        bool isContains = rect().contains(mapFromGlobal(m_appDragWidget->mapToGlobal(e->pos())));
        if (isContains) {
            if (event->type() == QEvent::DragMove) {
                handleDragMove(static_cast<QDragMoveEvent *>(event), true);
            } else if (event->type() == QEvent::Drop) {
                m_appDragWidget->hide();
                DockItem *item = qobject_cast<DockItem *>(e->source());
                if(item)
                    handleDragDrop(item, m_appAreaWidget->mapFromGlobal(m_appDragWidget->mapToGlobal(e->pos())));
                return true;
            }
        }

        return false;
    }

    if (event->type() != QEvent::MouseMove)
        return false;

    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if (!mouseEvent || mouseEvent->buttons() != Qt::LeftButton)
        return false;

    DockItem *item = qobject_cast<DockItem *>(watched);
    if (!item)
        return false;

    if (item->itemType() != DockItem::App && item->itemType() != DockItem::DirApp)
        return false;

    const QPoint pos = mouseEvent->globalPos();
    const QPoint distance = pos - m_mousePressPos;
    if (distance.manhattanLength() < QApplication::startDragDistance())
        return false;

    beforeIndex = m_appAreaLayout->indexOf(item);
    startDrag(item);
    beforeIndex = -1;

    return QWidget::eventFilter(watched, event);
}

void MainPanelControl::startDrag(DockItem *item)
{
    const QPixmap pixmap = item->grab();

    item->update();

    QDrag *drag = nullptr;
    if (item->itemType() == DockItem::App) {
        AppDrag *appDrag = new AppDrag(item);

        m_appDragWidget = appDrag->appDragWidget();

        connect(m_appDragWidget, &AppDragWidget::destroyed, this, [ = ] {
            m_appDragWidget = nullptr;
        });

        appDrag->appDragWidget()->setOriginPos((m_appAreaWidget->mapToGlobal(item->pos())));
        appDrag->appDragWidget()->setDockInfo(m_position, QRect(mapToGlobal(pos()), size()));

        if (DWindowManagerHelper::instance()->hasComposite()) {
            appDrag->setPixmap(pixmap);
            m_appDragWidget->show();

            static_cast<QGraphicsView *>(m_appDragWidget)->viewport()->installEventFilter(this);
        } else {
            const QPixmap &dragPix = qobject_cast<AppItem *>(item)->appIcon();

            appDrag->QDrag::setPixmap(dragPix);
            appDrag->setHotSpot(dragPix.rect().center() / dragPix.devicePixelRatioF());
        }

        drag = appDrag;
    } else {
        drag = new QDrag(item);
        drag->setPixmap(pixmap);
    }
    drag->setHotSpot(pixmap.rect().center() / pixmap.devicePixelRatioF());
    drag->setMimeData(new QMimeData);
    drag->exec(Qt::MoveAction);

    // app关闭特效情况下移除
    if (item->itemType() == DockItem::App && !DWindowManagerHelper::instance()->hasComposite()) {
        if (m_appDragWidget->isRemoveAble())
            qobject_cast<AppItem *>(item)->undock();
    }

    m_appDragWidget = nullptr;
    item->update();
}

void MainPanelControl::dropTargetItem(DockItem *sourceItem, QPoint point)
{
    static QPoint lastPos;

    point = m_appAreaWidget->mapFromParent(point);

    for (int i = 0 ; i < m_appAreaLayout->count(); ++i) {
        DockItem *dockItem = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(i)->widget());
        if (!dockItem)
            continue;

        QRect rect;
        rect.setTopLeft(dockItem->pos());
        rect.setSize(dockItem->size());

        if(m_position == Top || m_position == Bottom)
        {
            if(rect.left() > point.x() || rect.right() < point.x() )
                continue;
        }
        else
        {
            if(rect.top() > point.y() || rect.bottom() < point.y() )
                continue;
        }

            const int width = DockSettings::Instance().itemSize();
            qreal ratio = 1;

            if(m_position == Top || m_position == Bottom)
            {
                if(sourceItem == dockItem)
                {
                    ratio = 1 - (qFabs(rect.center().x() - point.x())) / ( width / 1.0);
                }
                else if(qFabs(rect.center().x() - point.x()) < rect.width()/5)
                {
                    if(m_appAreaLayout->indexOf(sourceItem) > -1)
                    {
                        removeAppAreaItem(sourceItem);
                        sourceItem->setVisible(false);
                    }
                }
                else if (qFabs(rect.center().x() - point.x()) > rect.width()/5)
                {
                    if(qFabs(rect.center().x() - point.x()) > qFabs(rect.center().x() - lastPos.x()))
                    {
                        if(m_appAreaLayout->indexOf(sourceItem) == -1)
                        {
                            addAppAreaItem(m_appAreaLayout->indexOf(dockItem) + ( point.x() > rect.center().x() ? 1 : 0 ) , sourceItem);
                            sourceItem->setVisible(true);
                        }
                    }
                    ratio = qFabs(rect.center().x() - point.x()) / ( width / 1.0 );
                }
            }
            else
            {
                if(sourceItem == dockItem)
                {
                        ratio = 1 - qFabs(rect.center().y() - point.y()) / ( width / 1.0);
                }
                else if(qFabs(rect.center().y() - point.y()) < rect.width()/5)
                {
                    if(m_appAreaLayout->indexOf(sourceItem) > -1)
                    {
                        removeAppAreaItem(sourceItem);
                        sourceItem->setVisible(false);
                    }
                }
                else if (qFabs(rect.center().y() - point.y()) > rect.width()/5)
                {
                    if(qFabs(rect.center().y() - point.y()) > qFabs(rect.center().y() - lastPos.y()))
                    {
                        if(m_appAreaLayout->indexOf(sourceItem) == -1)
                        {
                            addAppAreaItem(m_appAreaLayout->indexOf(dockItem) + ( point.y() > rect.center().y() ? 1 : 0 ), sourceItem);
                            sourceItem->setVisible(true);
                        }
                    }
                    ratio = qFabs(rect.center().y() - point.y()) / ( width / 1.0 );
                }
            }

            sourceItem->setFixedSize(width * ratio, width * ratio);
            lastPos = point;
            return;
    }

    lastPos = point;

    if (m_appAreaLayout->count() > 0) {
        // appitem调整顺序是，判断是否拖放在两边空白区域
        int index = -2;

        DockItem *first = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(0)->widget());
        DockItem *last = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(m_appAreaLayout->count() - 1)->widget());

        if (m_position == Dock::Top || m_position == Dock::Bottom) {
            if (point.x() < 0 && sourceItem != first) {
                index = 0;
            } else if(point.x() > m_appAreaWidget->width() && sourceItem != last) {
                index = -1;
            }
        } else {
            if (point.y() < 0 && sourceItem != first) {
                index = 0;
            } else if(point.y() > m_appAreaWidget->height() && sourceItem != last) {
                index = -1;
            }
        }

        if(index > -2)
        {
            addAppAreaItem(index, sourceItem);
        }
    }
}

void MainPanelControl::handleDragDrop(DockItem *sourceItem, QPoint point)
{
    bool needUpdateDirApp = false;
    bool needUpdateWindowSize = false;

    DockItem *targetItem = nullptr;
    AppItem *source;

    if(sourceItem && sourceItem->itemType() == DockItem::App)
    {
        source = qobject_cast<AppItem *>(sourceItem);
        for (int i = 0 ; i < m_appAreaLayout->count(); ++i)
        {
            DockItem *dockItem = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(i)->widget());
            if (!dockItem || dockItem == source)
                continue;

            QRect rect(dockItem->pos(), dockItem->size());

            if (m_position == Top || m_position == Bottom)
            {
                if(qFabs(rect.center().x() - point.x()) < rect.width() / 4)
                {
                    targetItem = dockItem;
                    break;
                }
            }
            else
            {
                if(qFabs(rect.center().y() - point.y()) < rect.width() / 4)
                {
                    targetItem = dockItem;
                    break;
                }
            }
        }
    }

    if(targetItem)
    {
        DockItem *replaceItem;
        QList<QPointer<DockItem>> appList = DockItemManager::instance()->itemList();

        const int sourceIndex = appList.indexOf(source);
        int replaceIndex;

        DirItem *sourceDir = source->getDirItem();
        if(sourceDir)
        {
            if(sourceDir == targetItem)
                return;

            sourceDir->removeItem(source);
        } else
            removeAppAreaItem(source);

        if(targetItem->itemType() == DockItem::DirApp)
        {
            DirItem *dirItem = qobject_cast<DirItem *>(targetItem);

            replaceItem = dirItem->lastItem();
            dirItem->addItem(source);
        }
        else
        {
            replaceItem = targetItem;

            int currentIndex = m_appAreaLayout->indexOf(targetItem);
            if(!sourceDir)
            {
                replaceIndex = appList.indexOf(replaceItem);
                if(replaceIndex > sourceIndex)
                    currentIndex --;
            }

            removeAppAreaItem(targetItem);

            DirItem *createDirItem = new DirItem();
            createDirItem->addItem(qobject_cast<AppItem *>(targetItem));
            createDirItem->addItem(source);

            addAppAreaItem(currentIndex, createDirItem);

            DockItemManager::instance()->addDirApp(createDirItem);
        }

        needUpdateDirApp = true;
        needUpdateWindowSize = true;

        replaceIndex = appList.indexOf(replaceItem);
        if(replaceIndex < sourceIndex)
        {
            if(replaceIndex == sourceIndex -1)
                replaceItem = nullptr;
            else
                replaceItem = appList.at(replaceIndex + 1).data();
        }

        if(replaceItem)
            emit itemMoved(source, replaceItem);
    }
    else
    {
        int afterIndex = m_appAreaLayout->indexOf(sourceItem);
        sourceItem->setFixedSize(DockSettings::Instance().itemSize(), DockSettings::Instance().itemSize());

        if(afterIndex == -1)
        {
            if(beforeIndex > -1)
            {
                addAppAreaItem(beforeIndex, sourceItem);
                sourceItem->setVisible(true);
            }
            else
            {
                DirItem *sourceDir = source->getDirItem();
                sourceDir->rerender(source);
            }
        }
        else if(beforeIndex == -1 || beforeIndex != afterIndex)
        {
            DockItem *target = nullptr;
            if(beforeIndex == -1)
            {
                DirItem *sourceDir = source->getDirItem();
                int dirIndex = m_appAreaLayout->indexOf(sourceDir);

                sourceDir->removeItem(source);
                needUpdateWindowSize = true;

                if(dirIndex > afterIndex)
                {
                    int i = 1;
                    while(!target)
                    {
                        if(m_appAreaLayout->count() > afterIndex + i)
                        {
                            target = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(afterIndex + i)->widget());
                            if(target->itemType() == DockItem::DirApp)
                                target = qobject_cast<DirItem *>(target)->firstItem();

                            i++;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                else
                {
                    int i = 1;
                    while(!target)
                    {
                        if(afterIndex - i >= 0)
                        {
                            target = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(afterIndex - i)->widget());
                            if(target->itemType() == DockItem::DirApp)
                                target = qobject_cast<DirItem *>(target)->lastItem();

                            i++;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
            else if(beforeIndex != afterIndex)
            {
                target = qobject_cast<DockItem *>(afterIndex > beforeIndex ? m_appAreaLayout->itemAt(afterIndex - 1)->widget() : m_appAreaLayout->itemAt(afterIndex + 1)->widget());
                if (target->itemType() == DockItem::DirApp)
                {
                    if(afterIndex < beforeIndex)
                        target = qobject_cast<DirItem *>(target)->firstItem();
                    else
                        target = qobject_cast<DirItem *>(target)->lastItem();
                }
            }

            if(target && target != sourceItem)
            {
                if(sourceItem->itemType() == DockItem::App)
                    emit itemMoved(sourceItem, target);
                else
                    for(auto oneSource : qobject_cast<DirItem *>(sourceItem)->getAppList())
                        emit itemMoved(oneSource, target);
            }
        }
    }

    QList<QPointer<DirItem>> dirs = DockItemManager::instance()->dirList();
    for(auto item = dirs.begin(); item != dirs.end();)
    {
        if((*item)->isEmpty())
        {
            removeAppAreaItem(*item);
            (*item)->setVisible(false);
            // (*item)->deleteLater();
            // item = dirs.erase(item);
            item++;

            needUpdateDirApp = true;
            needUpdateWindowSize = true;
        }
        else
        {
            item++;
        }
    }

    if(needUpdateDirApp)
        emit dirAppChanged();
    if(needUpdateWindowSize)
        emit itemCountChanged();
}

void MainPanelControl::itemUpdated(DockItem *item)
{
    item->parentWidget()->adjustSize();
}

void MainPanelControl::resizeDockIcon()
{
    int size = DockSettings::Instance().itemSize();
    QSize s(size, size);
    if(m_fixedAreaLayout->count() > 0)
    {
        m_fixedAreaLayout->itemAt(0)->widget()->setFixedSize(s);
    }
    for (int i = 0; i < m_appAreaLayout->count(); ++ i) {
        m_appAreaLayout->itemAt(i)->widget()->setFixedSize(s);
    }
}