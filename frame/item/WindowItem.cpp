#include "WindowItem.h"

#include "components/appsnapshot.h"
#include "components/appspreviewprovider.h"
#include "../util/docksettings.h"
#include "../util/XUtils.h"

#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <sys/shm.h>
#include <KWindowSystem>

SHMInfo *getImageDSHM(WId wId)
{
    const auto display = QX11Info::display();

    Atom atom_prop = XInternAtom(display, "_DEEPIN_DXCB_SHM_INFO", true);
    if (!atom_prop) {
        return nullptr;
    }

    Atom actual_type_return_deepin_shm;
    int actual_format_return_deepin_shm;
    unsigned long nitems_return_deepin_shm;
    unsigned long bytes_after_return_deepin_shm;
    unsigned char *prop_return_deepin_shm;

    XGetWindowProperty(display, wId, atom_prop, 0, 32 * 9, false, AnyPropertyType,
                       &actual_type_return_deepin_shm, &actual_format_return_deepin_shm, &nitems_return_deepin_shm,
                       &bytes_after_return_deepin_shm, &prop_return_deepin_shm);

    return reinterpret_cast<SHMInfo *>(prop_return_deepin_shm);
}

XImage *getImageXlib(WId wId)
{
    const auto display = QX11Info::display();
    Window unused_window;
    int unused_int;
    unsigned unused_uint, w, h;
    XGetGeometry(display, wId, &unused_window, &unused_int, &unused_int, &w, &h, &unused_uint, &unused_uint);
    return XGetImage(display, wId, 0, 0, w, h, AllPlanes, ZPixmap);
}

QRect rectRemovedShadow(WId wId, const QImage &qimage, unsigned char *prop_to_return_gtk)
{
    const auto display = QX11Info::display();

    const Atom gtk_frame_extents = XInternAtom(display, "_GTK_FRAME_EXTENTS", true);
    Atom actual_type_return_gtk;
    int actual_format_return_gtk;
    unsigned long n_items_return_gtk;
    unsigned long bytes_after_return_gtk;

    const auto r = XGetWindowProperty(display, wId, gtk_frame_extents, 0, 4, false, XA_CARDINAL,
                                      &actual_type_return_gtk, &actual_format_return_gtk, &n_items_return_gtk, &bytes_after_return_gtk, &prop_to_return_gtk);
    if (!r && prop_to_return_gtk && n_items_return_gtk == 4 && actual_format_return_gtk == 32) {
        qDebug() << "remove shadow frame...";
        const unsigned long *extents = reinterpret_cast<const unsigned long *>(prop_to_return_gtk);
        const int left = extents[0];
        const int right = extents[1];
        const int top = extents[2];
        const int bottom = extents[3];
        const int width = qimage.width();
        const int height = qimage.height();

        return QRect(left, top, width - left - right, height - top - bottom);
    } else {
        return QRect(0, 0, qimage.width(), qimage.height());
    }
}

struct SHMInfo {
    long shmid;
    long width;
    long height;
    long bytesPerLine;
    long format;

    struct Rect {
        long x;
        long y;
        long width;
        long height;
    } rect;
};

WindowItem::WindowItem(AppItem *appItem, WId wId, WindowInfo windowInfo, bool closeable, QWidget *parent) :
    DockItem(parent)
    , m_appItem(appItem)
    , m_WId(wId)
    , m_windowInfo(windowInfo)
    , m_closeable(closeable)
    , m_appPreview(nullptr)
{
    timer = new QTimer(this);
    timer->setSingleShot(false);
    timer->setInterval(10000);
    connect(timer, &QTimer::timeout, [ this ]{
        if(this->window()->isVisible())
            fetchSnapshot();
    });
    timer->start();

    update();

    QTimer::singleShot(2000, this, &WindowItem::fetchSnapshot);
}

WindowItem::~WindowItem()
{

}

void WindowItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);
    QPainter painter(this);

    const QPixmap icon = m_appItem->appIcon();
    const QRectF itemRect = rect();

    if(m_snapshot.isNull())
    {
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const auto ratio = devicePixelRatioF();
        const QRectF iconRect = icon.rect();
        const qreal iconX = itemRect.center().x() - iconRect.center().x() / ratio;
        const qreal iconY = itemRect.center().y() - iconRect.center().y() / ratio;

        painter.drawPixmap(QPoint(iconX, iconY), icon);
        return;
    }

    painter.save();

    const auto ratio = devicePixelRatioF();

    const qreal offset_x = width() / 2.0 - m_snapshotSrcRect.width() / ratio / 2 - m_snapshotSrcRect.left() / ratio;
    const qreal offset_y = height() / 2.0 - m_snapshotSrcRect.height() / ratio / 2 - m_snapshotSrcRect.top() / ratio;

    const QImage  &im = m_snapshot;
    int radius = 3;
    QBrush brush;
    brush.setTextureImage(im);
    painter.setBrush(brush);
    painter.setPen(Qt::NoPen);
    painter.scale(1 / ratio, 1 / ratio);
    painter.translate(QPoint(offset_x * ratio, offset_y * ratio));
    painter.drawRoundedRect(m_snapshotSrcRect, radius * ratio, radius * ratio);

    painter.restore();
    int smallIconSize = itemRect.width() / 3;
    painter.drawPixmap(QPoint(itemRect.width() - smallIconSize - itemRect.width() * .1, itemRect.width() - smallIconSize - itemRect.width() * .1), icon.scaled(smallIconSize, smallIconSize));
}

void WindowItem::mouseReleaseEvent(QMouseEvent *e)
{
    if(XUtils::getFocusWindowId() != m_WId)
        m_appItem->requestActivateWindow(m_WId);
    else
        KWindowSystem::minimizeWindow(m_WId);

    hidePopup();
}

void WindowItem::resizeEvent(QResizeEvent *e)
{
    update();
}

void WindowItem::enterEvent(QEvent *e)
{
    DockItem::enterEvent(e);
    timer->stop();
    fetchSnapshot();
    timer->start();
}

void WindowItem::leaveEvent(QEvent *e)
{
    DockItem::leaveEvent(e);
    if(m_appPreview && m_appPreview->isVisible())
    {
        m_appPreview->prepareHide();
    }
}

void WindowItem::dragEnterEvent(QDragEnterEvent *e)
{
    // ignore drag from panel
    if (e->source()) {
        return e->ignore();
    }

    // ignore request dock event
    QString draggingMimeKey = e->mimeData()->formats().contains("RequestDock") ? "RequestDock" : "text/plain";
    if (QMimeDatabase().mimeTypeForFile(e->mimeData()->data(draggingMimeKey)).name() == "application/x-desktop") {
        return e->ignore();
    }

    e->accept();
    hidePopup();
}

void WindowItem::dragMoveEvent(QDragMoveEvent *e)
{
    DockItem::dragMoveEvent(e);

    if (!PopupWindow->isVisible())
        showPreview();
}

void WindowItem::dropEvent(QDropEvent *e)
{
    QStringList uriList;
    for (auto uri : e->mimeData()->urls()) {
        uriList << uri.toEncoded();
    }

    qDebug() << "accept drop event with URIs: " << uriList;
    m_appItem->handleDragDrop(QX11Info::getTimestamp(), uriList);
}

void WindowItem::showHoverTips()
{
    return showPreview();
    // DockItem::showHoverTips();
}

void WindowItem::showPreview()
{
    WindowInfoMap map;
    map.insert(m_WId, m_windowInfo);
    WindowList list;
    if(m_closeable)
        list.append(m_WId);
    m_appPreview = PreviewWindow(map, list, DockPosition);

    connect(m_appPreview, &PreviewContainer::requestActivateWindow, m_appItem, &AppItem::requestActivateWindow, Qt::QueuedConnection);
    connect(m_appPreview, &PreviewContainer::requestPreviewWindow, m_appItem, &AppItem::requestPreviewWindow, Qt::QueuedConnection);
    connect(m_appPreview, &PreviewContainer::requestCancelPreviewWindow, m_appItem, &AppItem::requestCancelPreview);
    connect(m_appPreview, &PreviewContainer::requestCheckWindows, m_appItem, &AppItem::check);
    connect(m_appPreview, &PreviewContainer::requestHidePopup, this, &AppItem::hidePopup);

    connect(m_appPreview, &PreviewContainer::requestActivateWindow, [ = ] { m_appPreview = nullptr; });
    connect(m_appPreview, &PreviewContainer::requestCancelPreviewWindow, [ = ] { m_appPreview = nullptr; });
    connect(m_appPreview, &PreviewContainer::requestHidePopup, [ = ] { m_appPreview = nullptr; });

    showPopupWindow(m_appPreview, true);
}

void WindowItem::fetchSnapshot()
{
    QImage qimage;
    SHMInfo *info = nullptr;
    uchar *image_data = nullptr;
    XImage *ximage = nullptr;
    unsigned char *prop_to_return_gtk = nullptr;

    do {
        // get window image from shm(only for deepin app)
        info = getImageDSHM(m_WId);
        if (info) {
            // qInfo() << "get Image from dxcbplugin SHM...";
            //qDebug() << info->shmid << info->width << info->height << info->bytesPerLine << info->format << info->rect.x << info->rect.y << info->rect.width << info->rect.height;
            image_data = (uchar *)shmat(info->shmid, 0, 0);
            if ((qint64)image_data != -1) {
                m_snapshot = QImage(image_data, info->width, info->height, info->bytesPerLine, (QImage::Format)info->format);
                m_snapshotSrcRect = QRect(info->rect.x, info->rect.y, info->rect.width, info->rect.height);
                break;
            }
            // qInfo() << "invalid pointer of shm!";
            image_data = nullptr;
        }

        if (!image_data || qimage.isNull()) {
            // get window image from XGetImage(a little slow)
            // qInfo() << "get Image from dxcbplugin SHM failed!";
            // qInfo() << "get Image from Xlib...";
            ximage = getImageXlib(m_WId);
            if (!ximage) {
                // qInfo() << "get Image from Xlib failed! giving up...";
                m_appItem->check();
                return;
            }
            qimage = QImage((const uchar *)(ximage->data), ximage->width, ximage->height, ximage->bytes_per_line, QImage::Format_RGB32);
        }

        Q_ASSERT(!qimage.isNull());

        // remove shadow frame
        m_snapshotSrcRect = rectRemovedShadow(m_WId, qimage, prop_to_return_gtk);
        m_snapshot = qimage;
    } while (false);


    QSizeF size(rect().marginsRemoved(QMargins(rect().width() * .1,  rect().height() * .1, rect().width() * .1, rect().height() * .1)).size());
    const auto ratio = devicePixelRatioF();
    size = m_snapshotSrcRect.size().scaled(size * ratio, Qt::KeepAspectRatio);

    qreal scale = qreal(size.width()) / m_snapshotSrcRect.width();
    m_snapshot = m_snapshot.scaled(qRound(m_snapshot.width() * scale), qRound(m_snapshot.height() * scale), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    m_snapshotSrcRect.moveTop(m_snapshotSrcRect.top() * scale + 0.5);
    m_snapshotSrcRect.moveLeft(m_snapshotSrcRect.left() * scale + 0.5);
    m_snapshotSrcRect.setWidth(size.width() - 0.5);
    m_snapshotSrcRect.setHeight(size.height() - 0.5);

    m_snapshot.setDevicePixelRatio(ratio);

    if (image_data) shmdt(image_data);
    if (ximage) XDestroyImage(ximage);
    if (info) XFree(info);
    if (prop_to_return_gtk) XFree(prop_to_return_gtk);

    update();
}