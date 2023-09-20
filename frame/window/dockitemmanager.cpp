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

DockItemManager::DockItemManager() : QObject()
    , m_taskmanager(TaskManager::instance())
    , m_qsettings(new QSettings(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/setting.ini", QSettings::IniFormat))
{
    m_qsettings->setIniCodec(QTextCodec::codecForName("UTF-8"));

    // 应用信号
    connect(m_taskmanager, &TaskManager::entryAdded, this, [this](const Entry *entry, int index){
        appItemAdded(entry, index, true);
    });
    connect(m_taskmanager, &TaskManager::entryRemoved, this, static_cast<void (DockItemManager::*)(const QString &)>(&DockItemManager::appItemRemoved), Qt::QueuedConnection);
    connect(m_taskmanager, &TaskManager::serviceRestarted, this, &DockItemManager::reloadAppItems);
    // connect(DockSettings::instance(), &DockSettings::showMultiWindowChanged, this, &DockItemManager::onShowMultiWindowChanged);

    if (Dtk::Widget::DApplication *app = qobject_cast<Dtk::Widget::DApplication *>(qApp)) {
        connect(app, &Dtk::Widget::DApplication::iconThemeChanged, this, &DockItemManager::refreshItemsIcon);
    }

    connect(qApp, &QApplication::aboutToQuit, this, &QObject::deleteLater);

    // reloadAppItems();

    // 刷新图标
    // QMetaObject::invokeMethod(this, "refreshItemsIcon", Qt::QueuedConnection);
}


DockItemManager *DockItemManager::instance()
{
    static DockItemManager INSTANCE;
    return &INSTANCE;
}

void DockItemManager::refreshItemsIcon() {
    for (auto item : m_itemList) {
        if (item.isNull())
            continue;

        item->refreshIcon();
        item->update();
    }
}

MergeMode DockItemManager::getDockMergeMode()
{
    int i = m_qsettings->value("mergeMode", MergeDock).toInt();
    if(i < 0 || i > 1)
        i = 0;
    return MergeMode(i);
}

void DockItemManager::saveDockMergeMode(MergeMode mode)
{
    if(mode != getDockMergeMode())
    {
        m_qsettings->setValue("mergeMode", mode);
        m_qsettings->sync();
        emit mergeModeChanged(mode);
    }
}

bool DockItemManager::isEnableHoverScaleAnimation()
{
    return m_qsettings->value("animation/hover", true).toBool();
}

bool DockItemManager::isEnableInOutAnimation()
{
    return m_qsettings->value("animation/inout", true).toBool();
}

bool DockItemManager::isEnableDragAnimation()
{
    return m_qsettings->value("animation/drag", true).toBool();
}

bool DockItemManager::isEnableHoverHighlight()
{
    return m_qsettings->value("animation/highlight", true).toBool();
}

void DockItemManager::setHoverScaleAnimation(bool enable)
{
    if(enable != isEnableHoverScaleAnimation())
        m_qsettings->setValue("animation/hover", enable);
}

void DockItemManager::setInOutAnimation(bool enable)
{
    if(enable != isEnableInOutAnimation())
        m_qsettings->setValue("animation/inout", enable);
}

void DockItemManager::setDragAnimation(bool enable)
{
    m_qsettings->setValue("animation/drag", enable);
}

void DockItemManager::setHoverHighlight(bool enable)
{
    if(isEnableHoverHighlight() != enable) {
        m_qsettings->setValue("animation/highlight", enable);
        emit hoverHighlighted(enable);
    }
}

DockItemManager::ActivateAnimationType DockItemManager::animationType() {
    return m_qsettings->value("animation/activate", Jump).value<ActivateAnimationType>();
}

void DockItemManager::setAnimationType(ActivateAnimationType type) {
    m_qsettings->setValue("animation/activate", type);
    m_qsettings->sync();
}

bool DockItemManager::hasWindowItem()
{
    for(auto item : m_itemList)
        if(item->itemType() == DockItem::App && qobject_cast<AppItem *>(item)->windowCount() > 0)
            return true;

    return false;
}

int DockItemManager::itemSize()
{
    return DockSettings::instance()->getWindowSizeFashion();
}

int DockItemManager::itemCount()
{
    int count = m_itemList.count() + m_folderList.count() + 1 + 1;
    for(auto item : m_dirList)
        count = count - item->currentCount() + 1;

    for(auto item : m_itemList)
        if(item->itemType() == DockItem::App)
            count += qobject_cast<AppItem *>(item)->windowCount();

    return count;
}

const QList<QPointer<AppItem>> DockItemManager::itemList()
{
    return m_itemList;
}

bool DockItemManager::appIsOnDock(const QString &appDesktop) const
{
    return m_taskmanager->isDocked(appDesktop);
}

void DockItemManager::itemMoved(AppItem *const sourceItem, AppItem *const targetItem)
{
    Q_ASSERT(sourceItem != targetItem);

    const int moveIndex = m_itemList.indexOf(sourceItem);
    const int replaceIndex = m_itemList.indexOf(targetItem);

    m_itemList.removeAt(moveIndex);
    m_itemList.insert(replaceIndex, sourceItem);

    m_taskmanager->moveEntry(moveIndex, replaceIndex);
}

void DockItemManager::itemAdded(const QString &appDesktop, int idx)
{
    m_taskmanager->requestDock(appDesktop, idx);
}

void DockItemManager::appItemAdded(const Entry *entry, const int index, bool updateFrame)
{
    AppItem *item = new AppItem(entry);

    if (m_appIDist.contains(item->appId())) {
        item->deleteLater();
        return;
    }

    m_itemList.insert(index, item);
    m_appIDist.append(item->appId());

    // connect(item, &DockItem::requestRefreshWindowVisible, this, &DockItemManager::requestRefershWindowVisible, Qt::UniqueConnection);
    connect(item, &DockItem::requestWindowAutoHide, this, &DockItemManager::requestWindowAutoHide, Qt::UniqueConnection);
    connect(item, &AppItem::requestPreviewWindow, m_taskmanager, &TaskManager::previewWindow);
    connect(item, &AppItem::requestCancelPreview, m_taskmanager, &TaskManager::cancelPreviewWindow);
    connect(item, &AppItem::windowCountChanged, this, &DockItemManager::onAppWindowCountChanged);
    connect(this, &DockItemManager::requestUpdateDockItem, item, &AppItem::requestUpdateEntryGeometries);

    connect(item, &AppItem::windowItemInserted, item, [this](WindowItem *item){
        emit itemInserted(-1, item);
        emit itemCountChanged();
    });
    connect(item, &AppItem::windowItemRemoved, item, [this](WindowItem *item, bool animation){
        emit itemRemoved(item, animation);
        if(animation)
            connect(item, &DockItem::inoutFinished, this, [this](bool in){ emit itemCountChanged(); });
        else
            emit itemCountChanged();
    });

    for(auto dirItem : m_dirList)
    {
        if(dirItem->hasId(item->getDesktopFile()))
        {
            dirItem->addItem(item);

            if(index == -1 && dirItem->currentCount() == 1)
            {
                emit itemInserted(-1, dirItem, updateFrame);
                if(updateFrame) emit itemCountChanged();
            }
            item->fetchWindowInfos();
            return;
        }
    }

    // 插入dockItem
    emit itemInserted(index != -1 ? index : m_itemList.size(), item);
    // 向后插入多开窗口
    // updateMultiItems(item, true);

    item->fetchWindowInfos();

    if(updateFrame) emit itemCountChanged();
}

void DockItemManager::appItemRemoved(const QString &appId)
{
    bool bingo = false;
    for (auto app : m_itemList) {
        if (!app->isValid() || app->appId() == appId) {
            appItemRemoved(app);
            bingo = true;
        }
    }
    if(bingo)
        QTimer::singleShot(500, [ this ] { emit itemCountChanged(); });
    m_appIDist.removeAll(appId);
}

void DockItemManager::appItemRemoved(AppItem *appItem, bool animation)
{
    m_itemList.removeOne(appItem);
    appItem->removeWindowItem(animation);
    if(appItem->getPlace() == DockItem::DirPlace) {
        appItem->getDirItem()->removeItem(appItem, false);
        appItem->deleteLater();
    } else
        emit itemRemoved(appItem, animation);
}

void DockItemManager::reloadAppItems()
{
    static bool first = true;
    if(first)
    {
        emit itemInserted(0, new LauncherItem, false);
        emit itemInserted(0, new TrashItem, false);
        loadFolderData();
        for(auto item : m_folderList) emit itemInserted(0, item, false);
        first = false;
    }
    else
    {
        while (!m_itemList.isEmpty())
            appItemRemoved(qobject_cast<AppItem *>(m_itemList.first().data()), false);
        m_itemList.clear();
        m_appIDist.clear();

        for(auto item : m_dirList)
        {
            emit itemRemoved(item, false);
            // QTimer::singleShot(10, [ item ] { item->deleteLater(); });
        }
        m_dirList.clear();
    }

    loadDirAppData();
    for (auto entry : m_taskmanager->getEntries()) appItemAdded(entry, -1, false);

    emit itemCountChanged();
}

DirItem *DockItemManager::createDir(const QString title)
{
    DirItem *item = new DirItem(title);
    m_dirList.append(item);
    connect(item, &DirItem::updateContent, this, &DockItemManager::updateDirApp);
    connect(item, &DockItem::requestWindowAutoHide, this, &DockItemManager::requestWindowAutoHide, Qt::UniqueConnection);
    connect(item, &DockItem::destroyed, this, &DockItemManager::itemCountChanged);
    return item;
}

FolderItem *DockItemManager::createFolder(const QString path) {
    FolderItem *folder = new FolderItem(path);
    connect(folder, &DockItem::requestWindowAutoHide, this, &DockItemManager::requestWindowAutoHide, Qt::UniqueConnection);
    connect(folder, &DockItem::destroyed, this, [this, path] {
        itemCountChanged();

        int index = m_folderList.count();
        m_qsettings->beginWriteArray("folder", index);
        m_qsettings->setArrayIndex(index);
        m_qsettings->remove("");
        while(--index >= 0) {
            m_qsettings->setArrayIndex(index);
            m_qsettings->setValue("path", m_folderList.at(index)->getPath());
        }
        m_qsettings->endArray();
        m_qsettings->sync();
    });
    connect(folder, &FolderItem::undocked, this, [this, folder]{
        m_folderList.removeOne(folder);
        emit itemRemoved(folder, true);
    });
    m_folderList.append(folder);
    return folder;
}

void DockItemManager::folderAdded(const QString &path)
{
    for(auto item : m_folderList)
        if(item->getPath() == path) return;

    FolderItem *folder = createFolder(path);
    emit itemCountChanged();
    emit itemInserted(0, folder, true);

    m_qsettings->beginWriteArray("folder");
    m_qsettings->setArrayIndex(m_folderList.count() - 1);
    m_qsettings->setValue("path", path);
    m_qsettings->endArray();
    m_qsettings->sync();
}

void DockItemManager::loadDirAppData()
{
    int count = m_qsettings->value("count", 0).toInt();
    while (count >=1)
    {
        DirItem *item = createDir(m_qsettings->value(QString("dir_%1/title").arg(count), "").toString());
        item->setIndex(m_qsettings->value(QString("dir_%1/index").arg(count), -1).toInt());
        QStringList desktopFiles = m_qsettings->value(QString("dir_%1/ids").arg(count), QStringList()).value<QStringList>();
        item->setIds(QSet<QString>(desktopFiles.begin(), desktopFiles.end()));
        count--;
    }
}

void DockItemManager::updateDirApp()
{
    QList<DirItem*> emptyList;
    m_qsettings->setFallbacksEnabled(true);
    int index = 0, originCount = m_qsettings->value("count", 0).toInt();
    for(auto itemDir : m_dirList)
    {
        QSet<QString> ids;
        for(auto item : itemDir->getAppList())
            ids.insert(item->getDesktopFile());

        if(ids.isEmpty()) {
            emptyList.append(itemDir);
            continue;
        }

        index++;
        m_qsettings->setValue(QString("dir_%1/title").arg(index), itemDir->getTitle());
        m_qsettings->setValue(QString("dir_%1/index").arg(index), itemDir->getIndex());
        m_qsettings->setValue(QString("dir_%1/ids").arg(index), QStringList(ids.values()));
    }

    while(index < originCount)
        m_qsettings->remove(QString("dir_%1").arg(originCount--));

    m_qsettings->setValue("count", index);
    m_qsettings->sync();

    for(auto item : emptyList) {
        m_dirList.removeOne(item);
        emit itemRemoved(item, true);
    }
}

void DockItemManager::loadFolderData() {
    int index = m_qsettings->beginReadArray("folder");
    while(--index >= 0) {
        m_qsettings->setArrayIndex(index);
        createFolder(m_qsettings->value("path").toString());
    }
    m_qsettings->endArray();
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
