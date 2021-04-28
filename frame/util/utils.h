#include <QPixmap>
#include <QImageReader>
#include <QApplication>
#include <QScreen>
#include <QPixmapCache>

namespace Utils {
    static QPixmap renderSVG(const QString &path, const QSize &size, const qreal devicePixelRatio) {
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

    static QScreen * screenAt(const QPoint &point) {
        for (QScreen *screen : qApp->screens()) {
            const QRect r { screen->geometry() };
            const QRect rect { r.topLeft(), r.size() * screen->devicePixelRatio() };
            if (rect.contains(point)) {
                return screen;
            }
        }

        return nullptr;
    }

    static QScreen * screenAtByScaled(const QPoint &point) {
        for (QScreen *screen : qApp->screens()) {
            if (screen->geometry().contains(point)) {
                return screen;
            }
        }

        return nullptr;
    }


    static const QPixmap getIcon(const QString iconName, const int size, const qreal ratio)
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

}
