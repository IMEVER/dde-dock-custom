/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     wangshaojun <wangshaojun_cm@deepin.com>
 *
 * Maintainer: wangshaojun <wangshaojun_cm@deepin.com>
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

#include "dockitemmanager.h"
#include "item/appitem.h"
#include "util/docksettings.h"

#include <QDebug>
#include <QGSettings>

DockItemManager *DockItemManager::INSTANCE = nullptr;

DockItemManager::DockItemManager(QObject *parent)
    : QObject(parent)
    , launcherItem(new LauncherItem)
    , m_appInter(new DBusDock("com.deepin.dde.daemon.Dock", "/com/deepin/dde/daemon/Dock", QDBusConnection::sessionBus(), this))
{
    // 应用区域
    for (auto entry : m_appInter->entries()) {
        AppItem *it = new AppItem(entry);

        manageItem(it);
        connect(it, &AppItem::requestActivateWindow, m_appInter, &DBusDock::ActivateWindow, Qt::QueuedConnection);
        connect(it, &AppItem::requestPreviewWindow, m_appInter, &DBusDock::PreviewWindow);
        connect(it, &AppItem::requestCancelPreview, m_appInter, &DBusDock::CancelPreviewWindow);

        m_itemList.append(it);        
    }

    // 应用信号
    connect(m_appInter, &DBusDock::EntryAdded, this, &DockItemManager::appItemAdded);
    connect(m_appInter, &DBusDock::EntryRemoved, this, static_cast<void (DockItemManager::*)(const QString &)>(&DockItemManager::appItemRemoved), Qt::QueuedConnection);
    connect(m_appInter, &DBusDock::ServiceRestarted, this, &DockItemManager::reloadAppItems);

    // 刷新图标
    QMetaObject::invokeMethod(this, "refershItemsIcon", Qt::QueuedConnection);
}


DockItemManager *DockItemManager::instance(QObject *parent)
{
    if (!INSTANCE)
        INSTANCE = new DockItemManager(parent);

    return INSTANCE;
}

LauncherItem * DockItemManager::getLauncherItem()
{
    return launcherItem;
}

const QList<QPointer<DockItem>> DockItemManager::itemList() const
{
    return m_itemList;
}

bool DockItemManager::appIsOnDock(const QString &appDesktop) const
{
    return m_appInter->IsOnDock(appDesktop);
}

void DockItemManager::refershItemsIcon()
{
    for (auto item : m_itemList) {
        item->refershIcon();
        item->update();
    }
    launcherItem->refershIcon();
    launcherItem->update();
}

void DockItemManager::itemMoved(DockItem *const sourceItem, DockItem *const targetItem)
{
    Q_ASSERT(sourceItem != targetItem);

    const DockItem::ItemType moveType = sourceItem->itemType();
    const DockItem::ItemType replaceType = targetItem->itemType();

    // app move
    if (moveType == DockItem::App || moveType == DockItem::Placeholder)
        if (replaceType != DockItem::App)
            return;

    // plugins move
    if (moveType == DockItem::Plugins || moveType == DockItem::TrayPlugin)
        if (replaceType != DockItem::Plugins && replaceType != DockItem::TrayPlugin)
            return;

    const int moveIndex = m_itemList.indexOf(sourceItem);
    const int replaceIndex = m_itemList.indexOf(targetItem);

    m_itemList.removeAt(moveIndex);
    m_itemList.insert(replaceIndex, sourceItem);

    // for app move, index 0 is launcher item, need to pass it.
    if (moveType == DockItem::App && replaceType == DockItem::App)
        m_appInter->MoveEntry(moveIndex, replaceIndex);
}

void DockItemManager::itemAdded(const QString &appDesktop, int idx)
{
    m_appInter->RequestDock(appDesktop, idx);
}

void DockItemManager::appItemAdded(const QDBusObjectPath &path, const int index)
{
    int insertIndex = index != -1 ? index : m_itemList.count();

    AppItem *item = new AppItem(path);
    manageItem(item);

    connect(item, &AppItem::requestActivateWindow, m_appInter, &DBusDock::ActivateWindow, Qt::QueuedConnection);
    connect(item, &AppItem::requestPreviewWindow, m_appInter, &DBusDock::PreviewWindow);
    connect(item, &AppItem::requestCancelPreview, m_appInter, &DBusDock::CancelPreviewWindow);

    m_itemList.insert(insertIndex, item);
    emit itemInserted(insertIndex, item);
}

void DockItemManager::appItemRemoved(const QString &appId)
{
    for (int i(0); i < m_itemList.size(); ++i) {
        if (m_itemList[i]->itemType() != DockItem::App)
            continue;

        AppItem *app = static_cast<AppItem *>(m_itemList[i].data());
        if (!app) {
            continue;
        }
        if (!app->isValid() || app->appId() == appId) {
            appItemRemoved(app);
            break;
        }
    }
}

void DockItemManager::appItemRemoved(AppItem *appItem)
{
    m_itemList.removeOne(appItem);

    if (appItem->isDragging()) {
        QDrag::cancel();
    }
    appItem->deleteLater();
    emit itemRemoved(appItem);
}

void DockItemManager::reloadAppItems()
{
    for (auto item : m_itemList)
        appItemRemoved(static_cast<AppItem *>(item.data()));

    for (auto path : m_appInter->entries())
        appItemAdded(path, -1);
}

void DockItemManager::manageItem(DockItem *item)
{
    connect(item, &DockItem::requestRefreshWindowVisible, this, &DockItemManager::requestRefershWindowVisible, Qt::UniqueConnection);
    connect(item, &DockItem::requestWindowAutoHide, this, &DockItemManager::requestWindowAutoHide, Qt::UniqueConnection);
}
