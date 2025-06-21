#include "WindowItem.h"

#include "components/previewcontainer.h"
#include "util/XUtils.h"
#include "xcb/xcb_misc.h"

#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <KX11Extras>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QPainter>
#include <QPainterPath>

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
        const unsigned long *extents = reinterpret_cast<const unsigned long *>(prop_to_return_gtk);
        const int left = extents[0];
        const int right = extents[1];
        const int top = extents[2];
        const int bottom = extents[3];
        const int width = qimage.width();
        const int height = qimage.height();

        return QRect(left, top, width - left - right, height - top - bottom);
    } else
        return QRect(0, 0, qimage.width(), qimage.height());
}

WindowItem::WindowItem(AppItem *appItem, WindowInfo windowInfo, QWidget *parent) :
    DockItem(parent)
    , m_appItem(appItem)
    , m_WId(windowInfo.wid)
    , m_windowInfo(windowInfo)
    , m_isActive(false)
{
    m_icon = m_appItem->appIcon();

    timer = new QTimer(this);
    timer->setSingleShot(false);
    timer->setInterval(10000);
    connect(timer, &QTimer::timeout, this, &WindowItem::fetchSnapshot);

    m_updateIconGeometryTimer = new QTimer(this);
    m_updateIconGeometryTimer->setInterval(500);
    m_updateIconGeometryTimer->setSingleShot(true);
    connect(m_updateIconGeometryTimer, &QTimer::timeout, this, [this]{
        const QRect r(mapToGlobal(QPoint(0, 0)), mapToGlobal(QPoint(width(), height())));
        XcbMisc::instance()->set_window_icon_geometry(m_WId, r);
    });

    connect(m_appItem, &AppItem::windowActiveChanged, this, [this](bool active, WId wid) {
        setActive(active &&  wid == m_WId);
    });

    QTimer::singleShot(500, this, &WindowItem::fetchSnapshot);
}

WindowItem::~WindowItem() {}

void WindowItem::setActive(bool active) {
    if(m_isActive != active) {
        m_isActive = active;
        update(indicatorRect());
    }
}

void WindowItem::updateTitle(const QString &title) {
    m_windowInfo.title = title;
}

void WindowItem::paintEvent(QPaintEvent *e)
{
    if(isScaling()) return;

    if(m_snapshot.isNull())
        return DockItem::paintEvent(e);

    QPainter painter(this);

    QRect indicator = indicatorRect();
    if (m_isActive and e->rect().contains(indicator))
    {
        QRadialGradient radialGrad(indicator.center(), qMax(indicator.width(), indicator.height())/2);
        // radialGrad.setColorAt(0, QColor("#0b74dd"));
        // radialGrad.setColorAt(.8, QColor("#70209cff"));
        radialGrad.setColorAt(0, palette().highlight().color());
        radialGrad.setColorAt(.5, palette().highlight().color());
        radialGrad.setColorAt(1, Qt::transparent);
        painter.fillRect(indicator, QBrush(radialGrad));
    }

    if(e->rect() == indicator) return;

    if(e->rect() != QRect(width() - 20, 0, 20, 20)) {

    const auto ratio = devicePixelRatioF();
    const qreal offset_x = width() / 2.0 - m_snapshotSrcRect.width() / ratio / 2 - m_snapshotSrcRect.left() / ratio;
    const qreal offset_y = height() / 2.0 - m_snapshotSrcRect.height() / ratio / 2 - m_snapshotSrcRect.top() / ratio;

    int radius = 3;
    QBrush brush;
    brush.setTextureImage(m_snapshot);
    painter.save();
    painter.setBrush(brush);
    painter.setPen(Qt::NoPen);
    painter.scale(1 / ratio, 1 / ratio);
    painter.translate(QPoint(offset_x * ratio, offset_y * ratio));
    painter.drawRoundedRect(m_snapshotSrcRect, radius * ratio, radius * ratio);

    painter.restore();
    const QRectF itemRect = rect();
    int smallIconSize = itemRect.width() / 3;
    painter.drawPixmap(QPoint(itemRect.width() - smallIconSize - itemRect.width() * .1, itemRect.width() - smallIconSize - itemRect.width() * .1), m_icon.pixmap(smallIconSize));
    }

    if(m_appItem->hasMpris() and e->rect().contains(QRect(width() - 20, 0, 20, 20))) {
        painter.translate(width() - 20, 0);

        QPainterPath path;
        path.addRect(0, 6, 4, 8);
        path.addPolygon(QPolygon({QPoint(4, 6), QPoint(10, 0), QPoint(10, 20), QPoint(4, 14)}));

        path.moveTo(12, 6);
        path.cubicTo(QPoint(14, 10), QPoint(16, 10), QPoint(12, 14));

        path.moveTo(14, 2);
        path.cubicTo(QPoint(20, 10), QPoint(20, 10), QPoint(14, 18));

        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().highlight());
        painter.drawPath(path);

        painter.translate(QPoint(0, 0));
    }
}

void WindowItem::mouseReleaseEvent(QMouseEvent *e)
{
    if(e->button() == Qt::LeftButton)
        m_appItem->requestActivateWindow(m_WId);
    else if(e->button() == Qt::MiddleButton)
        m_appItem->close(m_WId);
}

void WindowItem::wheelEvent(QWheelEvent *e)
{
    DockItem::wheelEvent(e);

    if (qAbs(e->angleDelta().y()) > 20)
        emit m_appItem->requestPresentWindows();
}

void WindowItem::moveEvent(QMoveEvent *e)
{
    DockItem::moveEvent(e);
    m_updateIconGeometryTimer->start();
}

void WindowItem::resizeEvent(QResizeEvent *e)
{
    // fetchSnapshot();
    m_updateIconGeometryTimer->start();
}

void WindowItem::enterEvent(QEvent *e)
{
    DockItem::enterEvent(e);
    timer->start();
    fetchSnapshot();
}

void WindowItem::leaveEvent(QEvent *e)
{
    DockItem::leaveEvent(e);
    PreviewContainer *m_appPreview = PreviewContainer::instance();
    if(m_appPreview->isVisible())
        m_appPreview->prepareHide();
}

void WindowItem::hideEvent(QHideEvent *e) {
    timer->stop();
    DockItem::hideEvent(e);
}

void WindowItem::showEvent(QShowEvent *e) {
    timer->start();
    DockItem::showEvent(e);
}

void WindowItem::dragEnterEvent(QDragEnterEvent *e)
{
    // ignore drag from panel
    if (e->source()) return e->ignore();

    // ignore request dock event
    QString draggingMimeKey = e->mimeData()->formats().contains("RequestDock") ? "RequestDock" : "text/plain";
    if (QMimeDatabase().mimeTypeForFile(e->mimeData()->data(draggingMimeKey)).name() == "application/x-desktop")
        return e->ignore();

    e->accept();
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

QPixmap WindowItem::itemPixmap() {
    return grab();
}

// void WindowItem::closeWindow() {
//     const auto display = QX11Info::display();

//     XEvent e;

//     memset(&e, 0, sizeof(e));
//     e.xclient.type = ClientMessage;
//     e.xclient.window = m_WId;
//     e.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", true);
//     e.xclient.format = 32;
//     e.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", false);
//     e.xclient.data.l[1] = CurrentTime;

//     XSendEvent(display, m_WId, false, NoEventMask, &e);
//     XFlush(display);
// }

void WindowItem::showHoverTips()
{
    showPreview();
}

void WindowItem::showPreview()
{
    WindowInfoMap map;
    map.insert(m_WId, m_windowInfo);
    PreviewContainer *m_appPreview = PreviewContainer::instance(map, DockPosition);
    connect(m_appPreview, &PreviewContainer::requestActivateWindow, m_appItem, &AppItem::requestActivateWindow, Qt::QueuedConnection);
    connect(m_appPreview, &PreviewContainer::requestPreviewWindow, m_appItem, &AppItem::requestPreviewWindow, Qt::QueuedConnection);
    connect(m_appPreview, &PreviewContainer::requestCancelPreviewWindow, m_appItem, &AppItem::requestCancelPreview);
    connect(m_appPreview, &PreviewContainer::requestCheckWindows, m_appItem, &AppItem::check);
    connect(m_appPreview, &PreviewContainer::requestClose, m_appItem, &AppItem::close);
    connect(m_appPreview, &PreviewContainer::requestHidePopup, this, [this]{
        emit m_appItem->requestCancelPreview();
        hidePopup();
    });

    showPopupWindow(m_appPreview, true);
}

void WindowItem::fetchSnapshot()
{
    if(window()->isVisible() == false) return;

    QImage qimage;
    XImage *ximage = nullptr;
    unsigned char *prop_to_return_gtk = nullptr;

    do {
        // get window image from XGetImage(a little slow)
        // qInfo() << "get Image from dxcbplugin SHM failed!";
        // qInfo() << "get Image from Xlib...";
        ximage = getImageXlib(m_WId);
        if (!ximage) {
            // qInfo() << "get Image from Xlib failed! giving up...";
            // m_appItem->check();
            return;
        }
        qimage = QImage((const uchar *)(ximage->data), ximage->width, ximage->height, ximage->bytes_per_line, QImage::Format_RGB32);

        Q_ASSERT(!qimage.isNull());

        // remove shadow frame
        m_snapshotSrcRect = rectRemovedShadow(m_WId, qimage, prop_to_return_gtk);
        m_snapshot = qimage;
    } while (false);


    QSizeF size(rect().marginsRemoved(QMargins(rect().width() * .14,  rect().height() * .14, rect().width() * .14, rect().height() * .14)).size());
    const auto ratio = devicePixelRatioF();
    size = m_snapshotSrcRect.size().scaled(size * ratio, Qt::KeepAspectRatio);

    qreal scale = qreal(size.width()) / m_snapshotSrcRect.width();
    m_snapshot = m_snapshot.scaled(qRound(m_snapshot.width() * scale), qRound(m_snapshot.height() * scale), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    m_snapshotSrcRect.moveTop(m_snapshotSrcRect.top() * scale + 1);
    m_snapshotSrcRect.moveLeft(m_snapshotSrcRect.left() * scale + 1);
    m_snapshotSrcRect.setWidth(size.width() - 1);
    m_snapshotSrcRect.setHeight(size.height() - 1);

    m_snapshot.setDevicePixelRatio(ratio);

    if (ximage) XDestroyImage(ximage);
    if (prop_to_return_gtk) XFree(prop_to_return_gtk);


    auto r = rect();
    switch (DockPosition)
    {
    case Top:
    case Bottom:
        r.setBottom(3);
        break;
    case Left:
        r.setX(3);
        break;
    case Right:
        r.setRight(3);
        break;
    }

    update(r);
}

void WindowItem::invokedMenuItem(const QString &itemId, const bool checked) {
    if(itemId == "close")
        m_appItem->close(m_WId);
    else if(itemId == "max") {
        if(XUtils::checkIfWinMaximum(m_WId))
            XUtils::unmaximizeWindow(m_WId);
        else
            XUtils::maximizeWindow(m_WId);
        XUtils::checkIfWinMaximum(m_WId);
    } else if(itemId == "min")
        KX11Extras::minimizeWindow(m_WId);
    else if(itemId == "active")
        m_appItem->requestActivateWindow(m_WId);
}

const QString WindowItem::contextMenu() const {
    QJsonObject menu;
    QJsonArray items;

    QJsonObject item;
    item.insert("itemText", "激活窗口");
    item.insert("itemId", "active");
    item.insert("isActive", !m_isActive);
    items.append(item);

    const bool isMax = XUtils::checkIfWinMaximum(m_WId);
    QJsonObject maxItem;
    maxItem.insert("itemText", isMax ? "还原窗口" : "最大化窗口");
    maxItem.insert("itemId", "max");
    maxItem.insert("isActive", true);
    items.append(maxItem);

    QJsonObject minItem;
    minItem.insert("itemText", "最小化窗口");
    minItem.insert("itemId", "min");
    minItem.insert("isActive", XUtils::checkIfWinMinimun(m_WId) == false);
    items.append(minItem);

    QJsonObject closeItem;
    closeItem.insert("itemText", "关闭窗口");
    closeItem.insert("itemId", "close");
    closeItem.insert("isActive", m_windowInfo.closable);
    items.append(closeItem);

    menu.insert("items", items);
    return QJsonDocument(menu).toJson();
}