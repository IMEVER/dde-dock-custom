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

#include "util/utils.h"

#include <QIcon>
#include <QPainter>
#include <QPixmapCache>
#include <QFile>
#include <QCryptographicHash>
#include <QImageReader>
#include <QApplication>

const QPixmap Utils::loadSvg(const QString &iconName, const QString &localPath, const int size, const qreal ratio)
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

const QPixmap Utils::loadSvg(const QString &iconName, const QSize size, const qreal ratio)
{
    QIcon icon = QIcon::fromTheme(iconName);
    if (!icon.isNull()) {
        QPixmap pixmap = icon.pixmap(size*ratio);
        pixmap.setDevicePixelRatio(ratio);
        return pixmap;
    }
    return QPixmap();
}

const QPixmap Utils::lighterEffect(const QPixmap pixmap, const int delta)
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

const QPixmap Utils::getIcon(const QString iconName, const int size, const qreal ratio)
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


QPixmap Utils::renderSVG(const QString &path, const QSize &size, const qreal devicePixelRatio) {
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

QScreen * Utils::screenAt(const QPoint &point) {
        for (QScreen *screen : qApp->screens()) {
            const QRect r { screen->geometry() };
            const QRect rect { r.topLeft(), r.size() * screen->devicePixelRatio() };
            if (rect.contains(point)) {
                return screen;
            }
        }

        return nullptr;
    }

QScreen * Utils::screenAtByScaled(const QPoint &point) {
        for (QScreen *screen : qApp->screens()) {
            if (screen->geometry().contains(point)) {
                return screen;
            }
        }

        return nullptr;
    }

/**
 * @brief SettingsPtr 根据给定信息返回一个QGSettings指针
 * @param schema_id The id of the schema
 * @param path If non-empty, specifies the path for a relocatable schema
 * @param parent 创建指针的付对象
 * @return
 */
QGSettings *Utils::SettingsPtr(const QString &schema_id, const QByteArray &path, QObject *parent) {
    if (QGSettings::isSchemaInstalled(schema_id.toUtf8())) {
        QGSettings *settings = new QGSettings(schema_id.toUtf8(), path, parent);
        return settings;
    }
    qDebug() << "Cannot find gsettings, schema_id:" << schema_id;
    return nullptr;
}

/**
 * @brief SettingsPtr 根据给定信息返回一个QGSettings指针
 * @param module 传入QGSettings构造函数时，会添加"com.deepin.dde.dock.module."前缀
 * @param path If non-empty, specifies the path for a relocatable schema
 * @param parent 创建指针的付对象
 * @return
 */
const QGSettings *Utils::ModuleSettingsPtr(const QString &module, const QByteArray &path, QObject *parent) {
    return Utils::SettingsPtr("com.deepin.dde.dock.module." + module, path, parent);
}

/* convert 'some-key' to 'someKey' or 'SomeKey'.
 * the second form is needed for appending to 'set' for 'setSomeKey'
 */
QString Utils::qtify_name(const char *name)
{
    bool next_cap = false;
    QString result;

    while (*name) {
        if (*name == '-') {
            next_cap = true;
        } else if (next_cap) {
            result.append(QChar(*name).toUpper().toLatin1());
            next_cap = false;
        } else {
            result.append(*name);
        }

        name++;
    }

    return result;
}

/**
 * @brief SettingValue 根据给定信息返回获取的值
 * @param schema_id The id of the schema
 * @param path If non-empty, specifies the path for a relocatable schema
 * @param key 对应信息的key值
 * @param fallback 如果找不到信息，返回此默认值
 * @return
 */
const QVariant Utils::SettingValue(const QString &schema_id, const QByteArray &path, const QString &key, const QVariant &fallback){
    const QGSettings *settings = Utils::SettingsPtr(schema_id, path);

    if (settings && ((settings->keys().contains(key)) || settings->keys().contains(Utils::qtify_name(key.toUtf8().data())))) {
        QVariant v = settings->get(key);
        delete settings;
        return v;
    } else{
        qDebug() << "Cannot find gsettings, schema_id:" << schema_id
                 << " path:" << path << " key:" << key
                 << "Use fallback value:" << fallback;
        // 如果settings->keys()不包含key则会存在内存泄露，所以需要释放
        if (settings)
            delete settings;

        return fallback;
    }
}

bool Utils::SettingSaveValue(const QString &schema_id, const QByteArray &path, const QString &key, const QVariant &value ){
    QGSettings *settings = Utils::SettingsPtr(schema_id, path);

    if (settings && ((settings->keys().contains(key)) || settings->keys().contains(Utils::qtify_name(key.toUtf8().data())))) {
        settings->set(key, value);
        delete settings;
        return true;
    } else{
        qDebug() << "Cannot find gsettings, schema_id:" << schema_id
                 << " path:" << path << " key:" << key;
        if (settings)
            delete settings;

        return false;
    }
}