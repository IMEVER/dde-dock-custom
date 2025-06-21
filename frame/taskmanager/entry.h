// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ENTRY_H
#define ENTRY_H

#include "appinfo.h"
#include "appmenu.h"
#include "windowinfomap.h"
#include "windowinfobase.h"

#include <QMap>
#include <QVector>
#include <QObject>
#include <qscopedpointer.h>

// 单个应用类
class TaskManager;
class DBusAdaptorEntry;
class WindowInfo;

typedef QMap<XWindow, WindowInfo> WindowInfoMap;

class Entry: public QObject
{
    Q_OBJECT
public:
    Entry(TaskManager *_taskmanager, AppInfo *_app, QObject *parent = nullptr);
    Entry(TaskManager *_taskmanager, WindowInfoBase *window, QObject *parent = nullptr);
    ~Entry();

    void updateName();
    void updateMenu();
    void updateIcon();
    void updateIsActive();
    void launchApp(uint32_t timestamp);

    void setIsDocked(bool value);
    void setMenu(AppMenu *_menu);
    void setPropIcon(QString value);
    void setPropName(QString value);
    void setPropIsActive(bool active);
    void setAppInfo(AppInfo *appinfo);
    void setPropCurrentWindow(XWindow value);
    void setCurrentWindowInfo(WindowInfoBase *windowInfo);

    void check();
    void forceQuit();
    void presentWindows();
    void active(uint32_t timestamp);
    void activeWindow(quint32 winId);
    void close(const XWindow wId);
    void requestDock(bool dockToEnd = false);
    void requestUndock(bool dockToEnd = false);
    void handleMenuItem(uint32_t timestamp, QString itemId);
    void handleDragDrop(uint32_t timestamp, QStringList files);

    bool containsWindow(XWindow xid);
    bool detachWindow(WindowInfoBase *info, bool del);
    bool attachWindow(WindowInfoBase *info);

    bool getIsDocked() const;
    bool getIsActive() const;

    QString getId() const;
    QString getMenu() const;

    bool isValid();
    bool hasWindow();
    bool hasCloseableWindow();
    bool hasMpris();

    QString getName();
    QString getIcon();
    QString getInnerId() const { return m_isValid ? m_appInfo->getInnerId() : (m_current ? m_current->getInnerId() : ""); }
    QString getFileName();
    QString getExec();
    QString getCmdLine();

    XWindow getCurrentWindow();

    AppInfo *getAppInfo();

    WindowInfoBase *findNextLeader();
    WindowInfoBase *getCurrentWindowInfo();
    WindowInfoBase *getWindowInfoByPid(int pid);
    WindowInfoBase *getWindowInfoByWinId(XWindow windowId);

    const WindowInfoMap &getExportWindowInfos() const;
    QVector<XWindow> getAllowedClosedWindowIds();

    inline int lastUpdateTime() const { return m_lastUpdateTime; }

Q_SIGNALS:
    void isActiveChanged(bool);
    void isDockedChanged(bool);
    void menuChanged(QString);
    void iconChanged(QString);
    void nameChanged(QString);
    void titleChanged(XWindow wid, const QString &title);
    void currentWindowChanged(uint32_t);
    void windowInfoAdded(const WindowInfo&);
    void windowInfoRemoved(const WindowInfo&);
    void mprisChanged();

private:
    // 右键菜单项
    bool killProcess(int pid);

    AppMenuItem getMenuItemLaunch();
    AppMenuItem getMenuItemCloseAll();
    AppMenuItem getMenuItemForceQuit();

    AppMenuItem getMenuItemDock();
    AppMenuItem getMenuItemUndock();
    AppMenuItem getMenuItemAllWindows();
    AppMenuItem getMenuItemForceQuitAndroid();
    QVector<AppMenuItem> getMenuItemDesktopActions();

private:
    bool m_isActive;
    bool m_isValid;
    bool m_isDocked;
    int m_lastUpdateTime;

    QString m_id;
    QString m_name;
    QString m_icon;

    DBusAdaptorEntry *m_adapterEntry;
    TaskManager *m_taskmanager;
    WindowInfoMap m_exportWindowInfos;      // 该应用导出的窗口属性
    WindowInfoBase *m_current; // 当前窗口
    XWindow m_currentWindow; //当前窗口Id

    QScopedPointer<AppInfo> m_appInfo;
    QScopedPointer<AppMenu> m_appMenu;
    QMap<XWindow, WindowInfoBase *> m_windowInfoMap; // 该应用所有窗口
};

#endif // ENTRY_H
