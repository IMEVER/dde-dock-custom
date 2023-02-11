#include "trashitem.h"

#include "util/utils.h"

#include <QPainter>
#include <QProcess>
#include <QMouseEvent>
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <DDesktopServices>
#include <QMessageBox>
#include <QFileSystemWatcher>

static const QString TRASHPATH = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/Trash";
static const QString TRASHEXPUNGED = TRASHPATH + "/expunged";
static const QString TRASHFILE = TRASHPATH + "/files";
static const QString TRASHINFO = TRASHPATH + "/info";

TrashItem::TrashItem(QWidget *parent) : DockItem(parent)
    , m_watcher(new QFileSystemWatcher({TRASHFILE}, this))
{
    setAcceptDrops(true);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &TrashItem::refershIcon);
}

void TrashItem::refershIcon()
{
    const int iconSize = qMin(width(), height());
    QDir dir(TRASHFILE);
    if (dir.exists() == false or dir.isEmpty())
        m_icon = Utils::getIcon("user-trash", iconSize * 0.9, devicePixelRatioF());
    else
        m_icon = Utils::getIcon("user-trash-full", iconSize * 0.9, devicePixelRatioF());
    update();
}

void TrashItem::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        QProcess::startDetached("xdg-open", {TRASHFILE});

    DockItem::mousePressEvent(e);
}

void TrashItem::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
    {
        bool canDelete = true;
        for (auto url : e->mimeData()->urls())
        {
            canDelete = url.isLocalFile();
            if (canDelete)
                canDelete = QFileInfo(url.toLocalFile()).isWritable();
            if (!canDelete)
                break;
        }

        if (canDelete)
        {
            e->setDropAction(Qt::MoveAction);
            e->accept();
            hidePopup();
            return e->accept();
        }
    }

    return e->ignore();
}

void TrashItem::dropEvent(QDropEvent *e)
{
    foreach (auto url, e->mimeData()->urls())
        QFile(url.toLocalFile()).moveToTrash();

    refershIcon();
}

void TrashItem::invokedMenuItem(const QString &itemId, const bool checked)
{
    if (itemId == "open")
        QProcess::startDetached("xdg-open", {TRASHFILE});
    else if (itemId == "clear")
    {
        if (QMessageBox::Ok == QMessageBox::warning(this, "警告", "清空后数据将不可恢复！\n确认清空回收站？", QMessageBox::Cancel | QMessageBox::Ok, QMessageBox::Ok))
        {
            m_watcher->blockSignals(true);
            QProcess::execute("rm", {"-r", TRASHFILE, TRASHINFO});
            QDir dir(TRASHPATH);
            dir.mkdir("files");
            dir.mkdir("info");
            Dtk::Widget::DDesktopServices::playSystemSoundEffect(Dtk::Widget::DDesktopServices::SSE_EmptyTrash);
            refershIcon();
            m_watcher->blockSignals(false);
        }
    }
}

const QString TrashItem::contextMenu() const
{
    QDir dir(TRASHFILE);
    return QString("{\"items\": [ {\"itemText\": \"打开回收站\", \"itemId\": \"open\", \"isActive\": true}, \
        {\"itemText\": \"清空回收站\", \"itemId\": \"clear\", \"isActive\": %1} ]}")
        .arg(!dir.exists() or dir.isEmpty() ? "false" : "true");
}