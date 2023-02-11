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

#ifndef DOCKITEM_H
#define DOCKITEM_H

#include "../interfaces/constants.h"

#include <QFrame>
#include <QPointer>
#include <QMenu>

using namespace Dock;
class DockItem : public QWidget
{
    Q_OBJECT

public:
    enum ItemType {
        Launcher,
        App,
        Plugins,
        Placeholder,
        DirApp,
        Window
    };

    enum Place {
        DockPlace,
        DirPlace
    };

public:
    explicit DockItem(QWidget *parent = nullptr);
    ~DockItem();

    static void setDockPosition(const Position side) { DockPosition = side; }
    inline virtual ItemType itemType() const {Q_UNREACHABLE(); return App;}
    virtual Place getPlace() { return DockPlace; }

public slots:
    virtual void refershIcon() {}

    void hidePopup();
    void easeIn();
    void easeOut();

signals:
    void itemDropped(QObject *destination, const QPoint &dropPoint) const;
    void requestWindowAutoHide(const bool autoHide) const;

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *e) override;
    void enterEvent(QEvent *e) override;
    void leaveEvent(QEvent *e) override;

    const QRect perfectIconRect() const;
    virtual const QPoint popupMarkPoint() ;
    const QPoint topleftPoint() const;

    void hideNonModel();
    virtual void showPopupWindow(QWidget *const content, const bool model = false);
    virtual void showHoverTips();
    virtual void invokedMenuItem(const QString &itemId, const bool checked);
    virtual const QString contextMenu() const;
    virtual QString popupTips();
    bool popupVisible() const;

protected slots:
    void showContextMenu();
    bool isScaling() const { return m_animation; }

private:
    void menuActionClicked(QAction *action);

protected:
    QTimer *m_popupTipsDelayTimer;
    static Position DockPosition;
    QVariantAnimation *m_scale;
    QPixmap m_icon;

private:
    QVariantAnimation *m_animation;
};

#endif // DOCKITEM_H
