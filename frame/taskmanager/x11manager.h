// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef X11MANAGER_H
#define X11MANAGER_H

#include "windowinfox.h"
#include "xcbutils.h"

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QTimer>

class TaskManager;

class X11Manager : public QObject
{
    Q_OBJECT
public:
    explicit X11Manager(TaskManager *_taskmanager, QObject *parent = nullptr);

    WindowInfoX *findWindowByXid(XWindow xid);
    WindowInfoX *registerWindow(XWindow xid);

    void handleClientListChanged();
    void handleActiveWindowChangedX();
    void listenRootWindowXEvent();

    void handleMapNotifyEvent(XWindow xid);

    void listenWindowEvent(WindowInfoX *winInfo);
    void listenXEventUseXlib();
    void listenXEventUseXCB();

private:
    WindowInfoX *unregisterWindow(XWindow xid);
    void listenWindowXEvent(const XWindow window);
    void handleCreateNotifyEvent(XWindow xid);
    void handleDestroyNotifyEvent(XWindow xid);
    void handlePropertyNotifyEvent(XWindow xid, XCBAtom atom);
    void handleConfigureNotifyEvent(XWindow xid, int x, int y, int width, int height);

Q_SIGNALS:
    void requestUpdateHideState(bool delay);
    void requestHandleActiveWindowChange(WindowInfoBase *info);
    void requestAttachOrDetachWindow(WindowInfoBase *info);

private:
    QMap<XWindow, WindowInfoX *> m_windowInfoMap;
    TaskManager *m_taskmanager;
    QMutex *m_mutex;
    XWindow m_rootWindow;                                                         // 根窗口
};

#endif // X11MANAGER_H
