#include "folderitem.h"

#include "util/dockpopupwindow.h"

#include <QMouseEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QStyleOption>
#include <DGuiApplicationHelper>
#include <QListWidget>
#include <QScrollBar>
#include <QFileIconProvider>
#include <QFileSystemWatcher>
#include <QStandardPaths>

static DockPopupWindow *dirPopupWindow(nullptr);
static QFileIconProvider provider;

class FolderWidget;
class DesktopItemView : public QWidget {
    Q_OBJECT
public:
    DesktopItemView(const QString &file, QWidget *parent) : QWidget(parent)
    , m_current(false)
    {
        m_path = file;

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setSpacing(5);
        layout->setContentsMargins(10, 10, 10, 10);
        QLabel *iconLabel = new QLabel(this);
        iconLabel->setFixedSize(50, 50);
        iconLabel->setScaledContents(true);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setPixmap(provider.icon(QFileInfo(file)).pixmap(70, 70));
        layout->addWidget(iconLabel, 0, Qt::AlignCenter);

        QLabel *titleLabel = new QLabel(this);
        titleLabel->setFixedSize(80, 25);
        titleLabel->setAlignment(Qt::AlignCenter);
        QFont font = titleLabel->font();
        font.setBold(true);
        titleLabel->setFont(font);
        const QString fileName = QFileInfo(m_path).fileName();
        QString txt = titleLabel->fontMetrics().elidedText(fileName, Qt::ElideRight, titleLabel->width());
        if(txt != fileName) setToolTip(fileName);
        titleLabel->setText(txt);
        layout->addWidget(titleLabel);

        setFixedSize(100, 100);
        setMouseTracking(true);

        currentChanged(false, false);
    }

    void open() {
        QProcess::startDetached("xdg-open", {m_path});
    }

    void currentChanged(bool current, bool hover=false) {
        m_current = current;
        setStyleSheet(QString("QLabel {color: %1} DesktopItemView { background-color: %2; border-radius: 10px; }")
                    .arg(m_current ? palette().highlight().color().name() : palette().windowText().color().name())
                    .arg(color(current, hover)));
    }


protected:
    void enterEvent(QEvent *event) override {
        if(!m_current)
            currentChanged(m_current, true);
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override {
        if(!m_current)
            currentChanged(m_current, false);
        QWidget::leaveEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QStyleOption opt;
        opt.init(this);
        QPainter p(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    }

private:
    QString color(bool on, bool hover) {
        if(DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::LightType)
            return QString("rgba(0, 0, 0, %1)").arg(on ? "40%" : (hover ? "30%" : "20%"));
        else
            return QString("rgba(255,255,255, %1)").arg(on ? "40%" : (hover ? "30%" : "20%"));
    }

private:
    QString m_path;
    bool m_current;

    friend class FolderWidget;
};

class FolderWidget : public QListWidget {
    Q_OBJECT
public:
    FolderWidget(QString path) : QListWidget()
    , m_path(path)
    , m_mouseLeaveTimer(new QTimer(this))
    , m_watcher(new QFileSystemWatcher({path}, this))
    {
        setFlow(QListWidget::LeftToRight);
        setViewMode(QListWidget::IconMode);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setResizeMode(QListView::Adjust);
        setSelectionBehavior(QListWidget::SelectItems);
        setSelectionMode(QListWidget::SingleSelection);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setVerticalScrollMode(QListWidget::ScrollPerPixel);
        setDragDropMode(QListWidget::DragDrop);
        setUniformItemSizes(true);
        setMouseTracking(true);
        verticalScrollBar()->setSingleStep(30);

        setSpacing(30);
        setContentsMargins(30, 10, 30, 10);

        connect(this, &FolderWidget::itemClicked, [this](QListWidgetItem *item){
            qobject_cast<DesktopItemView*>(itemWidget(item))->open();
        });

        connect(this, &FolderWidget::currentItemChanged, this, [this](QListWidgetItem *current, QListWidgetItem *prev){
            if(current)
                if(DesktopItemView *v = qobject_cast<DesktopItemView*>(itemWidget(current)))
                    v->currentChanged(true);
            if(prev)
                if(DesktopItemView *v = qobject_cast<DesktopItemView*>(itemWidget(prev)))
                    v->currentChanged(false);
        });

        connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [this]{
            const int current = currentRow();
            for(int i=0, len=count(); i<len; i++)
                qobject_cast<DesktopItemView*>(itemWidget(item(i)))->currentChanged(current == i);
        });

        m_mouseLeaveTimer->setSingleShot(true);
        m_mouseLeaveTimer->setInterval(100);

        connect(m_mouseLeaveTimer, &QTimer::timeout, this, &FolderWidget::checkMouseLeave);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &FolderWidget::refresh);
        refresh();
    }

    ~FolderWidget() {
        // m_watcher->cancel();
    }

    void refresh() {
        clear();

        QDir dir(m_path);

        if(!dir.exists() || dir.isEmpty(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) return;

        for(auto file : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            QListWidgetItem *item = new QListWidgetItem(this);
            item->setSizeHint(QSize(100, 100));
            this->addItem(item);

            DesktopItemView *desktopItemView = new DesktopItemView(file.absoluteFilePath(), this);
            this->setItemWidget(item, desktopItemView);
        }
    }

    void clear() {
        while(QListWidgetItem *item = takeItem(0)) {
            itemWidget(item)->deleteLater();
            delete item;
        }
    }

    void prepareHide()
    {
        m_mouseLeaveTimer->start();
    }

    void checkMouseLeave()
    {
        if(!underMouse()) emit requestHidePopup();
    }

    void enterEvent(QEvent *e)
    {
        QWidget::enterEvent(e);
        m_mouseLeaveTimer->stop();
    }

    void leaveEvent(QEvent *e)
    {
        QWidget::leaveEvent(e);
        m_mouseLeaveTimer->start();
    }

    QStringList mimeTypes() const {
        return QStringList("text/uri-list");
    }

    QMimeData *mimeData(const QList<QListWidgetItem *> items) const {
        QMimeData *data = new QMimeData();
        QList<QUrl> urls;
        for(auto item : items)
            urls << QUrl::fromLocalFile(qobject_cast<DesktopItemView*>(itemWidget(item))->m_path);

        data->setUrls(urls);
        return data;
    }

signals:
    void requestHidePopup();

private:
    QString m_path;
    QTimer *m_mouseLeaveTimer;
    QFileSystemWatcher *m_watcher;
};

FolderItem::FolderItem(QString path, QWidget *parent) : DockItem(parent)
    , m_path(path) {
    if (!dirPopupWindow) {
        dirPopupWindow = new DockPopupWindow(nullptr);
        // dirPopupWindow->setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
        dirPopupWindow->setAttribute(Qt::WA_AlwaysShowToolTips);
        dirPopupWindow->setShadowBlurRadius(20);
        dirPopupWindow->setRadius(10);
        dirPopupWindow->setShadowYOffset(2);
        dirPopupWindow->setShadowXOffset(0);
        dirPopupWindow->setArrowWidth(18);
        dirPopupWindow->setArrowHeight(10);
    }

    m_popupGrid = new FolderWidget(m_path);
    m_popupGrid->setFixedSize(560, 400);

    connect(m_popupGrid, &FolderWidget::requestHidePopup, this, &FolderItem::hideDirpopupWindow);

    QString m_foldType = "folder";
    if(m_path == QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)) m_foldType = "folder-desktop";
    else if(m_path == QStandardPaths::writableLocation(QStandardPaths::HomeLocation)) m_foldType = "folder-home";
    else if(m_path == QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)) m_foldType = "folder-documents";
    else if(m_path == QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)) m_foldType = "folder-downloads";
    else if(m_path == QStandardPaths::writableLocation(QStandardPaths::MusicLocation)) m_foldType = "folder-music";
    else if(m_path == QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)) m_foldType = "folder-pictures";
    else if(m_path == QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)) m_foldType = "folder-videos";
    m_icon = QIcon::fromTheme(m_foldType);
}

FolderItem::~FolderItem() {
    m_popupGrid->deleteLater();
}

void FolderItem::leaveEvent(QEvent *e)
{
    DockItem::leaveEvent(e);
    m_popupGrid->prepareHide();
}

void FolderItem::mouseReleaseEvent(QMouseEvent *e)
{
    if(!m_popupGrid->isVisible())
    {
        hidePopup();
        showDirPopupWindow();
    }
    else if(e->button() != Qt::RightButton)
        hideDirpopupWindow();

    DockItem::mouseReleaseEvent(e);
}

void FolderItem::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
    {
        bool isFile = true;
        for (auto url : e->mimeData()->urls())
        {
            isFile = url.isLocalFile();
            if (isFile)
                isFile = QFileInfo(url.toLocalFile()).isReadable();
            if (!isFile)
                break;
        }

        if (isFile)
        {
            e->setDropAction(Qt::MoveAction);
            e->accept();
            hidePopup();
            return e->accept();
        }
    }

    return e->ignore();
}

void FolderItem::dropEvent(QDropEvent *e)
{
    foreach (auto url, e->mimeData()->urls()) {
        QFileInfo file(url.toLocalFile());
        if(file.isFile())
            QFile::copy(file.absolutePath(), m_path + '/' + QFileInfo(file).fileName());
    }
}

void FolderItem::invokedMenuItem(const QString &itemId, const bool checked)
{
    if (itemId == "open")
        QProcess::startDetached("xdg-open", {m_path});
    else if (itemId == "remove")
        emit undocked();
}

const QString FolderItem::contextMenu() const
{
    return QString("{\"items\": [ {\"itemText\": \"打开\", \"itemId\": \"open\", \"isActive\": true}, \
        {\"itemText\": \"移除\", \"itemId\": \"remove\", \"isActive\": true} ]}");
}

void FolderItem::showDirPopupWindow()
{
    emit requestWindowAutoHide(false);

    switch (DockPosition) {
    case Top:
    case Bottom: dirPopupWindow->setArrowDirection(DockPopupWindow::ArrowBottom);  break;
    case Left:  dirPopupWindow->setArrowDirection(DockPopupWindow::ArrowLeft);    break;
    case Right: dirPopupWindow->setArrowDirection(DockPopupWindow::ArrowRight);   break;
    }

    dirPopupWindow->setContent(m_popupGrid);
    dirPopupWindow->show(popupMarkPoint(), true);

    connect(dirPopupWindow, &DockPopupWindow::accept, this, &FolderItem::hideDirpopupWindow, Qt::UniqueConnection);
}

void FolderItem::hideDirpopupWindow()
{
    hidePopup();
    dirPopupWindow->hide();
}

#include "folderitem.moc"