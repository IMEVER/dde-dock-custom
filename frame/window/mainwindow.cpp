/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             zhaolong <zhaolong@uniontech.com>
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

#include "mainwindow.h"
#include "mainpanelcontrol.h"
#include "dockitemmanager.h"
#include "util/docksettings.h"
#include "util/imageutil.h"

#include <DStyle>
#include <DPlatformWindowHandle>
#include <DPlatformTheme>
#include <QDebug>
#include <QEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QX11Info>
#include <qpa/qplatformwindow.h>

#include <X11/X.h>
#include <X11/Xutil.h>

#define MAINWINDOW_MAX_SIZE       DOCK_MAX_SIZE
#define MAINWINDOW_MIN_SIZE       (40)
#define DRAG_AREA_SIZE (5)

class DragWidget : public QWidget
{
        Q_OBJECT

    private:
        bool m_dragStatus;
        QPoint m_resizePoint;

    public:
        DragWidget(QWidget *parent) : QWidget(parent)
        {
            setObjectName("DragWidget");
            m_dragStatus = false;
        }

    signals:
        void dragPointOffset(QPoint);
        void dragFinished();

    private:
        void mousePressEvent(QMouseEvent *event) override
        {
            if (event->button() == Qt::LeftButton) {
                m_resizePoint = event->globalPos();
                m_dragStatus = true;
                this->grabMouse();
            }
        }

        void mouseMoveEvent(QMouseEvent *event) override
        {
            if (m_dragStatus) {
                QPoint offset = QPoint(QCursor::pos() - m_resizePoint);
                emit dragPointOffset(offset);
            }
        }

        void mouseReleaseEvent(QMouseEvent *event) override
        {
            if (!m_dragStatus)
                return;

            m_dragStatus =  false;
            releaseMouse();
            emit dragFinished();
        }

        void enterEvent(QEvent *) override
        {
            if (QApplication::overrideCursor() && QApplication::overrideCursor()->shape() != cursor()) {
                QApplication::setOverrideCursor(cursor());
            }
        }

        void leaveEvent(QEvent *) override
        {
            QApplication::restoreOverrideCursor();
        }
};

const QPoint rawXPosition(const QPoint &scaledPos)
{
    QScreen const *screen = ImageUtil::screenAtByScaled(scaledPos);

    return screen ? screen->geometry().topLeft() +
           (scaledPos - screen->geometry().topLeft()) *
           screen->devicePixelRatio()
           : scaledPos;
}

const QPoint scaledPos(const QPoint &rawXPos)
{
    QScreen const *screen = ImageUtil::screenAt(rawXPos);

    return screen
           ? screen->geometry().topLeft() +
           (rawXPos - screen->geometry().topLeft()) / screen->devicePixelRatio()
           : rawXPos;
}

MainWindow::MainWindow(QWidget *parent) : DBlurEffectWidget(parent)
    , m_launched(false)
    , m_mainPanel(new MainPanelControl(this))
    , m_platformWindowHandle(this)
    , m_wmHelper(DWindowManagerHelper::instance())
    , m_eventInter(new XEventMonitor("com.deepin.api.XEventMonitor", "/com/deepin/api/XEventMonitor", QDBusConnection::sessionBus()))
    , m_positionUpdateTimer(new QTimer(this))
    , m_expandDelayTimer(new QTimer(this))
    , m_leaveDelayTimer(new QTimer(this))
    , m_shadowMaskOptimizeTimer(new QTimer(this))
    , m_panelShowAni(new QVariantAnimation(this))
    , m_panelHideAni(new QVariantAnimation(this))
    , m_xcbMisc(XcbMisc::instance())
    , m_dragWidget(new DragWidget(this))
    , m_mouseCauseDock(false)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setAcceptDrops(true);

    DPlatformWindowHandle::enableDXcbForWindow(this, true);
    m_platformWindowHandle.setEnableBlurWindow(true);
    m_platformWindowHandle.setTranslucentBackground(true);
    m_platformWindowHandle.setWindowRadius(3);
    m_platformWindowHandle.setShadowOffset(QPoint(0, 5));
    m_platformWindowHandle.setShadowColor(QColor(0, 0, 0, 0.3 * 255));

    m_settings = &DockSettings::Instance();
    m_xcbMisc->set_window_type(winId(), XcbMisc::Dock);
    m_size = m_settings->windowSize();
    initComponents();
    initConnections();

    resizeMainPanelWindow();

    DockItemManager::instance()->reloadAppItems();

    m_dragWidget->setMouseTracking(true);
    m_dragWidget->setFocusPolicy(Qt::NoFocus);

    m_curDockPos = m_settings->position();
    m_newDockPos = m_curDockPos;

    if ((Top == m_curDockPos) || (Bottom == m_curDockPos)) {
        m_dragWidget->setCursor(Qt::SizeVerCursor);
    } else {
        m_dragWidget->setCursor(Qt::SizeHorCursor);
    }

    connect(m_panelShowAni, &QVariantAnimation::valueChanged, [ this ](const QVariant & value) {
        if (m_panelShowAni->state() != QPropertyAnimation::Running)
            return;

        // dock的宽度或高度值
        int val = value.toInt();
        // 当前dock尺寸
        const QRectF windowRect = m_settings->windowRect(m_curDockPos, false);

        if (m_curDockPos == Dock::Top || m_curDockPos == Dock::Bottom) {
            QWidget::setFixedHeight(val);
        } else {
            QWidget::setFixedWidth(val);
        }

        switch (m_curDockPos) {
            case Dock::Bottom:
                QWidget::move(windowRect.left(), windowRect.bottom() - val);
                break;
            case Dock::Left:
                m_mainPanel->move(val - windowRect.width(), 0);
                QWidget::move(windowRect.left(), windowRect.top());
                break;
            case Dock::Right:
                QWidget::move(windowRect.right() - val, windowRect.top());
                break;
            default: break;
        }
    });

    connect(m_panelHideAni, &QVariantAnimation::valueChanged, [ this ](const QVariant & value) {
        if (m_panelHideAni->state() != QPropertyAnimation::Running)
            return;

        // dock的宽度或高度
        int val = value.toInt();
        // dock隐藏后的rect
        const QRectF windowRect = m_settings->windowRect(m_curDockPos, false);

        if (!m_mouseCauseDock) {
            switch (m_curDockPos) {
            case Dock::Bottom:
                QWidget::move(windowRect.left(), windowRect.bottom() - val);
                break;
            case Dock::Left:
                QWidget::move(val - windowRect.right(), windowRect.top());
                break;
            case Dock::Right:
                QWidget::move(windowRect.right() - val, windowRect.top());
                break;
            default: break;
            }
        }

        if (m_curDockPos == Dock::Top || m_curDockPos == Dock::Bottom) {
            QWidget::setFixedHeight(val);
        } else {
            QWidget::setFixedWidth(val);
        }

    });

    connect(m_panelShowAni, &QVariantAnimation::finished, [ this ]() {
        const QRect windowRect = m_settings->windowRect(m_curDockPos, false);
        QWidget::move(windowRect.left(), windowRect.top());
        QWidget::setFixedSize(windowRect.size());
        resizeMainPanelWindow();
    });

    connect(m_panelHideAni, &QVariantAnimation::finished, [ this ]() {
        m_mouseCauseDock = false;
        m_curDockPos = m_newDockPos;
        const QRect windowRect = m_settings->windowRect(m_curDockPos, true);

        QWidget::move(windowRect.left(), windowRect.top());
        QWidget::setFixedSize(windowRect.size());
        if(m_settings->hideMode() != HideMode::KeepShowing)
            setVisible(false);
    });

    // updateRegionMonitorWatch();
}

MainWindow::~MainWindow()
{
    delete m_xcbMisc;
}

void MainWindow::launch()
{
    setVisible(false);
    QTimer::singleShot(400, this, [&] {
        m_launched = true;
        qApp->processEvents();
        QWidget::move(m_settings->windowRect(m_curDockPos).topLeft());
        qDebug()<<"Window width: " << width() << ", height: " << height();
        setVisible(true);
        updatePanelVisible();
        resetPanelEnvironment(false);
    });
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    e->ignore();
    if (e->button() == Qt::RightButton) {
        m_settings->showDockSettingsMenu();
    }
}

void MainWindow::enterEvent(QEvent *e)
{
    QWidget::enterEvent(e);

    m_leaveDelayTimer->stop();
    if (m_settings->hideState() != Show && m_panelShowAni->state() != QPropertyAnimation::Running)
        m_expandDelayTimer->start();

    if (QApplication::overrideCursor() && QApplication::overrideCursor()->shape() != Qt::ArrowCursor)
        QApplication::restoreOverrideCursor();
}

void MainWindow::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    if (m_panelHideAni->state() == QPropertyAnimation::Running)
        return;

    m_expandDelayTimer->stop();
    m_leaveDelayTimer->start();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    QWidget::dragEnterEvent(e);

    if (m_settings->hideState() != Show) {
        m_expandDelayTimer->start();
    }
}

void MainWindow::initComponents()
{
    m_positionUpdateTimer->setSingleShot(true);
    m_positionUpdateTimer->setInterval(20);
    m_positionUpdateTimer->start();

    m_expandDelayTimer->setSingleShot(true);
    m_expandDelayTimer->setInterval(m_settings->expandTimeout());

    m_leaveDelayTimer->setSingleShot(true);
    m_leaveDelayTimer->setInterval(m_settings->narrowTimeout());

    m_shadowMaskOptimizeTimer->setSingleShot(true);
    m_shadowMaskOptimizeTimer->setInterval(100);

    m_panelShowAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_panelHideAni->setEasingCurve(QEasingCurve::InOutCubic);

    QTimer::singleShot(1, this, &MainWindow::compositeChanged);

    themeTypeChanged(DGuiApplicationHelper::instance()->themeType());
}

void MainWindow::compositeChanged()
{
    const bool composite = m_wmHelper->hasComposite();
    setEffectEnabled(composite);

// NOTE(justforlxz): On the sw platform, there is an unstable
// display position error, disable animation solution
#ifndef DISABLE_SHOW_ANIMATION
    const int duration = composite ? 300 : 0;
#else
    const int duration = 0;
#endif

    m_panelHideAni->setDuration(duration);
    m_panelShowAni->setDuration(duration);

    m_shadowMaskOptimizeTimer->start();
}

void MainWindow::internalMove(const QPoint &p)
{
    const bool isHide = m_settings->hideState() == HideState::Hide && !testAttribute(Qt::WA_UnderMouse);
    const bool pos_adjust = m_settings->hideMode() != HideMode::KeepShowing &&
                            isHide &&
                            m_panelShowAni->state() == QVariantAnimation::Stopped;
    if (!pos_adjust) {
        m_mainPanel->move(0, 0);
        return QWidget::move(p);
    }


    QPoint rp = rawXPosition(p);
    const auto ratio = devicePixelRatioF();

    const QRect &r = m_settings->primaryRawRect();
    switch (m_curDockPos) {
    case Left:      rp.setX(r.x());             break;
    case Top:       rp.setY(r.y());             break;
    case Right:     rp.setX(r.right() - 1);     break;
    case Bottom:    rp.setY(r.bottom() - 1);    break;
    }

    int hx = height() * ratio, wx = width() * ratio;
    if (m_settings->hideMode() != HideMode::KeepShowing &&
            isHide &&
            m_panelHideAni->state() == QVariantAnimation::Stopped &&
            m_panelShowAni->state() == QVariantAnimation::Stopped) {
        switch (m_curDockPos) {
        case Top:
        case Bottom:
            hx = 2;
            break;
        case Left:
        case Right:
            wx = 2;
        }
    }

    // using platform window to set real window position
   windowHandle()->handle()->setGeometry(QRect(rp.x(), rp.y(), wx, hx));
}

void MainWindow::initConnections()
{
    connect(m_settings, &DockSettings::dataChanged, m_positionUpdateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_settings, &DockSettings::positionChanged, this, &MainWindow::positionChanged);
    connect(m_settings, &DockSettings::autoHideChanged, m_leaveDelayTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_settings, &DockSettings::windowGeometryChanged, this, &MainWindow::updateGeometry, Qt::DirectConnection);
    connect(m_settings, &DockSettings::windowHideModeChanged, this, &MainWindow::setStrutPartial, Qt::QueuedConnection);
    connect(m_settings, &DockSettings::windowHideModeChanged, [this] { resetPanelEnvironment(true); });
    connect(m_settings, &DockSettings::windowHideModeChanged, m_leaveDelayTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_settings, &DockSettings::windowVisibleChanged, this, &MainWindow::updatePanelVisible, Qt::QueuedConnection);
    connect(&DockSettings::Instance(), &DockSettings::opacityChanged, this, &MainWindow::setMaskAlpha);

    connect(m_positionUpdateTimer, &QTimer::timeout, this, &MainWindow::updateGeometry, Qt::QueuedConnection);
    connect(m_expandDelayTimer, &QTimer::timeout, this, &MainWindow::expand, Qt::QueuedConnection);
    connect(m_leaveDelayTimer, &QTimer::timeout, this, &MainWindow::updatePanelVisible, Qt::QueuedConnection);
    connect(m_shadowMaskOptimizeTimer, &QTimer::timeout, this, &MainWindow::adjustShadowMask, Qt::QueuedConnection);

    connect(m_panelHideAni, &QPropertyAnimation::finished, m_shadowMaskOptimizeTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_panelShowAni, &QPropertyAnimation::finished, m_shadowMaskOptimizeTimer, static_cast<void (QTimer::*)()>(&QTimer::start));

    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &MainWindow::compositeChanged, Qt::QueuedConnection);
    connect(&m_platformWindowHandle, &DPlatformWindowHandle::frameMarginsChanged, m_shadowMaskOptimizeTimer, static_cast<void (QTimer::*)()>(&QTimer::start));

    connect(DockItemManager::instance(), &DockItemManager::itemInserted, m_mainPanel, &MainPanelControl::insertItem, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::itemRemoved, m_mainPanel, &MainPanelControl::removeItem, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::itemUpdated, m_mainPanel, &MainPanelControl::itemUpdated, Qt::DirectConnection);
    connect(DockItemManager::instance(), &DockItemManager::requestRefershWindowVisible, this, &MainWindow::updatePanelVisible, Qt::QueuedConnection);
    connect(DockItemManager::instance(), &DockItemManager::requestWindowAutoHide, m_settings, &DockSettings::setAutoHide);

    connect(m_mainPanel, &MainPanelControl::itemAdded, this, [this](const QString &appDesktop, int idx){
        m_settings->calculateWindowConfig();
    }, Qt::DirectConnection);

    connect(m_mainPanel, &MainPanelControl::itemMoved, this, [this](DockItem *sourceItem, DockItem *targetItem){
        m_settings->calculateWindowConfig();
    }, Qt::DirectConnection);

    connect(m_mainPanel, &MainPanelControl::itemMoved, DockItemManager::instance(), &DockItemManager::itemMoved, Qt::DirectConnection);
    connect(m_mainPanel, &MainPanelControl::itemAdded, DockItemManager::instance(), &DockItemManager::itemAdded, Qt::DirectConnection);
    connect(m_mainPanel, &MainPanelControl::itemCountChanged, m_settings, &DockSettings::onWindowSizeChanged);
    connect(m_mainPanel, &MainPanelControl::dirAppChanged, DockItemManager::instance(), &DockItemManager::updateDirApp);

    connect(m_dragWidget, &DragWidget::dragPointOffset, this, &MainWindow::onMainWindowSizeChanged);
    connect(m_dragWidget, &DragWidget::dragFinished, this, &MainWindow::onDragFinished);
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, &MainWindow::themeTypeChanged);
    connect(m_eventInter, &XEventMonitor::CursorMove, this, &MainWindow::onRegionMonitorChanged);
}

void MainWindow::positionChanged(const Position prevPos, const Position nextPos)
{
    m_newDockPos = nextPos;
    // paly hide animation and disable other animation
    clearStrutPartial();
    narrow(prevPos);

    // set strut
    QTimer::singleShot(400, this, [&] {
        setStrutPartial();
    });

    // reset to right environment when animation finished
    QTimer::singleShot(500, this, [&] {
        m_mainPanel->setPositonValue(m_curDockPos);
        resetPanelEnvironment(true);

        if ((Top == m_curDockPos) || (Bottom == m_curDockPos))
        {
            m_dragWidget->setCursor(Qt::SizeVerCursor);
        } else
        {
            m_dragWidget->setCursor(Qt::SizeHorCursor);
        }

        updatePanelVisible();
    });
}

void MainWindow::updateGeometry()
{
    // DockDisplayMode and DockPosition MUST be set before invoke setFixedSize method of MainPanel

    //为了防止当后端发送错误值，然后发送正确值时，任务栏没有移动在相应的位置
    //当ｑｔ没有获取到屏幕资源时候，move函数会失效。可以直接return
    if (m_settings->primaryRect().width() == 0 || m_settings->primaryRect().height() == 0) {
        return;
    }

    setStrutPartial();

    m_mainPanel->setPositonValue(m_curDockPos);

    bool isHide = m_settings->hideState() == Hide && !testAttribute(Qt::WA_UnderMouse);

    const QRect windowRect = m_settings->windowRect(m_curDockPos, isHide);

    internalMove(windowRect.topLeft());

    QWidget::move(windowRect.topLeft());
    QWidget::setFixedSize(m_settings->windowSize());

    resizeMainPanelWindow();

    m_mainPanel->update();
}

void MainWindow::clearStrutPartial()
{
    m_xcbMisc->clear_strut_partial(winId());
}

void MainWindow::setStrutPartial()
{
    // first, clear old strut partial
    clearStrutPartial();

    // reset env
    //resetPanelEnvironment(true);

    if (m_settings->hideMode() != Dock::KeepShowing)
    {
        // m_xcbMisc->set_strut_partial(winId(), XcbMisc::OrientationBottom, m_settings->screenRawHeight(), rawXPosition(m_settings->windowRect(m_curDockPos).topLeft()).x(), m_settings->currentRawRect().right());
        return;
    }

    const auto ratio = devicePixelRatioF();
    const int maxScreenHeight = m_settings->screenRawHeight();
    const int maxScreenWidth = m_settings->screenRawWidth();
    const Position side = m_curDockPos;
    const QPoint &p = rawXPosition(m_settings->windowRect(m_curDockPos).topLeft());
    const QSize &s = m_settings->windowSize();
    const QRect &primaryRawRect = m_settings->currentRawRect();

    XcbMisc::Orientation orientation = XcbMisc::OrientationTop;
    uint strut = 0;
    uint strutStart = 0;
    uint strutEnd = 0;

    QRect strutArea(0, 0, maxScreenWidth, maxScreenHeight);
    switch (side) {
    case Position::Top:
        orientation = XcbMisc::OrientationTop;
        strut = p.y() + s.height() * ratio;
        strutStart = p.x();
        strutEnd = qMin(qRound(p.x() + s.width() * ratio), primaryRawRect.right());
        strutArea.setLeft(strutStart);
        strutArea.setRight(strutEnd);
        strutArea.setBottom(strut);
        break;
    case Position::Bottom:
        orientation = XcbMisc::OrientationBottom;
        strut = maxScreenHeight - p.y();
        strutStart = p.x();
        strutEnd = qMin(qRound(p.x() + s.width() * ratio), primaryRawRect.right());
        strutArea.setLeft(strutStart);
        strutArea.setRight(strutEnd);
        strutArea.setTop(p.y());
        break;
    case Position::Left:
        orientation = XcbMisc::OrientationLeft;
        strut = p.x() + s.width() * ratio;
        strutStart = p.y();
        strutEnd = qMin(qRound(p.y() + s.height() * ratio), primaryRawRect.bottom());
        strutArea.setTop(strutStart);
        strutArea.setBottom(strutEnd);
        strutArea.setRight(strut);
        break;
    case Position::Right:
        orientation = XcbMisc::OrientationRight;
        strut = maxScreenWidth - p.x();
        strutStart = p.y();
        strutEnd = qMin(qRound(p.y() + s.height() * ratio), primaryRawRect.bottom());
        strutArea.setTop(strutStart);
        strutArea.setBottom(strutEnd);
        strutArea.setLeft(p.x());
        break;
    default:
        Q_ASSERT(false);
    }

    // pass if strut area is intersect with other screen
    //优化了文件管理的代码 会导致bug 15351 需要注释一下代码
    //    int count = 0;
    //    const QRect pr = m_settings->primaryRect();
    //    for (auto *screen : qApp->screens()) {
    //        const QRect sr = screen->geometry();
    //        if (sr == pr)
    //            continue;

    //        if (sr.intersects(strutArea))
    //            ++count;
    //    }
    //    if (count > 0) {
    //        qWarning() << "strutArea is intersects with another screen.";
    //        qWarning() << maxScreenHeight << maxScreenWidth << side << p << s;
    //        return;
    //    }

    m_xcbMisc->set_strut_partial(winId(), orientation, strut, strutStart, strutEnd);
}

void MainWindow::expand()
{
    qApp->processEvents();
    setVisible(true);

    if (m_panelHideAni->state() == QPropertyAnimation::Running) {
        m_panelHideAni->stop();
        emit m_panelHideAni->finished();
    }

    const auto showAniState = m_panelShowAni->state();

    int startValue = 2;
    int endValue = 2;

    // resetPanelEnvironment(true, false);
    if (showAniState != QPropertyAnimation::Running) {
        const QRectF windowRect = m_settings->windowRect(m_curDockPos, false);
        switch (m_curDockPos) {
            case Top:
            case Bottom:
                startValue = height();
                endValue = windowRect.height();
                break;
            case Left:
            case Right:
                startValue = width();
                endValue = windowRect.width();
                break;
        }

        if (startValue > DOCK_MAX_SIZE || endValue > DOCK_MAX_SIZE) {
            return;
        }

        if (startValue > endValue)
            return;

        m_panelShowAni->setStartValue(startValue);
        m_panelShowAni->setEndValue(endValue);
        m_panelShowAni->start();
        m_shadowMaskOptimizeTimer->start();
    }
}

void MainWindow::narrow(const Position prevPos)
{
    int startValue = 2;
    int endValue = 2;

    switch (prevPos) {
    case Top:
    case Bottom:
        startValue = height();
        endValue = 2;
        break;
    case Left:
    case Right:
        startValue = width();
        endValue = 2;
        break;
    }

    m_panelShowAni->stop();
    m_panelHideAni->setStartValue(startValue);
    m_panelHideAni->setEndValue(endValue);
    m_panelHideAni->start();
}

void MainWindow::resetPanelEnvironment(const bool visible, const bool resetPosition)
{
    if (!m_launched)
        return;

    resizeMainPanelWindow();
    updateRegionMonitorWatch();
    if (m_size != m_settings->windowSize()) {
        m_size = m_settings->windowSize();
        setStrutPartial();
    }
}

void MainWindow::updatePanelVisible()
{
    if (m_settings->hideMode() == KeepShowing) {
        if (!m_registerKey.isEmpty()) {
            m_eventInter->UnregisterArea(m_registerKey);
        }
        return expand();
    }

    if (m_registerKey.isEmpty()) {
        updateRegionMonitorWatch();
    }

    const Dock::HideState state = m_settings->hideState();

    do {
        if (state != Hide)
            break;

        if (!m_settings->autoHide())
            break;

        QRectF r(pos(), size());
        switch (m_curDockPos) {
        case Dock::Top:
            r.setY(r.y());
            break;
        case Dock::Bottom:
            r.setHeight(r.height());
            break;
        case Dock::Left:
            r.setX(r.x());
            break;
        case Dock::Right:
            r.setWidth(r.width());
            break;
        }
        if (r.contains(QCursor::pos())) {
            break;
        }

        return narrow(m_curDockPos);

    } while (false);

    return expand();
}

void MainWindow::adjustShadowMask()
{
    if (!m_launched || m_shadowMaskOptimizeTimer->isActive())
        return;

    const bool composite = m_wmHelper->hasComposite();
    int radius = 3;

    if(composite)
    {
        DPlatformTheme *theme = DGuiApplicationHelper::instance()->systemTheme();
        radius = theme->windowRadius(radius);
    }

    m_platformWindowHandle.setWindowRadius(radius);
}

void MainWindow::setEffectEnabled(const bool enabled)
{
    setMaskColor(AutoColor);
    setMaskAlpha(DockSettings::Instance().Opacity());
    m_platformWindowHandle.setBorderWidth(enabled ? 1 : 0);
}

void MainWindow::resizeMainWindow()
{
    const Position position = m_curDockPos;
    QSize size = m_settings->windowSize();
    const QRect windowRect = m_settings->windowRect(position, false);
    internalMove(windowRect.topLeft());
    resizeMainPanelWindow();
    QWidget::setFixedSize(size);

}

void MainWindow::resizeMainPanelWindow()
{
    m_mainPanel->setFixedSize(m_settings->windowSize());

    switch (m_curDockPos) {
    case Dock::Top:
        m_dragWidget->setGeometry(0, height() - DRAG_AREA_SIZE, width(), DRAG_AREA_SIZE);
        break;
    case Dock::Bottom:
        m_dragWidget->setGeometry(0, 0, width(), DRAG_AREA_SIZE);
        break;
    case Dock::Left:
        m_dragWidget->setGeometry(width() - DRAG_AREA_SIZE, 0, DRAG_AREA_SIZE, height());
        break;
    case Dock::Right:
        m_dragWidget->setGeometry(0, 0, DRAG_AREA_SIZE, height());
        break;
    default: break;
    }
}

void MainWindow::onMainWindowSizeChanged(QPoint offset)
{
    int newWidth = 0;
    if (Dock::Bottom == m_curDockPos) {
        newWidth = qBound(MAINWINDOW_MIN_SIZE, m_size.height() - offset.y(), MAINWINDOW_MAX_SIZE);
    } else if (Dock::Left == m_curDockPos) {
        newWidth = qBound(MAINWINDOW_MIN_SIZE, m_size.width() + offset.x(), MAINWINDOW_MAX_SIZE);
    } else if (Dock::Right == m_curDockPos){
        newWidth = qBound(MAINWINDOW_MIN_SIZE, m_size.width() - offset.x(), MAINWINDOW_MAX_SIZE);
    }

    m_settings->setDockWindowSize(newWidth);
    resizeMainWindow();
    // m_settings->onWindowSizeChanged();
}

void MainWindow::onDragFinished()
{
    if (m_size == m_settings->windowSize())
        return;

    m_size = m_settings->windowSize();

    if (Dock::Bottom == m_curDockPos) {
        m_settings->m_dockInter->setWindowSizeFashion(m_size.height());
        m_settings->m_dockInter->setWindowSize(m_size.height());
    } else {
        m_settings->m_dockInter->setWindowSizeFashion(m_size.width());
        m_settings->m_dockInter->setWindowSize(m_size.width());
    }

    setStrutPartial();
}

void MainWindow::themeTypeChanged(DGuiApplicationHelper::ColorType themeType)
{
    if (m_wmHelper->hasComposite()) {
        if (themeType == DGuiApplicationHelper::DarkType)
            m_platformWindowHandle.setBorderColor(QColor(0, 0, 0, 255 * 0.3));
        else
            m_platformWindowHandle.setBorderColor(QColor(QColor::Invalid));
    }
}

void MainWindow::onRegionMonitorChanged(int x, int y, const QString &key)
{
    if (m_settings->hideMode() == KeepShowing)
        return;

    if (!isVisible())
        setVisible(true);
}

void MainWindow::updateRegionMonitorWatch()
{
    if (m_settings->hideMode() == KeepShowing)
        return;

    if (!m_registerKey.isEmpty())
    {
        m_eventInter->UnregisterArea(m_registerKey);
    }

    const int flags = Motion | Button | Key;
    bool isHide = m_settings->hideState() == Hide && !testAttribute(Qt::WA_UnderMouse);
    const QRect windowRect = m_settings->windowRect(m_curDockPos, isHide);
    const qreal scale = devicePixelRatioF();
    int val = 5;
    int x, y, w, h;

    switch (m_curDockPos) {
    case Dock::Top: {
        x = windowRect.topLeft().x();
        y = windowRect.topLeft().y();
        w = m_settings->primaryRect().width();
        h = val;
    }
    break;
    case Dock::Bottom: {
        x = windowRect.bottomLeft().x();
        y = windowRect.bottomLeft().y() - val;
        w = x + windowRect.width();
        h = m_settings->primaryRect().height();
    }
    break;
    case Dock::Left: {
        x = windowRect.topLeft().x();
        y = windowRect.topLeft().y();
        w = val;
        h = y + windowRect.height();
    }
    break;
    case Dock::Right: {
        x = windowRect.topRight().x() - val;
        y = windowRect.topRight().y();
        w = m_settings->primaryRect().width();
        h = y + windowRect.height();
    }
    break;
    }
    // qDebug()<<QString("Monitor: (%1, %2, %3, %4)").arg(x).arg(y).arg(w).arg(h);
    m_registerKey = m_eventInter->RegisterArea(x * scale, y * scale, w * scale, h * scale, flags);
}

#include "mainwindow.moc"
