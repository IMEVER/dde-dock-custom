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

#ifndef DOCKPOPUPWINDOW_H
#define DOCKPOPUPWINDOW_H

#include <darrowrectangle.h>
#include <DWindowManagerHelper>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE
class XEventMonitorInter;

class DockPopupWindow : public Dtk::Widget::DArrowRectangle
{
    Q_OBJECT

public:
    explicit DockPopupWindow(QWidget *parent = 0);
    ~DockPopupWindow();

    bool model() const;

    void setContent(QWidget *content);
    void setExtendWidget(QWidget *widget);
    QWidget *extendWidget() const;
    using Dtk::Widget::DArrowRectangle::show;

public slots:
    void show(const QPoint &pos, const bool model = false);
    void hide();

signals:
    void requestWindowAutoHide(const bool autoHide) const;

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onButtonPress(int type, int x, int y, const QString &key);

private:
    bool m_enableMouseRelease;
    bool m_model;

    XEventMonitorInter *m_eventInter;
    QString m_registerKey;
    QWidget *m_extendWidget;
    QWidget *m_parent;
};

#endif // DOCKPOPUPWINDOW_H
