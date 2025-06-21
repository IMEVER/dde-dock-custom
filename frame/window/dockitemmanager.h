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

#ifndef DOCKITEMMANAGER_H
#define DOCKITEMMANAGER_H

#include "../item/dockitem.h"
#include "../item/appitem.h"
#include "../item/launcheritem.h"
#include "../item/placeholderitem.h"
#include "../item/diritem.h"
#include "../item/folderitem.h"
#include "../taskmanager/taskmanager.h"

#include <QObject>

class DockItemManager : public QObject
{
    Q_OBJECT
public:
    static DockItemManager *instance();

    DirItem *createDir(const QString title={});
    FolderItem *createFolder(const QString path);
    bool appIsOnDock(const QString &appDesktop) const;
    int itemSize();

signals:
    void itemInserted(const int index, DockItem *item) const;
    void itemRemoved(DockItem *item) const;
    void requestWindowAutoHide(const bool autoHide) const;
    void itemCountChanged();
    void requestUpdateDockItem() const;


public slots:
    void reloadAppItems();
    void itemMoved(QStringList&);
    void itemAdded(const QString &appDesktop, int idx);
    void folderAdded(const QString &path);
    void updateDirApp();

private:
    explicit DockItemManager();
    void refreshItemsIcon();
    void appItemAdded(const Entry *entry, int index);
    void appItemRemoved(const QString &appId);
    void appItemRemoved(AppItem *appItem);
    void loadDirAppData();

    void onAppWindowCountChanged();
    // void onShowMultiWindowChanged();

    // void updateMultiItems(AppItem *appItem, bool emitSignal = false);
    // bool multiWindowExist(quint32 winId) const;
    // bool needRemoveMultiWindow(AppMultiItem *multiItem) const;


private:
    TaskManager *m_taskmanager;
    QTimer *m_timer;

    QMap<QString, AppItem*> m_itemList;
    QList<DirItem*> m_dirList;
    QList<FolderItem*> m_folderList;
};

#endif // DOCKITEMMANAGER_H
