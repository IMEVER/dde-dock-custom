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
    enum ActivateAnimationType {
        Swing = 0,
        Jump = 1,
        Scale = 2,
        Popup = 3,
        No = 4
    };

    static DockItemManager *instance();

    const QList<QPointer<AppItem> > itemList();
    DirItem *createDir(const QString title={});
    FolderItem *createFolder(const QString path);
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
    int itemCount();

signals:
    void itemInserted(const int index, DockItem *item, bool animation = true) const;
    void itemRemoved(DockItem *item, bool animation = true) const;
    void requestWindowAutoHide(const bool autoHide) const;
    void mergeModeChanged(MergeMode mode);
    void itemCountChanged();
    void hoverHighlighted(bool enabled);
    void requestUpdateDockItem() const;


public slots:
    void reloadAppItems();
    void itemMoved(AppItem *const sourceItem, AppItem *const targetItem);
    void itemAdded(const QString &appDesktop, int idx);
    void folderAdded(const QString &path);
    void updateDirApp();

private:
    explicit DockItemManager();
    void refreshItemsIcon();
    void appItemAdded(const Entry *entry, const int index, bool updateFrame=true);
    void appItemRemoved(const QString &appId);
    void appItemRemoved(AppItem *appItem, bool animation = true);
    void loadDirAppData();
    void loadFolderData();

    void onAppWindowCountChanged();
    // void onShowMultiWindowChanged();

    // void updateMultiItems(AppItem *appItem, bool emitSignal = false);
    // bool multiWindowExist(quint32 winId) const;
    // bool needRemoveMultiWindow(AppMultiItem *multiItem) const;


private:
    TaskManager *m_taskmanager;
    QSettings *m_qsettings;

    QList<QPointer<AppItem>> m_itemList;
    QList<QString> m_appIDist;
    QList<QPointer<DirItem>> m_dirList;
    QList<FolderItem*> m_folderList;
};

Q_DECLARE_METATYPE(DockItemManager::ActivateAnimationType);

#endif // DOCKITEMMANAGER_H
