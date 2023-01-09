#include "pluginitem.h"

#include "util/utils.h"

#include <QPainter>
#include <QProcess>
#include <QMouseEvent>
#include <QApplication>
#include <QDir>
#include <QStandardPaths>

DCORE_USE_NAMESPACE

PluginItem::PluginItem(QWidget *parent)
    : DockItem(parent)
    , m_tips(new TipsWidget(this))
{
    m_tips->setVisible(false);
    m_tips->setText("垃圾桶");

    setAcceptDrops(true);
}

void PluginItem::refershIcon()
{
    const int iconSize = qMin(width(), height());
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/Trash/files/");
    if(dir.isEmpty())
        m_icon = Utils::getIcon("user-trash", iconSize * 0.9, devicePixelRatioF());
    else
        m_icon = Utils::getIcon("user-trash-full", iconSize * 0.9, devicePixelRatioF());
    update();
}

void PluginItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);

    if (!isVisible())
        return;

    QPainter painter(this);

    const auto ratio = devicePixelRatioF();
    const int iconX = rect().center().x() - m_icon.rect().center().x() / ratio;
    const int iconY = rect().center().y() - m_icon.rect().center().y() / ratio;

    painter.drawPixmap(iconX, iconY, m_icon);
}

void PluginItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);
    refershIcon();
}

void PluginItem::mousePressEvent(QMouseEvent *e)
{
    if(e->button() == Qt::LeftButton)
        QProcess::startDetached("xdg-open", {QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/Trash/files/"});

    DockItem::mousePressEvent(e);
}

void PluginItem::dragEnterEvent(QDragEnterEvent *e)
{
    // ignore drag from panel
    if (e->source())
        return e->ignore();

    // ignore request dock event
    QString draggingMimeKey = e->mimeData()->formats().contains("RequestDock") ? "RequestDock" : "text/plain";
    if (QMimeDatabase().mimeTypeForFile(e->mimeData()->data(draggingMimeKey)).name() == "application/x-desktop") {
        return e->ignore();
    }

    if(e->mimeData()->hasUrls() == false)
        return e->ignore();

    e->setDropAction(Qt::MoveAction);
    e->accept();
    hidePopup();
}

void PluginItem::dropEvent(QDropEvent *e)
{
    for (auto url : e->mimeData()->urls()) {
        if(url.isLocalFile()) QFile(url.toLocalFile()).moveToTrash();
    }

    refershIcon();
}

QWidget *PluginItem::popupTips()
{
    return m_tips;
}

void PluginItem::invokedMenuItem(const QString &itemId, const bool checked) {
    if(itemId == "open")
        QProcess::startDetached("xdg-open", {QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/Trash/files/"});
    else if(itemId == "clear") {
        QString filesPath = QDir::homePath() + "/.local/share/Trash/files/";
        QString infoPath = QDir::homePath() + "/.local/share/Trash/info/";

        QProcess::execute("rm", { "-r", infoPath, filesPath});
        QDir dir(QDir::homePath() + "/.local/share/Trash/");
        dir.mkdir("files");
        dir.mkdir("info");
        refershIcon();
    }
}

const QString PluginItem::contextMenu() const {
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/Trash/files/");
    return QString("{\"items\": [ {\"itemText\": \"打开回收站\", \"itemId\": \"open\", \"isActive\": true}, \
         {\"itemText\": \"清空回收站\", \"itemId\": \"clear\", \"isActive\": %1} ]}").arg(dir.isEmpty() ? "false" : "true");
}