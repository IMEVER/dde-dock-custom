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
#include "../item/trashitem.h"
#include "../util/utils.h"

#include <QSet>
#include <DApplication>

#define SETTING DockSettings::instance()

DockItemManager::DockItemManager() : QObject()
    , m_taskmanager(TaskManager::instance())
{
    m_timer = new QTimer(this);
    m_timer->setInterval(100);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &DockItemManager::itemCountChanged);

    if (Dtk::Widget::DApplication *app = qobject_cast<Dtk::Widget::DApplication *>(qApp))
        connect(app, &Dtk::Widget::DApplication::iconThemeChanged, this, &DockItemManager::refreshItemsIcon);
}


DockItemManager *DockItemManager::instance()
{
    static DockItemManager INSTANCE;
    return &INSTANCE;
}

void DockItemManager::refreshItemsIcon() {
    for (auto item : m_itemList) {
        item->refreshIcon();
        item->update();
    }
}

int DockItemManager::itemSize()
{
    return DockSettings::instance()->getWindowSizeFashion();
}

bool DockItemManager::appIsOnDock(const QString &appDesktop) const
{
    return m_taskmanager->isDocked(appDesktop);
}

void DockItemManager::itemMoved(QStringList &apps)
{
    m_taskmanager->updateEntryOrder(apps);
}

void DockItemManager::itemAdded(const QString &appDesktop, int idx)
{
    m_taskmanager->requestDock(appDesktop, idx);
}

void DockItemManager::appItemAdded(const Entry *entry, int index)
{
    if (m_itemList.contains(entry->getId()))
        return;

    AppItem *item = new AppItem(entry);
    m_itemList.insert(entry->getId(), item);

    connect(item, &DockItem::destroyed, m_timer, qOverload<>(&QTimer::start));
    connect(item, &AppItem::requestPreviewWindow, m_taskmanager, &TaskManager::previewWindow);
    connect(item, &AppItem::requestCancelPreview, m_taskmanager, &TaskManager::cancelPreviewWindow);
    connect(item, &AppItem::windowCountChanged, this, &DockItemManager::onAppWindowCountChanged);
    connect(this, &DockItemManager::requestUpdateDockItem, item, &AppItem::requestUpdateEntryGeometries);

    connect(item, &AppItem::windowItemInserted, item, [this](WindowItem * item){
        emit itemInserted(-1, item);
        connect(item, &DockItem::destroyed, m_timer, qOverload<>(&QTimer::start));
        emit itemCountChanged();
    });
    connect(item, &AppItem::windowItemRemoved, this, [this](WindowItem *item){ emit itemRemoved(item); });

    for(auto dirItem : m_dirList)
    {
        if(dirItem->hasId(item->appId()))
        {
            dirItem->addItem(item);

            if(index == -1 && dirItem->currentCount() == 1)
            {
                emit itemInserted(-1, dirItem);
                m_timer->start();
            }
            return;
        }
    }

    emit itemInserted(index, item);
    // 向后插入多开窗口
    // updateMultiItems(item, true);
    m_timer->start();
}

void DockItemManager::appItemRemoved(const QString &appId)
{
    if(auto item = m_itemList.value(appId))
        appItemRemoved(item);
}

void DockItemManager::appItemRemoved(AppItem *appItem)
{
    m_itemList.remove(appItem->appId());
    appItem->removeWindowItem();
    if(appItem->getPlace() == DockItem::DirPlace) {
        appItem->getDirItem()->removeItem(appItem, false);
        appItem->deleteLater();
    } else
        emit itemRemoved(appItem);
}

void DockItemManager::reloadAppItems()
{
    static bool first = true;
    if(first)
    {
        emit itemInserted(0, new LauncherItem);
        emit itemInserted(0, new TrashItem);

        for(auto path : SETTING->loadLoaders())
            createFolder(path);

        for(auto item : m_folderList) emit itemInserted(0, item);
        first = false;

        // 应用信号
        connect(m_taskmanager, &TaskManager::entryAdded, this, [this](const Entry *entry, int index){
            appItemAdded(entry, index);
        });
        connect(m_taskmanager, &TaskManager::entryRemoved, this, static_cast<void (DockItemManager::*)(const QString &)>(&DockItemManager::appItemRemoved));

        loadDirAppData();
        for (auto entry : m_taskmanager->getEntries()) appItemAdded(entry, -1);

        m_timer->start();
    }
}

DirItem *DockItemManager::createDir(const QString title)
{
    DirItem *item = new DirItem(title);
    m_dirList.append(item);
    connect(item, &DirItem::updateContent, this, &DockItemManager::updateDirApp);
    connect(item, &DockItem::destroyed, m_timer, qOverload<>(&QTimer::start));
    return item;
}

FolderItem *DockItemManager::createFolder(const QString path) {
    FolderItem *folder = new FolderItem(path);
    connect(folder, &DockItem::destroyed, this, [this, path] {
        m_timer->start();
        QStringList folders;
        for(auto folder : m_folderList)
            folders << folder->getPath();

        SETTING->updateFolders(folders);
    });
    connect(folder, &FolderItem::undocked, this, [this, folder]{
        m_folderList.removeOne(folder);
        emit itemRemoved(folder);
    });
    m_folderList.append(folder);
    return folder;
}

void DockItemManager::folderAdded(const QString &path)
{
    for(auto item : m_folderList)
        if(item->getPath() == path) return;

    FolderItem *folder = createFolder(path);
    m_timer->start();
    emit itemInserted(0, folder);

    SETTING->addFolder(path, m_folderList.count() - 1);
}

void DockItemManager::loadDirAppData()
{
    for (auto dir : SETTING->loadDirDatas())
    {
        DirItem *item = createDir(dir.title);
        item->setIndex(dir.index);
        item->setIds(dir.ids);
    }
}

void DockItemManager::updateDirApp()
{
    QList<DirItem*> emptyList;
    QList<DirData> dirDatas;

    for(auto itemDir : m_dirList)
    {
        auto ids = itemDir->getIds();

        if(ids.isEmpty()) {
            emptyList.append(itemDir);
            continue;
        }

        dirDatas.append(DirData{itemDir->getTitle(), itemDir->getIndex(), itemDir->getIds()});
    }

    SETTING->setDirDatas(dirDatas);

    for(auto item : emptyList) {
        m_dirList.removeOne(item);
        emit itemRemoved(item);
    }
}

void DockItemManager::onAppWindowCountChanged()
{
    // AppItem *appItem = static_cast<AppItem *>(sender());
    // updateMultiItems(appItem, true);
}

// void DockItemManager::updateMultiItems(AppItem *appItem, bool emitSignal)
// {
//     // 如果系统设置不开启应用多窗口拆分，则无需之后的操作
//     if (!m_taskmanager->showMultiWindow())
//         return;

//     // 如果开启了多窗口拆分，则同步窗口和多窗口应用的信息
//     const WindowInfoMap &windowInfoMap = appItem->windowsInfos();
//     QList<AppMultiItem *> removeItems;
//     // 同步当前已经存在的多开窗口的列表，删除不存在的多开窗口
//     for (int i = 0; i < m_itemList.size(); i++) {
//         QPointer<DockItem> dockItem = m_itemList[i];
//         AppMultiItem *multiItem = qobject_cast<AppMultiItem *>(dockItem.data());
//         if (!multiItem || multiItem->appItem() != appItem)
//             continue;

//         // 如果查找到的当前的应用的窗口不需要移除，则继续下一个循环
//         if (!needRemoveMultiWindow(multiItem))
//             continue;

//         removeItems << multiItem;
//     }
//     // 从itemList中移除多开窗口
//     for (AppMultiItem *dockItem : removeItems)
//         m_itemList.removeOne(dockItem);
//     if (emitSignal) {
//         // 移除发送每个多开窗口的移除信号
//         for (AppMultiItem *dockItem : removeItems)
//             Q_EMIT itemRemoved(dockItem);
//     }
//     qDeleteAll(removeItems);

//     // 遍历当前APP打开的所有窗口的列表，如果不存在多开窗口的应用，则新增，同时发送信号
//     for (auto it = windowInfoMap.begin(); it != windowInfoMap.end(); it++) {
//         if (multiWindowExist(it.key()))
//             continue;

//         const WindowInfo &windowInfo = it.value();
//         // 如果不存在这个窗口对应的多开窗口，则新建一个窗口，同时发送窗口新增的信号
//         AppMultiItem *multiItem = new AppMultiItem(appItem, it.key(), windowInfo);
//         m_itemList << multiItem;
//         if (emitSignal)
//             Q_EMIT itemInserted(-1, multiItem);
//     }
// }

// // 检查对应的窗口是否存在多开窗口
// bool DockItemManager::multiWindowExist(quint32 winId) const
// {
//     for (QPointer<DockItem> dockItem : m_itemList) {
//         AppMultiItem *multiItem = qobject_cast<AppMultiItem *>(dockItem.data());
//         if (!multiItem)
//             continue;

//         if (multiItem->winId() == winId)
//             return true;
//     }

//     return false;
// }


// // 检查当前多开窗口是否需要移除
// // 如果当前多开窗口图标对应的窗口在这个窗口所属的APP中所有打开窗口中不存在，那么则认为该多窗口已经被关闭
// bool DockItemManager::needRemoveMultiWindow(AppMultiItem *multiItem) const
// {
//     // 查找多分窗口对应的窗口在应用所有的打开的窗口中是否存在，只要它对应的窗口存在，就无需删除
//     // 只要不存在，就需要删除
//     AppItem *appItem = multiItem->appItem();
//     const WindowInfoMap &windowInfoMap = appItem->windowsInfos();
//     for (auto it = windowInfoMap.begin(); it != windowInfoMap.end(); it++) {
//         if (it.key() == multiItem->winId())
//             return false;
//     }

//     return true;
// }


// void DockItemManager::onShowMultiWindowChanged()
// {
//     if (m_taskmanager->showMultiWindow()) {
//         // 如果当前设置支持窗口多开，那么就依次对每个APPItem加载多开窗口
//         for (int i = 0; i < m_itemList.size(); i++) {
//             const QPointer<DockItem> &dockItem = m_itemList[i];
//             if (dockItem->itemType() != DockItem::ItemType::App)
//                 continue;

//             updateMultiItems(static_cast<AppItem *>(dockItem.data()), true);
//         }
//     } else {
//         // 如果当前设置不支持窗口多开，则删除所有的多开窗口
//         QList<DockItem *> multiWindows;
//         for (const QPointer<DockItem> &dockItem : m_itemList) {
//             if (dockItem->itemType() != DockItem::AppMultiWindow)
//                 continue;

//             multiWindows << dockItem.data();
//         }
//         for (DockItem *multiItem : multiWindows) {
//             m_itemList.removeOne(multiItem);
//             Q_EMIT itemRemoved(multiItem);
//             multiItem->deleteLater();
//         }
//     }
// }
