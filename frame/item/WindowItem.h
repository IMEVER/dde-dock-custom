#ifndef WINDOW_ITEM_H
#define WINDOW_ITEM_H

#include "dockitem.h"
#include "appitem.h"
#include "../taskmanager/windowinfomap.h"

class AppItem;
class WindowItem : public DockItem
{
    Q_OBJECT

    public:
        explicit WindowItem(AppItem *appItem, WId wId, WindowInfo windowInfoBase, bool closeable, QWidget *parent=Q_NULLPTR);
        ~WindowItem();
        ItemType itemType() const override { return DockItem::Window; }
        void fetchSnapshot();

    protected:
        void paintEvent(QPaintEvent *e) override;
        void mouseReleaseEvent(QMouseEvent *e) override;
        void wheelEvent(QWheelEvent *e) override;
        void moveEvent(QMoveEvent *e) override;
        void resizeEvent(QResizeEvent *e) override;
        void enterEvent(QEvent *e) override;
        void leaveEvent(QEvent *e) override;
        void dragEnterEvent(QDragEnterEvent *e) override;
        void dragMoveEvent(QDragMoveEvent *e) override;
        void dropEvent(QDropEvent *e) override;

        void invokedMenuItem(const QString &itemId, const bool checked) Q_DECL_OVERRIDE;
        const QString contextMenu() const Q_DECL_OVERRIDE;

    private:
        void showPreview();
        void showHoverTips() override;
        void closeWindow();

    private:
        AppItem *m_appItem;
        WId m_WId;
        WindowInfo m_windowInfo;
        bool m_closeable;
        QImage m_snapshot;
        QRectF m_snapshotSrcRect;
        QTimer *timer;
        QTimer *m_updateIconGeometryTimer;
};

#endif