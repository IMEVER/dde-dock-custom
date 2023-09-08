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

#include <dtkwidget_global.h>
#include <dtkgui_global.h>
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
DGUI_USE_NAMESPACE

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
                emit m_parent->requestResizeDockSize(m_parent->isHorizontal() ? m_parent->height() : m_parent->width(), false);
            }
        }

        void mousePressEvent(QMouseEvent *event) override {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if(mouseEvent->button() == Qt::RightButton)
                emit m_parent->requestConttextMenu();
            else if(mouseEvent->button() == Qt::LeftButton) {
                dragging = true;
                lastPos = QCursor::pos();
                lastSize = m_parent->isHorizontal() ? m_parent->height() : m_parent->width();
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
static AppDrag *appDrag(nullptr);

MainPanelControl::MainPanelControl(QWidget *parent) : QWidget(parent)
    , m_fixedAreaLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_appAreaLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_windowAreaLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_lastAreaLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_splitter(new SplitterWidget(this))
    , m_splitter2(new SplitterWidget(this))
    , m_position(Position::Bottom)
    , m_placeholderItem(nullptr)
{
    init();
    updateMainPanelLayout();
    m_splitter->hide();
    setAcceptDrops(true);
}

MainPanelControl::~MainPanelControl(){}

void MainPanelControl::init()
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    QBoxLayout *m_mainPanelLayout(new QBoxLayout(QBoxLayout::LeftToRight, this));
    m_mainPanelLayout->setAlignment(Qt::AlignCenter);
    m_mainPanelLayout->setMargin(0);
    m_mainPanelLayout->setSpacing(MODE_PADDING);

    m_mainPanelLayout->addStretch(1);
    m_mainPanelLayout->addLayout(m_fixedAreaLayout);
    m_mainPanelLayout->addLayout(m_appAreaLayout);
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
    m_appAreaLayout->setMargin(0);
    m_appAreaLayout->setContentsMargins(0, 0, 0, 0);
    m_appAreaLayout->setSpacing(MODE_PADDING);
    m_appAreaLayout->setAlignment(Qt::AlignCenter);

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
            qobject_cast<QBoxLayout*>(layout())->setDirection(QBoxLayout::LeftToRight);
            m_fixedAreaLayout->setDirection(QBoxLayout::LeftToRight);
            m_appAreaLayout->setDirection(QBoxLayout::LeftToRight);
            m_windowAreaLayout->setDirection(QBoxLayout::LeftToRight);
            m_lastAreaLayout->setDirection(QBoxLayout::LeftToRight);
            break;
        case Position::Right:
        case Position::Left:
            qobject_cast<QBoxLayout*>(layout())->setDirection(QBoxLayout::TopToBottom);
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

void MainPanelControl::insertItem(int index, DockItem *item, bool animation)
{
    connect(item, &DockItem::inoutFinished, this, [this, item](bool in){
        if(in == false)
        {
            switch (item->itemType()) {
                case DockItem::App:
                case DockItem::Placeholder:
                case DockItem::DirApp:
                    removeAppAreaItem(item);
                    break;
                case DockItem::Plugins:
                    m_lastAreaLayout->removeWidget(item);
                    break;
                case DockItem::Window:
                    m_windowAreaLayout->removeWidget(item);
                    if(m_windowAreaLayout->isEmpty()) m_splitter->hide();
                    break;
                default:
                    break;
            }
        }
    });

    if(item->itemType() == DockItem::App || item->itemType() == DockItem::DirApp || item->itemType() == DockItem::Launcher)
        item->installEventFilter(this);

    switch (item->itemType()) {
        case DockItem::Launcher:
            addFixedAreaItem(0, item);
            break;
        case DockItem::App:
        case DockItem::Placeholder:
        case DockItem::DirApp:
            addAppAreaItem(index ==-1 ? m_appAreaLayout->count() : index, item);
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

    item->easeIn(animation);
}

void MainPanelControl::removeItem(DockItem *item, bool animation)
{
    item->easeOut(animation);
}

bool MainPanelControl::eventFilter(QObject *watched, QEvent *event)
{
    if (appDrag && watched == static_cast<QGraphicsView *>(appDrag->appDragWidget())->viewport()) {
        QDropEvent *e = static_cast<QDropEvent *>(event);
        if (e && rect().contains(mapFromGlobal(appDrag->appDragWidget()->mapToGlobal(e->pos())))) {
            QPoint pos = mapFromGlobal(appDrag->appDragWidget()->mapToGlobal(e->pos()));
            if (event->type() == QEvent::DragMove) {
                dropTargetItem(qobject_cast<DockItem *>(e->source()), pos);
                return true;
            } else if (event->type() == QEvent::Drop) {
                handleDragDrop(qobject_cast<DockItem *>(e->source()), pos);
                appDrag->appDragWidget()->hide();
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
        static int m_mousePressTime;
        if(event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if(mouseEvent->button() == Qt::LeftButton) {
                m_mousePressPos = mouseEvent->globalPos();
                m_mousePressTime = QDateTime::currentMSecsSinceEpoch();
            }
        } else if(event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if(mouseEvent->button() == Qt::LeftButton)
                m_mousePressPos *= 0;
        } else if(event->type() == QEvent::MouseMove && !m_mousePressPos.isNull()) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            const QPoint distance = mouseEvent->globalPos() - m_mousePressPos;
            const int disTime = QDateTime::currentMSecsSinceEpoch() - m_mousePressTime;
            if (distance.manhattanLength() >= QApplication::startDragDistance() && disTime >= 100 /*QApplication::startDragTime()*/) {
                beforeIndex = m_appAreaLayout->indexOf(item);
                startDrag(item);
                beforeIndex = -1;
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void MainPanelControl::dragEnterEvent(QDragEnterEvent *event) {

    // 拖app到dock上
    auto DragmineData = event->mimeData();
    m_draggingMimeKey = DragmineData->formats().contains("RequestDock") ? "RequestDock" : "text/plain";

    // dragging item is NOT a desktop file
    if (QMimeDatabase().mimeTypeForFile(DragmineData->data(m_draggingMimeKey)).name() != "application/x-desktop")
        event->ignore();
    else if (QString(DragmineData->data(m_draggingMimeKey)).endsWith("dde-trash.desktop"))
        event->ignore();
    else if (DockItemManager::instance()->appIsOnDock(DragmineData->data(m_draggingMimeKey)))
        event->ignore();
    else event->accept(m_appAreaLayout->geometry());

    if(event->isAccepted() == false && DragmineData->hasUrls()) {
        QList<QUrl> urls = DragmineData->urls();
        if(urls.size() == 1 && urls.first().isLocalFile()) {
            QFileInfo info(urls.first().toLocalFile());//TODO: add features
            if(info.exists() && info.isDir()) {
                event->accept(m_lastAreaLayout->geometry() | m_splitter2->geometry());
                m_draggingMimeKey.clear();
            }
        }
    }
}

void MainPanelControl::dragMoveEvent(QDragMoveEvent *event) {
    if (m_placeholderItem.isNull())
        m_placeholderItem = new PlaceholderItem;

    if(m_draggingMimeKey.isEmpty()) {
        if(m_lastAreaLayout->geometry().united(m_splitter2->geometry()).contains(event->pos())) {
            if(m_lastAreaLayout->indexOf(m_placeholderItem) == -1) {
                const int width = DockItemManager::instance()->itemSize();
                m_placeholderItem->setFixedSize(width-2, width-2);
                m_lastAreaLayout->insertWidget(0, m_placeholderItem);
            }
        } else
            m_lastAreaLayout->removeWidget(m_placeholderItem);
    } else
        dropTargetItem(m_placeholderItem, event->pos());
}

void MainPanelControl::dropEvent(QDropEvent *event) {
    if (m_placeholderItem) {
        QPoint point = event->pos();
        if(m_draggingMimeKey.isEmpty()) {
            if(m_lastAreaLayout->geometry().contains(event->pos()))
                emit folderAdded(event->mimeData()->urls().first().toLocalFile());

            m_lastAreaLayout->removeWidget(m_placeholderItem);
            m_placeholderItem->deleteLater();
        } else {
            if(m_appAreaLayout->geometry().contains(point)) {
                DirItem *targetItem = nullptr;

                for (int i = 0; i < m_appAreaLayout->count(); ++i)
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
                    targetItem->addId(event->mimeData()->data(m_draggingMimeKey));
                }

                emit itemAdded(event->mimeData()->data(m_draggingMimeKey), index-1);
            }

            removeAppAreaItem(m_placeholderItem);
            m_placeholderItem->deleteLater();
        }
    }
}

void MainPanelControl::dragLeaveEvent(QDragLeaveEvent *event) {
    if (m_placeholderItem) {
        removeAppAreaItem(m_placeholderItem);
        m_placeholderItem->deleteLater();
    }
}

void MainPanelControl::startDrag(DockItem *item)
{
    item->update();
    const QPixmap pixmap = item->grab();
    appDrag = new AppDrag(item);
    connect(appDrag->appDragWidget(), &AppDragWidget::finished, this, [this, item](bool undock){
        if(undock && qobject_cast<AppItem*>(item))
            qobject_cast<AppItem*>(item)->undock();
        else
            handleDragDrop(item, item->geometry().center());
    });
    appDrag->appDragWidget()->setOriginPos((mapToGlobal(item->pos())));
    appDrag->appDragWidget()->setDockInfo(m_position, QRect(mapToGlobal(QPoint(0, 0)), size()));

    appDrag->setPixmap(pixmap);
    static_cast<QGraphicsView *>(appDrag->appDragWidget())->viewport()->installEventFilter(this);
    appDrag->setHotSpot(pixmap.rect().center() / pixmap.devicePixelRatioF());

    appDrag->setMimeData(new QMimeData);
    appDrag->exec(Qt::MoveAction);
    appDrag = nullptr;

    item->update();
}

void MainPanelControl::dropTargetItem(DockItem *sourceItem, QPoint point)
{
    static QPoint lastPos;
    const int width = DockItemManager::instance()->itemSize();

    if(m_appAreaLayout->geometry().contains(point) == false) {
        if(m_appAreaLayout->indexOf(sourceItem) == -1) {
            sourceItem->setFixedSize(width * .1, width * .1);
            if(isHorizontal())
                addAppAreaItem(point.x() < m_appAreaLayout->geometry().x() ? 0 : m_appAreaLayout->count(), sourceItem);
            else
                addAppAreaItem(point.y() < m_appAreaLayout->geometry().y() ? 0 : m_appAreaLayout->count(), sourceItem);
        }
        return;
    }
    if(m_appAreaLayout->isEmpty())
        return addAppAreaItem(0, sourceItem);
    if(m_appAreaLayout->count() == 1 && m_appAreaLayout->itemAt(0)->widget() == sourceItem)
        return;

    const bool animation = DockItemManager::instance()->isEnableDragAnimation();

    for (int i = 0; i < m_appAreaLayout->count(); ++i)
    {
        DockItem *dockItem = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(i)->widget());
        qreal ratio = 1;
        QRect rect = dockItem->geometry();

        if(isHorizontal())
        {
            if(rect.left()-MODE_PADDING/2 <= point.x() && rect.right()+MODE_PADDING/2 >= point.x() ) {
                if(sourceItem == dockItem)
                {
                    if(animation) ratio = 1 - (qFabs(rect.center().x() - point.x())) / ( width * 1.0);
                }
                else if(qFabs(rect.center().x() - point.x()) < rect.width()/4)
                {
                    // if(animation && sourceItem->itemType() != DockItem::DirApp && m_appAreaLayout->indexOf(sourceItem) > -1)
                    // {
                    //     removeAppAreaItem(sourceItem);
                    //     sourceItem->setVisible(false);
                    // }
                    if(m_appAreaLayout->indexOf(sourceItem) == -1)
                    {
                        addAppAreaItem(m_appAreaLayout->indexOf(dockItem) + ( point.x() > rect.center().x() ? 1 : 0 ) , sourceItem);
                        sourceItem->setVisible(true);
                    }
                    if(animation) ratio = .1;
                }
                else if (qFabs(rect.center().x() - point.x()) > rect.width()/4)
                {
                    if(qFabs(rect.center().x() - point.x()) > qFabs(rect.center().x() - lastPos.x()))
                    {
                        if(m_appAreaLayout->indexOf(sourceItem) == -1)
                        {
                            addAppAreaItem(m_appAreaLayout->indexOf(dockItem) + ( point.x() > rect.center().x() ? 1 : 0 ) , sourceItem);
                            sourceItem->setVisible(true);
                        }
                        else //if(!animation || sourceItem->itemType() == DockItem::DirApp)
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
                    }
                    if(animation) ratio = qMax(.1, qFabs(rect.center().x() - point.x()) / ( width * 1.0 ));
                }

                if(animation) sourceItem->setFixedSize(width * ratio, width * ratio);

                lastPos = point;
                return;
            }
        }
        else
        {
            if(rect.top()-MODE_PADDING/2 <= point.y() && rect.bottom()+MODE_PADDING/2 >= point.y() ) {
                if(sourceItem == dockItem)
                {
                    if(animation) ratio = 1 - qFabs(rect.center().y() - point.y()) / ( width / 1.0);
                }
                else if(qFabs(rect.center().y() - point.y()) < rect.width()/5)
                {
                    // if(animation && sourceItem->itemType() != DockItem::DirApp && m_appAreaLayout->indexOf(sourceItem) > -1)
                    // {
                    //     removeAppAreaItem(sourceItem);
                    //     sourceItem->setVisible(false);
                    // }
                    if(m_appAreaLayout->indexOf(sourceItem) == -1)
                    {
                        addAppAreaItem(m_appAreaLayout->indexOf(dockItem) + ( point.y() > rect.center().y() ? 1 : 0 ), sourceItem);
                        sourceItem->setVisible(true);
                    }
                    if(animation) ratio = .1;
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
                        else //if(!animation || sourceItem->itemType() == DockItem::DirApp)
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
                    }
                    if(animation) ratio = qMax(.1, qFabs(rect.center().y() - point.y()) / ( width / 1.0 ));
                }

                if(animation) sourceItem->setFixedSize(width * ratio, width * ratio);

                lastPos = point;
                return;
            }
        }
    }
    lastPos = point;
}

void MainPanelControl::handleDragDrop(DockItem *sourceItem, QPoint point)
{
    bool needUpdateDirApp = false;
    bool needUpdateWindowSize = false;
    DockItem *targetItem = nullptr;

    if(sourceItem->itemType() == DockItem::App && m_appAreaLayout->geometry().contains(point))
    {
        for (int i = 0; i < m_appAreaLayout->count(); ++i)
        {
            DockItem *dockItem = qobject_cast<DockItem *>(m_appAreaLayout->itemAt(i)->widget());
            QRect rect(dockItem->pos(), dockItem->size());
            if (isHorizontal())
            {
                if(qFabs(rect.center().x() - point.x()) < rect.width() / 2 + MODE_PADDING/2) {
                    if(qFabs(rect.center().x() - point.x()) < rect.width() / 4)
                        targetItem = dockItem;
                    break;
                }
            }
            else
            {
                if(qFabs(rect.center().y() - point.y()) < rect.width() / 2 + MODE_PADDING/2)
                {
                    if(qFabs(rect.center().y() - point.y()) < rect.width() / 4)
                        targetItem = dockItem;
                    break;
                }
            }
        }
        if(targetItem == sourceItem) targetItem = nullptr;
    }

    if(targetItem)
    {
        AppItem *replaceItem;
        AppItem *source = qobject_cast<AppItem *>(sourceItem);
        QList<QPointer<AppItem>> appList = DockItemManager::instance()->itemList();

        const int sourceIndex = appList.indexOf(source);
        int replaceIndex;

        DirItem *sourceDir = source->getDirItem();
        if(sourceDir)
        {
            if(sourceDir == targetItem) return;

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
            replaceItem = qobject_cast<AppItem*>(targetItem);

            int currentIndex = m_appAreaLayout->indexOf(targetItem);
            if(!sourceDir)
            {
                replaceIndex = appList.indexOf(replaceItem);
                if(replaceIndex > sourceIndex) currentIndex --;
            }

            removeAppAreaItem(targetItem);

            DirItem *createDirItem = DockItemManager::instance()->createDir();
            createDirItem->addItem(qobject_cast<AppItem *>(targetItem));
            createDirItem->addItem(source);

            addAppAreaItem(currentIndex, createDirItem);
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

        if(replaceItem) emit itemMoved(source, replaceItem);
    }
    else
    {
        const int afterIndex = m_appAreaLayout->indexOf(sourceItem);
        sourceItem->setFixedSize(DockItemManager::instance()->itemSize(), DockItemManager::instance()->itemSize());

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
                while(!target && prevIndex >=0)
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
                emit itemMoved(qobject_cast<AppItem*>(sourceItem), qobject_cast<AppItem*>(target));
            else
                for(auto oneSource : qobject_cast<DirItem *>(sourceItem)->getAppList())
                    emit itemMoved(oneSource, qobject_cast<AppItem*>(target));
        }
    }

    if(needUpdateDirApp == false)
        for(int i=0,len=m_appAreaLayout->count(); i<len; i++)
        {
            auto item = qobject_cast<DirItem*>(m_appAreaLayout->itemAt(i)->widget());
            if(item && item->isEmpty())
            {
                needUpdateDirApp = true;
                needUpdateWindowSize = true;
                break;
            }
        }

    if(needUpdateDirApp) emit dirAppChanged();
    if(needUpdateWindowSize) emit itemCountChanged();
}

void MainPanelControl::resizeDockIcon()
{
    if(m_fixedAreaLayout->count() == 0) return;

    const int oldSize = m_fixedAreaLayout->itemAt(0)->widget()->width();
    int size;

    if (isHorizontal()) {
        size = qMax(height() - 2, 0);
        if(size == oldSize) return;
        m_splitter->setFixedSize(SPLITER_SIZE, size);
        m_splitter2->setFixedSize(SPLITER_SIZE, size);
    } else {
        size = qMax(width() - 2, 0);
        if(size == oldSize) return;
        m_splitter->setFixedSize(size, SPLITER_SIZE);
        m_splitter2->setFixedSize(size, SPLITER_SIZE);
    }

    QSize s(size, size);
    if(m_fixedAreaLayout->count() > 0)
        m_fixedAreaLayout->itemAt(0)->widget()->setFixedSize(s);

    for (int i = 0; i < m_appAreaLayout->count(); ++ i)
        m_appAreaLayout->itemAt(i)->widget()->setFixedSize(s);

    for(int i(0); i < m_windowAreaLayout->count(); ++i)
        m_windowAreaLayout->itemAt(i)->widget()->setFixedSize(s);

    for(int i(0); i < m_lastAreaLayout->count(); ++i)
        m_lastAreaLayout->itemAt(i)->widget()->setFixedSize(s);
}