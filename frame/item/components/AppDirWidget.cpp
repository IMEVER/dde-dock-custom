#include "AppDirWidget.h"

#include <QDrag>

AppDirWidget::AppDirWidget(QString title, QWidget *parent) : QWidget(parent)
, m_row(0)
, m_column(0)
, m_mouseLeaveTimer(new QTimer(this))
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    QHBoxLayout *hbox = new QHBoxLayout;

    m_TextField = new QLineEdit(this);
    m_TextField->installEventFilter(this);
    m_TextField->setFixedWidth(200);
    m_TextField->setAlignment(Qt::AlignCenter);
    m_TextField->setText(title);
    m_TextField->setEnabled(false);
    m_TextField->clearFocus();

    connect(m_TextField, &QLineEdit::editingFinished, [ this ](){
        QString title = m_TextField->text();
        if(!title.isEmpty())
        {
            m_TextField->setEnabled(false);
            m_TextField->clearFocus();
            Q_EMIT updateTitle(title);
        }
    });

    hbox->addWidget(m_TextField, 0, Qt::AlignCenter);
    vbox->addLayout(hbox);

    m_Layout = new QGridLayout;
    m_Layout->setVerticalSpacing(10);
    vbox->addLayout(m_Layout);

    setLayout(vbox);

    setFixedWidth(360);
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setContentsMargins(10, 5, 10, 5);
    hide();

    m_mouseLeaveTimer->setSingleShot(true);
    m_mouseLeaveTimer->setInterval(100);

    connect(m_mouseLeaveTimer, &QTimer::timeout, this, &AppDirWidget::checkMouseLeave);
}

AppDirWidget::~AppDirWidget()
{

}

void AppDirWidget::addAppItem(AppItem *item)
{
    m_Layout->addWidget(item, m_row, m_column++);
    item->installEventFilter(this);

    if(m_column == 4)
    {
        m_row++;
        m_column = 0;

        // setFixedHeight(60 * (m_row + 2));
    }

    connect(item, &AppItem::enterPreviewWindow, [ this ]{ m_mouseLeaveTimer->stop(); });
    connect(item, &AppItem::leavePreviewWindow, [ this ]{ checkMouseLeave(); });
    update();
}

void AppDirWidget::removeAppItem(AppItem *item)
{
    m_Layout->removeWidget(item);
    item->removeEventFilter(this);

    disconnect(item, &AppItem::enterPreviewWindow, 0, 0);
    disconnect(item, &AppItem::leavePreviewWindow, 0, 0);
    update();
}

void AppDirWidget::prepareHide()
{
    m_mouseLeaveTimer->start();
}

void AppDirWidget::checkMouseLeave()
{
    const bool hover = underMouse();

    if (hover)
        return;

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
    m_mouseLeaveTimer->start();
}

void AppDirWidget::dragEnterEvent(QDragEnterEvent *e)
{
    QWidget::dragEnterEvent(e);
}

void AppDirWidget::dragLeaveEvent(QDragLeaveEvent *e)
{
    QWidget::dragLeaveEvent(e);
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
            DockItem *item = qobject_cast<DockItem *>(object);
            if(item)
            {
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