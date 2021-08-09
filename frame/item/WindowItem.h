#ifndef WINDOW_ITEM_H
#define WINDOW_ITEM_H

#include "dockitem.h"
#include <com_deepin_dde_daemon_dock_entry.h>

class WindowItem : public DockItem
{
    Q_OBJECT

    public:
        explicit WindowItem(WId wId, WindowInfo windowInfo, QWidget *parent=Q_NULLPTR);
        ~WindowItem();
        ItemType itemType() const override { return DockItem::Window; }

    private:
        void paintEvent(QPaintEvent *e) override;
        QPoint windowIconPosition();
        void refreshIcon();

    private:
        WId m_WId;
        WindowInfo m_windowInfo;
        QPixmap m_windowIcon;
};

#endif