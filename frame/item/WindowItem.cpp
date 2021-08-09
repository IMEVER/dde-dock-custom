#include "WindowItem.h"

#include "components/appsnapshot.h"


WindowItem::WindowItem(WId wId, WindowInfo windowInfo, QWidget *parent) :
    DockItem(parent)
    , m_WId(wId)
    , m_windowInfo(windowInfo)
{

}

WindowItem::~WindowItem()
{

}

void WindowItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);

    QPainter painter(this);
    if (!painter.isActive())
        return;
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    painter.drawPixmap(windowIconPosition(), m_windowIcon);
}

QPoint WindowItem::windowIconPosition()
{
    const auto ratio = devicePixelRatioF();
    const QRectF itemRect = rect();
    const QRectF iconRect = m_windowIcon.rect();
    const qreal iconX = itemRect.center().x() - iconRect.center().x() / ratio;
    const qreal iconY = itemRect.center().y() - iconRect.center().y() / ratio;

    return QPoint(iconX, iconY);
}

void WindowItem::refreshIcon()
{

    const int iconSize = qMin(width(), height());

    // m_appIcon = ImageUtil::getIcon(icon, iconSize * 0.85, devicePixelRatioF());

    update();
}