#include "diritem.h"
#include "window/dockitemmanager.h"
#include "util/dockpopupwindow.h"

#include <QPen>

static DockPopupWindow *dirPopupWindow(nullptr);

DirItem::DirItem(QString title, QWidget *parent) : DockItem(parent)
, m_index(0)
{
    if (!dirPopupWindow) {
        dirPopupWindow = new DockPopupWindow(nullptr);
        dirPopupWindow->setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint);
        // arrowRectangle->setAttribute(Qt::WA_InputMethodEnabled, true);
        dirPopupWindow->setShadowBlurRadius(20);
        dirPopupWindow->setRadius(10);
        dirPopupWindow->setShadowYOffset(2);
        dirPopupWindow->setShadowXOffset(0);
        dirPopupWindow->setArrowWidth(18);
        dirPopupWindow->setArrowHeight(10);
    }

    setAcceptDrops(true);

    m_title = title.isNull() ? "集合" : title;

    m_popupGrid = new AppDirWidget(m_title, this);
    m_showPopupTimer = new QTimer(this);
    m_showPopupTimer->setSingleShot(true);
    m_showPopupTimer->setInterval(310);
    connect(m_showPopupTimer, &QTimer::timeout, this, &DirItem::showDirPopupWindow);

    connect(m_popupGrid, &AppDirWidget::updateTitle, [ this ](QString title){
        if(m_title != title)
        {
            m_title = title;
            Q_EMIT updateContent();
        }
    });
    connect(m_popupGrid, &AppDirWidget::requestHidePopup, this, &DirItem::hideDirpopupWindow);
}

DirItem::~DirItem()
{

}

QString DirItem::getTitle()
{
    return m_title;
}

void DirItem::setTitle(QString title)
{
    m_title = title;
}

void DirItem::setIds(QSet<QString> ids)
{
    m_ids = ids;
}

void DirItem::addId(QString id)
{
    m_ids.insert(id);
}

bool DirItem::hasId(QString id)
{
    return m_ids.contains(id);
}

int DirItem::getIndex()
{
    return parentWidget() ? parentWidget()->layout()->indexOf(this) : (m_index ? m_index : -1);
}

void DirItem::setIndex(int index)
{
    m_index = index;
}

void DirItem::addItem(AppItem *appItem)
{
    if(m_appList.contains(appItem))
        return;

    appItem->setFixedSize(QSize(DockItemManager::instance()->itemSize(), DockItemManager::instance()->itemSize()));
    m_appList.append(appItem);
    m_popupGrid->addAppItem(appItem);
    m_ids.insert(appItem->getDesktopFile());
    appItem->setDirItem(this);
    update();
}

void DirItem::removeItem(AppItem *appItem, bool removeId)
{
    m_appList.removeOne(appItem);
    m_popupGrid->removeAppItem(appItem);
    if(removeId)
        m_ids.remove(appItem->getDesktopFile());
    appItem->removeDirItem();
    update();
}

void DirItem::rerender(AppItem *appItem)
{
    m_popupGrid->addAppItem(appItem);
}

bool DirItem::isEmpty()
{
    return m_appList.isEmpty();
}

const QPoint DirItem::popupDirMarkPoint()
{
    if(DockPosition == Position::Left)
        return dirPopupWindow->pos() + QPoint(dirPopupWindow->width(), dirPopupWindow->height() / 2);
    if(DockPosition == Position::Right)
        return dirPopupWindow->pos() + QPoint(0, dirPopupWindow->height() / 2);
    else
        return dirPopupWindow->pos() + QPoint(dirPopupWindow->width() / 2, 0);
}

int DirItem::currentCount()
{
    return m_appList.count();
}

QList<AppItem *> DirItem::getAppList()
{
    return m_appList;
}

AppItem *DirItem::firstItem()
{
    return m_appList.isEmpty() ? nullptr : m_appList.first();
}

AppItem *DirItem::lastItem()
{
    return m_appList.isEmpty() ? nullptr : m_appList.last();
}

void DirItem::paintEvent(QPaintEvent *e)
{
    if(isScaling()) return ;

    DockItem::paintEvent(e);

    QPainter painter(this);
    painter.setPen(QPen(Qt::darkCyan, 2));
    // painter.setOpacity(.7);

    QRect border = rect().adjusted(2, 2, -2, -2);
    QRect line(3, 3, border.width()-2, border.height()-2);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawRoundedRect(line, 6, 6, Qt::AbsoluteSize);

    // painter.setOpacity(.9);

    int padding = 8;
    int spacing = 4;
    qreal w = (rect().width() - spacing) / 2 - padding;
    int i = 0;
    for(auto appItem : m_appList)
    {
        QPixmap pixmap = appItem->appIcon();

        QRect appRect;

        if(i == 0)
            appRect = QRect(padding, padding, w, w);
        else if (i == 1)
            appRect = QRect(padding + w + spacing, padding, w, w);
        else if (i == 2)
            appRect = QRect(padding, padding + w + spacing, w, w);
        else if (i == 3)
            appRect = QRect(padding + w + spacing, padding + w + spacing, w, w);

        painter.drawPixmap(appRect, pixmap);

        if(++i == 4)
            break;
    }
}

void DirItem::enterEvent(QEvent *e)
{
    DockItem::enterEvent(e);
    m_showPopupTimer->start();
}

void DirItem::leaveEvent(QEvent *e)
{
    DockItem::leaveEvent(e);

    m_showPopupTimer->stop();
    m_popupGrid->prepareHide();
}

void DirItem::mouseReleaseEvent(QMouseEvent *e)
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

void DirItem::dragEnterEvent(QDragEnterEvent *e)
{
    DockItem::dragEnterEvent(e);

    if(e->source())
        return e->ignore();

    // ignore request dock event
    QString draggingMimeKey = e->mimeData()->formats().contains("RequestDock") ? "RequestDock" : "text/plain";
    if (QMimeDatabase().mimeTypeForFile(e->mimeData()->data(draggingMimeKey)).name() == "application/x-desktop")
        return e->ignore();

    e->accept();
}

void DirItem::dragMoveEvent(QDragMoveEvent *e)
{
    DockItem::dragMoveEvent(e);

    if(!m_popupGrid->isVisible())
        showDirPopupWindow();
}

void DirItem::showDirPopupWindow()
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

    connect(dirPopupWindow, &DockPopupWindow::accept, this, &DirItem::hideDirpopupWindow, Qt::UniqueConnection);
}

void DirItem::hideDirpopupWindow()
{
    hidePopup();
    dirPopupWindow->hide();
}
