#ifndef WINDOW_ITEM_H
#define WINDOW_ITEM_H

#include "dockitem.h"
#include "appitem.h"
#include "../taskmanager/windowinfomap.h"

class AppItem;

typedef uint32_t XWindow;

class WindowItem : public DockItem
{
    Q_OBJECT

    public:
        explicit WindowItem(AppItem *appItem, WindowInfo windowInfo, QWidget *parent=Q_NULLPTR);
        ~WindowItem();
        ItemType itemType() const override { return DockItem::Window; }
        void fetchSnapshot();
        void setActive(bool active);
        void updateTitle(const QString &);

    protected:
        void paintEvent(QPaintEvent *e) override;
        void mouseReleaseEvent(QMouseEvent *e) override;
        void wheelEvent(QWheelEvent *e) override;
        void moveEvent(QMoveEvent *e) override;
        void resizeEvent(QResizeEvent *e) override;
        void enterEvent(QEvent *e) override;
        void hideEvent(QHideEvent *e) override;
        void showEvent(QShowEvent *e) override;
        void leaveEvent(QEvent *e) override;
        void dragEnterEvent(QDragEnterEvent *e) override;
        void dropEvent(QDropEvent *e) override;

        QPixmap itemPixmap() override;
        void invokedMenuItem(const QString &itemId, const bool checked) Q_DECL_OVERRIDE;
        const QString contextMenu() const Q_DECL_OVERRIDE;

    private:
        void showPreview();
        void showHoverTips() override;
        // void closeWindow();

    private:
        AppItem *m_appItem;
        XWindow m_WId;
        WindowInfo m_windowInfo;
        bool m_isActive;
        QImage m_snapshot;
        QRectF m_snapshotSrcRect;
        QTimer *timer;
        QTimer *m_updateIconGeometryTimer;
};

#endif