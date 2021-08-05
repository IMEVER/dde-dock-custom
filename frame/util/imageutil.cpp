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

#include "imageutil.h"

#include <QIcon>
#include <QPainter>
#include <QPixmapCache>
#include <QFile>
#include <QCryptographicHash>
#include <QImageReader>
#include <QApplication>

const QPixmap ImageUtil::loadSvg(const QString &iconName, const QString &localPath, const int size, const qreal ratio)
{
    QIcon icon = QIcon::fromTheme(iconName);
    if (!icon.isNull()) {
        QPixmap pixmap = icon.pixmap(int(size * ratio), int(size * ratio));
        pixmap.setDevicePixelRatio(ratio);
        return pixmap;
    }

    QPixmap pixmap(int(size * ratio), int(size * ratio));
    QString localIcon = QString("%1%2%3").arg(localPath).arg(iconName).arg(iconName.contains(".svg") ? "" : ".svg");
    QSvgRenderer renderer(localIcon);
    pixmap.fill(Qt::transparent);

    QPainter painter;
    painter.begin(&pixmap);
    renderer.render(&painter);
    painter.end();
    pixmap.setDevicePixelRatio(ratio);

    return pixmap;
}

const QPixmap ImageUtil::loadSvg(const QString &iconName, const QSize size, const qreal ratio)
{
    QIcon icon = QIcon::fromTheme(iconName);
    if (!icon.isNull()) {
        QPixmap pixmap = icon.pixmap(size*ratio);
        pixmap.setDevicePixelRatio(ratio);
        return pixmap;
    }
}

const QPixmap ImageUtil::lighterEffect(const QPixmap pixmap, const int delta)
{
    QImage image = pixmap.toImage();

    const int width = image.width();
    const int height = image.height();
    const int bytesPerPixel = image.bytesPerLine() / image.width();

    for (int i(0); i != height; ++i)
    {
        uchar *scanLine = image.scanLine(i);
        for (int j(0); j != width; ++j)
        {
            QRgb &rgba = *(QRgb*)scanLine;
            if (qAlpha(rgba) == 0xff) {
                rgba = QColor::fromRgba(rgba).lighter(delta).rgba();
//                const QColor hsv = QColor::fromRgba(rgba).toHsv();
//                // check brightness first, color with max brightness processed with lighter will
//                // become white like color which will ruin the whole image in general cases.
//                if (hsv.value() < 255) {
//                    rgba = QColor::fromRgba(rgba).lighter(delta).rgba();
//                }
            }
            scanLine += bytesPerPixel;
        }
    }

    return QPixmap::fromImage(image);
}

const QPixmap ImageUtil::getIcon(const QString iconName, const int size, const qreal ratio)
    {
        QPixmap pixmap;
        QString key;
        QIcon icon;
        // 把size改为小于size的最大偶数 :)
        const int s = int(size * ratio) & ~1;

        do {
            // load pixmap from our Cache
            if (iconName.startsWith("data:image/")) {
                key = QCryptographicHash::hash(iconName.toUtf8(), QCryptographicHash::Md5).toHex();

                // FIXME(hualet): The cache can reduce memory usage,
                // that is ~2M on HiDPI enabled machine with 9 icons loaded,
                // but I don't know why since QIcon has its own cache and all of the
                // icons loaded are loaded by QIcon::fromTheme, really strange here.
                if (QPixmapCache::find(key, &pixmap))
                    break;
            }

            // load pixmap from Byte-Data
            if (iconName.startsWith("data:image/")) {
                const QStringList strs = iconName.split("base64,");
                if (strs.size() == 2)
                    pixmap.loadFromData(QByteArray::fromBase64(strs.at(1).toLatin1()));

                if (!pixmap.isNull())
                    break;
            }

            // load pixmap from File
            if (QFile::exists(iconName)) {
                pixmap = QPixmap(iconName);
                if (!pixmap.isNull())
                    break;
            }

            icon = QIcon::fromTheme(iconName);
            if (icon.isNull()) {
                icon = QIcon::fromTheme("deepinwine-" + iconName);
            } else {
                icon = QIcon::fromTheme(iconName, QIcon::fromTheme("application-x-desktop"));
            }

            // load pixmap from Icon-Theme
            const int fakeSize = std::max(48, s); // cannot use 16x16, cause 16x16 is label icon
            pixmap = icon.pixmap(QSize(fakeSize, fakeSize));
            if (!pixmap.isNull())
                break;

            // fallback to a Default pixmap
            pixmap = QPixmap(":/icons/resources/application-x-desktop.svg");
            if (!pixmap.isNull())
                break;

            Q_UNREACHABLE();

        } while (false);

        if (!key.isEmpty()) {
            QPixmapCache::insert(key, pixmap);
        }

        if (pixmap.size().width() != s) {
            pixmap = pixmap.scaled(s, s, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        pixmap.setDevicePixelRatio(ratio);
        return pixmap;
    }


QPixmap ImageUtil::renderSVG(const QString &path, const QSize &size, const qreal devicePixelRatio) {
        QImageReader reader;
        QPixmap pixmap;
        reader.setFileName(path);
        if (reader.canRead()) {
            reader.setScaledSize(size * devicePixelRatio);
            pixmap = QPixmap::fromImage(reader.read());
            pixmap.setDevicePixelRatio(devicePixelRatio);
        }
        else {
            pixmap.load(path);
        }

        return pixmap;
    }

QScreen * ImageUtil::screenAt(const QPoint &point) {
        for (QScreen *screen : qApp->screens()) {
            const QRect r { screen->geometry() };
            const QRect rect { r.topLeft(), r.size() * screen->devicePixelRatio() };
            if (rect.contains(point)) {
                return screen;
            }
        }

        return nullptr;
    }

QScreen * ImageUtil::screenAtByScaled(const QPoint &point) {
        for (QScreen *screen : qApp->screens()) {
            if (screen->geometry().contains(point)) {
                return screen;
            }
        }

        return nullptr;
    }
