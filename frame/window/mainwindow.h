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
#include <DBlurEffectWidget>
#include <DGuiApplicationHelper>

#include <QWidget>

DWIDGET_USE_NAMESPACE

using namespace Dock;

class MainPanelControl;
class MultiScreenWorker;

class MainWindow : public DBlurEffectWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    inline MainPanelControl *panel() { return m_mainPanel; }
    friend class MainPanelControl;

public slots:
    void launch();

    void resizeDock(int offset, bool dragging);
    QStringList GetLoadedPlugins();
    QString getPluginKey(QString pluginName);
    bool getPluginVisible(QString pluginName);
    void setPluginVisible(QString pluginName, bool visible);

protected:
    void initConnections();
    void moveEvent(QMoveEvent *event) override;

signals:
    void pluginVisibleChanged(QString pluginName, bool visible);
    void geometryChanged(QRect rect);

private:
    MainPanelControl *m_mainPanel;
    MultiScreenWorker *m_multiScreenWorker;

    DPlatformWindowHandle m_platformWindowHandle;
    TopPanelInterface *m_topPanelInterface;
};

#endif // MAINWINDOW_H
