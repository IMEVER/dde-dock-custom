#include "AppDirWidget.h"

#include <QDrag>
#include <QMouseEvent>

static bool autoHide = true;

AppDirWidget::AppDirWidget(QString title, QWidget *parent) : QWidget(parent)
, m_row(0)
, m_column(0)
, m_mouseLeaveTimer(new QTimer(this))
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 5, 10, 5);

    m_TextField = new QLineEdit(this);
    m_TextField->installEventFilter(this);
    m_TextField->setFixedWidth(200);
    m_TextField->setAlignment(Qt::AlignCenter);
    m_TextField->setText(title);
    m_TextField->setEnabled(false);
    m_TextField->clearFocus();

    connect(m_TextField, &QLineEdit::editingFinished, [ this ]{
        QString title = m_TextField->text();
        if(!title.isEmpty())
        {
            m_TextField->setEnabled(false);
            m_TextField->clearFocus();
            Q_EMIT updateTitle(title);
        }
    });

    vbox->addWidget(m_TextField, 0, Qt::AlignHCenter);

    m_Layout = new QGridLayout;
    m_Layout->setVerticalSpacing(10);
    vbox->addLayout(m_Layout);

    m_mouseLeaveTimer->setSingleShot(true);
    m_mouseLeaveTimer->setInterval(100);
    connect(m_mouseLeaveTimer, &QTimer::timeout, this, &AppDirWidget::checkMouseLeave);

    // setFixedWidth(360);
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    hide();
}

void AppDirWidget::addAppItem(AppItem *item)
{
    m_Layout->addWidget(item, m_row, m_column++);
    item->installEventFilter(this);

    if(m_column == 4)
    {
        m_row++;
        m_column = 0;
    }

    connect(item, &DockItem::requestWindowAutoHide, this, [ this ](bool hide) {
        autoHide = hide;
        if(!hide)
            m_mouseLeaveTimer->stop();
    });
    m_Layout->update();
}

void AppDirWidget::removeAppItem(AppItem *item)
{
    bool bingo = false;
    int row=0, column=0;
    while (row < m_row && column < 4)
    {
        if(bingo)
        {
            if(row < m_row || (row == m_row && column <= m_column))
            {
                m_Layout->addItem(m_Layout->itemAtPosition(row, column), column == 0 ? row - 1 : row, column == 0 ? 3 : column - 1);
            }
        }
        else if(m_Layout->itemAtPosition(row, column)->widget() == item)
        {
            m_Layout->removeWidget(item);
            bingo = true;
        }

        column ++;
        if(column == 4)
        {
            row++;
            column = 0;
        }
    }

    if(bingo)
    {
        m_column = m_column == 0 ? 3 : m_column -1;
        if(m_column == 3)
            m_row--;
    }

    item->removeEventFilter(this);

    disconnect(item, &DockItem::requestWindowAutoHide, this, 0);
    m_Layout->update();
}

void AppDirWidget::prepareHide()
{
    if(autoHide)
        m_mouseLeaveTimer->start();
}

void AppDirWidget::checkMouseLeave()
{
    if(!underMouse())
        emit requestHidePopup();
}

void AppDirWidget::enterEvent(QEvent *e)
{
    QWidget::enterEvent(e);
    m_mouseLeaveTimer->stop();
}

void AppDirWidget::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    if(!QRect(mapToGlobal({0, 0}), size()).contains(QCursor::pos()) && autoHide)
        m_mouseLeaveTimer->start();
}

bool AppDirWidget::eventFilter(QObject* object, QEvent* event)
{
    if(object == m_TextField && event->type() == QEvent::MouseButtonPress)
    {
        m_TextField->setEnabled(true);
        // return true;
    }

    if(event->type() == QEvent::MouseMove)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent && mouseEvent->buttons() == Qt::LeftButton)
        {
            if(DockItem *item = qobject_cast<DockItem *>(object)) {
                QPixmap pixmap = item->grab();
                QDrag *drag = new QDrag(item);
                drag->setPixmap(pixmap);

                drag->setHotSpot(pixmap.rect().center() / pixmap.devicePixelRatioF());
                drag->setMimeData(new QMimeData);
                drag->exec(Qt::MoveAction);
            }
        }
    }
    return QWidget::eventFilter(object, event);
}