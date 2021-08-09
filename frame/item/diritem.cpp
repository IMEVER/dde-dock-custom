#include "diritem.h"

#include "../util/docksettings.h"

#include <QPen>

static QPointer<DockPopupWindow> dirPopupWindow(nullptr);

DirItem::DirItem(QString title, QWidget *parent) : DockItem(parent)
, m_index(0)
, m_dirTips(new TipsWidget(this))
{

    if (dirPopupWindow.isNull()) {
        DockPopupWindow *arrowRectangle = new DockPopupWindow(nullptr);
        arrowRectangle->setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint);
        // arrowRectangle->setAttribute(Qt::WA_InputMethodEnabled, true);
        arrowRectangle->setShadowBlurRadius(20);
        arrowRectangle->setRadius(10);
        arrowRectangle->setShadowYOffset(2);
        arrowRectangle->setShadowXOffset(0);
        arrowRectangle->setArrowWidth(18);
        arrowRectangle->setArrowHeight(10);
        arrowRectangle->setObjectName("dirpopup");
        dirPopupWindow = arrowRectangle;
    }

    setAcceptDrops(true);

    m_title = title.isNull() ? "集合" : title;

    m_dirTips->setText(m_title);
    m_dirTips->hide();

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

void DirItem::setIds(QStringList ids)
{
    m_ids = ids;
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
    appItem->setFixedSize(QSize(DockSettings::Instance().dockWindowSize(), DockSettings::Instance().dockWindowSize()));
    m_appList.append(appItem);
    m_popupGrid->addAppItem(appItem);
    m_ids.append(appItem->getDesktopFile());
    appItem->setDirItem(this);
    update();
}

void DirItem::removeItem(AppItem *appItem)
{
    m_appList.removeOne(appItem);
    m_popupGrid->removeAppItem(appItem);
    m_ids.removeAll(appItem->getDesktopFile());
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

QWidget * DirItem::popupTips()
{
    return nullptr;
}

const QPoint DirItem::popupDirMarkPoint()
{
    if(DockPosition == Position::Left)
        return dirPopupWindow.data()->pos() + QPoint(dirPopupWindow.data()->width(), dirPopupWindow.data()->height() / 2);
    if(DockPosition == Position::Right)
        return dirPopupWindow.data()->pos() + QPoint(0, dirPopupWindow.data()->height() / 2);
    else
        return dirPopupWindow.data()->pos() + QPoint(dirPopupWindow.data()->width() / 2, 0);
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
    DockItem::paintEvent(e);

    QPainter painter(this);
    // painter.setPen(QPen(Qt::darkYellow, 2));
    painter.setPen(QPen(Qt::green, 2));
    painter.setOpacity(.7);

    QRect border = rect();
    QRect line(border.width() * .1, border.height() * .1, border.width() * .8, border.height() * .8);

    painter.drawRoundedRect(line, 10, 10, Qt::AbsoluteSize);

    int padding = border.width() * .1 + 5;
    int spacing = 4;
    qreal w = (border.width() - spacing) / 2 - padding;
    int i = 0;
    for(auto appItem : m_appList)
    {
        QPixmap pixmap = appItem->appIcon();
        if(pixmap.isNull())
            pixmap = QPixmap(":/icons/resources/application-x-desktop.svg");

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
    if(m_showPopupTimer->isActive())
        m_showPopupTimer->stop();
    m_popupGrid->prepareHide();
}

void DirItem::mousePressEvent(QMouseEvent *e)
{
    DockItem::mousePressEvent(e);
}

void DirItem::mouseReleaseEvent(QMouseEvent *e)
{
    if(!m_popupGrid->isVisible())
    {
        hidePopup();
        showDirPopupWindow();
    }
    else
    {
        hideDirpopupWindow();
    }

    DockItem::mouseReleaseEvent(e);
}

void DirItem::dragEnterEvent(QDragEnterEvent *e)
{
    DockItem::dragEnterEvent(e);

    if(e->source())
        return e->ignore();

    // ignore request dock event
    QString draggingMimeKey = e->mimeData()->formats().contains("RequestDock") ? "RequestDock" : "text/plain";
    if (QMimeDatabase().mimeTypeForFile(e->mimeData()->data(draggingMimeKey)).name() == "application/x-desktop") {
        return e->ignore();
    }

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
    DockPopupWindow *popup = dirPopupWindow.data();

    emit requestWindowAutoHide(false);

    QWidget *lastContent = popup->getContent();
    if (lastContent)
        lastContent->setVisible(false);

    switch (DockPosition) {
    case Top:   popup->setArrowDirection(DockPopupWindow::ArrowTop);     break;
    case Bottom: popup->setArrowDirection(DockPopupWindow::ArrowBottom);  break;
    case Left:  popup->setArrowDirection(DockPopupWindow::ArrowLeft);    break;
    case Right: popup->setArrowDirection(DockPopupWindow::ArrowRight);   break;
    }
    popup->resize(m_popupGrid->sizeHint());
    popup->setContent(m_popupGrid);

    const QPoint p = popupMarkPoint();
    if (!popup->isVisible())
        QMetaObject::invokeMethod(popup, "show", Qt::QueuedConnection, Q_ARG(QPoint, p), Q_ARG(bool, false));
    else
        popup->show(p, false);

    connect(popup, &DockPopupWindow::accept, this, &DirItem::hideDirpopupWindow, Qt::UniqueConnection);
}

void DirItem::hideDirpopupWindow()
{
    if (!dirPopupWindow->isVisible())
        return;

    disconnect(dirPopupWindow.data(), &DockPopupWindow::accept, this, &DirItem::hideDirpopupWindow);

    hidePopup();

    dirPopupWindow->hide();
}
