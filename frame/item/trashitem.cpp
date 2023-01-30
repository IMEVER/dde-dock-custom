#include "trashitem.h"

#include "util/utils.h"

#include <QPainter>
#include <QProcess>
#include <QMouseEvent>
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <DDesktopServices>

static const QString TRASHPATH = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/Trash/files/";

TrashItem::TrashItem(QWidget *parent) : DockItem(parent)
{
    setAcceptDrops(true);
}

void TrashItem::refershIcon()
{
    const int iconSize = qMin(width(), height());
    QDir dir(TRASHPATH);
    if(dir.isEmpty())
        m_icon = Utils::getIcon("user-trash", iconSize * 0.9, devicePixelRatioF());
    else
        m_icon = Utils::getIcon("user-trash-full", iconSize * 0.9, devicePixelRatioF());
    update();
}

void TrashItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);

    QPainter painter(this);

    const auto ratio = devicePixelRatioF();
    const int iconX = rect().center().x() - m_icon.rect().center().x() / ratio;
    const int iconY = rect().center().y() - m_icon.rect().center().y() / ratio;

    painter.drawPixmap(iconX, iconY, m_icon);
}

void TrashItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);
    refershIcon();
}

void TrashItem::mousePressEvent(QMouseEvent *e)
{
    if(e->button() == Qt::LeftButton)
        QProcess::startDetached("xdg-open", {TRASHPATH});

    DockItem::mousePressEvent(e);
}

void TrashItem::dragEnterEvent(QDragEnterEvent *e)
{
    if(e->mimeData()->hasUrls()) {
        bool canDelete = true;
        for(auto url : e->mimeData()->urls()) {
            canDelete = url.isLocalFile();
            if(canDelete) canDelete = QFileInfo(url.toLocalFile()).isWritable();
            if(!canDelete) break;
        }

        if(canDelete) {
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

void TrashItem::invokedMenuItem(const QString &itemId, const bool checked) {
    if(itemId == "open")
        QProcess::startDetached("xdg-open", {TRASHPATH});
    else if(itemId == "clear") {
        QProcess::execute("rm", { "-r", QDir::homePath() + "/.local/share/Trash/info/", TRASHPATH});
        QDir dir(QDir::homePath() + "/.local/share/Trash/");
        dir.mkdir("files");
        dir.mkdir("info");
        Dtk::Widget::DDesktopServices::playSystemSoundEffect(Dtk::Widget::DDesktopServices::SSE_EmptyTrash);
        refershIcon();
    }
}

const QString TrashItem::contextMenu() const {
    QDir dir(TRASHPATH);
    return QString("{\"items\": [ {\"itemText\": \"打开回收站\", \"itemId\": \"open\", \"isActive\": true}, \
        {\"itemText\": \"清空回收站\", \"itemId\": \"clear\", \"isActive\": %1} ]}").arg(dir.isEmpty() ? "false" : "true");
}