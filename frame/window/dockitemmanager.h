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
    enum ActivateAnimationType {
        Swing = 0,
        Jump = 1,
        No = 2
    };

    static DockItemManager *instance();

    void setDbusDock(DBusDock *dbus);

    const QList<QPointer<DockItem> > itemList();
    const QList<QPointer<DirItem>> dirList();
    void addDirApp(DirItem *dirItem);
    bool appIsOnDock(const QString &appDesktop) const;
    MergeMode getDockMergeMode();
    void saveDockMergeMode(MergeMode mode);
    bool isEnableHoverScaleAnimation();
    bool isEnableInOutAnimation();
    bool isEnableDragAnimation();
    bool isEnableHoverHighlight();
    void setHoverScaleAnimation(bool enable);
    void setInOutAnimation(bool enable);
    void setDragAnimation(bool enable);
    void setHoverHighlight(bool enable);
    ActivateAnimationType animationType();
    void setAnimationType(ActivateAnimationType type);
    bool hasWindowItem();
    int itemSize();

signals:
    void itemInserted(const int index, DockItem *item) const;
    void itemRemoved(DockItem *item, bool animation = true) const;
    void itemUpdated(DockItem *item) const;
    void requestWindowAutoHide(const bool autoHide) const;
    void requestRefershWindowVisible() const;
    void mergeModeChanged(MergeMode mode);
    void itemCountChanged();
    void hoverScaleChanged(bool hoverScale);
    void hoverHighlighted(bool enabled);
    void inoutChanged(bool enabled);

public slots:
    void reloadAppItems();
    void itemMoved(DockItem *const sourceItem, DockItem *const targetItem);
    void itemAdded(const QString &appDesktop, int idx);
    void updateDirApp();

private:
    explicit DockItemManager();
    void appItemAdded(const QDBusObjectPath &path, const int index, const bool updateFrame = true);
    void appItemRemoved(const QString &appId);
    void appItemRemoved(AppItem *appItem, bool animation = true);
    void loadDirAppData();

private:
    DBusDock *m_appInter;
    QSettings *m_qsettings;

    QList<QPointer<DockItem>> m_itemList;
    QList<QPointer<DirItem>> m_dirList;
};

Q_DECLARE_METATYPE(DockItemManager::ActivateAnimationType);

#endif // DOCKITEMMANAGER_H
