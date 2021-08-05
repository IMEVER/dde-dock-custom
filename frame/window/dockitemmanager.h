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

#include "item/dockitem.h"
#include "item/appitem.h"
#include "item/launcheritem.h"
#include "item/placeholderitem.h"
#include "item/diritem.h"

#include <com_deepin_dde_daemon_dock.h>
#include <QObject>

using DBusDock = com::deepin::dde::daemon::Dock;

class DockItemManager : public QObject
{
    Q_OBJECT

public:
    static DockItemManager *instance(QObject *parent = nullptr);

    const QList<QPointer<DockItem> > itemList();
    const QList<QPointer<DirItem>> dirList();
    void addDirApp(DirItem *dirItem);
    bool appIsOnDock(const QString &appDesktop) const;
    LauncherItem* getLauncherItem();

signals:
    void itemInserted(const int index, DockItem *item) const;
    void itemRemoved(DockItem *item) const;
    void itemUpdated(DockItem *item) const;
    void requestWindowAutoHide(const bool autoHide) const;
    void requestRefershWindowVisible() const;

public slots:
    void reloadAppItems();
    void refershItemsIcon();
    void itemMoved(DockItem *const sourceItem, DockItem *const targetItem);
    void itemAdded(const QString &appDesktop, int idx);
    void updateDirApp();

private:
    explicit DockItemManager(QObject *parent = nullptr);
    void appItemAdded(const QDBusObjectPath &path, const int index);
    void appItemRemoved(const QString &appId);
    void appItemRemoved(AppItem *appItem);
    void loadDirAppData();

private:
    DBusDock *m_appInter;
    QSettings *m_qsettings;

    static DockItemManager *INSTANCE;

    LauncherItem *launcherItem;

    QList<QPointer<DockItem>> m_itemList;
    QList<QPointer<DirItem>> m_dirList;
};

#endif // DOCKITEMMANAGER_H
