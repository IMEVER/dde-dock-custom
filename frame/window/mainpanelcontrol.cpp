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

#define SPLITER_SIZE 3
#define MODE_PADDING 5

DWIDGET_USE_NAMESPACE

class SplitterWidget : public QWidget {
    public:
        explicit SplitterWidget(MainPanelControl *parent) : QWidget(parent), m_parent(parent) {
            m_type = DGuiApplicationHelper::instance()->themeType();
            connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [this](DGuiApplicationHelper::ColorType type) {
                m_type = type;
                update();
            });
            setFixedSize(3, 3);
        }

    protected:
        void paintEvent(QPaintEvent *e) override {
            Q_UNUSED(e)
            QPainter painter(this);
            QColor color;
            if (m_type == DGuiApplicationHelper::LightType)
            {
                color = Qt::black;
                painter.setOpacity(0.4);
            } else {
                color = Qt::white;
                painter.setOpacity(0.4);
            }
            QRect r = rect();
            if(m_parent->isHorizontal())
                r.adjust(1, 3, -1, -3);
            else
                r.adjust(3, 1, -3, -1);
            painter.fillRect(r, color);
        }

        void enterEvent(QEvent *event) override {
            QGuiApplication::setOverrideCursor(QCursor(m_parent->isHorizontal() ? Qt::SizeVerCursor : Qt::SizeHorCursor ));
        }

        void leaveEvent(QEvent *event) override {
            QGuiApplication::restoreOverrideCursor();
        }

        void mouseReleaseEvent(QMouseEvent *event) override {
            releaseMouse();
            if(dragging) {
                dragging = false;
                emit m_parent->requestResizeDockSizeFinished();
            }
        }

        void mousePressEvent(QMouseEvent *event) override {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if(mouseEvent->button() == Qt::RightButton) {
                emit m_parent->requestConttextMenu();
            } else if(mouseEvent->button() == Qt::LeftButton) {
                dragging = true;
                lastPos = QCursor::pos();
                lastSize = m_parent->isHorizontal() ? height() : width();
                grabMouse();
            }
        }

        void mouseMoveEvent(QMouseEvent *event) override {
            if(dragging) {
                QPoint diffPos = QCursor::pos() - lastPos;
                int s = lastSize;
                if(m_parent->m_position == Bottom) s = lastSize - diffPos.y();
                else if(m_parent->m_position == Left) s = lastSize + diffPos.x();
                else if(m_parent->m_position == Right) s = lastSize - diffPos.x();
                emit m_parent->requestResizeDockSize(s, true);
            }
        }

    private:
        MainPanelControl *m_parent;
        DGuiApplicationHelper::ColorType m_type;

        QPoint lastPos;
        int lastSize;
        bool dragging;
};

int beforeIndex = -1;

MainPanelControl::MainPanelControl(QWidget *parent) : QWidget(parent)
    , m_mainPanelLayout(new QBoxLayout(QBoxLayout::LeftToRight, this))
    , m_appAreaWidget(new QWidget(this))
    , m_fixedAreaLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_appAreaLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_windowAreaLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_lastAreaLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_splitter(new SplitterWidget(this))
    , m_splitter2(new SplitterWidget(this))
    , m_position(Position::Bottom)
    , m_placeholderItem(nullptr)
    , m_appDragWidget(nullptr)
{
    init();
    updateMainPanelLayout();
    // m_appAreaWidget->setMouseTracking(true);
    m_appAreaWidget->setAcceptDrops(true);
    m_appAreaWidget->installEventFilter(this);
}

MainPanelControl::~MainPanelControl(){}

void MainPanelControl::init()
{
    m_mainPanelLayout->setAlignment(Qt::AlignCenter);
    m_mainPanelLayout->setMargin(0);

    m_mainPanelLayout->setSpacing(MODE_PADDING);
    m_mainPanelLayout->addStretch(1);
    m_mainPanelLayout->addLayout(m_fixedAreaLayout);
    m_mainPanelLayout->addWidget(m_appAreaWidget);
    m_mainPanelLayout->addWidget(m_splitter, 0, Qt::AlignCenter);
    m_mainPanelLayout->addLayout(m_windowAreaLayout);
    m_mainPanelLayout->addWidget(m_splitter2, 0, Qt::AlignCenter);
    m_mainPanelLayout->addLayout(m_lastAreaLayout);
    m_mainPanelLayout->addStretch(1);

    // 固定区域
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
    m_appAreaLayout->addStretch(1);
    m_appAreaLayout->addStretch(1);

    m_windowAreaLayout->setMargin(0);
    m_windowAreaLayout->setContentsMargins(0, 0, 0, 0);
    m_windowAreaLayout->setSpacing(MODE_PADDING);
    m_windowAreaLayout->setAlignment(Qt::AlignCenter);

    m_lastAreaLayout->setMargin(0);
    m_lastAreaLayout->setContentsMargins(0, 0, 0, 0);
    m_lastAreaLayout->setSpacing(MODE_PADDING);
    m_lastAreaLayout->setAlignment(Qt::AlignCenter);
}

void MainPanelControl::updateMainPanelLayout()
{
    switch (m_position) {
        case Position::Top:
        case Position::Bottom:
            m_appAreaWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_mainPanelLayout->setDirection(QBoxLayout::LeftToRight);
            m_fixedAreaLayout->setDirection(QBoxLayout::LeftToRight);
            m_appAreaLayout->setDirection(QBoxLayout::LeftToRight);
            m_windowAreaLayout->setDirection(QBoxLayout::LeftToRight);
            m_lastAreaLayout->setDirection(QBoxLayout::LeftToRight);
            break;
        case Position::Right:
        case Position::Left:
            m_appAreaWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_mainPanelLayout->setDirection(QBoxLayout::TopToBottom);
            m_fixedAreaLayout->setDirection(QBoxLayout::TopToBottom);
            m_appAreaLayout->setDirection(QBoxLayout::TopToBottom);
            m_windowAreaLayout->setDirection(QBoxLayout::TopToBottom);
            m_lastAreaLayout->setDirection(QBoxLayout::TopToBottom);
            break;
    }
    resizeDockIcon();
}

void MainPanelControl::addFixedAreaItem(int index, QWidget *wdg)
{
    wdg->setFixedSize(DockItemManager::instance()->itemSize(), DockItemManager::instance()->itemSize());
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

void MainPanelControl::addWindowAreaItem(int index, QWidget *wdg)
{
    m_windowAreaLayout->insertWidget(index, wdg, 0, Qt::AlignCenter);
    m_splitter->show();
}

void MainPanelControl::removeWindowAreaItem(QWidget *wdg)
{
    m_windowAreaLayout->removeWidget(wdg);
    if(m_windowAreaLayout->isEmpty()) m_splitter->hide();
}

void MainPanelControl::addLastAreaItem(int index, QWidget *wdg)
{
    wdg->setFixedSize(DockItemManager::instance()->itemSize(), DockItemManager::instance()->itemSize());
    m_lastAreaLayout->insertWidget(index, wdg);
}

void MainPanelControl::resizeEvent(QResizeEvent *event)
{
    resizeDockIcon();
    // m_appAreaWidget->adjustSize();
    return QWidget::resizeEvent(event);
}

void MainPanelControl::setPositonValue(Dock::Position position)
{
    if (m_position != position)
    {
        m_position = position;
        updateMainPanelLayout();
        DockItem::setDockPosition(m_position);
    }
}

void MainPanelControl::insertItem(int index, DockItem *item)
{
    if(item->itemType() == DockItem::App || item->itemType() == DockItem::DirApp || item->itemType() == DockItem::Launcher)
        item->installEventFilter(this);

    if(DockItemManager::instance()->isEnableInOutAnimation())
    {
        item->setFixedSize(QSize(0, 0));
        item->easeIn();
    }

    switch (item->itemType()) {
        case DockItem::Launcher:
            addFixedAreaItem(0, item);
            break;
        case DockItem::App:
        case DockItem::Placeholder:
        case DockItem::DirApp:
            addAppAreaItem(index ==-1 ? m_appAreaLayout->count() -1 : index, item);
            break;
        case DockItem::Window:
            addWindowAreaItem(index, item);
            break;
        case DockItem::Plugins:
            addLastAreaItem(index, item);
            break;
        default:
            break;
    }
}

void MainPanelControl::removeItem(DockItem *item, bool animation)
{
    // if(item->itemType() == DockItem::App || item->itemType() == DockItem::DirApp)
        // item->removeEventFilter(this);

    if(DockItemManager::instance()->isEnableInOutAnimation() && animation)
        item->easeOut();

    QTimer::singleShot(animation ? 310 : 0, [ this, item ]{
        switch (item->itemType()) {
            case DockItem::App:
            case DockItem::Placeholder:
            case DockItem::DirApp:
                removeAppAreaItem(item);
                break;
            case DockItem::Window:
                removeWindowAreaItem(item);
                break;
            default:
                break;
        }
    });
}

void MainPanelControl::moveItem(DockItem *sourceItem, DockItem *targetItem)
{
    if (targetItem->itemType() != DockItem::App && targetItem->itemType() == DockItem::DirApp)
        return;

    // get target index
    int idx = m_appAreaLayout->indexOf(targetItem);

    // remove old item
    removeItem(sourceItem, false);

    // insert new position
    insertItem(idx, sourceItem);
}

bool MainPanelControl::eventFilter(QObject *watched, QEvent *event)
{
    if (m_appDragWidget && watched == static_cast<QGraphicsView *>(m_appDragWidget)->viewport()) {
        QDropEvent *e = static_cast<QDropEvent *>(event);
        bool isContains = m_appAreaWidget->rect().contains(mapFromGlobal(m_appDragWidget->mapToGlobal(e->pos())));
        // bool isContains = m_appAreaWidget->rect().contains(mapFromGlobal(m_appDragWidget->mapToGlobal(e->pos())));
        if (isContains) {
            if (event->type() == QEvent::DragMove) {
                dropTargetItem(qobject_cast<DockItem *>(e->source()), m_appAreaWidget->mapFromGlobal(m_appDragWidget->mapToGlobal(e->pos())));
                return true;
            } else if (event->type() == QEvent::Drop) {
                DockItem *item = qobject_cast<DockItem *>(e->source());
                handleDragDrop(item, m_appAreaWidget->mapFromGlobal(m_appDragWidget->mapToGlobal(e->pos())));
                m_appDragWidget->hide();
                return true;
            }
        }
        return false;
    } else if(LauncherItem *item = qobject_cast<LauncherItem *>(watched)) {
        if(event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if(mouseEvent->button() == Qt::RightButton) {
                emit requestConttextMenu();
                return true;
            }
        }
    } else if(DockItem *item = qobject_cast<DockItem *>(watched)) {
        static QPoint m_mousePressPos;
        if(event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if(mouseEvent->button() == Qt::LeftButton)
                m_mousePressPos = mouseEvent->globalPos();
        } else if(event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if(mouseEvent->button() == Qt::LeftButton)
                m_mousePressPos *= 0;
        } else if(event->type() == QEvent::MouseMove && !m_mousePressPos.isNull()) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            const QPoint distance = mouseEvent->globalPos() - m_mousePressPos;
            if (distance.manhattanLength() >= QApplication::startDragDistance()) {
                beforeIndex = m_appAreaLayout->indexOf(item);
                startDrag(item);
                beforeIndex = -1;
                return true;
            }
        }
    } else if(watched == m_appAreaWidget) {
        switch (event->type())
        {
        case QEvent::DragEnter: {
            QDragEnterEvent *mouseEvent = static_cast<QDragEnterEvent *>(event);
            // if(DockItem *sourceItem = qobject_cast<DockItem *>(mouseEvent->source())){
            //     event->accept();
            //     return true;
            // }

            // 拖app到dock上
            static const char *RequestDockKey = "RequestDock";
            static const char *RequestDockKeyFallback = "text/plain";
            static const char *DesktopMimeType = "application/x-desktop";
            auto DragmineData = mouseEvent->mimeData();
            m_draggingMimeKey = DragmineData->formats().contains(RequestDockKey) ? RequestDockKey : RequestDockKeyFallback;

            // dragging item is NOT a desktop file
            if (QMimeDatabase().mimeTypeForFile(DragmineData->data(m_draggingMimeKey)).name() != DesktopMimeType) {
                m_draggingMimeKey.clear();
                mouseEvent->ignore();
                return false;
            }

            //如果当前从桌面拖拽的的app是trash，则不能放入app任务栏中
            static const QString str = "file://" + QStandardPaths::locate(QStandardPaths::DesktopLocation, "dde-trash.desktop");
            static const QString str_t = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, "dde-trash.desktop");

            if ((str == DragmineData->data(m_draggingMimeKey)) || (str_t == DragmineData->data(m_draggingMimeKey))) {
                mouseEvent->ignore();
                return false;
            }

            if (DockItemManager::instance()->appIsOnDock(DragmineData->data(m_draggingMimeKey))) {
                mouseEvent->ignore();
                return false;
            }
            event->accept();
            return true;
            break;
        }
        case QEvent::DragLeave:
            if (m_placeholderItem) {
                removeAppAreaItem(m_placeholderItem);
                m_placeholderItem->deleteLater();
            }
            break;
        case QEvent::DragMove: {
            QDragMoveEvent *mouseEvent = static_cast<QDragMoveEvent *>(event);
            // if(DockItem *sourceItem = qobject_cast<DockItem *>(mouseEvent->source())) {
            //     // other dockItem调整顺序
            //     dropTargetItem(sourceItem, mouseEvent->pos());
            // } else {
                // 应用程序拖到dock上
                if (m_placeholderItem.isNull()) m_placeholderItem = new PlaceholderItem;
                dropTargetItem(m_placeholderItem, mouseEvent->pos());
            // }
            event->accept();
            return true;
            break;
        }
        case QEvent::Drop: {
            QDropEvent *mouseEvent = static_cast<QDropEvent *>(event);
            if (m_placeholderItem) {
                QPoint point = mouseEvent->pos();
                DirItem *targetItem = nullptr;

                for (int i = 1 ; i < m_appAreaLayout->count()-2; ++i)
                {
                    DockItem *dockItem = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(i)->widget());
                    if (!dockItem || dockItem == m_placeholderItem || dockItem->itemType() != DockItem::DirApp)
                        continue;

                    QRect rect(dockItem->pos(), dockItem->size());

                    if (isHorizontal())
                    {
                        if(qFabs(rect.center().x() - point.x()) < rect.width() / 4)
                        {
                            targetItem = qobject_cast<DirItem *>(dockItem);
                            break;
                        }
                    }
                    else
                    {
                        if(qFabs(rect.center().y() - point.y()) < rect.width() / 4)
                        {
                            targetItem = qobject_cast<DirItem *>(dockItem);
                            break;
                        }
                    }
                }

                int index = m_appAreaLayout->indexOf(m_placeholderItem);
                if(targetItem)
                {
                    index = m_appAreaLayout->indexOf(targetItem) + targetItem->currentCount();
                    targetItem->addId(mouseEvent->mimeData()->data(m_draggingMimeKey));
                }

                emit itemAdded(mouseEvent->mimeData()->data(m_draggingMimeKey), index-1);
                removeAppAreaItem(m_placeholderItem);
                m_placeholderItem->deleteLater();
            }
            // else if(DockItem *source = qobject_cast<DockItem *>(mouseEvent->source())) {
            //     handleDragDrop(source, mouseEvent->pos());
            // }
            event->accept();
            return true;
            break;
        }
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void MainPanelControl::startDrag(DockItem *item)
{
    const QPixmap pixmap = item->grab();

    item->update();

    AppDrag *appDrag = new AppDrag(item);

    m_appDragWidget = appDrag->appDragWidget();

    connect(m_appDragWidget, &AppDragWidget::destroyed, this, [ this ] {
        m_appDragWidget = nullptr;
    });

    appDrag->appDragWidget()->setOriginPos((m_appAreaWidget->mapToGlobal(item->pos())));
    appDrag->appDragWidget()->setDockInfo(m_position, QRect(mapToGlobal(m_appAreaWidget->pos()), m_appAreaWidget->size()));

    if (DWindowManagerHelper::instance()->hasComposite()) {
        appDrag->setPixmap(pixmap);
        // m_appDragWidget->show();

        static_cast<QGraphicsView *>(m_appDragWidget)->viewport()->installEventFilter(this);
        appDrag->setHotSpot(pixmap.rect().center() / pixmap.devicePixelRatioF());
    } else if(qobject_cast<AppItem *>(item)) {
        const QPixmap &dragPix = qobject_cast<AppItem *>(item)->appIcon();
        appDrag->QDrag::setPixmap(dragPix);
        appDrag->setHotSpot(dragPix.rect().center() / dragPix.devicePixelRatioF());
    } else {
        appDrag->QDrag::setPixmap(pixmap);
        appDrag->setHotSpot(pixmap.rect().center() / pixmap.devicePixelRatioF());
    }

    appDrag->setMimeData(new QMimeData);
    appDrag->exec(Qt::MoveAction);

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
    const bool animation = DockItemManager::instance()->isEnableDragAnimation();
    const int width = DockItemManager::instance()->itemSize();

    for (int i = 1 ; i < m_appAreaLayout->count()-2; ++i)
    {
        DockItem *dockItem = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(i)->widget());

        qreal ratio = 1;
        QRect rect;
        rect.setTopLeft(dockItem->pos());
        rect.setSize(dockItem->size());

        if(isHorizontal())
        {
            if(rect.left() <= point.x() && rect.right() >= point.x() ) {
                if(sourceItem == dockItem)
                {
                    if(animation)
                        ratio = 1 - (qFabs(rect.center().x() - point.x())) / ( width / 1.0);
                }
                else if(qFabs(rect.center().x() - point.x()) < rect.width()/4)
                {
                    if(animation && sourceItem->itemType() != DockItem::DirApp && m_appAreaLayout->indexOf(sourceItem) > -1)
                    {
                        removeAppAreaItem(sourceItem);
                        sourceItem->setVisible(false);
                    }
                }
                else if (qFabs(rect.center().x() - point.x()) > rect.width()/4)
                {
                    if(qFabs(rect.center().x() - point.x()) > qFabs(rect.center().x() - lastPos.x()))
                    {
                        if(!animation || sourceItem->itemType() == DockItem::DirApp)
                        {
                            if(point.x() > rect.center().x())
                            {
                                if(m_appAreaLayout->indexOf(sourceItem) < m_appAreaLayout->indexOf(dockItem))
                                {
                                    removeAppAreaItem(sourceItem);
                                    addAppAreaItem(m_appAreaLayout->indexOf(dockItem) + 1 , sourceItem);
                                }
                            }
                            else if(point.x() < rect.center().x())
                            {
                                if(m_appAreaLayout->indexOf(sourceItem) > m_appAreaLayout->indexOf(dockItem))
                                {
                                    removeAppAreaItem(sourceItem);
                                    addAppAreaItem(m_appAreaLayout->indexOf(dockItem) , sourceItem);
                                }
                            }
                        }
                        else if(m_appAreaLayout->indexOf(sourceItem) == -1)
                        {
                            addAppAreaItem(m_appAreaLayout->indexOf(dockItem) + ( point.x() > rect.center().x() ? 1 : 0 ) , sourceItem);
                            sourceItem->setVisible(true);
                        }
                    }
                    if(animation)
                        ratio = qFabs(rect.center().x() - point.x()) / ( width / 1.0 );
                }

                if(animation)
                    sourceItem->setFixedSize(width * ratio, width * ratio);

                lastPos = point;
                return;
            }
        }
        else
        {
            if(rect.top() <= point.y() && rect.bottom() >= point.y() ) {
                if(sourceItem == dockItem)
                {
                    if(animation)
                        ratio = 1 - qFabs(rect.center().y() - point.y()) / ( width / 1.0);
                }
                else if(qFabs(rect.center().y() - point.y()) < rect.width()/5)
                {
                    if(animation && sourceItem->itemType() != DockItem::DirApp && m_appAreaLayout->indexOf(sourceItem) > -1)
                    {
                        removeAppAreaItem(sourceItem);
                        sourceItem->setVisible(false);
                    }
                }
                else if (qFabs(rect.center().y() - point.y()) > rect.width()/5)
                {
                    if(qFabs(rect.center().y() - point.y()) > qFabs(rect.center().y() - lastPos.y()))
                    {
                        if(!animation || sourceItem->itemType() == DockItem::DirApp)
                        {
                            if(point.y() > rect.center().y())
                            {
                                if(m_appAreaLayout->indexOf(sourceItem) < m_appAreaLayout->indexOf(dockItem))
                                {
                                    removeAppAreaItem(sourceItem);
                                    addAppAreaItem(m_appAreaLayout->indexOf(dockItem) + 1 , sourceItem);
                                }
                            }
                            else if(point.y() < rect.center().y())
                            {
                                if(m_appAreaLayout->indexOf(sourceItem) > m_appAreaLayout->indexOf(dockItem))
                                {
                                    removeAppAreaItem(sourceItem);
                                    addAppAreaItem(m_appAreaLayout->indexOf(dockItem) , sourceItem);
                                }
                            }
                        }
                        else if(m_appAreaLayout->indexOf(sourceItem) == -1)
                        {
                            addAppAreaItem(m_appAreaLayout->indexOf(dockItem) + ( point.y() > rect.center().y() ? 1 : 0 ), sourceItem);
                            sourceItem->setVisible(true);
                        }
                    }
                    if(animation)
                        ratio = qFabs(rect.center().y() - point.y()) / ( width / 1.0 );
                }

                if(animation)
                    sourceItem->setFixedSize(width * ratio, width * ratio);

                lastPos = point;
                return;
            }
        }
    }

    lastPos = point;

    if (m_appAreaLayout->count() > 2) {
        // appitem调整顺序是，判断是否拖放在两边空白区域
        int index = -2;

        DockItem *first = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(1)->widget());
        DockItem *last = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(m_appAreaLayout->count() - 2)->widget());

        if (isHorizontal()) {
            if (point.x() < 0 && sourceItem != first) {
                index = 1;
            } else if(point.x() > m_appAreaWidget->width() && sourceItem != last) {
                index = m_appAreaLayout->count()-1;
            }
        } else {
            if (point.y() < 0 && sourceItem != first) {
                index = 1;
            } else if(point.y() > m_appAreaWidget->height() && sourceItem != last) {
                index = m_appAreaLayout->count()-1;
            }
        }

        if(index > -2)
            addAppAreaItem(index, sourceItem);
    }
}

void MainPanelControl::handleDragDrop(DockItem *sourceItem, QPoint point)
{
    bool needUpdateDirApp = false;
    bool needUpdateWindowSize = false;

    DockItem *targetItem = nullptr;

    if(sourceItem->itemType() == DockItem::App)
    {
        for (int i = 1 ; i < m_appAreaLayout->count()-2; ++i)
        {
            DockItem *dockItem = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(i)->widget());
            if (!dockItem || dockItem == sourceItem)
                continue;

            QRect rect(dockItem->pos(), dockItem->size());

            if (isHorizontal())
            {
                if(qFabs(rect.center().x() - point.x()) < rect.width() / 2 + MODE_PADDING) {
                    if(qFabs(rect.center().x() - point.x()) < rect.width() / 4)
                        targetItem = dockItem;
                    break;
                }
            }
            else
            {
                if(qFabs(rect.center().y() - point.y()) < rect.width() / 2 + MODE_PADDING)
                {
                    if(qFabs(rect.center().y() - point.y()) < rect.width() / 4)
                        targetItem = dockItem;
                    break;
                }
            }
        }
    }

    if(targetItem)
    {
        DockItem *replaceItem;
        AppItem *source = qobject_cast<AppItem *>(sourceItem);
        QList<QPointer<DockItem>> appList = DockItemManager::instance()->itemList();

        const int sourceIndex = appList.indexOf(sourceItem);
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
        sourceItem->setFixedSize(DockItemManager::instance()->itemSize(), DockItemManager::instance()->itemSize());

        if(afterIndex == -1)
        {
            if(beforeIndex > -1)
            {
                addAppAreaItem(beforeIndex, sourceItem);
                sourceItem->setVisible(true);
            }
            else if(AppItem *source = qobject_cast<AppItem *>(sourceItem))
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
                AppItem *source = qobject_cast<AppItem *>(sourceItem);
                DirItem *sourceDir = source->getDirItem();
                const int dirIndex = m_appAreaLayout->indexOf(sourceDir);

                sourceDir->removeItem(source);
                needUpdateWindowSize = true;

                if(dirIndex > afterIndex)
                {
                    int nextIndex = afterIndex + 1;
                    while(!target && dirIndex >= nextIndex)
                    {
                        target = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(nextIndex++)->widget());
                        if(target->itemType() == DockItem::DirApp)
                            target = qobject_cast<DirItem *>(target)->firstItem();
                    }
                }
                else
                {
                    int prevIndex = afterIndex - 1;
                    while(!target && prevIndex >=1)
                    {
                        target = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(prevIndex--)->widget());
                        if(target->itemType() == DockItem::DirApp)
                            target = qobject_cast<DirItem *>(target)->lastItem();
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
    for(auto item = dirs.begin(); item != dirs.end(); item++)
    {
        if((*item)->isEmpty())
        {
            removeAppAreaItem(*item);
            (*item)->setVisible(false);

            needUpdateDirApp = true;
            needUpdateWindowSize = true;
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
    int size;

    if (isHorizontal()) {
        size = DockItemManager::instance()->isEnableHoverScaleAnimation() ? height() * .8 : height() - 2;
        size = qMax(size, 0);
        m_splitter->setFixedSize(SPLITER_SIZE, size);
        m_splitter2->setFixedSize(SPLITER_SIZE, size);
    } else {
        size = DockItemManager::instance()->isEnableHoverScaleAnimation() ? width() * .8 : width() - 2;
        size = qMax(size, 0);
        m_splitter->setFixedSize(size, SPLITER_SIZE);
        m_splitter2->setFixedSize(size, SPLITER_SIZE);
    }

    QSize s(size, size);
    if(m_fixedAreaLayout->count() > 0)
        m_fixedAreaLayout->itemAt(0)->widget()->setFixedSize(s);

    for (int i = 1; i < m_appAreaLayout->count()-2; ++ i)
        m_appAreaLayout->itemAt(i)->widget()->setFixedSize(s);

    for(int i(0); i < m_windowAreaLayout->count(); ++i)
        m_windowAreaLayout->itemAt(i)->widget()->setFixedSize(s);

    for(int i(0); i < m_lastAreaLayout->count(); ++i)
        m_lastAreaLayout->itemAt(i)->widget()->setFixedSize(s);
}