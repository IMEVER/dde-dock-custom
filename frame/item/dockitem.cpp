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
#include "components/appeffect.h"

#include <QMouseEvent>
#include <QJsonObject>
#include <QCursor>

Position DockItem::DockPosition = Position::Top;
static DockPopupWindow *PopupWindow(nullptr);

DockItem::DockItem(QWidget *parent) : QWidget(parent)
    , m_popupTipsDelayTimer(new QTimer(this))
    , m_animation(nullptr)
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

    if(DockItemManager::instance()->isEnableHoverHighlight())
        setGraphicsEffect(new HoverHighlightEffect(this));

    connect(DockItemManager::instance(), &DockItemManager::hoverHighlighted, this, [this](const bool enabled){
        setGraphicsEffect(enabled ? new HoverHighlightEffect(this) : nullptr);
    });

    connect(m_popupTipsDelayTimer, &QTimer::timeout, this, &DockItem::showHoverTips);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFixedSize(DockItemManager::instance()->itemSize()-2, DockItemManager::instance()->itemSize()-2);
    connect(this, &WindowItem::inoutFinished, this, [this](bool in){
        if(in == false) QTimer::singleShot(50, this, &DockItem::deleteLater);
    });
}

DockItem::~DockItem()
{
    hidePopup();
}

void DockItem::paintEvent(QPaintEvent *e)
{
    if(m_icon.isNull() || m_animation) return;

    QPainter painter(this);

    QPixmap pixmap = m_icon.pixmap(width() *.9);

    const auto ratio = devicePixelRatioF();
    const int iconX = rect().center().x() - pixmap.rect().center().x() / ratio;
    const int iconY = rect().center().y() - pixmap.rect().center().y() / ratio;

    painter.drawPixmap(iconX, iconY, pixmap);
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
        if(m_animation)
            m_animation->setDirection(QVariantAnimation::Forward);
        else {
            m_animation = AppEffect::PopupEffect(this, (m_icon.isNull() || itemType() == Window) ? grab() : m_icon.pixmap(width() *.9), DockPosition);
            connect(m_animation, &QVariantAnimation::stateChanged, this, [this](const QVariantAnimation::State newState, const QVariantAnimation::State oldState) {
                if(newState == QVariantAnimation::Running)
                    update();
                else if(newState == QVariantAnimation::Stopped && m_animation->direction() == QVariantAnimation::Backward) {
                    m_animation = nullptr;
                    update();
                }
            });
        }
        m_animation->start();
    }
    return QWidget::enterEvent(e);
}

void DockItem::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);

    hideNonModel();
    m_popupTipsDelayTimer->stop();

    if(m_animation)
    {
        m_animation->setDirection(QVariantAnimation::Backward);
        m_animation->start();
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
    connect(m_contextMenu, &QMenu::triggered, this, [this](QAction *action){
        invokedMenuItem(action->data().toString(), action->isChecked());
    });

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

void DockItem::easeIn(bool animation)
{
    if(animation && DockItemManager::instance()->isEnableInOutAnimation()) {
        if(m_animation) m_animation->stop();
        m_animation = AppEffect::ScaleEffect(this, (m_icon.isNull() || itemType() == Window) ? grab() : m_icon.pixmap(width() *.9), DockPosition);
        connect(m_animation, &QVariantAnimation::stateChanged, this, [this](const QVariantAnimation::State newState, const QVariantAnimation::State oldState) {
            if(newState == QVariantAnimation::Running)
                update();
            else if(newState == QVariantAnimation::Stopped) {
                m_animation = nullptr;
                update();
                emit inoutFinished(true);
            }
        });
        m_animation->start();
    } else
        emit inoutFinished(true);
}

void DockItem::easeOut(bool animation)
{
    if(animation && DockItemManager::instance()->isEnableInOutAnimation()) {
        if(m_animation) m_animation->stop();
        m_animation = AppEffect::ScaleEffect(this, (m_icon.isNull() || itemType() == Window) ? grab() : m_icon.pixmap(width() *.9), DockPosition);
        m_animation->setDirection(QAbstractAnimation::Backward);
        connect(m_animation, &QVariantAnimation::stateChanged, this, [this](const QVariantAnimation::State newState, const QVariantAnimation::State oldState) {
            if(newState == QVariantAnimation::Running)
                update();
            else if(newState == QVariantAnimation::Stopped) {
                m_animation = nullptr;
                update();
                emit inoutFinished(false);
            }
        });
        m_animation->start();
    } else
        emit inoutFinished(false);
}
void DockItem::hideNonModel()
{
    // auto hide if popup is not model window
    if (!PopupWindow->model())
        hidePopup();
}
