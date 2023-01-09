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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../interfaces/constants.h"
#include "TopPanelInterface.h"

#include <com_deepin_api_xeventmonitor.h>
#include <DPlatformWindowHandle>
#include <DWindowManagerHelper>
#include <DBlurEffectWidget>
#include <DGuiApplicationHelper>

#include <QWidget>

DWIDGET_USE_NAMESPACE

using namespace Dock;

class DragWidget;
class MainPanelControl;
class MultiScreenWorker;
class MenuWorker;

class MainWindow : public DBlurEffectWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void updateDragCursor();
    MainPanelControl *panel();
    friend class MainPanelControl;

public slots:
    void launch();
    void resizeDock(int offset, bool dragging);
    void compositeChanged();
    QStringList GetLoadedPlugins();
    QString getPluginKey(QString pluginName);
    bool getPluginVisible(QString pluginName);
    void setPluginVisible(QString pluginName, bool visible);

protected:
    void mousePressEvent(QMouseEvent *e) override;

    void initComponents();
    void initConnections();

private slots:

    void onMainWindowSizeChanged(QPoint offset);
    void resetDragWindow();
    void themeTypeChanged(DGuiApplicationHelper::ColorType themeType);

signals:
    void pluginVisibleChanged(QString pluginName, bool visible);
    void geometryChanged(QRect rect);

private:
    bool m_launched;
    MainPanelControl *m_mainPanel;
    DWindowManagerHelper *m_wmHelper;
    MultiScreenWorker *m_multiScreenWorker;
    MenuWorker *m_menuWorker;

    DPlatformWindowHandle m_platformWindowHandle;
    DragWidget *m_dragWidget;
    QTimer *m_timer;
    TopPanelInterface *m_topPanelInterface;
};

#endif // MAINWINDOW_H
