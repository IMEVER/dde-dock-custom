/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     xuwenw <xuwenw@xuwenw.so>
 *
 * Maintainer:  <@xuwenw.so>
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

#ifndef MAINPANELCONTROL_H
#define MAINPANELCONTROL_H

#include "constants.h"

#include <QWidget>
#include <QBoxLayout>
#include <QLabel>

using namespace Dock;

class MainPanelDelegate
{
public:
    virtual bool appIsOnDock(const QString &appDesktop) = 0;
};

class DockItem;
class PlaceholderItem;
class AppDragWidget;
class MainPanelControl : public QWidget
{
    Q_OBJECT
public:
    MainPanelControl(QWidget *parent = 0);
    ~MainPanelControl();

    void addFixedAreaItem(int index, QWidget *wdg);
    void addAppAreaItem(int index, QWidget *wdg);
    void removeFixedAreaItem(QWidget *wdg);
    void removeAppAreaItem(QWidget *wdg);
    void setPositonValue(Position position);

    MainPanelDelegate *delegate() const;
    void setDelegate(MainPanelDelegate *delegate);

signals:
    void itemMoved(DockItem *sourceItem, DockItem *targetItem);
    void itemAdded(const QString &appDesktop, int idx);

private:
    void resizeEvent(QResizeEvent *event) override;

    void init();
    void updateAppAreaSonWidgetSize();
    void updateMainPanelLayout();

    void dragMoveEvent(QDragMoveEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dropEvent(QDropEvent *) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *e) override;

    void startDrag(DockItem *);
    DockItem *dropTargetItem(DockItem *sourceItem, QPoint point);
    void moveItem(DockItem *sourceItem, DockItem *targetItem);
    void handleDragMove(QDragMoveEvent *e, bool isFilter);
    void resizeDockIcon();
public slots:
    void insertItem(const int index, DockItem *item);
    void removeItem(DockItem *item);
    void itemUpdated(DockItem *item);
    
private:
    QBoxLayout *m_mainPanelLayout;
    QWidget *m_fixedAreaWidget;
    QWidget *m_appAreaWidget;
    QBoxLayout *m_fixedAreaLayout;
    QBoxLayout *m_appAreaLayout;
    Position m_position;
    QPointer<PlaceholderItem> m_placeholderItem;
    MainPanelDelegate *m_delegate;
    QString m_draggingMimeKey;
    AppDragWidget *m_appDragWidget;
    QPoint m_mousePressPos;
};

#endif // MAINPANELCONTROL_H
