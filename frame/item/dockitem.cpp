/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
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

#include "dockitem.h"
#include "../window/dockitemmanager.h"

#include <QMouseEvent>
#include <QJsonObject>
#include <QCursor>

#define ITEM_MAXSIZE    100

Position DockItem::DockPosition = Position::Top;
QPointer<DockPopupWindow> DockItem::PopupWindow(nullptr);

DockItem::DockItem(QWidget *parent)
    : QWidget(parent)
    , m_hover(false)
    , m_popupShown(false)
    , m_hoverEffect(new HoverHighlightEffect(this))
    , m_popupTipsDelayTimer(new QTimer(this))
    , m_popupAdjustDelayTimer(new QTimer(this))
    , m_scaleLarger(new QVariantAnimation(this))
    , m_scaleSmaller(new QVariantAnimation(this))

{
    if (PopupWindow.isNull()) {
        DockPopupWindow *arrowRectangle = new DockPopupWindow(nullptr);
        arrowRectangle->setShadowBlurRadius(20);
        arrowRectangle->setRadius(18);
        arrowRectangle->setShadowYOffset(2);
        arrowRectangle->setShadowXOffset(0);
        arrowRectangle->setArrowWidth(18);
        arrowRectangle->setArrowHeight(10);
        arrowRectangle->setObjectName("apppopup");
        PopupWindow = arrowRectangle;
    }

    m_popupTipsDelayTimer->setInterval(500);
    m_popupTipsDelayTimer->setSingleShot(true);

    m_popupAdjustDelayTimer->setInterval(10);
    m_popupAdjustDelayTimer->setSingleShot(true);

    setGraphicsEffect(m_hoverEffect);

    connect(m_popupTipsDelayTimer, &QTimer::timeout, this, &DockItem::showHoverTips);
    connect(m_popupAdjustDelayTimer, &QTimer::timeout, this, &DockItem::updatePopupPosition, Qt::QueuedConnection);
    connect(&m_contextMenu, &QMenu::triggered, this, &DockItem::menuActionClicked);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_scaleLarger->setDuration(300);
    m_scaleLarger->setEasingCurve(QEasingCurve::Linear);
    connect(m_scaleLarger, &QVariantAnimation::valueChanged, this, [this](const QVariant &value){
        int size = value.toInt();
        setFixedSize(size, size);
    });
    // connect(m_scaleLarger, &QVariantAnimation::finished, this, [this](){
        // int size = DockSettings::Instance().dockWindowSize();
        // setFixedSize(QSize(size, size));
    // });

    m_scaleSmaller->setDuration(300);
    m_scaleSmaller->setEasingCurve(QEasingCurve::Linear);
    connect(m_scaleSmaller, &QVariantAnimation::valueChanged, this, [this](const QVariant &value){
        setFixedSize(value.toInt(), value.toInt());
    });
    // connect(m_scaleSmaller, &QVariantAnimation::finished, this, [this](){
        // int size = DockSettings::Instance().itemSize();
        // setFixedSize(QSize(size, size));
    // });

    setFixedSize(20, 20);
}

QSize DockItem::sizeHint() const
{
    int size = qMin(maximumWidth(), maximumHeight());
    if (size > ITEM_MAXSIZE)
        size = ITEM_MAXSIZE;

    return QSize(size, size);
}

DockItem::~DockItem()
{
    if (m_popupShown)
        popupWindowAccept();
}

void DockItem::setDockPosition(const Position side)
{
    DockPosition = side;
}

bool DockItem::event(QEvent *event)
{
    if (m_popupShown) {
        switch (event->type()) {
        case QEvent::Paint:
            if (!m_popupAdjustDelayTimer->isActive())
                m_popupAdjustDelayTimer->start();
            break;
        default:;
        }
    }

    return QWidget::event(event);
}

void DockItem::updatePopupPosition()
{
    Q_ASSERT(sender() == m_popupAdjustDelayTimer);

    if (!m_popupShown || !PopupWindow->model())
        return;

    if (PopupWindow->getContent() != m_lastPopupWidget.data())
        return popupWindowAccept();

    const QPoint p = popupMarkPoint();
    PopupWindow->show(p, PopupWindow->model());
}

void DockItem::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);
}

void DockItem::mousePressEvent(QMouseEvent *e)
{
    m_popupTipsDelayTimer->stop();
    hideNonModel();

    if (e->button() == Qt::RightButton) {
        if (perfectIconRect().contains(e->pos())) {
            return showContextMenu();
        }
    }

    // same as e->ignore above
    QWidget::mousePressEvent(e);
}

void DockItem::enterEvent(QEvent *e)
{
    // Remove the bottom area to prevent unintentional operation in auto-hide mode.
    if (!rect().adjusted(0, 0, width(), height() - 5).contains(mapFromGlobal(QCursor::pos()))) {
        return;
    }

    m_hover = true;

    if(DockItemManager::instance()->isEnableHoverHighlight())
        m_hoverEffect->setHighlighting(true);

    m_popupTipsDelayTimer->start();

    if(getPlace() == DockPlace && DockItemManager::instance()->isEnableHoverScaleAnimation())
    {
        if (m_scaleSmaller->state() == QVariantAnimation::Running)
        {
            m_scaleSmaller->stop();
        }

        if (m_scaleLarger->state() == QVariantAnimation::Stopped)
        {
            int originSize = DockItemManager::instance()->itemSize();
            m_scaleLarger->setStartValue(size().width());
            m_scaleLarger->setEndValue(originSize);
            m_scaleLarger->start();
        }
    }
    update();
    return QWidget::enterEvent(e);
}

void DockItem::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);

    m_hover = false;

    if(DockItemManager::instance()->isEnableHoverHighlight())
        m_hoverEffect->setHighlighting(false);

    m_popupTipsDelayTimer->stop();

    // auto hide if popup is not model window
    hideNonModel();

    if(getPlace() == DockPlace && DockItemManager::instance()->isEnableHoverScaleAnimation())
    {
        if (m_scaleLarger->state() == QVariantAnimation::Running)
        {
            m_scaleLarger->stop();
        }

        if (m_scaleSmaller->state() == QVariantAnimation::Stopped)
        {
            int originSize = DockItemManager::instance()->itemSize();
            m_scaleSmaller->setStartValue(size().width());
            m_scaleSmaller->setEndValue(originSize);
            m_scaleSmaller->start();
        }
    }
    update();
}

const QRect DockItem::perfectIconRect() const
{
    const QRect itemRect = rect();
    QRect iconRect;

    const int iconSize = std::min(itemRect.width(), itemRect.height()) * 0.8;
    iconRect.setWidth(iconSize);
    iconRect.setHeight(iconSize);

    iconRect.moveTopLeft(itemRect.center() - iconRect.center());
    return iconRect;
}

void DockItem::showContextMenu()
{
    const QString menuJson = contextMenu();
    if (menuJson.isEmpty())
        return;

    QJsonDocument jsonDocument = QJsonDocument::fromJson(menuJson.toLocal8Bit().data());
    if (jsonDocument.isNull())
        return;

    QJsonObject jsonMenu = jsonDocument.object();

    qDeleteAll(m_contextMenu.actions());

    QJsonArray jsonMenuItems = jsonMenu.value("items").toArray();
    for (auto item : jsonMenuItems) {
        QJsonObject itemObj = item.toObject();
        QAction *action = new QAction(itemObj.value("itemText").toString());
        action->setCheckable(itemObj.value("isCheckable").toBool());
        action->setChecked(itemObj.value("checked").toBool());
        action->setData(itemObj.value("itemId").toString());
        action->setEnabled(itemObj.value("isActive").toBool());
        m_contextMenu.addAction(action);
    }


    // hideNonModel();
    emit requestWindowAutoHide(false);

    m_contextMenu.exec(QCursor::pos());

    onContextMenuAccepted();
}

void DockItem::menuActionClicked(QAction *action)
{
    invokedMenuItem(action->data().toString(), true);
}

void DockItem::onContextMenuAccepted()
{
    emit requestRefreshWindowVisible();
    // if(getPlace() == DockPlace)
        emit requestWindowAutoHide(true);
}

void DockItem::showHoverTips()
{
    // another model popup window already exists
    if (PopupWindow->model())
        return;

    // if not in geometry area
    const QRect r(topleftPoint(), size());
    if (!r.contains(QCursor::pos()))
        return;

    QWidget *const content = popupTips();
    if (!content)
        return;

    showPopupWindow(content);
}

void DockItem::showPopupWindow(QWidget *const content, const bool model)
{
    PopupWindow->setRadius(10);

    m_popupShown = true;
    m_lastPopupWidget = content;

    if (model)
        emit requestWindowAutoHide(false);

    DockPopupWindow *popup = PopupWindow.data();
    QWidget *lastContent = popup->getContent();
    if (lastContent)
        lastContent->setVisible(false);

    switch (DockPosition) {
    case Top:   popup->setArrowDirection(DockPopupWindow::ArrowTop);     break;
    case Bottom: popup->setArrowDirection(DockPopupWindow::ArrowBottom);  break;
    case Left:  popup->setArrowDirection(DockPopupWindow::ArrowLeft);    break;
    case Right: popup->setArrowDirection(DockPopupWindow::ArrowRight);   break;
    }
    popup->resize(content->sizeHint());
    popup->setContent(content);

    const QPoint p = popupMarkPoint();
    if (!popup->isVisible())
        QMetaObject::invokeMethod(popup, "show", Qt::QueuedConnection, Q_ARG(QPoint, p), Q_ARG(bool, model));
    else
        popup->show(p, model);

    connect(popup, &DockPopupWindow::accept, this, &DockItem::popupWindowAccept, Qt::UniqueConnection);
}

void DockItem::popupWindowAccept()
{
    if (!PopupWindow->isVisible())
        return;

    disconnect(PopupWindow.data(), &DockPopupWindow::accept, this, &DockItem::popupWindowAccept);

    hidePopup();

    //TODO hide appdirwidget
}

void DockItem::showPopupApplet(QWidget *const applet)
{
    // another model popup window already exists
    if (PopupWindow->model())
        return;

    showPopupWindow(applet, true);
    //TODO cancel appdirwidget's hide timer
}

void DockItem::invokedMenuItem(const QString &itemId, const bool checked)
{
    Q_UNUSED(itemId)
    Q_UNUSED(checked)
}

const QString DockItem::contextMenu() const
{
    return QString();
}

QWidget *DockItem::popupTips()
{
    return nullptr;
}

const QPoint DockItem::popupMarkPoint()
{
    QPoint p(topleftPoint());
    const QRect r = rect();
    switch (DockPosition) {
    case Top: {
        p += QPoint(r.width() / 2, r.height());
        break;
    }
    case Bottom: {
        p += QPoint(r.width() / 2, 0);
        break;
    }
    case Left: {
        p += QPoint(r.width(), r.height() / 2);
        break;
    }
    case Right: {
        p += QPoint(0, r.height() / 2);
        break;
        }
    }
    return p;
}

const QPoint DockItem::topleftPoint() const
{
    QPoint p;
    const QWidget *w = this;
    do {
        p += w->pos();
        w = qobject_cast<QWidget *>(w->parent());
    } while (w);

    return p;
}

void DockItem::hidePopup()
{
    m_popupTipsDelayTimer->stop();
    m_popupAdjustDelayTimer->stop();
    m_popupShown = false;
    PopupWindow->hide();

    emit PopupWindow->accept();
    if(getPlace() == DockPlace)
        emit requestWindowAutoHide(true);
}

void DockItem::easeIn()
{
    if(m_scaleLarger->state() == QVariantAnimation::Running)
        m_scaleLarger->stop();
    if(m_scaleSmaller->state() == QVariantAnimation::Running)
        m_scaleSmaller->stop();

    m_scaleLarger->setStartValue(0);
    m_scaleLarger->setEndValue(DockItemManager::instance()->itemSize());
    m_scaleLarger->start();
}

void DockItem::easeOut()
{
    if(m_scaleLarger->state() == QVariantAnimation::Running)
        m_scaleLarger->stop();
    if(m_scaleSmaller->state() == QVariantAnimation::Running)
        m_scaleSmaller->stop();

    m_scaleSmaller->setStartValue(width());
    m_scaleSmaller->setEndValue(1);
    m_scaleSmaller->start();
}
void DockItem::hideNonModel()
{
    // auto hide if popup is not model window
    if (m_popupShown && !PopupWindow->model())
        hidePopup();
}

