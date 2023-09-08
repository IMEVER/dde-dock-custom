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
#include "menuworker.h"
#include "window/dockitemmanager.h"
#include "utils.h"
#include "docksettings.h"

#include <QAction>
#include <QMenu>
#include <QGSettings>
#include <DApplication>
#include <QProxyStyle>
#include <QStyleOption>
#include <QStyleOptionMenuItem>
#include <DDBusSender>
#include <QPainter>

class MenuProxyStyle: public QProxyStyle{
public:
    using QProxyStyle::QProxyStyle;
    void drawControl(ControlElement element, const QStyleOption *opt, QPainter *p, const QWidget *w) const override
    {
        if(element == QStyle::CE_MenuItem){
            if(const QStyleOptionMenuItem *o = qstyleoption_cast<const QStyleOptionMenuItem *>(opt)){
                QStyleOptionMenuItem menuitem = *o;
                if(menuitem.menuItemType == QStyleOptionMenuItem::Separator && !menuitem.text.isEmpty()) {
                    QString text = o->text;
                    menuitem.text = "";
                    QProxyStyle::drawControl(element, &menuitem, p, w);
                    int margin = 4;
                    int text_flags = Qt::AlignVCenter | Qt::AlignHCenter | Qt::TextDontClip | Qt::TextSingleLine;
                    p->drawText(menuitem.rect.adjusted(margin, margin, -margin, -margin),  text_flags, text);
                    return;
                }
            }
        }
        QProxyStyle::drawControl(element, opt, p, w);
    }
};

MenuWorker::MenuWorker(QWidget *parent) : QObject (parent)
{

}

QMenu *MenuWorker::createMenu()
{
    DockItemManager *m_itemManager = DockItemManager::instance();
    QMenu *m_settingsMenu = new QMenu();

    Dock::Position m_position = DockSettings::instance()->getPositionMode();
    QMenu *locationSubMenu = m_settingsMenu->addMenu("位置");
    QAction *m_bottomPosAct = locationSubMenu->addAction("下");
    m_bottomPosAct->setCheckable(true);
    m_bottomPosAct->setChecked(m_position == Bottom);
    QAction *m_leftPosAct = locationSubMenu->addAction("左");
    m_leftPosAct->setCheckable(true);
    m_leftPosAct->setChecked(m_position == Left);
    QAction *m_rightPosAct = locationSubMenu->addAction("右");
    m_rightPosAct->setCheckable(true);
    m_rightPosAct->setChecked(m_position == Right);

    Dock::HideMode m_hideMode = DockSettings::instance()->getHideMode();
    QMenu *statusSubMenu = m_settingsMenu->addMenu("状态");
    QAction *m_keepShownAct = statusSubMenu->addAction("一直显示");
    m_keepShownAct->setCheckable(true);
    m_keepShownAct->setChecked(m_hideMode == KeepShowing);
    QAction *m_keepHiddenAct = statusSubMenu->addAction("一直隐藏");
    m_keepHiddenAct->setCheckable(true);
    m_keepHiddenAct->setChecked(m_hideMode == KeepHidden);
    QAction *m_smartHideAct = statusSubMenu->addAction("智能隐藏");
    m_smartHideAct->setCheckable(true);
    m_smartHideAct->setChecked(m_hideMode == SmartHide);

    QMenu *animationSubMenu = m_settingsMenu->addMenu("动画");
    animationSubMenu->setStyle(new MenuProxyStyle(animationSubMenu->style()));

    QAction *m_hoverHighlightAct = animationSubMenu->addAction("悬停高亮");
    m_hoverHighlightAct->setCheckable(true);
    m_hoverHighlightAct->setChecked(m_itemManager->isEnableHoverHighlight());
    QAction *m_inoutAct = animationSubMenu->addAction("淡入淡出");
    m_inoutAct->setCheckable(true);
    m_inoutAct->setChecked(m_itemManager->isEnableInOutAnimation());
    QAction *m_hoverScaleAct = animationSubMenu->addAction("悬停缩放");
    m_hoverScaleAct->setCheckable(true);
    m_hoverScaleAct->setChecked(m_itemManager->isEnableHoverScaleAnimation());
    QAction *m_dragAct = animationSubMenu->addAction("拖动动画");
    m_dragAct->setCheckable(true);
    m_dragAct->setChecked(m_itemManager->isEnableDragAnimation());

    animationSubMenu->addSection("提醒动画");
    QActionGroup *group = new QActionGroup(this);
    QAction *swingAction = animationSubMenu->addAction("摆动");
    swingAction->setCheckable(true);
    swingAction->setChecked(m_itemManager->animationType() == DockItemManager::Swing);
    group->addAction(swingAction);
    QAction *jumpAction = animationSubMenu->addAction("跳动");
    jumpAction->setCheckable(true);
    jumpAction->setChecked(m_itemManager->animationType() == DockItemManager::Jump);
    group->addAction(jumpAction);
    QAction *noAction = animationSubMenu->addAction("无动画");
    noAction->setCheckable(true);
    noAction->setChecked(m_itemManager->animationType() == DockItemManager::No);
    group->addAction(noAction);

    MergeMode mode = m_itemManager->getDockMergeMode();
    QMenu *mergeSubMenu = m_settingsMenu->addMenu("窗口");
    QAction *m_mergeNoneAct = mergeSubMenu->addAction("从不合并");
    m_mergeNoneAct->setCheckable(true);
    m_mergeNoneAct->setChecked(mode == MergeNone);
    QAction *m_mergeDockAct = mergeSubMenu->addAction("合并Dock");
    m_mergeDockAct->setCheckable(true);
    m_mergeDockAct->setChecked(mode == MergeDock);

    // 多屏显示设置 复制模式也要显示菜单
    if (QApplication::screens().size() > 1) {
        bool onlyShowPrimary = Utils::SettingValue("com.deepin.dde.dock.mainwindow", "/com/deepin/dde/dock/mainwindow/", "onlyShowPrimary", false).toBool();

        QMenu *displaySubMenu = m_settingsMenu->addMenu("显示Dock");

        QAction *onlyPrimaryScreenModeAct = displaySubMenu->addAction(tr("只在主屏"));
        QAction *followMouseModeAct = displaySubMenu->addAction(tr("跟随鼠标"));

        onlyPrimaryScreenModeAct->setCheckable(true);
        followMouseModeAct->setCheckable(true);

        onlyPrimaryScreenModeAct->setChecked(onlyShowPrimary);
        followMouseModeAct->setChecked(!onlyShowPrimary);

        connect(onlyPrimaryScreenModeAct, &QAction::triggered, this, []{
            Utils::SettingSaveValue("com.deepin.dde.dock.mainwindow", "/com/deepin/dde/dock/mainwindow/", "onlyShowPrimary", true);
        });
        connect(followMouseModeAct, &QAction::triggered, this, []{
            Utils::SettingSaveValue("com.deepin.dde.dock.mainwindow", "/com/deepin/dde/dock/mainwindow/", "onlyShowPrimary", false);
        });
    }

    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction("设置个性化", []{
        DDBusSender().service("com.deepin.dde.ControlCenter")
        .path("/com/deepin/dde/ControlCenter")
        .interface("com.deepin.dde.ControlCenter")
        .method("ShowPage")
        .arg(QString("personalization"))
        .arg(QString("Dock"))
        .call();
    });

    connect(m_settingsMenu, &QMenu::triggered, [ = ](QAction *action){
        if(action == m_bottomPosAct)
            return DockSettings::instance()->setPositionMode(Bottom);
        if(action == m_leftPosAct)
            return DockSettings::instance()->setPositionMode(Left);
        if(action == m_rightPosAct)
            return DockSettings::instance()->setPositionMode(Right);

        if (action == m_keepShownAct)
            return DockSettings::instance()->setHideMode(KeepShowing);
        if (action == m_keepHiddenAct)
            return DockSettings::instance()->setHideMode(KeepHidden);
        if (action == m_smartHideAct)
            return DockSettings::instance()->setHideMode(SmartHide);

        if(action == m_hoverHighlightAct)
            return m_itemManager->setHoverHighlight(action->isChecked());
        if(action == m_inoutAct)
            return m_itemManager->setInOutAnimation(action->isChecked());
        if(action == m_dragAct)
            return m_itemManager->setDragAnimation(action->isChecked());
        if(action == m_hoverScaleAct)
            return m_itemManager->setHoverScaleAnimation(action->isChecked());

        if(action == m_mergeNoneAct)
            return m_itemManager->saveDockMergeMode(MergeNone);
        else if(action == m_mergeDockAct)
            return m_itemManager->saveDockMergeMode(MergeDock);

        if(action == swingAction)
            return m_itemManager->setAnimationType(DockItemManager::Swing);
        if(action == jumpAction)
            return m_itemManager->setAnimationType(DockItemManager::Jump);
        if(action == noAction)
            return m_itemManager->setAnimationType(DockItemManager::No);
    });

    return m_settingsMenu;
}

void MenuWorker::showDockSettingsMenu()
{
    // 菜单将要被打开
    emit autoHideChanged(false);

    QMenu *menu = createMenu();
    menu->exec(QCursor::pos());
    menu->deleteLater();
    // 菜单已经关闭
    emit autoHideChanged(true);
}

