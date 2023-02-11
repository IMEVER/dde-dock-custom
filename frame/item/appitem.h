/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             listenerri <listenerri@gmail.com>
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

#ifndef APPITEM_H
#define APPITEM_H

#include "dockitem.h"
#include "diritem.h"
#include "WindowItem.h"

#include <DGuiApplicationHelper>
#include <com_deepin_dde_daemon_dock_entry.h>

using DockEntryInter = com::deepin::dde::daemon::dock::Entry;
class DirItem;
class WindowItem;
class AppItem : public DockItem
{
    Q_OBJECT

public:
    explicit AppItem(const QString path, QWidget *parent = nullptr);
    ~AppItem();

    const QString appId() const;
    bool isValid() const;
    void undock();
    QWidget *appDragWidget();
    void setDockInfo(Dock::Position dockPosition, const QRect &dockGeometry);

    inline ItemType itemType() const Q_DECL_OVERRIDE { return App; }
    inline QPixmap appIcon() const {
        return m_icon.isNull() ? QPixmap(":/icons/resources/application-x-desktop.svg") : m_icon;
    }
    QString getDesktopFile();
    Place getPlace() override { return m_place; }
    DirItem *getDirItem();
    void setDirItem(DirItem *dirItem);
    void removeDirItem();
    void check();
    void fetchWindowInfos();
    void removeWindowItem(bool animation = true);
    int windowCount() { return m_windowMap.size(); }
    void handleDragDrop(uint timestamp, const QStringList &uris);

signals:
    void requestActivateWindow(const WId wid) const;
    void requestPreviewWindow(const WId wid) const;
    void requestCancelPreview() const;
    void requestPresentWindows();
    void dragReady(QWidget *dragWidget);

    void windowItemInserted(WindowItem *);
    void windowItemRemoved(WindowItem *, bool animation = true);

private:
    void moveEvent(QMoveEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void leaveEvent(QEvent *e) override;

    void showHoverTips() Q_DECL_OVERRIDE;
    void invokedMenuItem(const QString &itemId, const bool checked) Q_DECL_OVERRIDE;
    const QString contextMenu() const Q_DECL_OVERRIDE;
    QString popupTips() Q_DECL_OVERRIDE;
    const QPoint popupMarkPoint() override;
    bool hasAttention() const;

    QPoint appIconPosition() const;

private slots:
    void updateWindowInfos(const WindowInfoMap &info);
    void mergeModeChanged(MergeMode mode);
    void refershIcon() Q_DECL_OVERRIDE;
    void showPreview();
    void playSwingEffect();
    void stopSwingEffect();

private:
    DockEntryInter *m_itemEntryInter;
    QVariantAnimation *m_itemAnimation;

    unsigned long m_lastclickTimes;
    WindowInfoMap m_windowInfos;

    QTimer *m_updateIconGeometryTimer;

    Place m_place = DockPlace;
    DirItem *m_dirItem;
    QMap<WId, WindowItem *> m_windowMap;

    QPixmap m_horizontalIndicator;
    QPixmap m_verticalIndicator;
    QPixmap m_activeHorizontalIndicator;
    QPixmap m_activeVerticalIndicator;
};

#endif // APPITEM_H
