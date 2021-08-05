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
#include "../item/appitem.h"
#include "../util/docksettings.h"

#include <QSet>

DockItemManager *DockItemManager::INSTANCE = nullptr;
bool first = true;

DockItemManager::DockItemManager(QObject *parent)
    : QObject(parent)
    , m_appInter(new DBusDock("com.deepin.dde.daemon.Dock", "/com/deepin/dde/daemon/Dock", QDBusConnection::sessionBus(), this))
    , m_qsettings(new QSettings(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/setting.ini", QSettings::IniFormat))
    , launcherItem(new LauncherItem)
{
    // 应用信号
    connect(m_appInter, &DBusDock::EntryAdded, this, &DockItemManager::appItemAdded);
    connect(m_appInter, &DBusDock::EntryRemoved, this, static_cast<void (DockItemManager::*)(const QString &)>(&DockItemManager::appItemRemoved), Qt::QueuedConnection);
    connect(m_appInter, &DBusDock::ServiceRestarted, this, &DockItemManager::reloadAppItems);

    // 刷新图标
    // QMetaObject::invokeMethod(this, "refershItemsIcon", Qt::QueuedConnection);
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

const QList<QPointer<DockItem>> DockItemManager::itemList()
{
    return m_itemList;
}

const QList<QPointer<DirItem>> DockItemManager::dirList()
{
    return m_dirList;
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

    connect(item, &DockItem::requestRefreshWindowVisible, this, &DockItemManager::requestRefershWindowVisible, Qt::UniqueConnection);
    connect(item, &DockItem::requestWindowAutoHide, this, &DockItemManager::requestWindowAutoHide, Qt::UniqueConnection);

    connect(item, &AppItem::requestActivateWindow, m_appInter, &DBusDock::ActivateWindow, Qt::QueuedConnection);
    connect(item, &AppItem::requestPreviewWindow, m_appInter, &DBusDock::PreviewWindow);
    connect(item, &AppItem::requestCancelPreview, m_appInter, &DBusDock::CancelPreviewWindow);

    m_itemList.insert(insertIndex, item);

    for(auto dirItem : m_dirList)
    {
        if(dirItem->hasId(item->getDesktopFile()))
        {
            dirItem->addItem(item);

            if(index == -1 && dirItem->getIndex() == insertIndex)
                emit itemInserted(insertIndex, dirItem);

            return;
        }
    }

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

    appItem->deleteLater();
    emit itemRemoved(appItem);
}

void DockItemManager::reloadAppItems()
{
    if(first)
    {
        emit itemInserted(0, launcherItem);
    }
    else
    {
        for (auto item : m_itemList)
            appItemRemoved(static_cast<AppItem *>(item.data()));

        for(auto item : m_dirList)
        {
            item->deleteLater();
            emit itemRemoved(item);
        }
    }

    loadDirAppData();

    QList<QDBusObjectPath> list = m_appInter->entries();
    for (auto path : list)
        appItemAdded(path, -1);

    for(auto dirItem : m_dirList)
        if(!dirItem->parentWidget() && !dirItem->isEmpty())
            emit itemInserted(dirItem->getIndex(), dirItem);
}

void DockItemManager::addDirApp(DirItem *dirItem)
{
    m_dirList.append(dirItem);
    connect(dirItem, &DockItem::requestWindowAutoHide, this, &DockItemManager::requestWindowAutoHide, Qt::UniqueConnection);
}

void DockItemManager::loadDirAppData()
{
    int count = m_qsettings->value("count", 0).toInt();
    while (count >=1)
    {
        DirItem *item = new DirItem(m_qsettings->value(QString("dir_%1/title").arg(count), "").toString());
        item->setIndex(m_qsettings->value(QString("dir_%1/index").arg(count), -1).toInt());
        item->setIds(m_qsettings->value(QString("dir_%1/ids").arg(count), QStringList()).value<QStringList>());

        m_dirList.append(item);

        connect(item, &DirItem::updateContent, this, &DockItemManager::updateDirApp);
        connect(item, &DockItem::requestWindowAutoHide, this, &DockItemManager::requestWindowAutoHide, Qt::UniqueConnection);

        count--;
    }
}

void DockItemManager::updateDirApp()
{
    m_qsettings->setFallbacksEnabled(false);
    m_qsettings->clear();
    int index = 0;
    for(auto itemDir : m_dirList)
    {
        QSet<QString> ids;
        for(auto item : itemDir->getAppList())
            ids.insert(item->getDesktopFile());

        if(ids.isEmpty())
            continue;

        index++;
        m_qsettings->setValue(QString("dir_%1/title").arg(index), itemDir->getTitle());
        m_qsettings->setValue(QString("dir_%1/index").arg(index), itemDir->getIndex());
        m_qsettings->setValue(QString("dir_%1/ids").arg(index), QStringList(ids.values()));

    }

    m_qsettings->setValue("count", index);
    m_qsettings->sync();
}