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

#ifndef IMAGEUTIL_H
#define IMAGEUTIL_H

#include <QPixmap>
#include <QSvgRenderer>
#include <QScreen>
#include <QGSettings>
#include <QVariant>

class Utils
{
public:
    static const QPixmap loadSvg(const QString &iconName, const QString &localPath, const int size, const qreal ratio);
    static const QPixmap loadSvg(const QString &iconName, const QSize size, const qreal ratio);
    static const QPixmap lighterEffect(const QPixmap pixmap, const int delta = 120);
    static const QPixmap getIcon(const QString iconName, const int size, const qreal ratio);
    static QPixmap renderSVG(const QString &path, const QSize &size, const qreal devicePixelRatio);
    static QScreen *screenAt(const QPoint &point);
    static QScreen *screenAtByScaled(const QPoint &point);
    static QGSettings *SettingsPtr(const QString &schema_id, const QByteArray &path = QByteArray(), QObject *parent = nullptr);
    static QString qtify_name(const char *name);
    static const QGSettings *ModuleSettingsPtr(const QString &module, const QByteArray &path = QByteArray(), QObject *parent = nullptr);
    static const QVariant SettingValue(const QString &schema_id, const QByteArray &path = QByteArray(), const QString &key = QString(), const QVariant &fallback = QVariant());
    static bool SettingSaveValue(const QString &schema_id, const QByteArray &path, const QString &key, const QVariant &value );
};

#endif // IMAGEUTIL_H
