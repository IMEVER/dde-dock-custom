/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
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

#include "dbusdockadaptors.h"
#include "../util/utils.h"
#include "../window/mainwindow.h"
#include "TopPanelInterface.h"

#include <QGSettings>
#include <QDBusMetaType>

QDebug operator<<(QDebug argument, const DockItemInfo &info)
{
    argument << "name:" << info.name << ", displayName:" << info.displayName
            << "itemKey:" << info.itemKey << "SettingKey:" << info.settingKey
            << "icon_light:" << info.iconLight << "icon_dark:" << info.iconDark << "visible:" << info.visible;
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &arg, const DockItemInfo &info)
{
    arg.beginStructure();
    arg << info.name << info.displayName << info.itemKey << info.settingKey << info.iconLight << info.iconDark << info.visible;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, DockItemInfo &info)
{
    arg.beginStructure();
    arg >> info.name >> info.displayName >> info.itemKey >> info.settingKey >> info.iconLight >> info.iconDark >> info.visible;
    arg.endStructure();
    return arg;
}

void registerPluginInfoMetaType()
{
    qRegisterMetaType<DockItemInfo>("DockItemInfo");
    qDBusRegisterMetaType<DockItemInfo>();
    qRegisterMetaType<DockItemInfos>("DockItemInfos");
    qDBusRegisterMetaType<DockItemInfos>();
}

DBusDockAdaptors::DBusDockAdaptors(MainWindow* parent)
    : QDBusAbstractAdaptor(parent)
    , m_gsettings(Utils::SettingsPtr("com.deepin.dde.dock.mainwindow", QByteArray(), this))
    , m_window(parent)
{
    registerPluginInfoMetaType();

    connect(parent, &MainWindow::geometryChanged, this, &DBusDockAdaptors::geometryChanged);

    if (m_gsettings) {
        connect(m_gsettings, &QGSettings::changed, this, [ = ] (const QString &key) {
            if (key == "onlyShowPrimary") {
                Q_EMIT showInPrimaryChanged(m_gsettings->get(key).toBool());
            }
        });
    }

    m_topPanelInterface = new TopPanelInterface("me.imever.TopPanel", "/me/imever/TopPanel", QDBusConnection::sessionBus(), this);
    connect(m_topPanelInterface, &TopPanelInterface::pluginVisibleChanged, this, &DBusDockAdaptors::pluginVisibleChanged);
}

DBusDockAdaptors::~DBusDockAdaptors()
{

}

void DBusDockAdaptors::callShow()
{
    m_window->callShow();
}

void DBusDockAdaptors::ReloadPlugins()
{
    // if (qApp->property("PLUGINSLOADED").toBool())
    //     return;

    // // 发送事件，通知代理来加载插件
    // PluginLoadEvent event;
    // QCoreApplication::sendEvent(qApp, &event);

    // qApp->setProperty("PLUGINSLOADED", true);
    // // 退出安全模式
    // qApp->setProperty("safeMode", false);
}

QStringList DBusDockAdaptors::GetLoadedPlugins()
{
    return m_topPanelInterface->GetLoadedPlugins();
}

DockItemInfos DBusDockAdaptors::plugins()
{
    return m_topPanelInterface->plugins();
}

void DBusDockAdaptors::resizeDock(int offset, bool dragging)
{
    m_window->resizeDock(offset, dragging);
}

// 返回每个插件的识别Key(所以此值应始终不变)，供个性化插件根据key去匹配每个插件对应的图标
QString DBusDockAdaptors::getPluginKey(const QString &pluginName)
{
    return m_topPanelInterface->getPluginKey(pluginName);
}

bool DBusDockAdaptors::getPluginVisible(const QString &pluginName)
{
    return m_topPanelInterface->getPluginVisible(pluginName);
}

void DBusDockAdaptors::setPluginVisible(const QString &pluginName, bool visible)
{
    m_topPanelInterface->setPluginVisible(pluginName, visible);
}

void DBusDockAdaptors::setItemOnDock(const QString settingKey, const QString &itemKey, bool visible)
{
    m_topPanelInterface->setItemOnDock(settingKey, itemKey, visible);
}

QRect DBusDockAdaptors::geometry() const
{
    return m_window->geometry();
}

bool DBusDockAdaptors::showInPrimary() const
{
    return Utils::SettingValue("com.deepin.dde.dock.mainwindow", QByteArray(), "onlyShowPrimary", false).toBool();
}

void DBusDockAdaptors::setShowInPrimary(bool showInPrimary)
{
    if (this->showInPrimary() == showInPrimary)
        return;

    if (Utils::SettingSaveValue("com.deepin.dde.dock.mainwindow", QByteArray(), "onlyShowPrimary", showInPrimary)) {
        Q_EMIT showInPrimaryChanged(showInPrimary);
    }
}

