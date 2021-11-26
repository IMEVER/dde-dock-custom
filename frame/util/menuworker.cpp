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

#include <QAction>
#include <QMenu>
#include <QGSettings>

#include <DApplication>

MenuWorker::MenuWorker(DBusDock *dockInter,QWidget *parent)
    : QObject (parent)
    , m_dockInter(dockInter)
    , m_autoHide(true)
{

}

QMenu *MenuWorker::createMenu()
{
    DockItemManager *m_itemManager = DockItemManager::instance();
    m_autoHide = false;
    QMenu *m_settingsMenu = new QMenu();

    Dock::Position m_position = Dock::Position(m_dockInter->position());
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

    Dock::HideMode m_hideMode = Dock::HideMode(m_dockInter->hideMode());
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

    MergeMode mode = m_itemManager->getDockMergeMode();
    QMenu *mergeSubMenu = m_settingsMenu->addMenu("窗口");
    QAction *m_mergeNoneAct = mergeSubMenu->addAction("从不合并");
    m_mergeNoneAct->setCheckable(true);
    m_mergeNoneAct->setChecked(mode == MergeNone);
    QAction *m_mergeDockAct = mergeSubMenu->addAction("合并Dock");
    m_mergeDockAct->setCheckable(true);
    m_mergeDockAct->setChecked(mode == MergeDock);
    QAction *m_mergeAllAct = mergeSubMenu->addAction("合并所有");
    m_mergeAllAct->setCheckable(true);
    m_mergeAllAct->setChecked(mode == MergeAll);

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

    connect(m_settingsMenu, &QMenu::triggered, [ = ](QAction *action){
        if(action == m_bottomPosAct)
            return m_dockInter->setPosition(Bottom);
        if(action == m_leftPosAct)
            return m_dockInter->setPosition(Left);
        if(action == m_rightPosAct)
            return m_dockInter->setPosition(Right);

        if (action == m_keepShownAct)
            return m_dockInter->setHideMode(KeepShowing);
        if (action == m_keepHiddenAct)
            return m_dockInter->setHideMode(KeepHidden);
        if (action == m_smartHideAct)
            return m_dockInter->setHideMode(SmartHide);

        if(action == m_hoverHighlightAct)
            return m_itemManager->setHoverHighlight(action->isChecked());
        if(action == m_inoutAct)
            return m_itemManager->setInOutAnimation(action->isChecked());
        if(action == m_dragAct)
            return m_itemManager->setDragAnimation(action->isChecked());
        if(action == m_hoverScaleAct)
        {
            m_itemManager->setHoverScaleAnimation(action->isChecked());
            return updatePanelGeometry();
        }

        if(action == m_mergeNoneAct)
            return m_itemManager->saveDockMergeMode(MergeNone);
        else if(action == m_mergeAllAct)
            return m_itemManager->saveDockMergeMode(MergeAll);
        else if(action == m_mergeDockAct)
            return m_itemManager->saveDockMergeMode(MergeDock);
    });

    return m_settingsMenu;
}

void MenuWorker::showDockSettingsMenu()
{
    // 菜单将要被打开
    setAutoHide(false);

    QMenu *menu = createMenu();
    menu->exec(QCursor::pos());
    menu->deleteLater();
    menu = nullptr;
    // 菜单已经关闭
    setAutoHide(true);
}

void MenuWorker::setAutoHide(const bool autoHide)
{
    if (m_autoHide == autoHide)
        return;

    m_autoHide = autoHide;
    emit autoHideChanged(m_autoHide);
}
