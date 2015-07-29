#include "docklayout.h"
#include "abstractdockitem.h"

DockLayout::DockLayout(QWidget *parent) :
    QWidget(parent)
{
    this->setAttribute(Qt::WA_TranslucentBackground);
}

void DockLayout::addItem(AbstractDockItem *item)
{
    insertItem(item,appList.count());
}

void DockLayout::insertItem(AbstractDockItem *item, int index)
{
    item->setParent(this);
    item->show();
    int appCount = appList.count();
    index = index > appCount ? appCount : (index < 0 ? 0 : index);

    appList.insert(index,item);
    connect(item, &AbstractDockItem::mouseRelease, this, &DockLayout::slotItemRelease);
    connect(item, &AbstractDockItem::dragStart, this, &DockLayout::slotItemDrag);
    connect(item, &AbstractDockItem::dragEntered, this, &DockLayout::slotItemEntered);
    connect(item, &AbstractDockItem::dragExited, this, &DockLayout::slotItemExited);
    connect(item, &AbstractDockItem::widthChanged, this, &DockLayout::relayout);
    connect(item, &AbstractDockItem::posChanged, this, &DockLayout::frameUpdate);
    connect(item, &AbstractDockItem::frameUpdate, this, &DockLayout::frameUpdate);
    connect(item, &AbstractDockItem::moveAnimationFinished, [=]{
        m_animationItemCount --;
    });

    relayout();
}

void DockLayout::removeItem(int index)
{
    delete appList.takeAt(index);
    relayout();
}

void DockLayout::moveItem(int from, int to)
{
    appList.move(from,to);
    relayout();
}

void DockLayout::setSpacing(qreal spacing)
{
    this->itemSpacing = spacing;
}

void DockLayout::setVerticalAlignment(DockLayout::VerticalAlignment value)
{
    this->m_verticalAlignment = value;
}

void DockLayout::setSortDirection(DockLayout::Direction value)
{
    this->sortDirection = value;
}

void DockLayout::sortLeftToRight()
{
    if (appList.count() <= 0)
        return;

    switch (m_verticalAlignment)
    {
    case DockLayout::AlignTop:
        appList.at(0)->move(itemSpacing,0);
        break;
    case DockLayout::AlignVCenter:
        appList.at(0)->move(itemSpacing,(height() - appList.at(0)->height()) / 2);
        break;
    case DockLayout::AlignBottom:
        appList.at(0)->move(itemSpacing,height() - appList.at(0)->height());
        break;
    }
    appList.at(0)->setNextPos(appList.at(0)->pos());

    for (int i = 1; i < appList.count(); i ++)
    {
        AbstractDockItem * frontItem = appList.at(i - 1);
        AbstractDockItem * toItem = appList.at(i);
        switch (m_verticalAlignment)
        {
        case DockLayout::AlignTop:
            toItem->move(frontItem->pos().x() + frontItem->width() + itemSpacing,0);
            break;
        case DockLayout::AlignVCenter:
            toItem->move(frontItem->pos().x() + frontItem->width() + itemSpacing,(height() - toItem->height()) / 2);
            break;
        case DockLayout::AlignBottom:
            toItem->move(frontItem->pos().x() + frontItem->width() + itemSpacing,height() - toItem->height());
            break;
        }

        appList.at(i)->setNextPos(appList.at(i)->pos());
    }
}

void DockLayout::sortRightToLeft()
{
    if (appList.count()<=0)
        return;

    switch (m_verticalAlignment)
    {
    case DockLayout::AlignTop:
        appList.at(0)->move(getContentsWidth() - itemSpacing - appList.at(0)->width(),0);
        break;
    case DockLayout::AlignVCenter:
        appList.at(0)->move(getContentsWidth() - itemSpacing - appList.at(0)->width(),(height() - appList.at(0)->height()) / 2);
        break;
    case DockLayout::AlignBottom:
        appList.at(0)->move(getContentsWidth() - itemSpacing - appList.at(0)->width(),height() - appList.at(0)->height());
        break;
    }

    for (int i = 1; i < appList.count(); i++)
    {
        AbstractDockItem *fromItem = appList.at(i - 1);
        AbstractDockItem *toItem = appList.at(i);
        switch (m_verticalAlignment)
        {
        case DockLayout::AlignTop:
            toItem->move(fromItem->x() - itemSpacing - toItem->width(),0);
            break;
        case DockLayout::AlignVCenter:
            toItem->move(fromItem->x() - itemSpacing - toItem->width(),(height() - toItem->height()) / 2);
            break;
        case DockLayout::AlignBottom:
            toItem->move(fromItem->x() - itemSpacing - toItem->width(),height() - toItem->height());
            break;
        }
    }
}

bool DockLayout::hasSpacingItemInList()
{
    if (sortDirection == RightToLeft)
        return false;
    if (appList.count() <= 1)
        return false;
    if (appList.at(0)->x() > itemSpacing)
        return true;

    for (int i = 1; i < appList.count(); i ++)
    {
        if (appList.at(i)->x() - itemSpacing != appList.at(i - 1)->x() + appList.at(i - 1)->width())
        {
            return true;
        }
    }
    return false;
}

int DockLayout::spacingItemIndex()
{
    if (sortDirection == RightToLeft)
        return -1;
    if (appList.count() <= 1)
        return -1;
    if (appList.at(0)->getNextPos().x() > itemSpacing)
        return 0;

    for (int i = 1; i < appList.count(); i ++)
    {
        if (appList.at(i)->getNextPos().x() - itemSpacing != appList.at(i - 1)->getNextPos().x() + appList.at(i - 1)->width())
        {
            return i;
        }
    }
    return -1;
}

void DockLayout::moveWithSpacingItem(int hoverIndex)
{
    int itemWidth = 0;
    if (tmpAppMap.isEmpty())
        itemWidth = DockModeData::instance()->getNormalItemWidth();
    else
        itemWidth = tmpAppMap.firstKey()->width();

    if (sortDirection == LeftToRight)
    {
        int spacintIndex = spacingItemIndex();
        if (spacintIndex == -1)
            return;

        if (spacintIndex > hoverIndex)
        {
            for (int i = hoverIndex; i < spacintIndex; i ++)
            {
                m_animationItemCount ++;
                AbstractDockItem *targetItem = appList.at(i);
                targetItem->moveWithAnimation(QPoint(targetItem->x() + itemWidth + itemSpacing,0));
            }
        }
        else
        {
            for (int i = spacintIndex; i <= hoverIndex; i ++)
            {
                m_animationItemCount ++;
                AbstractDockItem *targetItem = appList.at(i);
                targetItem->moveWithAnimation(QPoint(targetItem->x() - itemWidth - itemSpacing,0));
            }
        }
    }
    //TODO RightToLeft
}

int DockLayout::indexOf(AbstractDockItem *item)
{
    return appList.indexOf(item);
}

int DockLayout::indexOf(int x, int y)
{
    //TODO
    return 0;
}

void DockLayout::restoreTmpItem()
{
    if (tmpAppMap.isEmpty())
        return;

    AbstractDockItem * tmpItem = tmpAppMap.firstKey();
    tmpAppMap.remove(tmpItem);
    tmpItem->setVisible(true);
    if (indexOf(tmpItem) == -1)
    {
        if (movingForward)
            insertItem(tmpItem,lastHoverIndex);
        else
            insertItem(tmpItem,lastHoverIndex + 1);
    }

    emit itemDropped();
    m_animationItemCount = 0;
}

void DockLayout::relayout()
{
    switch (sortDirection)
    {
    case LeftToRight:
        sortLeftToRight();
        break;
    case RightToLeft:
        sortRightToLeft();
        break;
    default:
        break;
    }

    emit contentsWidthChange();
}

void DockLayout::clearTmpItem()
{
    if (tmpAppMap.count() > 0)
    {
        AbstractDockItem * tmpItem = tmpAppMap.firstKey();
        tmpAppMap.clear();
        tmpItem->deleteLater();
    }
}

void DockLayout::addSpacingItem()
{
    int spacingValue = 0;
    if (tmpAppMap.isEmpty())
        spacingValue = DockModeData::instance()->getNormalItemWidth();
    else
    {
        AbstractDockItem *tmpItem = tmpAppMap.firstKey();
        spacingValue = tmpItem->width();
    }

    for (int i = appList.count() -1;i >= lastHoverIndex; i-- )
    {
        AbstractDockItem *targetItem = appList.at(i);
        targetItem->setNextPos(targetItem->x() + spacingValue + itemSpacing,0);

        QPropertyAnimation *animation = new QPropertyAnimation(targetItem, "pos");
        animation->setStartValue(targetItem->pos());
        animation->setEndValue(targetItem->getNextPos());
        animation->setDuration(150 + i * 10);
        animation->setEasingCurve(QEasingCurve::OutCubic);

        animation->start();
        connect(animation, SIGNAL(finished()),this, SIGNAL(contentsWidthChange()));
    }

//    emit contentsWidthChange();
}

void DockLayout::dragoutFromLayout(int index)
{
    AbstractDockItem * tmpItem = appList.takeAt(index);
    tmpItem->setVisible(false);
    tmpAppMap.insert(tmpItem,index);

    emit contentsWidthChange();
}

int DockLayout::getContentsWidth()
{
    int tmpWidth = appList.count() * itemSpacing;
    for (int i = 0; i < appList.count(); i ++)
    {
        tmpWidth += appList.at(i)->width();
    }

    if (spacingItemIndex() != -1 && !tmpAppMap.isEmpty() && tmpAppMap.firstKey())
        tmpWidth += tmpAppMap.firstKey()->width() + itemSpacing;

    return tmpWidth;
}

int DockLayout::getItemCount()
{
    return appList.count();
}

QList<AbstractDockItem *> DockLayout::getItemList() const
{
    return appList;
}

void DockLayout::dragEnterEvent(QDragEnterEvent *event)
{
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void DockLayout::dropEvent(QDropEvent *event)
{
    AbstractDockItem *sourceItem = NULL;
    sourceItem = dynamic_cast<AbstractDockItem *>(event->source());

    if (event->mimeData()->formats().indexOf("_DEEPIN_DND") != -1 && !sourceItem)
    {
        QString jsonStr = QString(event->mimeData()->data("_DEEPIN_DND")).split("uninstall").last().trimmed();

        //Tim at both ends of the string is added to a character SOH (start of heading)
        jsonStr = jsonStr.mid(1,jsonStr.length() - 2);
        QJsonObject dataObj = QJsonDocument::fromJson(jsonStr.trimmed().toUtf8()).object();
        if (dataObj.isEmpty() || m_ddam->IsDocked(dataObj.value("id").toString()))
        {
            relayout();
            return;
        }

        m_ddam->ReqeustDock(dataObj.value("id").toString(),dataObj.value("name").toString(),dataObj.value("icon").toString(),"");

    }
    else if (event->mimeData()->formats().indexOf("_DEEPIN_DND") == -1 && sourceItem)
        restoreTmpItem();
}

void DockLayout::enterEvent(QEvent *)
{
    restoreTmpItem();
}

void DockLayout::leaveEvent(QEvent *)
{
    restoreTmpItem();
}

void DockLayout::slotItemDrag()
{
//    qWarning() << "Item draging..."<<x<<y<<item;
    AbstractDockItem *item = qobject_cast<AbstractDockItem*>(sender());

    int tmpIndex = indexOf(item);
    if (tmpIndex != -1)
    {
        lastHoverIndex = tmpIndex;
        m_lastPost = QCursor::pos();
        dragoutFromLayout(tmpIndex);

        emit dragStarted();
    }
}

void DockLayout::slotItemRelease(int, int)
{
    //outside frame,destroy it
    //inside frame,insert it
    AbstractDockItem *item = qobject_cast<AbstractDockItem*>(sender());

    item->setVisible(true);
    if (indexOf(item) == -1)
    {
        insertItem(item,lastHoverIndex);
    }
}

void DockLayout::slotItemEntered(QDragEnterEvent *)
{
    AbstractDockItem *item = qobject_cast<AbstractDockItem*>(sender());

    int tmpIndex = indexOf(item);
    lastHoverIndex = tmpIndex;
    if (!hasSpacingItemInList())
    {
        addSpacingItem();
        return;
    }

    QPoint tmpPos = QCursor::pos();

    if (tmpPos.x() - m_lastPost.x() == 0)
        return;

    bool lastState = movingForward;
    switch (sortDirection)
    {
    case LeftToRight:
        movingForward = tmpPos.x() - m_lastPost.x() < 0;
        if (movingForward != lastState && m_animationItemCount > 0)
        {
            movingForward = lastState;
            return;
        }
        break;
    case RightToLeft:
        movingForward = tmpPos.x() - m_lastPost.x() > 0;
        if (movingForward != lastState && m_animationItemCount > 0)
        {
            movingForward = lastState;
            return;
        }
        break;
    }

    m_lastPost = tmpPos;

    moveWithSpacingItem(tmpIndex);
}

void DockLayout::slotItemExited(QDragLeaveEvent *)
{

}
