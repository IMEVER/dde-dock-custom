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

#include "../interfaces/constants.h"

#include <QWidget>
#include <QBoxLayout>

using namespace Dock;

class DockItem;
class PlaceholderItem;
class AppDragWidget;
class SplitterWidget;
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
    void addWindowAreaItem(int index, QWidget *wdg);
    void removeWindowAreaItem(QWidget *wdg);
    void addLastAreaItem(int index, QWidget *wdg);
    void setPositonValue(Position position);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void init();
    void updateAppAreaSonWidgetSize();
    void updateMainPanelLayout();

    void startDrag(DockItem *);
    void dropTargetItem(DockItem *sourceItem, QPoint point);
    void handleDragDrop(DockItem *sourceItem, QPoint point);
    void moveItem(DockItem *sourceItem, DockItem *targetItem);
    inline bool isHorizontal() const { return m_position == Bottom || m_position == Top; }
    void resizeDockIcon();

public slots:
    void insertItem(const int index, DockItem *item);
    void removeItem(DockItem *item, bool animation = true);
    void itemUpdated(DockItem *item);

signals:
    void itemMoved(DockItem *sourceItem, DockItem *targetItem);
    void itemAdded(const QString &appDesktop, int idx);
    void itemCountChanged();
    void dirAppChanged();
    void requestConttextMenu();
    void requestResizeDockSize(int offset, bool dragging);

private:
    QWidget *m_appAreaWidget;
    QBoxLayout *m_fixedAreaLayout;
    QBoxLayout *m_appAreaLayout;
    QBoxLayout *m_windowAreaLayout;
    QBoxLayout *m_lastAreaLayout;

    SplitterWidget *m_splitter;
    SplitterWidget *m_splitter2;

    Position m_position;
    QPointer<PlaceholderItem> m_placeholderItem;
    QString m_draggingMimeKey;
    AppDragWidget *m_appDragWidget;

    friend class SplitterWidget;
};

#endif // MAINPANELCONTROL_H
