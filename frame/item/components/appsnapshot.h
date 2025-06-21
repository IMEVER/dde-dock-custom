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

#ifndef APPSNAPSHOT_H
#define APPSNAPSHOT_H

#include <QWidget>
#include <QDebug>
#include <QTimer>

#include <QPushButton>

#define SNAP_WIDTH       200
#define SNAP_HEIGHT      130

struct _XImage;
typedef _XImage XImage;

class AppSnapshot : public QWidget
{
    Q_OBJECT

public:
    explicit AppSnapshot(const WId wid, QWidget *parent = 0);
    void setCloseAble(const bool value);

signals:
    void entered(const WId wid) const;
    void dragEntered(const WId wid) const;
    void clicked(const WId wid) const;
    void requestClose(const WId wid) const;
    void requestCheckWindow() const;

public slots:
    void setTitle(const QString &title);

private:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void enterEvent(QEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *e) override;
    void fetchSnapshot();
    XImage * getImageXlib();
    QRect rectRemovedShadow(const QImage &qimage, unsigned char *prop_to_return_gtk);

private:
    const WId m_wid;

    QImage m_snapshot;
    QRectF m_snapshotSrcRect;

    QString m_title;
    QPushButton *m_closeBtn2D;
};

#endif // APPSNAPSHOT_H
