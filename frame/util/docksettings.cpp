// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "docksettings.h"
#include "settings.h"
#include "../taskmanager/common.h"

#include <DConfig>

#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <qobjectdefs.h>

DCORE_USE_NAMESPACE

DockSettings::DockSettings(QObject *parent)
 : QObject (parent)
 , m_dockSettings(Settings::ConfigPtr(configDock))
{
    init();
}

void DockSettings::init()
{
    // 绑定属性
    if (m_dockSettings) {
            connect(m_dockSettings, &DConfig::valueChanged, this, [&] (const QString &key) {
                if (key == keyHideMode) {
                    Q_EMIT hideModeChanged(HideModeHandler(m_dockSettings->value(keyHideMode).toString()).toEnum());
                } else if (key == keyPosition) {
                    Q_EMIT positionModeChanged(Position(PositionModeHandler(m_dockSettings->value(key).toString()).toEnum()));
                } else if (key == keyForceQuitApp){
                    Q_EMIT forceQuitAppChanged(ForceQuitAppModeHandler(m_dockSettings->value(key).toString()).toEnum());
                } else if (key == keyShowRecent) {
                    Q_EMIT showRecentChanged(m_dockSettings->value(key).toBool());
                } else if (key == keyShowMultiWindow) {
                    Q_EMIT showMultiWindowChanged(m_dockSettings->value(key).toBool());
                } else if ( key == keyShowWindowName) {
                    Q_EMIT windowNameShowModeChanged(m_dockSettings->value(keyShowWindowName).toInt());
                } else if ( key == keyWindowSizeFashion) {
                    Q_EMIT windowSizeFashionChanged(m_dockSettings->value(keyWindowSizeFashion).toUInt());
                }
            });
    }
}

HideMode DockSettings::getHideMode()
{
    if (m_dockSettings) {
        QString mode = m_dockSettings->value(keyHideMode).toString();
        HideModeHandler handler(mode);
        return handler.toEnum();
    }
    return HideMode::KeepShowing;
}

void DockSettings::setHideMode(HideMode mode)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyHideMode, HideModeHandler(mode).toString());
    }
}

Position DockSettings::getPositionMode()
{
    Position ret = Position::Bottom;
    if (m_dockSettings) {
        QString mode = m_dockSettings->value(keyPosition).toString();
        PositionModeHandler handler(mode);
        ret = handler.toEnum();
    }
    return ret;
}

void DockSettings::setPositionMode(Position mode)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyPosition, PositionModeHandler(mode).toString());
    }
}

ForceQuitAppMode DockSettings::getForceQuitAppMode()
{
    ForceQuitAppMode ret = ForceQuitAppMode::Enabled;
    if (m_dockSettings) {
        QString mode = m_dockSettings->value(keyForceQuitApp).toString();
        ForceQuitAppModeHandler handler(mode);
        ret = handler.toEnum();
    }
    return ret;
}

void DockSettings::setForceQuitAppMode(ForceQuitAppMode mode)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyForceQuitApp, ForceQuitAppModeHandler(mode).toString());
    }
}

uint DockSettings::getIconSize()
{
    uint size = 36;
    if (m_dockSettings) {
        size = m_dockSettings->value(keyIconSize).toUInt();
    }
    return size;
}

void DockSettings::setIconSize(uint size)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyIconSize, size);
    }
}

uint DockSettings::getShowTimeout()
{
    uint time = 100;
    if (m_dockSettings) {
        time = m_dockSettings->value(keyShowTimeout).toUInt();
    }
    return time;
}

void DockSettings::setShowTimeout(uint time)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyShowTimeout, time);
    }
}

uint DockSettings::getHideTimeout()
{
    uint time = 0;
    if (m_dockSettings) {
        time = m_dockSettings->value(keyHideTimeout).toUInt();
    }
    return time;
}

void DockSettings::setHideTimeout(uint time)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyHideTimeout, time);
    }
}

uint DockSettings::getWindowSizeFashion()
{
    uint size = 48;
    if (m_dockSettings) {
        size = m_dockSettings->value(keyWindowSizeFashion).toUInt();
    }
    return size;
}

void DockSettings::setWindowSizeFashion(uint size)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyWindowSizeFashion, size);
    }
}

void DockSettings::saveStringList(const QString &key, const QStringList &values)
{
    if (!m_dockSettings)
        return;

    m_dockSettings->setValue(key, values);
}

QStringList DockSettings::loadStringList(const QString &key) const
{
    QStringList ret;
    if (!m_dockSettings)
        return ret;

    for(const auto &var : m_dockSettings->value(key).toList()) {
        if (var.isValid())
            ret.push_back(var.toString());
    }

    return ret;
}

QStringList DockSettings::getDockedApps()
{
    return loadStringList(keyDockedApps);
}

void DockSettings::setDockedApps(const QStringList &apps)
{
    saveStringList(keyDockedApps, apps);
}

QStringList DockSettings::getRecentApps() const
{
    return loadStringList(keyRecentApp);
}

void DockSettings::setRecentApps(const QStringList &apps)
{
    saveStringList(keyRecentApp, apps);
}

QVector<QString> DockSettings::getWinIconPreferredApps()
{
    QVector<QString> ret;
    if (m_dockSettings) {
        for(const auto &var : m_dockSettings->value(keyWinIconPreferredApps).toList()) {
            if (var.isValid())
                ret.push_back(var.toString());
        }
    }

    return ret;
}

void DockSettings::setShowRecent(bool visible)
{
    if (!m_dockSettings)
        return;

    m_dockSettings->setValue(keyShowRecent, visible);
}

bool DockSettings::showRecent() const
{
    if (!m_dockSettings)
        return false;

    return m_dockSettings->value(keyShowRecent).toBool();
}

void DockSettings::setShowMultiWindow(bool showMultiWindow)
{
    if (!m_dockSettings)
        return;

    m_dockSettings->setValue(keyShowMultiWindow, showMultiWindow);
}

bool DockSettings::showMultiWindow() const
{
    if (!m_dockSettings)
        return false;

    return m_dockSettings->value(keyShowMultiWindow).toBool();
}

int DockSettings::getWindowNameShowMode()
{
    if (!m_dockSettings)
        return 0;
    return m_dockSettings->value(keyShowWindowName).toInt();
}

void DockSettings::setWindowNameShowMode(int value)
{
    if (!m_dockSettings)
        return;
    m_dockSettings->setValue(keyShowWindowName, value);
}
