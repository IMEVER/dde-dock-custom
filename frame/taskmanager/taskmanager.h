// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOCK_H
#define DOCK_H

#include "../util/docksettings.h"
#include "entries.h"
#include "org_deepin_dde_kwayland_plasmawindow.h"

#include <QStringList>
#include <QTimer>
#include <QMutex>
#include <QObject>

class WindowIdentify;
class DBusHandler;
class WaylandManager;
class X11Manager;
class WindowInfoK;
class WindowInfoX;

using PlasmaWindow = org::deepin::dde::KWayland1::PlasmaWindow;

// 任务管理
class TaskManager : public QObject
{
    Q_OBJECT
public:
    static inline TaskManager *instance() {
        static TaskManager instance;
        return &instance;
    }

    // 将Entry dock在任务栏上
    bool dockEntry(Entry *entry, bool moveToEnd = false);
    void undockEntry(Entry *entry, bool moveToEnd = false);

    bool shouldShowOnDock(WindowInfoBase *info);
    void setDdeLauncherVisible(bool visible);
    void setPropHideState(HideState state);
    void attachWindow(WindowInfoBase *info);
    void detachWindow(WindowInfoBase *info, bool del=true);

    void launchApp(const QString desktopFile, uint32_t timestamp, QStringList files);
    void launchAppAction(const QString desktopFile, QString action, uint32_t timestamp);

    bool isWaylandEnv();
    WindowInfoK *handleActiveWindowChangedK(uint activeWin);
    void saveDockedApps();
    void removeAppEntry(Entry *entry);
    void handleWindowGeometryChanged();
    Entry *getEntryByWindowId(XWindow windowId);
    QString getDesktopFromWindowByBamf(XWindow windowId);

    void registerWindowWayland(const QString &objPath);
    void unRegisterWindowWayland(const QString &objPath);
    bool isShowingDesktop();

    AppInfo *identifyWindow(WindowInfoBase *winInfo);

    QRect getFrontendWindowRect();
    QStringList getDockedApps();
    QList<Entry*> getEntries();
    HideMode getHideMode();
    void setHideMode(HideMode mode);
    HideState getHideState();
    void setHideState(HideState state);
    uint getHideTimeout();
    void setHideTimeout(uint timeout);
    uint getIconSize();
    void setIconSize(uint size);
    int getPosition();
    void setPosition(int position);
    uint getShowTimeout();
    void setShowTimeout(uint timeout);
    uint getWindowSizeFashion();
    void setWindowSizeFashion(uint size);

    /******************************** dbus handler ****************************/
    PlasmaWindow *createPlasmaWindow(QString objPath);
    void listenKWindowSignals(WindowInfoK *windowInfo);
    void removePlasmaWindowHandler(PlasmaWindow *window);
    void presentWindows(QList<uint> windows);

    HideMode getDockHideMode();
    bool isActiveWindow(const WindowInfoBase *win);
    WindowInfoBase *getActiveWindow();
    void doActiveWindow(XWindow xid);

    void closeWindow(XWindow windowId);
    void MinimizeWindow(XWindow windowId);
    void setFrontendWindowRect(int32_t x, int32_t y, uint width, uint height);
    bool isDocked(const QString desktopFile);
    bool requestDock(QString desktopFile, int index);
    bool requestUndock(QString desktopFile);
    void moveEntry(int oldIndex, int newIndex);
    void updateEntryOrder(QStringList&);
    bool isOnDock(QString desktopFile);
    QString queryWindowIdentifyMethod(XWindow windowId);

    void previewWindow(uint xid);
    void cancelPreviewWindow();

    QList<XWindow> getClientList() { return m_clientList; }
    void setClientList(QList<XWindow> list) { m_clientList = list; }

Q_SIGNALS:
    void serviceRestarted();
    void entryAdded(const Entry* entry, int index);
    void entryRemoved(QString id);
    void hideStateChanged(int);
    void frontendWindowRectChanged(const QRect &dockRect);
    void launcherVisibleChanged(bool);
    void showRecentChanged(bool);

public Q_SLOTS:
    void updateHideState(bool delay);
    void handleActiveWindowChanged(WindowInfoBase *info);
    void smartHideModeTimerExpired();
    void attachOrDetachWindow(WindowInfoBase *info);

private:
    explicit TaskManager(QObject *parent = nullptr);
    ~TaskManager();
    void initSettings();
    void initEntries();
    void loadAppInfos();
    void initClientList();
    bool isWindowDockOverlapX(XWindow xid);
    bool hasInterSectionX(const Geometry &windowRect, QRect dockRect);
    bool isWindowDockOverlapK(WindowInfoBase *info);
    bool hasInterSectionK(const DockRect &windowRect, QRect dockRect);
    bool shouldHideOnSmartHideMode();
    QVector<XWindow> getActiveWinGroup(XWindow xid);
    void updateRecentApps();

private:
    void onShowRecentChanged(bool visible);

private:
    bool m_isWayland; // 判断是否为wayland环境
    bool m_showRecent;

    HideState m_hideState;    // 记录任务栏隐藏状态
    QRect m_frontendWindowRect;    // 前端任务栏大小, 用于智能隐藏时判断窗口是否重合
    bool m_ddeLauncherVisible;

    Entries *m_entries;   // 所有应用实例
    X11Manager *m_x11Manager;     // X11窗口管理
    WaylandManager *m_waylandManager; // wayland窗口管理
    WindowIdentify *m_windowIdentify; // 窗口识别

    QTimer *m_smartHideTimer; // 任务栏智能隐藏定时器
    DBusHandler *m_dbusHandler;   // 处理dbus交互
    WindowInfoBase *m_activeWindow;// 记录当前活跃窗口信息

    QList<XWindow> m_clientList; // 所有窗口
};

#endif // TASKMANAGER_H
