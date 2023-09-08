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
#include "xcb/xcb_misc.h"
#include "util/multiscreenworker.h"
#include "util/menuworker.h"

#include <DWindowManagerHelper>
#include <QEvent>
#include <QHBoxLayout>

#define MAINWINDOW_MAX_SIZE       DOCK_MAX_SIZE
#define MAINWINDOW_MIN_SIZE       (30)

MainWindow::MainWindow(QWidget *parent) : DBlurEffectWidget(parent)
    , m_mainPanel(new MainPanelControl(this))
    , m_multiScreenWorker(new MultiScreenWorker(this))
    , m_platformWindowHandle(this)
{
    setWindowFlag(Qt::WindowDoesNotAcceptFocus);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setMargin(0);
    layout->addWidget(m_mainPanel);

    DPlatformWindowHandle::enableDXcbForWindow(this, true);
    m_platformWindowHandle.setEnableBlurWindow(true);
    m_platformWindowHandle.setTranslucentBackground(true);
    m_platformWindowHandle.setShadowOffset(QPoint(0, 5));
    m_platformWindowHandle.setShadowColor(QColor(0, 0, 0, 0.3 * 255));
    m_platformWindowHandle.setBorderWidth(1);
    m_platformWindowHandle.setWindowRadius(6);

    XcbMisc::instance()->set_window_type(winId(), XcbMisc::Dock);
    initConnections();
}

void MainWindow::launch()
{
    QTimer::singleShot(400, this, [ this ] {

        DockItemManager::instance()->reloadAppItems();

        setMaskColor(AutoColor);
        setMaskAlpha(m_multiScreenWorker->opacity());

        DGuiApplicationHelper::ColorType themeType = DGuiApplicationHelper::instance()->themeType();
        if (themeType == DGuiApplicationHelper::DarkType)
            m_platformWindowHandle.setBorderColor(QColor(0, 0, 0, 255 * 0.3));
        else
            m_platformWindowHandle.setBorderColor(QColor(QColor::Invalid));

        m_multiScreenWorker->initShow();
    });
}

void MainWindow::callShow()
{
    if(isHidden())
        m_multiScreenWorker->updateDisplay();
}

void MainWindow::initConnections()
{
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [this](DGuiApplicationHelper::ColorType themeType){
        if (themeType == DGuiApplicationHelper::DarkType)
            m_platformWindowHandle.setBorderColor(QColor(0, 0, 0, 255 * 0.3));
        else
            m_platformWindowHandle.setBorderColor(QColor(QColor::Invalid));
    });

    connect(DockItemManager::instance(), &DockItemManager::itemInserted, m_mainPanel, &MainPanelControl::insertItem, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::itemRemoved, m_mainPanel, &MainPanelControl::removeItem, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::requestWindowAutoHide, m_multiScreenWorker, &MultiScreenWorker::onAutoHideChanged);

    connect(m_mainPanel, &MainPanelControl::itemMoved, DockItemManager::instance(), &DockItemManager::itemMoved, Qt::DirectConnection);
    connect(m_mainPanel, &MainPanelControl::itemAdded, DockItemManager::instance(), &DockItemManager::itemAdded, Qt::DirectConnection);
    connect(m_mainPanel, &MainPanelControl::folderAdded, DockItemManager::instance(), &DockItemManager::folderAdded, Qt::DirectConnection);

    connect(DockItemManager::instance(), &DockItemManager::itemCountChanged, m_multiScreenWorker, &MultiScreenWorker::updateDisplay, Qt::DirectConnection);
    connect(m_mainPanel, &MainPanelControl::itemCountChanged, m_multiScreenWorker, &MultiScreenWorker::updateDisplay);
    connect(m_mainPanel, &MainPanelControl::dirAppChanged, DockItemManager::instance(), &DockItemManager::updateDirApp);
    connect(m_mainPanel, &MainPanelControl::requestResizeDockSize, this, &MainWindow::resizeDock);

    connect(m_multiScreenWorker, &MultiScreenWorker::opacityChanged, this, &MainWindow::setMaskAlpha, Qt::QueuedConnection);
    connect(m_mainPanel, &MainPanelControl::requestConttextMenu, this, [this]{
        MenuWorker *m_menuWorker = new MenuWorker(this);
        connect(m_menuWorker, &MenuWorker::autoHideChanged, m_multiScreenWorker, &MultiScreenWorker::onAutoHideChanged);
        m_menuWorker->showDockSettingsMenu();
        m_menuWorker->deleteLater();
    });
}

void MainWindow::moveEvent(QMoveEvent *event) {
    DBlurEffectWidget::moveEvent(event);
    emit DWindowManagerHelper::instance()->windowManagerChanged();
}

void MainWindow::resizeDock(int offset, bool dragging)
{
    offset = qBound(MAINWINDOW_MIN_SIZE, offset, MAINWINDOW_MAX_SIZE);

    if(dragging)
        m_multiScreenWorker->setWindowSize(offset);

    const QRect &rect = m_multiScreenWorker->getDockShowGeometry();
    offset = m_multiScreenWorker->windowSize();

    setGeometry(rect);

    if(!dragging) {
        m_multiScreenWorker->updateDaemonDockSize(offset);
        emit geometryChanged(geometry());
    }
}
