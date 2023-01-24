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
#include "components/hoverhighlighteffect.h"

#include <QMouseEvent>
#include <QJsonObject>
#include <QCursor>

#define ITEM_MAXSIZE    100

Position DockItem::DockPosition = Position::Top;
QPointer<DockPopupWindow> DockItem::PopupWindow(nullptr);

DockItem::DockItem(QWidget *parent)
    : QWidget(parent)
    , m_popupShown(false)
    , m_popupTipsDelayTimer(new QTimer(this))
    , m_scale(nullptr)
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

    if(DockItemManager::instance()->isEnableHoverHighlight()) {
        HoverHighlightEffect *m_hoverEffect = new HoverHighlightEffect(this);
        setGraphicsEffect(m_hoverEffect);
    }
    connect(DockItemManager::instance(), &DockItemManager::hoverHighlighted, this, [this](const bool enabled){
        HoverHighlightEffect *m_hoverEffect = nullptr;
        if(enabled)
            m_hoverEffect = new HoverHighlightEffect(this);

        setGraphicsEffect(m_hoverEffect);
    });

    connect(m_popupTipsDelayTimer, &QTimer::timeout, this, &DockItem::showHoverTips);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto initScaleAnimation = [this](const bool enabled=false) {
        Q_UNUSED(enabled);
        const bool enabledScaleAnimation = DockItemManager::instance()->isEnableHoverScaleAnimation();
        const bool enabledInoutAnimation = DockItemManager::instance()->isEnableInOutAnimation();
        if((enabledScaleAnimation || enabledInoutAnimation) && !m_scale) {
            m_scale = new QVariantAnimation(this);
            m_scale->setDuration(300);
            m_scale->setEasingCurve(QEasingCurve::Linear);
            connect(m_scale, &QVariantAnimation::valueChanged, this, [this](const QVariant &value){
                int size = value.toInt();
                setFixedSize(size, size);
            });
            // connect(m_scaleLarger, &QVariantAnimation::finished, this, [this](){
                // int size = DockSettings::Instance().dockWindowSize();
                // setFixedSize(QSize(size, size));
            // });
        } else if (!enabledScaleAnimation && !enabledInoutAnimation && m_scale) {
            m_scale->stop();
            m_scale->deleteLater();
            m_scale = nullptr;
        }
    };
    connect(DockItemManager::instance(), &DockItemManager::hoverScaleChanged, this, initScaleAnimation);
    connect(DockItemManager::instance(), &DockItemManager::inoutChanged, this, initScaleAnimation);
    initScaleAnimation();

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

void DockItem::mousePressEvent(QMouseEvent *e)
{
    m_popupTipsDelayTimer->stop();
    hideNonModel();

    if (e->button() == Qt::RightButton && perfectIconRect().contains(e->pos()))
        return showContextMenu();

    // same as e->ignore above
    QWidget::mousePressEvent(e);
}

void DockItem::enterEvent(QEvent *e)
{
    // Remove the bottom area to prevent unintentional operation in auto-hide mode.
    if (!rect().adjusted(0, 0, width(), height() - 5).contains(mapFromGlobal(QCursor::pos())))
        return;

    m_popupTipsDelayTimer->start();

    if(getPlace() == DockPlace && DockItemManager::instance()->isEnableHoverScaleAnimation())
    {
        m_scale->stop();

        m_scale->setStartValue(size().width());
        m_scale->setEndValue(DockItemManager::instance()->itemSize());
        m_scale->start();
    }
    // update();
    return QWidget::enterEvent(e);
}

void DockItem::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);

    m_popupTipsDelayTimer->stop();

    // auto hide if popup is not model window
    hideNonModel();

    if(getPlace() == DockPlace && DockItemManager::instance()->isEnableHoverScaleAnimation())
    {
        m_scale->stop();

        m_scale->setStartValue(size().width());
        m_scale->setEndValue(int(DockItemManager::instance()->itemSize()*.8));
        m_scale->start();
    }
    // update();
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

    QMenu *m_contextMenu = new QMenu(this);
    connect(m_contextMenu, &QMenu::triggered, this, &DockItem::menuActionClicked);

    QJsonArray jsonMenuItems = jsonMenu.value("items").toArray();
    for (auto item : jsonMenuItems) {
        QJsonObject itemObj = item.toObject();
        QAction *action = new QAction(itemObj.value("itemText").toString(), m_contextMenu);
        action->setCheckable(itemObj.value("isCheckable").toBool());
        action->setChecked(itemObj.value("checked").toBool());
        action->setData(itemObj.value("itemId").toString());
        action->setEnabled(itemObj.value("isActive").toBool());
        m_contextMenu->addAction(action);
    }

    // hideNonModel();
    emit requestWindowAutoHide(false);

    m_contextMenu->exec(QCursor::pos());
    m_contextMenu->deleteLater();

    emit requestRefreshWindowVisible();
    // if(getPlace() == DockPlace)
    emit requestWindowAutoHide(true);
}

void DockItem::menuActionClicked(QAction *action)
{
    invokedMenuItem(action->data().toString(), true);
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

    if(auto content = popupTips())
        showPopupWindow(content);
}

void DockItem::showPopupWindow(QWidget *const content, const bool model)
{
    PopupWindow->setRadius(10);

    m_popupShown = true;

    if (model)
        emit requestWindowAutoHide(false);

    DockPopupWindow *popup = PopupWindow.data();
    if(QWidget *lastContent = popup->getContent())
        lastContent->setVisible(false);

    switch (DockPosition) {
    case Top:
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
    m_popupShown = false;
    PopupWindow->hide();

    emit PopupWindow->accept();
    if(getPlace() == DockPlace)
        emit requestWindowAutoHide(true);
}

void DockItem::easeIn()
{
    m_scale->stop();

    int w = DockItemManager::instance()->isEnableHoverScaleAnimation() ? int(DockItemManager::instance()->itemSize() * .8) : DockItemManager::instance()->itemSize()-2;
    m_scale->setStartValue(1);
    m_scale->setEndValue(w);
    m_scale->start();
}

void DockItem::easeOut()
{
    m_scale->stop();

    m_scale->setStartValue(size().width());
    m_scale->setEndValue(1);
    m_scale->start();
}
void DockItem::hideNonModel()
{
    // auto hide if popup is not model window
    if (m_popupShown && !PopupWindow->model())
        hidePopup();
}
