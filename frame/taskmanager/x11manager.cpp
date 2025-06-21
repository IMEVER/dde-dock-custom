// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "x11manager.h"
#include "taskmanager.h"
#include "common.h"

#include <QDebug>
#include <QTimer>

/*
 *  使用Xlib监听X Events
 *  使用XCB接口与X进行交互
 * */

#include <ctype.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <thread>

#define XCB XCBUtils::instance()

Display *dpy = nullptr;

X11Manager::X11Manager(TaskManager *_taskmanager, QObject *parent)
    : QObject(parent)
    , m_taskmanager(_taskmanager)
    , m_mutex(new QMutex(QMutex::NonRecursive))
{
    m_rootWindow = XCB->getRootWindow();
    dpy = XOpenDisplay (nullptr);
}

void X11Manager::listenXEventUseXlib()
{
    if (!dpy) {
        exit (1);
    }

    while (true) {
        XEvent event;
        XNextEvent (dpy, &event);

        switch (event.type) {
        // case DestroyNotify: {
        //     XDestroyWindowEvent *eD = (XDestroyWindowEvent *)(&event);
        //     // qDebug() <<  "DestroyNotify windowId=" << eD->window;

        //     handleDestroyNotifyEvent(XWindow(eD->window));
        //     break;
        // }
        // case CreateNotify: {
        //     auto eD = (XCreateWindowEvent*)(&event);
        //     handleCreateNotifyEvent(XWindow(eD->window));
        //     break;
        // }
        case MapNotify: {
            // XMapEvent *eM = (XMapEvent *)(&event);
            // qDebug() << "MapNotify windowId=" << eM->window;

            // handleMapNotifyEvent(XWindow(eM->window));
            break;
        }
        case ConfigureNotify: {
            XConfigureEvent *eC = (XConfigureEvent *)(&event);
            // qDebug() << "ConfigureNotify windowId=" << eC->window;

            handleConfigureNotifyEvent(XWindow(eC->window), eC->x, eC->y, eC->width, eC->height);
            break;
        }
        case PropertyNotify: {
            XPropertyEvent *eP = (XPropertyEvent *)(&event);
            // qDebug() << "PropertyNotify windowId=" << eP->window;

            handlePropertyNotifyEvent(XWindow(eP->window), XCBAtom(eP->atom));
            break;
        }
        case UnmapNotify: {
            // 当松开鼠标的时候会触发该事件，在松开鼠标的时候，需要检测当前窗口是否符合智能隐藏的条件，因此在此处加上该功能
            // 如果不加上该处理，那么就会出现将窗口从任务栏下方移动到屏幕中央的时候，任务栏不隐藏
            // handleActiveWindowChangedX();
            break;
        }
        default:
            qDebug() << "unused event type " << event.type;
            break;
        }
    }

    XCloseDisplay (dpy);
}

void X11Manager::listenXEventUseXCB()
{
    /*
    xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes(XCB->getConnect(), XCB->getRootWindow());
    xcb_get_window_attributes_reply_t *reply = xcb_get_window_attributes_reply(XCB->getConnect(), cookie, NULL);
    if (reply) {
        uint32_t valueMask = reply->your_event_mask;
        valueMask &= ~XCB_CW_OVERRIDE_REDIRECT;
        uint32_t mask[2] = {0};
        mask[0] = valueMask;
        //xcb_change_window_attributes(XCB->getConnect(), XCB->getRootWindow(), valueMask, mask);

        free(reply);
    }

    xcb_generic_event_t *event;
    while ( (event = xcb_wait_for_event (XCB->getConnect())) ) {
        eventHandler(event->response_type & ~0x80, event);
    }
    */
}

// void X11Manager::eventHandler(uint8_t type, void *event)
// {
//     qInfo() << "eventHandler" << "type = " << type;
//     switch (type) {
//     case XCB_MAP_NOTIFY:    // 17   注册新窗口
//         qInfo() << "eventHandler: XCB_MAP_NOTIFY";
//         break;
//     case XCB_DESTROY_NOTIFY:    // 19   销毁窗口
//         qInfo() << "eventHandler: XCB_DESTROY_NOTIFY";
//         break;
//     case XCB_CONFIGURE_NOTIFY:  // 22   窗口变化
//         qInfo() << "eventHandler: XCB_CONFIGURE_NOTIFY";
//         break;
//     case XCB_PROPERTY_NOTIFY:   // 28   窗口属性改变
//         qInfo() << "eventHandler: XCB_PROPERTY_NOTIFY";
//         break;
//     }
// }

/**
 * @brief X11Manager::registerWindow 注册X11窗口
 * @param xid
 * @return
 */
WindowInfoX *X11Manager::registerWindow(XWindow xid)
{
    // qInfo() << "registWindow: windowId=" << xid;
    WindowInfoX *ret = nullptr;

    if (!XCB->isGoodWindow(xid))
        return ret;

    WMClass wmClass = XCB->getWMClass(xid);

    if(wmClass.className.c_str() == frontendWindowWmClass
        or wmClass.className.c_str() == ddeTopPanelWmClass
        or wmClass.className.c_str() == ddeLauncherWMClass
        // or wmClass.className.c_str() == desktopWmClass
        )
        return ret;

    uint32_t pid = XCB->getWMPid(xid);
    auto wmName(XCB->getWMName(xid));
    if (pid == 0 and (wmClass.className.size() == 0 or wmClass.instanceName.size() == 0) and wmName.size() == 0 and XCB->getWMCommand(xid).size() == 0)
        return ret;

    do {
        if (m_windowInfoMap.find(xid) != m_windowInfoMap.end()) {
            ret = m_windowInfoMap[xid];
            break;
        }

        WindowInfoX *winInfo = new WindowInfoX(xid);
        if (!winInfo)
            break;

        listenWindowXEvent(xid);
        m_windowInfoMap[xid] = winInfo;
        ret = winInfo;
    } while (0);

    return ret;
}

// 取消注册X11窗口
WindowInfoX * X11Manager::unregisterWindow(XWindow xid)
{
    // qInfo() << "unregisterWindow: windowId=" << xid;
    return m_windowInfoMap.take(xid);
}

WindowInfoX *X11Manager::findWindowByXid(XWindow xid)
{
    return m_windowInfoMap.value(xid);
}

void X11Manager::handleClientListChanged()
{
    QSet<XWindow> newClientList, oldClientList, addClientList, rmClientList;
    for (auto atom : XCB->getClientList())
        newClientList.insert(atom);

    for (auto atom : m_taskmanager->getClientList())
        oldClientList.insert(atom);

    addClientList = newClientList - oldClientList;
    rmClientList = oldClientList - newClientList;
    m_taskmanager->setClientList(newClientList.values());

    // 处理新增窗口
    for (auto xid : addClientList) {
        if(auto info = registerWindow(xid))
            Q_EMIT requestAttachOrDetachWindow(info);
    }

    // 处理需要移除的窗口
    for (auto xid : rmClientList) {
        if(auto info = unregisterWindow(xid)) {
            m_taskmanager->detachWindow(info);
        } else if(auto entry = m_taskmanager->getEntryByWindowId(xid)) {
            if (!m_taskmanager->isDocked(entry->getFileName()))
                m_taskmanager->removeAppEntry(entry);
        }
    }
}

void X11Manager::handleActiveWindowChangedX()
{
    XWindow active = XCB->getActiveWindow();
    if(auto info = m_windowInfoMap.value(active))
        Q_EMIT requestHandleActiveWindowChange(info);
}

void X11Manager::listenRootWindowXEvent()
{
    listenWindowXEvent(m_rootWindow);
    // handleClientListChanged();
    // handleActiveWindowChangedX();
}

/**
 * @brief X11Manager::listenWindowXEvent 监听窗口事件
 * @param winInfo
 */
void X11Manager::listenWindowXEvent(const XWindow window)
{
    // uint32_t eventMask = EventMask::XCB_EVENT_MASK_PROPERTY_CHANGE/* | EventMask::XCB_EVENT_MASK_STRUCTURE_NOTIFY*/;
    // XCB->registerEvents(window, eventMask);

    const struct {
        const char *name;
        long mask;
    } events[] = {
        // { "keyboard", KeyPressMask | KeyReleaseMask | KeymapStateMask },
        // { "mouse", ButtonPressMask | ButtonReleaseMask | EnterWindowMask |
        //             LeaveWindowMask | PointerMotionMask | Button1MotionMask |
        //             Button2MotionMask | Button3MotionMask | Button4MotionMask |
        //             Button5MotionMask | ButtonMotionMask },
        // { "button", ButtonPressMask | ButtonReleaseMask },
        // { "expose", ExposureMask },
        // { "visibility", VisibilityChangeMask },
        // { "structure", StructureNotifyMask },
        // { "substructure", SubstructureNotifyMask | SubstructureRedirectMask },
        // { "focus", FocusChangeMask },
        { "property", PropertyChangeMask },
        // { "colormap", ColormapChangeMask },
        // { "owner_grab_button", OwnerGrabButtonMask },
        { nullptr, 0 }
    };

    XSetWindowAttributes attr;
    attr.event_mask = 0;
    for (int i = 0; events[i].name; i++)
        attr.event_mask |= events[i].mask;

    // if(window == m_rootWindow)
    //     attr.event_mask |= SubstructureNotifyMask;
    // else
        attr.event_mask |= StructureNotifyMask;

    XSelectInput(dpy, window, attr.event_mask);
}

// destory event
void X11Manager::handleDestroyNotifyEvent(XWindow xid)
{
    if(auto winInfo = unregisterWindow(xid))
        m_taskmanager->detachWindow(winInfo);
}

void X11Manager::handleCreateNotifyEvent(XWindow xid) {
    // using namespace std::chrono_literals;
    // std::this_thread::sleep_for(500ms);
    if(auto info = registerWindow(xid))
        Q_EMIT requestAttachOrDetachWindow(info);
}

// map event
void X11Manager::handleMapNotifyEvent(XWindow xid)
{
    WindowInfoX *winInfo = registerWindow(xid);
    if (!winInfo)
        return;

    // TODO QTimer不能在非主线程执行，使用单独线程开发定时器处理非主线程类似定时任务
    QTimer::singleShot(2 * 1000, this, [this, winInfo] {
        qInfo() << "handleMapNotifyEvent: pass 2s, now call idnetifyWindow, windowId=" << winInfo->getXid();

        if(auto appInfo = m_taskmanager->identifyWindow(winInfo)) {
            // m_taskmanager->markAppLaunched(appInfo);
            delete appInfo;
        }
    });
}

// config changed event 检测窗口大小调整和重绘应用，触发智能隐藏更新
void X11Manager::handleConfigureNotifyEvent(XWindow xid, int x, int y, int width, int height)
{
    auto winInfo = m_windowInfoMap.value(xid);
    if (!winInfo || m_taskmanager->getDockHideMode() != HideMode::SmartHide)
        return;

    Q_EMIT requestUpdateHideState(winInfo->isGeometryChanged(x, y, width, height));
}

// property changed event
void X11Manager::handlePropertyNotifyEvent(XWindow xid, XCBAtom atom)
{
    if (xid == m_rootWindow) {
        if (atom == XCB->getAtom("_NET_CLIENT_LIST")) {
            // 窗口列表改变
            handleClientListChanged();
        } else if (atom == XCB->getAtom("_NET_ACTIVE_WINDOW")) {
            // 活动窗口改变
            handleActiveWindowChangedX();
        } else if (atom == XCB->getAtom("_NET_SHOWING_DESKTOP")) {
            // 更新任务栏隐藏状态
            Q_EMIT requestUpdateHideState(false);
        }
        return;
    }

    WindowInfoX *winInfo = m_windowInfoMap.value(xid);
    if (!winInfo) return;

    Entry *entry = m_taskmanager->getEntryByWindowId(xid);
    if (!entry) return;

    QString newInnerId;
    bool needAttachOrDetach = false;
    bool needUpdateHideState = false;
    if (atom == XCB->getAtom("_NET_WM_STATE")) {
        needUpdateHideState = winInfo->isMaximized();
        winInfo->updateWmState();
        needAttachOrDetach = true;
        needUpdateHideState = needUpdateHideState != winInfo->isMaximized();
    } else if (atom == XCB->getAtom("_GTK_APPLICATION_ID")) {
        QString gtkAppId = XCB->getUTF8PropertyStr(xid, atom).c_str();
        winInfo->setGtkAppId(gtkAppId);
        newInnerId = winInfo->genInnerId(winInfo);
    } else if (atom == XCB->getAtom("_NET_WM_PID")) {
        winInfo->updateProcessInfo();
        newInnerId = winInfo->genInnerId(winInfo);
    } else if (atom == XCB->getAtom("_NET_WM_NAME")) {
        winInfo->updateWmName();
        newInnerId = winInfo->genInnerId(winInfo);
    } else if (atom == XCB->getAtom("_NET_WM_ICON")) {
        winInfo->updateIcon();
    } else if (atom == XCB->getAtom("_NET_WM_ALLOWED_ACTIONS")) {
        winInfo->updateWmAllowedActions();
    } else if (atom == XCB->getAtom("_MOTIF_WM_HINTS")) {
        winInfo->updateMotifWmHints();
    } else if (atom == XCB_ATOM_WM_CLASS) {
        winInfo->updateWmClass();
        newInnerId = winInfo->genInnerId(winInfo);
        needAttachOrDetach = true;
    } else if (atom == XCB->getAtom("_XEMBED_INFO")) {
        winInfo->updateHasXEmbedInfo();
        needAttachOrDetach = true;
    } else if (atom == XCB->getAtom("_NET_WM_WINDOW_TYPE")) {
        winInfo->updateWmWindowType();
        needAttachOrDetach = true;
    } else if (atom == XCB_ATOM_WM_TRANSIENT_FOR) {
        winInfo->updateHasWmTransientFor();
        needAttachOrDetach = true;
    }

    if (!newInnerId.isEmpty() && winInfo->getUpdateCalled() && winInfo->getInnerId() != newInnerId) {
        // winInfo.innerId changed
        m_taskmanager->detachWindow(winInfo, false);
        winInfo->setInnerId(newInnerId);
        needAttachOrDetach = true;
    }

    if (needAttachOrDetach)
        Q_EMIT requestAttachOrDetachWindow(winInfo);

    if (atom == XCB->getAtom("_NET_WM_STATE")) {
        // entry->updateExportWindowInfos();
        if(needUpdateHideState and m_taskmanager->getDockHideMode() == HideMode::SmartHide)
            emit requestUpdateHideState(true);
    } else if (atom == XCB->getAtom("_NET_WM_ICON")) {
        if (entry->getCurrentWindowInfo() == winInfo) {
            entry->updateIcon();
        }
    } else if (atom == XCB->getAtom("_NET_WM_NAME")) {
        if (entry->getCurrentWindowInfo() == winInfo) {
            entry->updateName();
        }
        // entry->updateExportWindowInfos();
    } else if (atom == XCB->getAtom("_NET_WM_ALLOWED_ACTIONS")) {
        entry->updateMenu();
    }
}
