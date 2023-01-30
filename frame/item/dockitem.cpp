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
#include "tipswidget.h"
#include "util/dockpopupwindow.h"
#include "../window/dockitemmanager.h"
#include "components/hoverhighlighteffect.h"

#include <QMouseEvent>
#include <QJsonObject>
#include <QCursor>

Position DockItem::DockPosition = Position::Top;
static DockPopupWindow *PopupWindow(nullptr);

DockItem::DockItem(QWidget *parent)
    : QWidget(parent)
    , m_popupTipsDelayTimer(new QTimer(this))
    , m_scale(nullptr)
{
    if (!PopupWindow) {
        PopupWindow = new DockPopupWindow(nullptr);
        PopupWindow->setShadowBlurRadius(20);
        PopupWindow->setRadius(10);
        PopupWindow->setShadowYOffset(2);
        PopupWindow->setShadowXOffset(0);
        PopupWindow->setArrowWidth(18);
        PopupWindow->setArrowHeight(10);
        PopupWindow->setObjectName("apppopup");
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

DockItem::~DockItem()
{
    hidePopup();
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
    m_popupTipsDelayTimer->start();

    if(getPlace() == DockPlace && DockItemManager::instance()->isEnableHoverScaleAnimation())
    {
        m_scale->stop();
        m_scale->setStartValue(width());
        m_scale->setEndValue(DockItemManager::instance()->itemSize());
        m_scale->start();
    }
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
    if (menuJson.isEmpty()) return;

    QJsonDocument jsonDocument = QJsonDocument::fromJson(menuJson.toLocal8Bit().data());
    if (jsonDocument.isNull()) return;

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

    auto tips = popupTips();
    if(!tips.isEmpty()) {
        static TipsWidget *content = new TipsWidget;
        content->setText(tips);
        showPopupWindow(content);
    }
}

void DockItem::showPopupWindow(QWidget *const content, const bool model)
{
    if (model)
        emit requestWindowAutoHide(false);

    switch (DockPosition) {
    case Top:
    case Bottom: PopupWindow->setArrowDirection(DockPopupWindow::ArrowBottom);  break;
    case Left:  PopupWindow->setArrowDirection(DockPopupWindow::ArrowLeft);    break;
    case Right: PopupWindow->setArrowDirection(DockPopupWindow::ArrowRight);   break;
    }
    PopupWindow->setContent(content);

    const QPoint p = popupMarkPoint();
    if (!PopupWindow->isVisible())
        QMetaObject::invokeMethod(PopupWindow, "show", Qt::QueuedConnection, Q_ARG(QPoint, p), Q_ARG(bool, model));
    else
        PopupWindow->show(p, model);

    if(model)
        connect(PopupWindow, &DockPopupWindow::accept, this, &DockItem::hidePopup, Qt::UniqueConnection);
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

QString DockItem::popupTips()
{
    return QString();
}

bool DockItem::popupVisible() const {
    return PopupWindow && PopupWindow->isVisible();
}

const QPoint DockItem::popupMarkPoint()
{
    QPoint p(topleftPoint());
    const QRect r = rect();
    switch (DockPosition) {
    case Top:
    case Bottom:
        p += QPoint(r.width() / 2, 0);
        break;
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
    PopupWindow->hide();

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
    if (!PopupWindow->model())
        hidePopup();
}
