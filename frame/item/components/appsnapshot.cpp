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

#include "appsnapshot.h"

#include <QEvent>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <QX11Info>
#include <QPainter>
#include <QSizeF>

AppSnapshot::AppSnapshot(const WId wid, QWidget *parent)
    : QWidget(parent)
    , m_wid(wid)
    , m_closeBtn2D(nullptr)
{
    setAcceptDrops(true);
    setFixedSize(SNAP_WIDTH, SNAP_HEIGHT);

    QTimer::singleShot(1, this, &AppSnapshot::fetchSnapshot);
}

void AppSnapshot::setCloseAble(const bool value) {
    if(value and !m_closeBtn2D) {
        m_closeBtn2D = new QPushButton(this);
        m_closeBtn2D->setFixedSize(32, 32);
        m_closeBtn2D->setIconSize(QSize(28, 28));
        m_closeBtn2D->setIcon(QIcon(":/icons/resources/close_round_normal.svg"));
        m_closeBtn2D->setVisible(false);
        m_closeBtn2D->setFlat(true);
        m_closeBtn2D->installEventFilter(this);
        connect(m_closeBtn2D, &QPushButton::clicked, this, [this]{emit requestClose(m_wid);}, Qt::QueuedConnection);
    } else if(!value and m_closeBtn2D) {
        m_closeBtn2D->deleteLater();
        m_closeBtn2D = nullptr;
    }
}

void AppSnapshot::setTitle(const QString &title)
{
    m_title = fontMetrics().elidedText(title, Qt::ElideRight, width()-30);
    if(isVisible()) update();
}

void AppSnapshot::dragEnterEvent(QDragEnterEvent *e)
{
    QWidget::dragEnterEvent(e);
    emit dragEntered(m_wid);
}

void AppSnapshot::fetchSnapshot()
{
    QImage qimage;
    XImage *ximage = nullptr;
    unsigned char *prop_to_return_gtk = nullptr;

    do {
        // get window image from XGetImage(a little slow)
        qDebug() << "get Image from dxcbplugin SHM failed!";
        qDebug() << "get Image from Xlib...";
        ximage = getImageXlib();
        if (!ximage) {
            qDebug() << "get Image from Xlib failed! giving up...";
            emit requestCheckWindow();
            return;
        }
        qimage = QImage((const uchar *)(ximage->data), ximage->width, ximage->height, ximage->bytes_per_line, QImage::Format_RGB32);

        Q_ASSERT(!qimage.isNull());

        // remove shadow frame
        m_snapshotSrcRect = rectRemovedShadow(qimage, prop_to_return_gtk);
        m_snapshot = qimage;
    } while (false);

    QSizeF size(rect().marginsRemoved(QMargins(8, 8, 8, 8)).size());
    const auto ratio = devicePixelRatioF();
    size = m_snapshotSrcRect.size().scaled(size * ratio, Qt::KeepAspectRatio);
    qreal scale = qreal(size.width()) / m_snapshotSrcRect.width();
    m_snapshot = m_snapshot.scaled(qRound(m_snapshot.width() * scale), qRound(m_snapshot.height() * scale),
                                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_snapshotSrcRect.moveTop(m_snapshotSrcRect.top() * scale + 0.5);
    m_snapshotSrcRect.moveLeft(m_snapshotSrcRect.left() * scale + 0.5);
    m_snapshotSrcRect.setWidth(size.width() - 0.5);
    m_snapshotSrcRect.setHeight(size.height() - 0.5);
    m_snapshot.setDevicePixelRatio(ratio);

    if (ximage) XDestroyImage(ximage);
    if (prop_to_return_gtk) XFree(prop_to_return_gtk);

    update();
}

void AppSnapshot::enterEvent(QEvent *e)
{
    QWidget::enterEvent(e);

    if(m_closeBtn2D) {
        m_closeBtn2D->setVisible(true);
        m_closeBtn2D->move(width() - m_closeBtn2D->width()-2, 2);
    }

    emit entered(m_wid);
    update();
}

void AppSnapshot::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);

    if(m_closeBtn2D) m_closeBtn2D->setVisible(false);

    update();
}

void AppSnapshot::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);

    if (m_snapshot.isNull())
        return;

    const auto ratio = devicePixelRatioF();
    const int radius = 5;

    // draw image
    const QImage &im = m_snapshot;

    const qreal offset_x = width() / 2.0 - m_snapshotSrcRect.width() / ratio / 2 - m_snapshotSrcRect.left() / ratio;
    const qreal offset_y = height() / 2.0 - m_snapshotSrcRect.height() / ratio / 2 - m_snapshotSrcRect.top() / ratio;


    QBrush brush;
    brush.setTextureImage(im);
    painter.save();
    painter.setBrush(brush);
    painter.setPen(Qt::NoPen);
    painter.scale(1 / ratio, 1 / ratio);
    painter.translate(QPoint(offset_x * ratio, offset_y * ratio));
    painter.drawRoundedRect(m_snapshotSrcRect, radius * ratio, radius * ratio);
    painter.restore();
    // draw attention background
    if (underMouse()) {
        // painter.setBrush(QColor(241, 138, 46, 255 * .8));
        painter.setPen(Qt::blue);
        painter.drawRoundedRect(rect().marginsRemoved({7, 7, 7, 7}), 2, 2);

        QRect titleRect{15, height() - 50, width() - 30, 30};

        painter.setBrush(palette().base());
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(titleRect, 4, 4);

        painter.setPen(QPen(palette().brightText(), 2));
        QTextOption option;
        option.setAlignment(Qt::AlignCenter);
        painter.drawText(titleRect, m_title, option);
    }
}

void AppSnapshot::mousePressEvent(QMouseEvent *e)
{
    QWidget::mousePressEvent(e);
    emit clicked(m_wid);
}

bool AppSnapshot::eventFilter(QObject *watched, QEvent *e)
{
    if (watched == m_closeBtn2D and m_closeBtn2D) {
        if (e->type() == QEvent::HoverEnter || e->type() == QEvent::HoverMove) {
            m_closeBtn2D->setIcon(QIcon(":/icons/resources/close_round_hover.svg"));
        } else if (e->type() == QEvent::HoverLeave) {
            m_closeBtn2D->setIcon(QIcon(":/icons/resources/close_round_normal.svg"));
        } else if (e->type() == QEvent::MouseButtonPress) {
            m_closeBtn2D->setIcon(QIcon(":/icons/resources/close_round_press.svg"));
        }
    }

    return QWidget::eventFilter(watched, e);
}

XImage *AppSnapshot::getImageXlib()
{
    const auto display = QX11Info::display();
    Window unused_window;
    int unused_int;
    unsigned unused_uint, w, h;
    XGetGeometry(display, m_wid, &unused_window, &unused_int, &unused_int, &w, &h, &unused_uint, &unused_uint);
    return XGetImage(display, m_wid, 0, 0, w, h, AllPlanes, ZPixmap);
}

QRect AppSnapshot::rectRemovedShadow(const QImage &qimage, unsigned char *prop_to_return_gtk)
{
    const auto display = QX11Info::display();

    const Atom gtk_frame_extents = XInternAtom(display, "_GTK_FRAME_EXTENTS", true);
    Atom actual_type_return_gtk;
    int actual_format_return_gtk;
    unsigned long n_items_return_gtk;
    unsigned long bytes_after_return_gtk;

    const auto r = XGetWindowProperty(display, m_wid, gtk_frame_extents, 0, 4, false, XA_CARDINAL,
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
