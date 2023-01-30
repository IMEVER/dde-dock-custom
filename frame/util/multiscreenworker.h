/*
 * Copyright (C) 2018 ~ 2020 Deepin Technology Co., Ltd.
 *
 * Author:     fanpengcheng <fanpengcheng_cm@deepin.com>
 *
 * Maintainer: fanpengcheng <fanpengcheng_cm@deepin.com>
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

#ifndef MULTISCREENWORKER_H
#define MULTISCREENWORKER_H

#include "interfaces/constants.h"

#include <com_deepin_dde_daemon_dock.h>
#include <com_deepin_daemon_display.h>
#include <com_deepin_daemon_display_monitor.h>
#include <com_deepin_api_xeventmonitor.h>
#include <com_deepin_dde_launcher.h>
#include <DWindowManagerHelper>
#include <QObject>
#include <QFlag>

#define WINDOWMARGIN ((m_displayMode == Dock::Efficient) ? 0 : 10)

DGUI_USE_NAMESPACE
/**
 * 多屏功能这部分看着很复杂，其实只需要把握住一个核心：及时更新数据！
 * 之前测试出的诸多问题都是在切换任务栏位置，切换屏幕，主屏更改，分辨率更改等情况发生后
 * 任务栏的鼠标唤醒区域或任务栏的大小没更新或者更新时的大小还是按照原来的屏幕信息计算而来的，
 */
using DBusDock = com::deepin::dde::daemon::Dock;
using DisplayInter = com::deepin::daemon::Display;
using MonitorInter = com::deepin::daemon::display::Monitor;
using XEventMonitor = ::com::deepin::api::XEventMonitor;
using DBusLuncher = ::com::deepin::dde::Launcher;

using namespace Dock;
class QVariantAnimation;
class QWidget;
class QTimer;
class MainWindow;
class QGSettings;

/**
 * @brief The DockScreen class
 * 保存任务栏的屏幕信息
 */
class DockScreen
{
public:
    explicit DockScreen(const QString &primary)
        : m_currentScreen(primary)
        , m_lastScreen(primary)
        , m_primary(primary)
    {}
    inline const QString &current() const {return m_currentScreen;}
    inline const QString &last() const {return m_lastScreen;}
    inline const QString &primary() const {return m_primary;}

    void updateDockedScreen(const QString &screenName)
    {
        m_lastScreen = m_currentScreen;
        m_currentScreen = screenName;
    }

    void updatePrimary(const QString &primary)
    {
        m_primary = primary;
        if (m_currentScreen.isEmpty()) {
            updateDockedScreen(primary);
        }
    }

private:
    QString m_currentScreen;
    QString m_lastScreen;
    QString m_primary;
};

class MultiScreenWorker : public QObject
{
    Q_OBJECT
public:
    enum Flag {
        Motion = 1 << 0,
        Button = 1 << 1,
        Key    = 1 << 2
    };

    enum AniAction {
        Show = 0,
        Hide
    };

    enum RunState {
        ChangePositionAnimationStart = 0x4, // 任务栏切换位置动画运行状态
        AutoHide = 0x8,                     // 和MenuWorker保持一致,未设置此state时表示菜单已经打开
        MousePress = 0x10,                  // 当前鼠标是否被按下
        TouchPress = 0x20,                  // 当前触摸屏下是否按下
        LauncherDisplay = 0x40,             // 启动器是否显示

        // 如果要添加新的状态，可以在上面添加
        RunState_Mask = 0xffffffff,
    };

    typedef QFlags<RunState> RunStates;

    MultiScreenWorker(MainWindow *parent);

    void initShow();

    inline bool testState(RunState state) { return (m_state & state); }
    void setStates(RunStates state, bool on = true);
    void setWindowSize(int size);
    void setComposite(bool composite);

    inline const QString &lastScreen() { return m_ds.last(); }
    inline const QString &deskScreen() { return m_ds.current(); }
    inline const Position &position() { return m_position; }
    inline const HideMode &hideMode() { return m_hideMode; }
    inline const HideState &hideState() { return m_hideState; }
    inline quint8 opacity() { return m_opacity * 255; }

    QRect dockRect(const QString &screenName, const Position &pos, const HideMode &hideMode);

signals:
    void dockInterReady(DBusDock *dockInter);
    void opacityChanged(const quint8 value) const;
    // 更新监视区域
    void requestUpdateFrontendGeometry();                       //!!! 给后端的区域不能为是或宽度为0的区域,否则会带来HideState死循环切换的bug
    void requestUpdateLayout();                                 //　界面需要根据任务栏更新布局的方向

public slots:
    void onAutoHideChanged(bool autoHide);
    void updateDaemonDockSize(int dockSize);
    void onRequestUpdateRegionMonitor();
    void requestNotifyWindowManager();
    void handleDbusSignal(QDBusMessage);
    void updateDisplay();

private:
    // 初始化数据信息
    void initConnection();

    void displayAnimation(const QString &screen, const Position &pos, AniAction act);
    void displayAnimation(const QString &screen, AniAction act);
    void changeDockPosition(QString lastScreen, QString deskScreen, const Position &fromPos, const Position &toPos);

    QString getValidScreen(const Position &pos);
    void resetDockScreen();

    void checkDaemonDockService();
    void checkXEventMonitorService();

    QRect dockRectWithoutScale(const QString &screenName, const Position &pos, const HideMode &hideMode);
    QRect getDockShowGeometry(const QString &screenName, const Position &pos, bool withoutScale = false);
    QRect getDockHideGeometry(const QRect showRect, const Position &pos);

    QScreen *screenByName(const QString &screenName);
    bool onScreenEdge(const QString &screenName, const QPoint &point);
    const QPoint rawXPosition(const QPoint &scaledPos);
    void updateDockRect(QRect &dockRect, QRect screenRect, Position position, qreal ratio, int dockSize, int count);
    bool isCopyMode();

private:
    MainWindow *m_parent;
    QPropertyAnimation *ani;

    // monitor screen
    XEventMonitor *m_eventInter;
    XEventMonitor *m_extralEventInter;

    // DBus interface
    DBusDock *m_dockInter;
    DBusLuncher *m_launcherInter;
    QTimer *m_delayWakeTimer;                   // sp3需求，切换屏幕显示延时，默认2秒唤起任务栏
    DockScreen m_ds;                            // 屏幕名称信息

    // 任务栏属性
    double m_opacity;
    Position m_position;
    HideMode m_hideMode;
    HideState m_hideState;
    int m_windowSize;

    /***************不和其他流程产生交互,尽量不要动这里的变量***************/
    QString m_registerKey;
    QString m_extralRegisterKey;
    QList<MonitRect> m_monitorRectList;         // 监听唤起任务栏区域
    QList<MonitRect> m_extralRectList;          // 任务栏外部区域,随m_monitorRectList一起更新
    QString m_delayScreen;                      // 任务栏将要切换到的屏幕名
    RunStates m_state;
    /*****************************************************************/
};

#endif // MULTISCREENWORKER_H
