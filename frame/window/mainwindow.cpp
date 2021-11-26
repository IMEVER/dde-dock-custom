/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             zhaolong <zhaolong@uniontech.com>
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

#include "mainwindow.h"
#include "mainpanelcontrol.h"
#include "dockitemmanager.h"
#include "util/utils.h"
#include "util/multiscreenworker.h"
#include "util/menuworker.h"

#include <DPlatformWindowHandle>
#include <DPlatformTheme>
#include <QDebug>
#include <QEvent>
#include <QApplication>
#include <QX11Info>
#include <qpa/qplatformwindow.h>

#include <X11/X.h>
#include <X11/Xutil.h>

#define MAINWINDOW_MAX_SIZE       DOCK_MAX_SIZE
#define MAINWINDOW_MIN_SIZE       (40)
#define DRAG_AREA_SIZE (5)
#define DRAG_STATE "DRAG_STATE"

class DragWidget : public QWidget
{
        Q_OBJECT
    private:
        bool m_dragStatus;
        QPoint m_resizePoint;

    public:
        DragWidget(QWidget *parent) : QWidget(parent)
        {
            m_dragStatus = false;
        }

    signals:
        void dragPointOffset(QPoint);
        void dragStarted();
        void dragFinished();

    private:
        void mousePressEvent(QMouseEvent *event) override
        {
            if (event->button() == Qt::LeftButton) {
                m_resizePoint = event->globalPos();
                m_dragStatus = true;
                emit dragStarted();
                this->grabMouse();
            }
        }

        void mouseMoveEvent(QMouseEvent *event) override
        {
            if (m_dragStatus) {
                QPoint offset = QPoint(QCursor::pos() - m_resizePoint);
                m_resizePoint = QCursor::pos();
                emit dragPointOffset(offset);
            }
        }

        void mouseReleaseEvent(QMouseEvent *event) override
        {
            if (m_dragStatus)
            {
                m_dragStatus =  false;
                releaseMouse();
                emit dragFinished();
            }
        }

        void enterEvent(QEvent *) override
        {
            if (QApplication::overrideCursor() && QApplication::overrideCursor()->shape() != cursor()) {
                QApplication::setOverrideCursor(cursor());
            }
        }

        void leaveEvent(QEvent *) override
        {
            QApplication::restoreOverrideCursor();
        }
};

MainWindow::MainWindow(QWidget *parent) : DBlurEffectWidget(parent)
    , m_launched(false)
    , m_mainPanel(new MainPanelControl(this))
    , m_wmHelper(DWindowManagerHelper::instance())
    , m_multiScreenWorker(new MultiScreenWorker(this, m_wmHelper->hasComposite()))
    , m_menuWorker(new MenuWorker(m_multiScreenWorker->dockInter(), this))
    , m_platformWindowHandle(this)
    , m_dragWidget(new DragWidget(this))
    , m_timer(new QTimer(this))
{
    setMouseTracking(true);
    setAcceptDrops(true);

    DPlatformWindowHandle::enableDXcbForWindow(this, true);
    m_platformWindowHandle.setEnableBlurWindow(true);
    m_platformWindowHandle.setTranslucentBackground(true);
    m_platformWindowHandle.setWindowRadius(0);
    m_platformWindowHandle.setShadowOffset(QPoint(0, 5));
    m_platformWindowHandle.setShadowColor(QColor(0, 0, 0, 0.3 * 255));

    XcbMisc::instance()->set_window_type(winId(), XcbMisc::Dock);
    initComponents();
    initConnections();

    DockItemManager::instance()->reloadAppItems();

    m_dragWidget->setMouseTracking(true);
    m_dragWidget->setFocusPolicy(Qt::NoFocus);
}

MainWindow::~MainWindow()
{
}

MainPanelControl *MainWindow::panel()
{
    return m_mainPanel;
}

void MainWindow::launch()
{
    setVisible(false);
    QTimer::singleShot(400, this, [ this ] {
        m_launched = true;
        setVisible(true);
        m_multiScreenWorker->initShow();
    });
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    e->ignore();
    if (e->button() == Qt::RightButton) {
        QTimer::singleShot(10, this, [this]{
            m_menuWorker->showDockSettingsMenu();
        });
    }
}

void MainWindow::initComponents()
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(100);

    QTimer::singleShot(1, this, &MainWindow::compositeChanged);
    themeTypeChanged(DGuiApplicationHelper::instance()->themeType());
}

void MainWindow::compositeChanged()
{
    const bool composite = m_wmHelper->hasComposite();

    setMaskColor(AutoColor);
    setMaskAlpha(m_multiScreenWorker->opacity());
    m_platformWindowHandle.setBorderWidth(composite ? 1 : 0);
    m_multiScreenWorker->setComposite(composite);

    m_timer->start();
}

void MainWindow::initConnections()
{
    // connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &MainWindow::compositeChanged, Qt::QueuedConnection);
    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, m_timer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(&m_platformWindowHandle, &DPlatformWindowHandle::frameMarginsChanged, m_timer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(&m_platformWindowHandle, &DPlatformWindowHandle::windowRadiusChanged, m_timer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_timer, &QTimer::timeout, this, [this]{
        if (m_launched && !m_timer->isActive())
        {
            int radius = m_wmHelper->hasComposite() ? DGuiApplicationHelper::instance()->systemTheme()->windowRadius(3) : 3;
            m_platformWindowHandle.setWindowRadius(radius);
        }
    });

    connect(DockItemManager::instance(), &DockItemManager::itemInserted, m_mainPanel, &MainPanelControl::insertItem, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::itemRemoved, m_mainPanel, &MainPanelControl::removeItem, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::itemUpdated, m_mainPanel, &MainPanelControl::itemUpdated, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::requestWindowAutoHide, m_multiScreenWorker, &MultiScreenWorker::onAutoHideChanged);

    connect(m_mainPanel, &MainPanelControl::itemMoved, DockItemManager::instance(), &DockItemManager::itemMoved, Qt::DirectConnection);
    connect(m_mainPanel, &MainPanelControl::itemAdded, DockItemManager::instance(), &DockItemManager::itemAdded, Qt::DirectConnection);

    connect(DockItemManager::instance(), &DockItemManager::itemCountChanged, m_multiScreenWorker, &MultiScreenWorker::updateDisplay, Qt::DirectConnection);
    connect(m_mainPanel, &MainPanelControl::itemCountChanged, m_multiScreenWorker, &MultiScreenWorker::updateDisplay);
    connect(m_mainPanel, &MainPanelControl::dirAppChanged, DockItemManager::instance(), &DockItemManager::updateDirApp);

    connect(m_dragWidget, &DragWidget::dragStarted, this, []{ qApp->setProperty(DRAG_STATE, true); });
    connect(m_dragWidget, &DragWidget::dragFinished, this, []{ qApp->setProperty(DRAG_STATE, false); });

    connect(m_dragWidget, &DragWidget::dragPointOffset, this, &MainWindow::onMainWindowSizeChanged);
    connect(m_dragWidget, &DragWidget::dragFinished, this, [this]{
            QRect rect = m_multiScreenWorker->dockRect(m_multiScreenWorker->deskScreen()
                                                    , m_multiScreenWorker->position()
                                                    , HideMode::KeepShowing);

            // 这个时候屏幕有可能是隐藏的，不能直接使用this->width()这种去设置任务栏的高度，而应该保证原值
            int dockSize = 0;
            if (m_multiScreenWorker->position() == Position::Bottom) {
                dockSize = this->height() == 0 ? rect.height() : this->height();
            } else {
                dockSize = this->width() == 0 ? rect.width() : this->width();
            }

            /** FIX ME
             * 作用：限制dockSize的值在40～100之间。
             * 问题1：如果dockSize为39，会导致dock的mainwindow高度变成99，显示的内容高度却是39。
             * 问题2：dockSize的值在这里不应该为39，但在高分屏上开启缩放后，拉高任务栏操作会概率出现。
             * 暂时未分析出原因，后面再修改。
             */
            dockSize = qBound(MAINWINDOW_MIN_SIZE, dockSize, MAINWINDOW_MAX_SIZE);

            // 通知窗管和后端更新数据
            m_multiScreenWorker->updateDaemonDockSize(dockSize);                                // 1.先更新任务栏高度
            onMainWindowSizeChanged(QPoint(0, 0));

            m_multiScreenWorker->requestUpdateFrontendGeometry();                               // 2.再更新任务栏位置,保证先1再2

            m_multiScreenWorker->requestNotifyWindowManager();
            m_multiScreenWorker->onRequestUpdateRegionMonitor();
     });

    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, &MainWindow::themeTypeChanged);

    connect(m_menuWorker, &MenuWorker::autoHideChanged, m_multiScreenWorker, &MultiScreenWorker::onAutoHideChanged);
    connect(m_menuWorker, &MenuWorker::updatePanelGeometry, m_multiScreenWorker, &MultiScreenWorker::updateDisplay);

    connect(m_multiScreenWorker, &MultiScreenWorker::opacityChanged, this, &MainWindow::setMaskAlpha, Qt::QueuedConnection);
    connect(m_multiScreenWorker, &MultiScreenWorker::requestUpdateFrontendGeometry, this, &MainWindow::resetDragWindow);
    // connect(m_multiScreenWorker, &MultiScreenWorker::requestUpdateDockEntry, DockItemManager::instance(), &DockItemManager::requestUpdateDockItem);
}

void MainWindow::updateDragCursor()
{
    if (Bottom == m_multiScreenWorker->position()) {
        m_dragWidget->setCursor(Qt::SizeVerCursor);
    } else {
        m_dragWidget->setCursor(Qt::SizeHorCursor);
    }
}

void MainWindow::onMainWindowSizeChanged(QPoint offset)
{
    const QRect &rect = m_multiScreenWorker->dockRect(m_multiScreenWorker->deskScreen()
                                                      , m_multiScreenWorker->position()
                                                      , HideMode::KeepShowing);
    QRect newRect;
    switch (m_multiScreenWorker->position()) {
    case Bottom: {
        newRect.setX(rect.x());
        newRect.setY(rect.y() + rect.height() - qBound(MAINWINDOW_MIN_SIZE, rect.height() - offset.y(), MAINWINDOW_MAX_SIZE));
        newRect.setWidth(rect.width());
        newRect.setHeight(qBound(MAINWINDOW_MIN_SIZE, rect.height() - offset.y(), MAINWINDOW_MAX_SIZE));
    }
        break;
    case Left: {
        newRect.setX(rect.x());
        newRect.setY(rect.y());
        newRect.setWidth(qBound(MAINWINDOW_MIN_SIZE, rect.width() + offset.x(), MAINWINDOW_MAX_SIZE));
        newRect.setHeight(rect.height());
    }
        break;
    case Right: {
        newRect.setX(rect.x() + rect.width() - qBound(MAINWINDOW_MIN_SIZE, rect.width() - offset.x(), MAINWINDOW_MAX_SIZE));
        newRect.setY(rect.y());
        newRect.setWidth(qBound(MAINWINDOW_MIN_SIZE, rect.width() - offset.x(), MAINWINDOW_MAX_SIZE));
        newRect.setHeight(rect.height());
    }
        break;
        case Top: break;
    }

    // 更新界面大小
    m_mainPanel->setFixedSize(newRect.size());
    setFixedSize(newRect.size());
    move(newRect.topLeft());

    if(qApp->property(DRAG_STATE).toBool())
        m_multiScreenWorker->setWindowSize(m_multiScreenWorker->position() == Bottom ? newRect.height() : newRect.width());
}

void MainWindow::resetDragWindow()
{
    if(width() > 0 && height() > 0)
    {
        switch (m_multiScreenWorker->position()) {
        case Dock::Bottom:
            m_dragWidget->setGeometry(0, 0, width(), DRAG_AREA_SIZE);
            break;
        case Dock::Left:
            m_dragWidget->setGeometry(width() - DRAG_AREA_SIZE, 0, DRAG_AREA_SIZE, height());
            break;
        case Dock::Right:
            m_dragWidget->setGeometry(0, 0, DRAG_AREA_SIZE, height());
            break;
        case Top: break;
        }

        updateDragCursor();
        m_timer->start();
    }
}

void MainWindow::resizeDock(int offset, bool dragging)
{
    qApp->setProperty(DRAG_STATE, dragging);

    const QRect &rect = m_multiScreenWorker->getDockShowMinGeometry(m_multiScreenWorker->deskScreen());
    QRect newRect;
    switch (m_multiScreenWorker->position()) {
    case Bottom: {
        newRect.setX(rect.x());
        newRect.setY(rect.y() + rect.height() - qBound(MAINWINDOW_MIN_SIZE, offset, MAINWINDOW_MAX_SIZE));
        newRect.setWidth(rect.width());
        newRect.setHeight(qBound(MAINWINDOW_MIN_SIZE, offset, MAINWINDOW_MAX_SIZE));
    }
        break;
    case Left: {
        newRect.setX(rect.x());
        newRect.setY(rect.y());
        newRect.setWidth(qBound(MAINWINDOW_MIN_SIZE, offset, MAINWINDOW_MAX_SIZE));
        newRect.setHeight(rect.height());
    }
        break;
    case Right: {
        newRect.setX(rect.x() + rect.width() - qBound(MAINWINDOW_MIN_SIZE, offset, MAINWINDOW_MAX_SIZE));
        newRect.setY(rect.y());
        newRect.setWidth(qBound(MAINWINDOW_MIN_SIZE, offset, MAINWINDOW_MAX_SIZE));
        newRect.setHeight(rect.height());
    }
        break;
    case Top: break;
    }

    // 更新界面大小
    m_mainPanel->setFixedSize(newRect.size());
    setFixedSize(newRect.size());
    move(newRect.topLeft());

    if (!dragging)
        resetDragWindow();
}

void MainWindow::themeTypeChanged(DGuiApplicationHelper::ColorType themeType)
{
    if (m_wmHelper->hasComposite()) {
        if (themeType == DGuiApplicationHelper::DarkType)
            m_platformWindowHandle.setBorderColor(QColor(0, 0, 0, 255 * 0.3));
        else
            m_platformWindowHandle.setBorderColor(QColor(QColor::Invalid));
    }
}

#include "mainwindow.moc"
