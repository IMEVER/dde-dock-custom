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
#include "../item/pluginitem.h"

#include <QSet>

DockItemManager::DockItemManager() : QObject()
    , m_appInter(new DBusDock("com.deepin.dde.daemon.Dock", "/com/deepin/dde/daemon/Dock", QDBusConnection::sessionBus(), this))
    , m_qsettings(new QSettings(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/setting.ini", QSettings::IniFormat))
    , launcherItem(new LauncherItem)
{
    m_qsettings->setIniCodec(QTextCodec::codecForName("UTF-8"));
    // 应用信号
    connect(m_appInter, &DBusDock::EntryAdded, this, &DockItemManager::appItemAdded);
    connect(m_appInter, &DBusDock::EntryRemoved, this, static_cast<void (DockItemManager::*)(const QString &)>(&DockItemManager::appItemRemoved), Qt::QueuedConnection);
    connect(m_appInter, &DBusDock::ServiceRestarted, this, [ this ] { QTimer::singleShot(500, [ this ] { reloadAppItems(); }); });

    // 刷新图标
    // QMetaObject::invokeMethod(this, "refershItemsIcon", Qt::QueuedConnection);
}


DockItemManager *DockItemManager::instance()
{
    static DockItemManager INSTANCE;
    return &INSTANCE;
}

MergeMode DockItemManager::getDockMergeMode()
{
    int i = m_qsettings->value("mergeMode", MergeAll).toInt();
    if(i < 0 || i > 2)
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
    m_qsettings->setValue("animation/hover", enable);
}

void DockItemManager::setInOutAnimation(bool enable)
{
    m_qsettings->setValue("animation/inout", enable);
}

void DockItemManager::setDragAnimation(bool enable)
{
    m_qsettings->setValue("animation/drag", enable);
}

void DockItemManager::setHoverHighlight(bool enable)
{
    m_qsettings->setValue("animation/highlight", enable);
}

bool DockItemManager::hasWindowItem()
{
    if(getDockMergeMode() != Dock::MergeAll)
    {
        for(auto item : m_itemList)
        {
            if(item->itemType() == DockItem::App && qobject_cast<AppItem *>(item)->windowCount() > 0)
                return true;
        }
    }
    return false;
}

int DockItemManager::itemSize()
{
    return m_appInter->windowSizeFashion();
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
    AppItem *item = new AppItem(path);

    connect(item, &DockItem::requestRefreshWindowVisible, this, &DockItemManager::requestRefershWindowVisible, Qt::UniqueConnection);
    connect(item, &DockItem::requestWindowAutoHide, this, &DockItemManager::requestWindowAutoHide, Qt::UniqueConnection);

    connect(item, &AppItem::requestActivateWindow, m_appInter, &DBusDock::ActivateWindow, Qt::QueuedConnection);
    connect(item, &AppItem::requestPreviewWindow, m_appInter, &DBusDock::PreviewWindow);
    connect(item, &AppItem::requestCancelPreview, m_appInter, &DBusDock::CancelPreviewWindow);

    connect(item, &AppItem::windowItemInserted, [this](WindowItem *item){
        emit itemInserted(-1, item);
        emit itemCountChanged();
    });
    connect(item, &AppItem::windowItemRemoved, [this](WindowItem *item, bool animation){
        emit itemRemoved(item, animation);
        emit itemCountChanged();
    });


    m_itemList.insert(index != -1 ? index : m_itemList.count(), item);

    for(auto dirItem : m_dirList)
    {
        if(dirItem->hasId(item->getDesktopFile()))
        {
            dirItem->addItem(item);
            item->fetchWindowInfos();

            if(index == -1 && dirItem->currentCount() == 1)
            {
                emit itemInserted(-1, dirItem);
                emit itemCountChanged();
            }

            return;
        }
    }

    item->fetchWindowInfos();
    emit itemInserted(index, item);

    if(index > -1)
        emit itemCountChanged();
}

void DockItemManager::appItemRemoved(const QString &appId)
{
    for (int i(0); i < m_itemList.size(); ++i) {
        if(AppItem *app = static_cast<AppItem *>(m_itemList[i].data()))
        {
            if (app->itemType() == DockItem::App && (!app->isValid() || app->appId() == appId)) {
                appItemRemoved(app);
                emit itemCountChanged();
                break;
            }
        }
    }
}

void DockItemManager::appItemRemoved(AppItem *appItem, bool animation)
{
    m_itemList.removeOne(appItem);

    if(appItem->getPlace() == DockItem::DirPlace)
    {
        appItem->getDirItem()->removeItem(appItem, false);
    }
    else
    {
        emit itemRemoved(appItem, animation);
    }

    appItem->removeWindowItem(animation);
    QTimer::singleShot(animation ? 500 : 10, [ appItem ] { appItem->deleteLater(); });
}

void DockItemManager::reloadAppItems()
{
    static bool first = true;
    if(first)
    {
        emit itemInserted(0, launcherItem);
        emit itemInserted(0, new PluginItem);
        first = false;
    }
    else
    {
        while (!m_itemList.isEmpty())
            appItemRemoved(qobject_cast<AppItem *>(m_itemList.first().data()), false);
        m_itemList.clear();

        for(auto item : m_dirList)
        {
            emit itemRemoved(item, false);
            QTimer::singleShot(10, [ item ] { item->deleteLater(); });
        }
        m_dirList.clear();
    }

    loadDirAppData();
    QList<QDBusObjectPath> list = m_appInter->entries();
    for (auto path : list)
        appItemAdded(path, -1);

    for(auto dirItem : m_dirList)
        if(!dirItem->parentWidget() && !dirItem->isEmpty())
            emit itemInserted(dirItem->getIndex(), dirItem);

    emit itemCountChanged();
}

void DockItemManager::addDirApp(DirItem *dirItem)
{
    m_dirList.append(dirItem);
    connect(dirItem, &DockItem::requestWindowAutoHide, this, &DockItemManager::requestWindowAutoHide, Qt::UniqueConnection);
    emit itemCountChanged();
}

void DockItemManager::loadDirAppData()
{
    int count = m_qsettings->value("count", 0).toInt();
    while (count >=1)
    {
        DirItem *item = new DirItem(m_qsettings->value(QString("dir_%1/title").arg(count), "").toString());
        item->setIndex(m_qsettings->value(QString("dir_%1/index").arg(count), -1).toInt());
        item->setIds(m_qsettings->value(QString("dir_%1/ids").arg(count), QStringList()).value<QStringList>().toSet());

        m_dirList.append(item);

        connect(item, &DirItem::updateContent, this, &DockItemManager::updateDirApp);
        connect(item, &DockItem::requestWindowAutoHide, this, &DockItemManager::requestWindowAutoHide, Qt::UniqueConnection);

        count--;
    }
}

void DockItemManager::updateDirApp()
{
    m_qsettings->setFallbacksEnabled(true);
    int index = 0, originCount = m_qsettings->value("count", 0).toInt();
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

    while(index < originCount)
    {
        m_qsettings->remove(QString("dir_%1").arg(originCount--));
    }

    m_qsettings->setValue("count", index);
    m_qsettings->sync();
}