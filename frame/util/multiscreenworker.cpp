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

#include "multiscreenworker.h"
#include "window/mainwindow.h"
#include "window/mainpanelcontrol.h"
#include "utils.h"
#include "xcb/xcb_misc.h"
#include "displaymanager.h"
#include "dockscreen.h"
#include "org_deepin_dde_appearance1.h"
#include "docksettings.h"
#include "window/dockitemmanager.h"

#include <QWidget>
#include <QScreen>
#include <QEvent>
#include <QRegion>
#include <QVariantAnimation>
#include <QX11Info>
#include <QDBusConnection>
#include <qpa/qplatformscreen.h>
#include <QApplication>

using Appearance = org::deepin::dde::Appearance1;

static const QString MonitorsSwitchTime = "monitorsSwitchTime";

#define DOCK_SCREEN DockScreen::instance()
#define DIS_INS DisplayManager::instance()
#define SPLITER_SIZE 3
#define MODE_PADDING 5

class SharedData {
    public:
        static SharedData *instance()
        {
            static SharedData *INSTANCE = new SharedData;
            return INSTANCE;
        }

        QRect getRect(const QString name, Position pos, qreal ratio, int itemCount)
        {
            return data.value(generalKey(name, pos, ratio, itemCount), {});
        }

        void setRect(const QString name, Position pos, qreal ratio, int itemCount, QRect rect)
        {
            if(rect.isValid())
                data.insert(generalKey(name, pos, ratio, itemCount), rect);
        }

        QString generalKey(const QString name, Position pos, qreal ratio, int itemCount)
        {
            return QString("%1_%2_%3_%4").arg(name).arg(pos).arg(ratio).arg(itemCount);
        }

        void clear() { data.clear(); }

    private:
        SharedData(){}

    private:
        QMap<QString, QRect> data;
};

MultiScreenWorker::MultiScreenWorker(MainWindow *parent) : QObject(parent)
    , m_parent(parent)
    , m_eventInter(nullptr)
    , m_extralEventInter(nullptr)
    , m_launcherInter(nullptr)
    , m_delayWakeTimer(new QTimer(this))
    , m_state(AutoHide)
{
    registerAreaListMetaType();

    m_delayWakeTimer->setSingleShot(true);
    m_delayWakeTimer->setInterval(Utils::SettingValue("com.deepin.dde.dock.mainwindow", "/com/deepin/dde/dock/mainwindow/", MonitorsSwitchTime, 2000).toInt());

    ani = new QPropertyAnimation(m_parent, "pos", this);
    ani->setDuration(300);
    ani->setEasingCurve(QEasingCurve::InOutCubic);
    connect(ani, &QPropertyAnimation::finished, this, [this]{
        if(ani->direction() == QPropertyAnimation::Backward) m_parent->hide();
        setStates(ShowAnimationStart, false);
        setStates(HideAnimationStart, false);
    });

    m_launcherInter = new LauncherInter("org.deepin.dde.Launcher1", "/org/deepin/dde/Launcher1", QDBusConnection::sessionBus(), this);

    setStates(LauncherDisplay, false);

    initConnection();
    checkXEventMonitorService();

    // init data
    m_position = DockSettings::instance()->getPositionMode();
    m_hideMode = DockSettings::instance()->getHideMode();
    m_hideState = HideState::Show;// static_cast<Dock::HideState >(m_dockInter->hideState());
    auto appearance = new Appearance("org.deepin.dde.Appearance1", "/org/deepin/dde/Appearance1", QDBusConnection::sessionBus(), this);
    m_opacity = appearance->opacity();
    connect(appearance, &Appearance::Changed, this, [this](const QString &ty, const QString &value){
        if(ty == "windowopacity") {
            m_opacity = value.toDouble();
            emit opacityChanged(m_opacity * 255);
        }
    });

    m_windowSize = DockSettings::instance()->getWindowSizeFashion();

    connect(DockSettings::instance(), &DockSettings::positionModeChanged, this, [this](Position position){
        if(position == Top) position = Bottom;
        if (m_position != position) {
            Position lastPos = m_position;
            m_position = position;

            DOCK_SCREEN->updateDockedScreen(getValidScreen());
            changeDockPosition(DOCK_SCREEN->last(), DOCK_SCREEN->current(), lastPos, position);
        }
    });
    connect(DockSettings::instance(), &DockSettings::hideModeChanged, this, [this](HideMode hideMode){
        if (m_hideMode != hideMode) {
            m_hideMode = hideMode;

            if (m_hideMode == HideMode::KeepShowing || m_hideState == HideState::Show)
                displayAnimation(AniAction::Show);
            else if (m_hideMode != HideMode::KeepShowing && m_hideState == HideState::Hide)
                displayAnimation(AniAction::Hide);
            requestNotifyWindowManager();
            onRequestUpdateRegionMonitor();
            emit requestUpdateFrontendGeometry();
        }
    });
    connect(TaskManager::instance(), &TaskManager::hideStateChanged, this, [this](int state){
        if (state == m_hideState)
            return;

        m_hideState = static_cast<HideState>(state);

        // 检查当前屏幕的当前位置是否允许显示,不允许需要更新显示信息(这里应该在函数外部就处理好,不应该走到这里)

        //TODO 这里是否存在屏幕找不到的问题，m_ds的当前屏幕是否可以做成实时同步的，公用一个指针？
        //TODO 这里真的有必要加以下代码吗，只是隐藏模式的切换，理论上不需要检查屏幕是否允许任务栏停靠
        if (!DIS_INS->canDock(DIS_INS->screen(DOCK_SCREEN->current()), m_position))
            DOCK_SCREEN->updateDockedScreen(getValidScreen());

        qDebug() << "hidestate change:" << m_hideState;
        if(testState(ChangePositionAnimationStart) || testState(ShowAnimationStart) || testState(HideAnimationStart)) return;

        if (m_hideMode == HideMode::KeepShowing || m_hideState == HideState::Show) {
            displayAnimation(AniAction::Show);
        } else if (m_hideState == HideState::Hide) {
            // 最后一个参数，当任务栏的隐藏状态发生变化的时候（从一直显示变成一直隐藏或者智能隐藏），需要考虑鼠标是否在任务栏上，如果在任务栏上，此时无需执行隐藏动画
            if (!getDockShowGeometry(DOCK_SCREEN->current(), m_position).contains(QCursor::pos()))
                displayAnimation(AniAction::Hide);
        }
    });

    connect(DockSettings::instance(), &DockSettings::windowSizeFashionChanged, this, &MultiScreenWorker::setWindowSize);
}

void MultiScreenWorker::initShow()
{
    // 仅在初始化时调用一次
    static bool first = true;
    if (first)
    {
        first = false;

        const QRect rect = getDockShowGeometry(DOCK_SCREEN->current(), m_position);
        m_parent->resize(rect.size());
        m_parent->panel()->setPositonValue(m_position);

        if (m_hideMode == HideMode::KeepShowing || m_hideState == HideState::Show)
            displayAnimation(AniAction::Show);
        else if (m_hideMode == HideMode::KeepHidden || m_hideState == HideState::Hide)
            displayAnimation(AniAction::Hide);
    }
}

/**
 * @brief MultiScreenWorker::setStates 用于存储一些状态
 * @param state 标识是哪一种状态，后面有需求可以扩充
 * @param on 设置状态为true或false
 */
void MultiScreenWorker::setStates(RunStates state, bool on)
{
    RunState type = static_cast<RunState>(int(state & RunState::RunState_Mask));

    if (on)
        m_state |= type;
    else
        m_state &= ~(type);
}

void MultiScreenWorker::setWindowSize(int size)
{
    if(m_windowSize != size) {
        SharedData::instance()->clear();
        m_windowSize = size;
    }
}

void MultiScreenWorker::onAutoHideChanged(bool autoHide)
{
    setStates(AutoHide, autoHide);
    if(autoHide && m_hideMode != KeepShowing && m_hideState == HideState::Hide && m_parent->isVisible() && !m_parent->underMouse() && ani->state() == QPropertyAnimation::Stopped)
        displayAnimation(AniAction::Hide);
}

/**
 * @brief updateDaemonDockSize
 * @param dockSize              这里的高度是通过qt获取的，不能使用后端的接口数据
 */
void MultiScreenWorker::updateDaemonDockSize(int dockSize)
{
    DockSettings::instance()->setWindowSizeFashion(uint(dockSize));

    setWindowSize(dockSize);

    requestNotifyWindowManager();
    requestUpdateFrontendGeometry();
    onRequestUpdateRegionMonitor();
}

void MultiScreenWorker::updateDisplay()
{
    if (ani->state() == QPropertyAnimation::Stopped) {
        const QRect rect = getDockShowGeometry(DOCK_SCREEN->current(), m_position);
        if(!m_parent->isHidden()) {
            // m_parent->setGeometry(rect);
            auto animation = new QPropertyAnimation(m_parent, "geometry", this);
            animation->setEasingCurve(QEasingCurve::InOutCubic);
            animation->setStartValue(m_parent->geometry());
            animation->setEndValue(rect);
            animation->setDuration(100);
            connect(animation, &QPropertyAnimation::finished, this, &MultiScreenWorker::requestUpdateFrontendGeometry);
            animation->start(QPropertyAnimation::DeleteWhenStopped);
        } else {
            m_parent->resize(rect.size());
            emit requestUpdateFrontendGeometry();
        }
    }
}

/**
 * @brief onRequestUpdateRegionMonitor  更新监听区域信息
 * 触发时机:屏幕大小,屏幕坐标,屏幕数量,发生变化
 *          任务栏位置发生变化
 *          任务栏'模式'发生变化
 */
void MultiScreenWorker::onRequestUpdateRegionMonitor()
{
    if(!m_eventInter || !m_extralEventInter) return;

    if (!m_registerKey.isEmpty()) {
        m_eventInter->UnregisterArea(m_registerKey);
        m_registerKey.clear();
    }

    if (!m_extralRegisterKey.isEmpty()) {
        m_extralEventInter->UnregisterArea(m_extralRegisterKey);
        m_extralRegisterKey.clear();
    }

    if(DIS_INS->screens().size() == 1 && m_hideMode == KeepShowing) return;

    const static int flags = Motion | Button | Key;
    const static int monitorHeight = static_cast<int>(10 * qApp->devicePixelRatio());
    // 后端认为的任务栏大小(无缩放因素影响)
    const int realDockSize = m_windowSize; //int( m_dockInter->windowSizeFashion() * qApp->devicePixelRatio());

    // 任务栏唤起区域
    m_monitorRectList.clear();
    for (auto s : DIS_INS->screens()) {
        // 屏幕此位置不可停靠时,不用监听这块区域
        if (!DIS_INS->canDock(s, m_position)) continue;

        MonitRect monitorRect;
        QRect screenRect = s->geometry();
        screenRect.setSize(screenRect.size() * s->devicePixelRatio());

        switch (m_position) {
        case Top:
        case Bottom: {
            monitorRect.x1 = screenRect.x();
            monitorRect.y1 = screenRect.bottom() - monitorHeight;
            monitorRect.x2 = screenRect.right();
            monitorRect.y2 = screenRect.bottom();
        }
        break;
        case Left: {
            monitorRect.x1 = screenRect.x();
            monitorRect.y1 = screenRect.y();
            monitorRect.x2 = screenRect.x() + monitorHeight;
            monitorRect.y2 = screenRect.bottom();
        }
        break;
        case Right: {
            monitorRect.x1 = screenRect.right() - monitorHeight;
            monitorRect.y1 = screenRect.y();
            monitorRect.x2 = screenRect.right();
            monitorRect.y2 = screenRect.bottom();
        }
        break;
        }

        m_monitorRectList << monitorRect;
    }

    m_extralRectList.clear();
    for (auto s : DIS_INS->screens()) {
        // 屏幕此位置不可停靠时,不用监听这块区域
        if (!DIS_INS->canDock(s, m_position)) continue;

        MonitRect monitorRect;
        QRect screenRect = s->geometry();
        screenRect.setSize(screenRect.size() * s->devicePixelRatio());

        switch (m_position) {
        case Top:
        case Bottom: {
            monitorRect.x1 = screenRect.x();
            monitorRect.y1 = screenRect.bottom() - realDockSize;
            monitorRect.x2 = screenRect.right();
            monitorRect.y2 = screenRect.bottom();
        }
        break;
        case Left: {
            monitorRect.x1 = screenRect.x();
            monitorRect.y1 = screenRect.y();
            monitorRect.x2 = screenRect.x() + realDockSize;
            monitorRect.y2 = screenRect.bottom();
        }
        break;
        case Right: {
            monitorRect.x1 = screenRect.right() - realDockSize;
            monitorRect.y1 = screenRect.y();
            monitorRect.x2 = screenRect.right();
            monitorRect.y2 = screenRect.bottom();
        }
        break;
        }

        m_extralRectList << monitorRect;
    }

    m_registerKey = m_eventInter->RegisterAreas(m_monitorRectList, flags);
    m_extralRegisterKey = m_extralEventInter->RegisterAreas(m_extralRectList, flags);
}

/**
 * @brief 判断屏幕是否为复制模式的依据，第一个屏幕的X和Y值是否和其他的屏幕的X和Y值相等
 * 对于复制模式，这两个值肯定是相等的，如果不是复制模式，这两个值肯定不等，目前支持双屏
 */
bool MultiScreenWorker::isCopyMode()
{
    QList<QScreen *> screens = DIS_INS->screens();
    if (screens.size() < 2)
        return false;

    // 在多个屏幕的情况下，如果所有屏幕的位置的X和Y值都相等，则认为是复制模式
    QRect rect0 = screens[0]->availableGeometry();
    for (int i = 1; i < screens.size(); i++) {
        QRect rect = screens[i]->availableGeometry();
        if (rect0.x() != rect.x() || rect0.y() != rect.y())
            return false;
    }

    return true;
}

/**
 * @brief 这里用到xcb去设置任务栏的高度，比较特殊，参考_NET_WM_STRUT_PARTIAL属性
 * 在屏幕旋转后，所有参数以控制中心自定义设置里主屏显示的图示为准（旋转不用特殊处理）
 */
void MultiScreenWorker::requestNotifyWindowManager()
{
    static QRect lastRect = QRect();
    static int lastScreenWidth = 0;
    static int lastScreenHeight = 0;

    /* 在非主屏或非一直显示状态时，清除任务栏区域，不挤占应用 */
    if (m_hideMode != HideMode::KeepShowing || (!isCopyMode() && DOCK_SCREEN->current() != DOCK_SCREEN->primary())) {
        lastRect = QRect();

        if (QX11Info::display()) XcbMisc::instance()->clear_strut_partial(xcb_window_t(m_parent->winId()));

        return;
    }

    QRect dockGeometry = getDockShowGeometry(DOCK_SCREEN->current(), m_position, true);
    if (lastRect == dockGeometry && lastScreenWidth == DIS_INS->screenRawWidth() && lastScreenHeight == DIS_INS->screenRawHeight())
        return;

    lastRect = dockGeometry;
    lastScreenWidth = DIS_INS->screenRawWidth();
    lastScreenHeight = DIS_INS->screenRawHeight();

    XcbMisc::Orientation orientation = XcbMisc::OrientationTop;
    double strut = 0;
    double strutStart = 0;
    double strutEnd = 0;

    switch (m_position) {
    case Top:
    case Position::Bottom:
        orientation = XcbMisc::OrientationBottom;
        strut = DIS_INS->screenRawHeight() - dockGeometry.y();
        strutStart = dockGeometry.x();
        strutEnd = qMin(dockGeometry.x() + dockGeometry.width(), dockGeometry.right());
        break;
    case Position::Left:
        orientation = XcbMisc::OrientationLeft;
        strut = dockGeometry.x() + dockGeometry.width();
        strutStart = dockGeometry.y();
        strutEnd = qMin(dockGeometry.y() + dockGeometry.height(), dockGeometry.bottom());
        break;
    case Position::Right:
        orientation = XcbMisc::OrientationRight;
        strut = DIS_INS->screenRawWidth() - dockGeometry.x();
        strutStart = dockGeometry.y();
        strutEnd = qMin(dockGeometry.y() + dockGeometry.height(), dockGeometry.bottom());
        break;
    }

    if (QX11Info::display()) {
        XcbMisc::instance()->set_strut_partial(static_cast<xcb_window_t>(m_parent->winId()), orientation,
                                           static_cast<uint>(strut), // 设置窗口与屏幕边缘距离，需要乘缩放
                                           static_cast<uint>(strutStart),                   // 设置任务栏起点坐标（上下为x，左右为y）
                                           static_cast<uint>(strutEnd));                    // 设置任务栏终点坐标（上下为x，左右为y）
    }
}

void MultiScreenWorker::initConnection()
{
    connect(qApp, &QApplication::primaryScreenChanged, this, [this]{
        DOCK_SCREEN->updatePrimary(DIS_INS->primary());

        if (DIS_INS->screenRawHeight() != 0 && DIS_INS->screenRawWidth() != 0)
            resetDockScreen();
    });
    connect(DIS_INS, &DisplayManager::primaryScreenChanged, this, [this]{
        DOCK_SCREEN->updatePrimary(DIS_INS->primary());

        if (DIS_INS->screenRawHeight() != 0 && DIS_INS->screenRawWidth() != 0)
            resetDockScreen();
    });
    connect(DIS_INS, &DisplayManager::screenInfoChanged, this, &MultiScreenWorker::resetDockScreen);

    connect(m_launcherInter, &LauncherInter::VisibleChanged, this, [ this ](bool value) { setStates(LauncherDisplay, value); });

    connect(this, &MultiScreenWorker::requestUpdateFrontendGeometry, this, [this]{
        const QRect rect = getDockShowGeometry(DOCK_SCREEN->current(), m_position, true);

        if(rect.width() == 0 || rect.height() == 0) return;

        int x = rect.x();
        int y = rect.y();

        if (QScreen *screen = DIS_INS->screen(DOCK_SCREEN->current())) {
            if (m_position == Dock::Position::Top || m_position == Dock::Position::Bottom)
                x = screen->handle()->geometry().x() + qMax(0, (int)((screen->handle()->geometry().width() - (rect.width() * qApp->devicePixelRatio())) / 2));
            else
                y = screen->handle()->geometry().y() + qMax(0, (int)((screen->handle()->geometry().height() - (rect.height() * qApp->devicePixelRatio())) / 2));
        }

        TaskManager::instance()->setFrontendWindowRect(x, y, uint(rect.width()), uint(rect.height()));
    });

    connect(m_delayWakeTimer, &QTimer::timeout, this, [this]{
        QString toScreen = m_delayScreen;
        m_delayScreen.clear();
        // 移动Dock至相应屏相应位置
        if (testState(LauncherDisplay) || testState(ChangePositionAnimationStart)) return;

        // 复制模式．不需要响应切换屏幕
        if (DIS_INS->screens().size() == 2 && DIS_INS->screens().first()->geometry() == DIS_INS->screens().last()->geometry()) {
            QRect rect = getDockShowGeometry(DOCK_SCREEN->current(), m_position);
            if(m_parent->isHidden()) rect = getDockHideGeometry(rect, m_position);
            m_parent->setGeometry(rect);
            return;
        }
        DOCK_SCREEN->updateDockedScreen(toScreen);

        // 检查当前屏幕的当前位置是否允许显示,不允许需要更新显示信息(这里应该在函数外部就处理好,不应该走到这里)
        // 检查边缘是否允许停靠
        QScreen *curScreen = DIS_INS->screen(toScreen);
        if (curScreen && DIS_INS->canDock(curScreen, m_position))
            changeDockPosition(DOCK_SCREEN->last(), DOCK_SCREEN->current(), m_position, m_position);
    });
}

/**
 * @brief MultiScreenWorker::displayAnimation
 * 任务栏显示或隐藏过程的动画。
 * @param screen 任务栏要显示的屏幕
 * @param pos 任务栏显示的位置（上：0，右：1，下：2，左：3）
 * @param act 显示（隐藏）任务栏
 * @return void
 */
void MultiScreenWorker::displayAnimation(const QString &screen, const Position &pos, AniAction act)
{
    if ((!testState(AutoHide) && !testState(ChangePositionAnimationStart)) || testState(ShowAnimationStart) || testState(HideAnimationStart)) return;

    if ((act == AniAction::Hide && m_parent->isHidden()) || (act == AniAction::Show && !m_parent->isHidden())) {
        emit ani->finished();
        return;
    }

    QRect dockShowRect = getDockShowGeometry(screen, pos);
    QRect dockHideRect = getDockHideGeometry(dockShowRect, pos);

    ani->setStartValue(dockHideRect.topLeft());
    ani->setEndValue(dockShowRect.topLeft());
    ani->setDirection(act == AniAction::Show ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);

    if(testState(ChangePositionAnimationStart) == false)
        setStates(act == AniAction::Show ? ShowAnimationStart : HideAnimationStart);
    ani->start();
    if(act == AniAction::Show) m_parent->show();
}

/**
 * @brief MultiScreenWorker::displayAnimation
 * 任务栏显示或隐藏过程的动画。
 * @param screen 任务栏要显示的屏幕
 * @param act 显示（隐藏）任务栏
 * @return void
 */
void MultiScreenWorker::displayAnimation(AniAction act)
{
    return displayAnimation(DOCK_SCREEN->current(), m_position, act);
}

/**
 * @brief changeDockPosition    做一个动画操作
 * @param fromScreen            上次任务栏所在的屏幕
 * @param toScreen              任务栏要移动到的屏幕
 * @param fromPos               任务栏上次的方向
 * @param toPos                 任务栏打算移动到的方向
 */
void MultiScreenWorker::changeDockPosition(QString fromScreen, QString toScreen, const Position &fromPos, const Position &toPos)
{
    if (!testState(AutoHide) || testState(ChangePositionAnimationStart) || testState(ShowAnimationStart) || testState(HideAnimationStart)) return;

    if (fromScreen == toScreen && fromPos == toPos) return;

    setStates(ChangePositionAnimationStart);

    // 更新屏幕信息
    DOCK_SCREEN->updateDockedScreen(toScreen);

    static QObject *conn(new QObject(this));
    connect(ani, &QPropertyAnimation::finished, conn, [this, fromPos, toPos, toScreen] {
        ani->disconnect(conn);

        // 如果更改了显示位置，在显示之前应该更新一下界面布局方向
        if (fromPos != toPos) {
            if(fromPos == Dock::Bottom || toPos == Dock::Bottom)
                m_parent->resize(getDockShowGeometry(DOCK_SCREEN->current(), toPos).size());
            m_parent->panel()->setPositonValue(toPos);
        }

        connect(ani, &QPropertyAnimation::finished, conn, [ this ] {
            setStates(ChangePositionAnimationStart, false);
            // conn->deleteLater();

            if(m_hideMode == HideMode::KeepShowing) requestNotifyWindowManager();
            emit requestUpdateFrontendGeometry();
            onRequestUpdateRegionMonitor();
        });

        displayAnimation(toScreen, toPos, Show);
    });

    displayAnimation(fromScreen, fromPos, Hide);
}

/**
 * @brief getValidScreen        获取一个当前任务栏可以停靠的屏幕，优先使用主屏
 * @return
 */
QString MultiScreenWorker::getValidScreen()
{
    //TODO 考虑在主屏幕名变化时自动更新，是不是就不需要手动处理了
    DOCK_SCREEN->updatePrimary(DIS_INS->primary());
    if(DIS_INS->screens().size() == 1)
        return DIS_INS->primary();

    if (DIS_INS->canDock(DIS_INS->screen(DOCK_SCREEN->current()), m_position))
        return DOCK_SCREEN->current();

    if (DIS_INS->canDock(qApp->primaryScreen(), m_position))
        return DIS_INS->primary();

    for (auto s : DIS_INS->screens()) {
        if (DIS_INS->canDock(s, m_position))
            return s->name();
    }

    return QString();
}

/**
 * @brief resetDockScreen     检查一下当前屏幕所在边缘是够允许任务栏停靠，不允许的情况需要更换下一块屏幕
 */
void MultiScreenWorker::resetDockScreen()
{
    if (ani->state() == QPropertyAnimation::Running || testState(ChangePositionAnimationStart)) return;

    DOCK_SCREEN->updateDockedScreen(getValidScreen());
    // 更新任务栏自身信息
    /**
      *注意这里要先对parent()进行setFixedSize，在分辨率切换过程中，setGeometry可能会导致其大小未改变
      */
    QRect rect = getDockShowGeometry(DOCK_SCREEN->current(), m_position);
    if(m_parent->isHidden()) rect = getDockHideGeometry(rect, m_position);
    m_parent->setGeometry(rect);

    requestNotifyWindowManager();
    emit requestUpdateFrontendGeometry();
    onRequestUpdateRegionMonitor();
}

bool MultiScreenWorker::isCursorOut(int x, int y)
{
    const int realDockSize = m_windowSize * qApp->devicePixelRatio();
    for (auto s : DIS_INS->screens()) {
        // 屏幕此位置不可停靠时,不用监听这块区域
        if (!DIS_INS->canDock(s, m_position))
            continue;

        QRect screenRect = s->geometry();
        screenRect.setSize(screenRect.size() * s->devicePixelRatio());

        if (m_position == Bottom) {
            // 如果任务栏在底部
            if (x < screenRect.x() || x > (screenRect.x() + screenRect.width()))
                continue;

            return (y < (screenRect.y() + screenRect.height() - realDockSize) || y > (screenRect.y() + screenRect.height()));
        }

        if (m_position == Left) {
            // 如果任务栏在左侧
            if (y < screenRect.y() || y > (screenRect.y() + screenRect.height()))
                continue;

            return (x > (screenRect.x() + realDockSize) || x < screenRect.x());
        }

        if (m_position == Right) {
            // 如果在任务栏右侧
            if (y < screenRect.y() || y > (screenRect.y() + screenRect.height()))
                continue;

            return (x < (screenRect.x() + screenRect.width() - realDockSize) || x > (screenRect.x() + screenRect.width()));
        }
    }

    return false;
}

// 鼠标在任务栏之外移动时,任务栏该响应隐藏时需要隐藏
void MultiScreenWorker::onExtralRegionMonitorChanged(int x, int y, const QString &key)
{
    // TODO 后续可以考虑去掉这部分的处理，不用一直监听外部区域的移动，xeventmonitor有一个CursorInto和CursorOut的信号，使用这个也可以替代，但要做好测试工作
    Q_UNUSED(x);
    Q_UNUSED(y);

    if (m_extralRegisterKey != key || testState(MousePress) || testState(ChangePositionAnimationStart))
        return;

    // FIXME:每次都要重置一下，是因为qt中的QScreen类缺少nameChanged信号，后面会给上游提交patch修复
    DOCK_SCREEN->updateDockedScreen(getValidScreen());

    // 鼠标移动到任务栏界面之外，停止计时器（延时2秒改变任务栏所在屏幕）
    m_delayWakeTimer->stop();

    if (m_hideMode == HideMode::KeepShowing || m_hideState == HideState::Show)
        displayAnimation(AniAction::Show);
    else if (m_hideState == HideState::Hide)
        displayAnimation(AniAction::Hide);
}

/**
 * @brief checkDaemonXEventMonitorService
 * 避免com.deepin.api.XEventMonitor服务比dock晚启动，导致dock启动后的状态错误
 */
void MultiScreenWorker::checkXEventMonitorService()
{
    const QString serverName = "org.deepin.dde.XEventMonitor1";

    auto connectionInit = [ this, serverName ] {
        m_eventInter = new XEventMonitorInter(serverName, "/org/deepin/dde/XEventMonitor1", QDBusConnection::sessionBus());
        m_extralEventInter = new XEventMonitorInter(serverName, "/org/deepin/dde/XEventMonitor1", QDBusConnection::sessionBus());

        connect(m_eventInter, &XEventMonitorInter::CursorMove, this, [this](int x, int y, const QString &key){
            if (m_registerKey == key && !testState(MousePress) && !testState(ChangePositionAnimationStart) && !testState(ShowAnimationStart) && !testState(HideAnimationStart)) {

                QString toScreen;

                /**
                 * 坐标位于当前屏幕边缘时,当做屏幕内移动处理(防止鼠标移动到边缘时不唤醒任务栏)
                 * 使用screenAtByScaled获取屏幕名时,实际上获取的不一定是当前屏幕
                 * 举例:点(100,100)不在(0,0,100,100)的屏幕上
                 */
                if (onScreenEdge(DOCK_SCREEN->current(), QPoint(x, y)))
                    toScreen = DOCK_SCREEN->current();
                else if (QScreen *screen = Utils::screenAtByScaled(QPoint(x, y)))
                    toScreen = screen->name();

                if(toScreen.isEmpty()) return;

                // 任务栏显示状态，但需要切换屏幕
                if (toScreen != DOCK_SCREEN->current()) {
                    if(!m_delayScreen.isEmpty() && m_delayScreen != toScreen) m_delayWakeTimer->stop();
                    if (!m_delayWakeTimer->isActive()) {
                        m_delayScreen = toScreen;
                        m_delayWakeTimer->start();
                    }
                } else if (m_parent->isHidden())
                    displayAnimation(AniAction::Show);
            }
        });
        connect(m_eventInter, &XEventMonitorInter::ButtonPress, this, [ this ] { setStates(MousePress, true); });
        connect(m_eventInter, &XEventMonitorInter::ButtonRelease, this, [ this ] { setStates(MousePress, false); });

        connect(m_extralEventInter, &XEventMonitorInter::CursorOut, this, [this](int x, int y, const QString &key){
            if (m_extralRegisterKey == key && testState(AutoHide) && !testState(MousePress) && isCursorOut(x, y))
            {
                if (testState(ChangePositionAnimationStart) || testState(ShowAnimationStart) || testState(HideAnimationStart)) {
                    // 在OUT后如果检测到当前的动画正在进行，在out后延迟500毫秒等动画结束再执行移出动画
                    QTimer::singleShot(500, this, [ = ] {
                        onExtralRegionMonitorChanged(x, y, key);
                    });
                } else
                    onExtralRegionMonitorChanged(x, y, key);
            }
        });

        // 只需要在屏幕信息变化的时候更新，其他时间不需要更新
        onRequestUpdateRegionMonitor();
    };

    QDBusConnectionInterface *ifc = QDBusConnection::sessionBus().interface();
    if (!ifc->isServiceRegistered(serverName)) {
        connect(ifc, &QDBusConnectionInterface::serviceOwnerChanged, this, [ this, serverName, connectionInit, ifc ](const QString & name, const QString & oldOwner, const QString & newOwner) {
            Q_UNUSED(oldOwner)
            if (name == serverName && !newOwner.isEmpty()) {
                connectionInit();
                disconnect(ifc);
            }
        });
    } else
        connectionInit();
}

QRect MultiScreenWorker::getDockShowGeometry() {
    return getDockShowGeometry(DOCK_SCREEN->current(), m_position);
}

/**
 * @brief 获取任务栏显示时的参数。目前多屏情况下缩放保持一致，如果后续缩放规则修改，这里需要重新调整
 *
 * @param screenName    当前屏幕名字
 * @param pos           任务栏位置
 * @param displaymode   任务栏显示模式
 * @param withoutScale  是否考虑缩放（true:获取的是真实值; false:获取的是前端认为的值(默认)）
 * @return QRect        任务栏参数
 */
QRect MultiScreenWorker::getDockShowGeometry(const QString &screenName, const Position &pos, bool withoutScale)
{
    const qreal ratio = withoutScale ? 1 : qApp->devicePixelRatio();
    const int dockSize = static_cast<int>( m_windowSize * (withoutScale ? qApp->devicePixelRatio() : 1));
    const int dockCount = DockItemManager::instance()->itemCount();

    QRect rect = SharedData::instance()->getRect(screenName, pos, ratio, dockCount);
    if(rect.isEmpty())
    {
        for (auto s : DIS_INS->screens()) {
            if (s->name() == screenName) {
                // 拿到当前显示器缩放之前的分辨率
                QRect screenRect = s->handle()->geometry();

                switch (pos) {
                case Top:
                case Position::Bottom:
                    rect.setY(static_cast<int>(screenRect.y() + screenRect.height() / ratio - dockSize));
                    rect.setHeight(dockSize);
                    break;
                case Position::Left:
                    rect.setX(screenRect.x());
                    rect.setWidth(dockSize);
                    break;
                case Position::Right:
                    rect.setX(static_cast<int>(screenRect.x() + screenRect.width() / ratio - dockSize));
                    rect.setWidth(dockSize);
                    break;
                }
                updateDockRect(rect, screenRect, pos, ratio, dockSize, dockCount);
                break;
            }
        }
        SharedData::instance()->setRect(screenName, pos, ratio, dockCount, rect);
    }

    return rect;
}

/**
 * @brief 获取任务栏隐藏时的参数。目前多屏情况下缩放保持一致，如果后续缩放规则修改，这里需要重新调整
 *
 * @param screenName    当前屏幕名字
 * @param pos           任务栏位置
 * @param displaymode   任务栏显示模式
 * @param withoutScale  是否考虑缩放（true:获取的是真实值; false:获取的是前端认为的值(默认)）
 * @return QRect        任务栏参数
 */
QRect MultiScreenWorker::getDockHideGeometry(const QRect showRect, const Position &pos)
{
    QRect rect = showRect;

    switch (pos) {
    case Position::Bottom:
        rect.setY(showRect.bottom());
        break;
    case Position::Left:
        rect.setX(showRect.x()-showRect.width());
        break;
    case Position::Right:
        rect.setX(showRect.right());
        break;
    case Top:
        break;
    }
    return rect;
}

QScreen *MultiScreenWorker::screenByName(const QString &screenName)
{
    foreach (QScreen *screen, qApp->screens()) {
        if (screen->name() == screenName)
            return screen;
    }
    return nullptr;
}

bool MultiScreenWorker::onScreenEdge(const QString &screenName, const QPoint &point)
{
    QScreen *screen = screenByName(screenName);
    if (screen) {
        const QRect r { screen->geometry() };
        const QRect rect { r.topLeft(), r.size() *screen->devicePixelRatio() };

        // 除了要判断鼠标的x坐标和当前区域的位置外，还需要判断当前的坐标的y坐标是否在任务栏的区域内
        // 因为有如下场景：任务栏在左侧，双屏幕屏幕上下拼接，此时鼠标沿着最左侧x=0的位置移动到另外一个屏幕
        // 如果不判断y坐标的话，此时就认为鼠标在当前任务栏的边缘，导致任务栏在这种状况下没有跟随鼠标
        if ((rect.x() == point.x() || rect.right() == point.x()) && point.y() >= rect.top() && point.y() <= rect.bottom())
            return true;

        // 同上，不过此时屏幕是左右拼接，任务栏位于上方或者下方
        if ((rect.y() == point.y() || rect.bottom() == point.y()) && point.x() >= rect.left() && point.x() <= rect.right())
            return true;
    }

    return false;
}

const QPoint MultiScreenWorker::rawXPosition(const QPoint &scaledPos)
{
    if(auto screen = Utils::screenAtByScaled(scaledPos))
        return screen->geometry().topLeft() + (scaledPos - screen->geometry().topLeft()) * screen->devicePixelRatio();

    return scaledPos;
}

void MultiScreenWorker::updateDockRect(QRect &dockRect, QRect screenRect, Position position, qreal ratio, int dockSize, int count)
{
    const int splitterWidth = (DockItemManager::instance()->hasWindowItem() ? 2 : 1) * (MODE_PADDING + SPLITER_SIZE);
    dockSize = dockSize-2;
    int length;

    switch (position)
    {
    case Top:
    case Bottom:
            length = screenRect.width() / ratio * .95;
            if ((dockSize + MODE_PADDING ) * count + splitterWidth > length)
            {
                dockRect.setWidth(length);
                setWindowSize(round((length - splitterWidth) / count - MODE_PADDING) + 2);
                dockRect.setHeight(m_windowSize);
            }
            else
                dockRect.setWidth((dockSize + MODE_PADDING) * count + splitterWidth);

            dockRect.moveLeft(screenRect.left() + (screenRect.width() / ratio - dockRect.width()) / 2);
        break;
    case Left:
    case Right:
            length = screenRect.height() / ratio * .95;
            if ((dockSize + MODE_PADDING) * count + splitterWidth > length)
            {
                dockRect.setHeight(length);
                setWindowSize(round((length - splitterWidth ) / count - MODE_PADDING) + 2);
                dockRect.setWidth(m_windowSize);
            }
            else
                dockRect.setHeight((dockSize + MODE_PADDING) * count + splitterWidth);

            dockRect.moveTop(screenRect.y() + (screenRect.height() / ratio - dockRect.height()) / 2);
    }
}
