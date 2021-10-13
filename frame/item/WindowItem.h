#ifndef WINDOW_ITEM_H
#define WINDOW_ITEM_H

#include "dockitem.h"
#include "appitem.h"
#include "components/previewcontainer.h"


#include <com_deepin_dde_daemon_dock_entry.h>

class AppItem;
class WindowItem : public DockItem
{
    Q_OBJECT

    public:
        explicit WindowItem(AppItem *appItem, WId wId, WindowInfo windowInfo, bool closeable, QWidget *parent=Q_NULLPTR);
        ~WindowItem();
        ItemType itemType() const override { return DockItem::Window; }
        void fetchSnapshot();

    protected:
        void paintEvent(QPaintEvent *e) override;
        void mouseReleaseEvent(QMouseEvent *e) override;
        void resizeEvent(QResizeEvent *e) override;
        void enterEvent(QEvent *e) override;
        void leaveEvent(QEvent *e) override;
        void dragEnterEvent(QDragEnterEvent *e) override;
        void dragMoveEvent(QDragMoveEvent *e) override;
        void dropEvent(QDropEvent *e) override;

    private:
        void showPreview();
        void showHoverTips() override;

    private:
        AppItem *m_appItem;
        WId m_WId;
        WindowInfo m_windowInfo;
        bool m_closeable;
        QImage m_snapshot;
        QRectF m_snapshotSrcRect;
        PreviewContainer *m_appPreview;
        QTimer *timer;
};

#endif