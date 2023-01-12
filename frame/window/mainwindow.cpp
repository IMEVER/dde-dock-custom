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

#include <DWindowManagerHelper>
#include <DPlatformWindowHandle>
#include <DPlatformTheme>
#include <QDebug>
#include <QEvent>
#include <QApplication>
#include <QX11Info>
#include <qpa/qplatformwindow.h>

#define MAINWINDOW_MAX_SIZE       DOCK_MAX_SIZE
#define MAINWINDOW_MIN_SIZE       (40)

MainWindow::MainWindow(QWidget *parent) : DBlurEffectWidget(parent)
    , m_mainPanel(new MainPanelControl(this))
    , m_multiScreenWorker(new MultiScreenWorker(this, DWindowManagerHelper::instance()->hasComposite()))
    , m_menuWorker(new MenuWorker(m_multiScreenWorker->dockInter(), this))
    , m_platformWindowHandle(this)
    , m_topPanelInterface(new TopPanelInterface("me.imever.dde.TopPanel", "/me/imever/dde/TopPanel", QDBusConnection::sessionBus(), this))
{
    DPlatformWindowHandle::enableDXcbForWindow(this, true);
    m_platformWindowHandle.setEnableBlurWindow(true);
    m_platformWindowHandle.setTranslucentBackground(true);
    m_platformWindowHandle.setWindowRadius(0);
    m_platformWindowHandle.setShadowOffset(QPoint(0, 5));
    m_platformWindowHandle.setShadowColor(QColor(0, 0, 0, 0.3 * 255));

    XcbMisc::instance()->set_window_type(winId(), XcbMisc::Dock);
    initConnections();
    DockItemManager::instance()->reloadAppItems();
}

MainPanelControl *MainWindow::panel()
{
    return m_mainPanel;
}

void MainWindow::launch()
{
    setVisible(false);
    QTimer::singleShot(400, this, [ this ] {
        setVisible(true);
        setMaskColor(AutoColor);
        setMaskAlpha(m_multiScreenWorker->opacity());

        const bool composite = DWindowManagerHelper::instance()->hasComposite();
        int radius = composite ? DGuiApplicationHelper::instance()->systemTheme()->windowRadius(3) : 3;

        if(composite) {
            DGuiApplicationHelper::ColorType themeType = DGuiApplicationHelper::instance()->themeType();
            if (themeType == DGuiApplicationHelper::DarkType)
                m_platformWindowHandle.setBorderColor(QColor(0, 0, 0, 255 * 0.3));
            else
                m_platformWindowHandle.setBorderColor(QColor(QColor::Invalid));
        }

        m_platformWindowHandle.setBorderWidth(composite ? 1 : 0);
        m_platformWindowHandle.setWindowRadius(radius);
        m_multiScreenWorker->setComposite(composite);
        m_multiScreenWorker->initShow();
    });
}

void MainWindow::initConnections()
{
    connect(DWindowManagerHelper::instance(), &DWindowManagerHelper::hasCompositeChanged, this, [this]{
        const bool composite = DWindowManagerHelper::instance()->hasComposite();
        setMaskColor(AutoColor);
        setMaskAlpha(m_multiScreenWorker->opacity());
        m_platformWindowHandle.setBorderWidth(composite ? 1 : 0);
        m_multiScreenWorker->setComposite(composite);
    });
    // connect(&m_platformWindowHandle, &DPlatformWindowHandle::frameMarginsChanged, m_timer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [this](DGuiApplicationHelper::ColorType themeType){
        if (DWindowManagerHelper::instance()->hasComposite()) {
            if (themeType == DGuiApplicationHelper::DarkType)
                m_platformWindowHandle.setBorderColor(QColor(0, 0, 0, 255 * 0.3));
            else
                m_platformWindowHandle.setBorderColor(QColor(QColor::Invalid));
        }
    });
    connect(&m_platformWindowHandle, &DPlatformWindowHandle::windowRadiusChanged, this, [this]{
        int radius = DWindowManagerHelper::instance()->hasComposite() ? DGuiApplicationHelper::instance()->systemTheme()->windowRadius(3) : 3;
        m_platformWindowHandle.setWindowRadius(radius);
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

    connect(m_mainPanel, &MainPanelControl::requestResizeDockSize, this, &MainWindow::resizeDock);
    connect(m_mainPanel, &MainPanelControl::requestResizeDockSizeFinished, this, [this]{
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
            // onMainWindowSizeChanged(QPoint(0, 0));

            m_multiScreenWorker->requestUpdateFrontendGeometry();                               // 2.再更新任务栏位置,保证先1再2

            m_multiScreenWorker->requestNotifyWindowManager();
            m_multiScreenWorker->onRequestUpdateRegionMonitor();
            emit geometryChanged(geometry());
     });


    connect(m_mainPanel, &MainPanelControl::requestConttextMenu, m_menuWorker, &MenuWorker::showDockSettingsMenu);
    connect(m_menuWorker, &MenuWorker::autoHideChanged, m_multiScreenWorker, &MultiScreenWorker::onAutoHideChanged);
    connect(m_menuWorker, &MenuWorker::updatePanelGeometry, m_multiScreenWorker, &MultiScreenWorker::updateDisplay);

    connect(m_multiScreenWorker, &MultiScreenWorker::opacityChanged, this, &MainWindow::setMaskAlpha, Qt::QueuedConnection);
    // connect(m_multiScreenWorker, &MultiScreenWorker::requestUpdateDockEntry, DockItemManager::instance(), &DockItemManager::requestUpdateDockItem);
    connect(m_topPanelInterface, &TopPanelInterface::pluginVisibleChanged, this, &MainWindow::pluginVisibleChanged);
}

void MainWindow::resizeDock(int offset, bool dragging)
{
    offset = qBound(MAINWINDOW_MIN_SIZE, offset, MAINWINDOW_MAX_SIZE);

    const Position pos = m_multiScreenWorker->position();
    if(((pos == Bottom ||pos == Top) && offset == height()) || ((pos == Left ||pos == Right) && offset == width()))
        return;

    if(dragging)
        m_multiScreenWorker->setWindowSize(offset);

    // const QRect &rect = m_multiScreenWorker->getDockShowMinGeometry(m_multiScreenWorker->deskScreen());
    const QRect &rect = m_multiScreenWorker->dockRect(m_multiScreenWorker->deskScreen()
                                                      , pos
                                                      , HideMode::KeepShowing);

    QRect newRect;
    switch (pos) {
    case Bottom: {
        newRect.setX(rect.x());
        newRect.setY(rect.y() + rect.height() - offset);
        newRect.setWidth(rect.width());
        newRect.setHeight(offset);
    }
        break;
    case Left: {
        newRect.setX(rect.x());
        newRect.setY(rect.y());
        newRect.setWidth(offset);
        newRect.setHeight(rect.height());
    }
        break;
    case Right: {
        newRect.setX(rect.x() + rect.width() - offset);
        newRect.setY(rect.y());
        newRect.setWidth(offset);
        newRect.setHeight(rect.height());
    }
        break;
    case Top: break;
    }

    // 更新界面大小
    m_mainPanel->setFixedSize(newRect.size());
    setFixedSize(newRect.size());
    move(newRect.topLeft());

    if(!dragging)
        emit m_mainPanel->requestResizeDockSizeFinished();
}

QStringList MainWindow::GetLoadedPlugins() {
    return m_topPanelInterface->GetLoadedPlugins();
}

QString MainWindow::getPluginKey(QString pluginName) {
    return m_topPanelInterface->getPluginKey(pluginName);
}

bool MainWindow::getPluginVisible(QString pluginName) {
    return m_topPanelInterface->getPluginVisible(pluginName);
}

void MainWindow::setPluginVisible(QString pluginName, bool visible) {
    m_topPanelInterface->setPluginVisible(pluginName, visible);
}

