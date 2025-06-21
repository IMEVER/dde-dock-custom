// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dockdaemonadaptors.h"
#include "../util/docksettings.h"
#include "../taskmanager/taskmanager.h"

DockDaemonDBusAdaptor::DockDaemonDBusAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
    connect(TaskManager::instance(), &TaskManager::hideStateChanged, this, &DockDaemonDBusAdaptor::HideStateChanged);
    connect(TaskManager::instance(), &TaskManager::frontendWindowRectChanged, this, &DockDaemonDBusAdaptor::FrontendWindowRectChanged);
    connect(TaskManager::instance(), &TaskManager::showRecentChanged, this, &DockDaemonDBusAdaptor::showRecentChanged);
}

DockDaemonDBusAdaptor::~DockDaemonDBusAdaptor()
{
    // destructor
}

int DockDaemonDBusAdaptor::displayMode() const
{
    return 0;
}

void DockDaemonDBusAdaptor::setDisplayMode(int value)
{
}

int DockDaemonDBusAdaptor::hideMode() const
{
    return TaskManager::instance()->getHideMode();
}

void DockDaemonDBusAdaptor::setHideMode(int value)
{
    if (hideMode() != value) {
        TaskManager::instance()->setHideMode(static_cast<HideMode>(value));
        Q_EMIT HideModeChanged(value);
    }
}

int DockDaemonDBusAdaptor::hideState() const
{
    return TaskManager::instance()->getHideState();
}

uint DockDaemonDBusAdaptor::hideTimeout() const
{
    return TaskManager::instance()->getHideTimeout();
}

void DockDaemonDBusAdaptor::setHideTimeout(uint value)
{
    if (hideTimeout() != value) {
        TaskManager::instance()->setHideTimeout(value);
        Q_EMIT HideTimeoutChanged(value);
    }
}

uint DockDaemonDBusAdaptor::windowSizeEfficient() const
{
    return TaskManager::instance()->getWindowSizeFashion();
}

void DockDaemonDBusAdaptor::setWindowSizeEfficient(uint value)
{
    if (windowSizeEfficient() != value) {
        TaskManager::instance()->setWindowSizeFashion(value);
        Q_EMIT WindowSizeEfficientChanged(value);
    }
}

uint DockDaemonDBusAdaptor::windowSizeFashion() const
{
    return TaskManager::instance()->getWindowSizeFashion();
}

void DockDaemonDBusAdaptor::setWindowSizeFashion(uint value)
{
    if (windowSizeFashion() != value) {
        TaskManager::instance()->setWindowSizeFashion(value);
        Q_EMIT WindowSizeFashionChanged(value);
    }
}

QRect DockDaemonDBusAdaptor::frontendWindowRect() const
{
    return TaskManager::instance()->getFrontendWindowRect();
}

uint DockDaemonDBusAdaptor::iconSize() const
{
    return TaskManager::instance()->getIconSize();
}

void DockDaemonDBusAdaptor::setIconSize(uint value)
{
    if (iconSize() != value) {
        TaskManager::instance()->setIconSize(value);
        Q_EMIT IconSizeChanged(value);
    }
}

int DockDaemonDBusAdaptor::position() const
{
    return TaskManager::instance()->getPosition();
}

void DockDaemonDBusAdaptor::setPosition(int value)
{
    if (position() != value) {
        TaskManager::instance()->setPosition(value);
        Q_EMIT PositionChanged(value);
    }
}

uint DockDaemonDBusAdaptor::showTimeout() const
{
    return TaskManager::instance()->getShowTimeout();
}

void DockDaemonDBusAdaptor::setShowTimeout(uint value)
{
    if (showTimeout() != value) {
        TaskManager::instance()->setShowTimeout(value);
        Q_EMIT ShowTimeoutChanged(value);
    }
}

bool DockDaemonDBusAdaptor::showRecent() const
{
    return DockSettings::instance()->showRecent();
}

bool DockDaemonDBusAdaptor::showMultiWindow() const
{
    return false;
}

bool DockDaemonDBusAdaptor::IsDocked(const QString &desktopFile)
{
    return TaskManager::instance()->isDocked(desktopFile);
}

bool DockDaemonDBusAdaptor::RequestDock(const QString &desktopFile, int index)
{
    return TaskManager::instance()->requestDock(desktopFile, index);
}

bool DockDaemonDBusAdaptor::RequestUndock(const QString &desktopFile)
{
    return TaskManager::instance()->requestUndock(desktopFile);
}

void DockDaemonDBusAdaptor::SetShowRecent(bool visible)
{
    DockSettings::instance()->setShowRecent(visible);
}

void DockDaemonDBusAdaptor::SetShowMultiWindow(bool showMultiWindow)
{

}
