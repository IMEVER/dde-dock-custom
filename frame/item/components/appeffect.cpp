#include "appeffect.h"

#include "util/utils.h"

const static qreal Frames[] = { 0,
                                0.327013,
                                0.987033,
                                1.77584,
                                2.61157,
                                3.45043,
                                4.26461,
                                5.03411,
                                5.74306,
                                6.37782,
                                6.92583,
                                7.37484,
                                7.71245,
                                7.92557,
                                8, 7.86164,
                                7.43184,
                                6.69344,
                                5.64142,
                                4.2916,
                                2.68986,
                                0.91694,
                                -0.91694,
                                -2.68986,
                                -4.2916,
                                -5.64142,
                                -6.69344,
                                -7.43184,
                                -7.86164,
                                -8,
                                -7.86164,
                                -7.43184,
                                -6.69344,
                                -5.64142,
                                -4.2916,
                                -2.68986,
                                -0.91694,
                                0.91694,
                                2.68986,
                                4.2916,
                                5.64142,
                                6.69344,
                                7.43184,
                                7.86164,
                                8,
                                7.93082,
                                7.71592,
                                7.34672,
                                6.82071,
                                6.1458,
                                5.34493,
                                4.45847,
                                3.54153,
                                2.65507,
                                1.8542,
                                1.17929,
                                0.653279,
                                0.28408,
                                0.0691776,
                                0,
                            };

AppEffect::AppEffect(QWidget *parent, const QPixmap &icon, DockItemManager::ActivateAnimationType type, Position position) : QGraphicsView()
    , m_parent(parent)
    , m_icon(icon)
    , m_position(position)
    , m_type(type){
    setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute( Qt::WA_TranslucentBackground);
    viewport()->setAutoFillBackground(false);
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignCenter);
    setFrameStyle(QFrame::NoFrame);
    setContentsMargins(0, 0, 0, 0);
    setRenderHints(QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_itemScene = new QGraphicsScene(this);
    setScene(m_itemScene);

    m_item = m_itemScene->addPixmap(Utils::lighterEffect(m_icon));
    m_item->setTransformationMode(Qt::SmoothTransformation);

    m_animation = new QVariantAnimation(this);
    m_animation->setDuration(1200);
    m_animation->setLoopCount(1);
    connect(m_animation, &QVariantAnimation::stateChanged, this, [this](QVariantAnimation::State newState, QVariantAnimation::State oldState) {
        if(newState == QVariantAnimation::Running)
            show();
        else if (newState == QVariantAnimation::Stopped) {
            if(m_type != DockItemManager::Scale or m_animation->direction() == QVariantAnimation::Backward)
                deleteLater();
        }
    });

    if(type == DockItemManager::Swing)
        initSwing();
    else if(type == DockItemManager::Jump)
        initJump();
    else
        initScale();
}

void AppEffect::initSwing() {
    move(m_parent->mapToGlobal(QPoint(0, 0)));

    m_item->setPos(QPointF(m_parent->rect().center()) - QPointF(m_icon.rect().center()) / m_parent->devicePixelRatioF());
    m_item->setTransformOriginPoint(m_parent->rect().center() + QPoint(0, 18));

    m_itemScene->setSceneRect(m_parent->rect());
    setSceneRect(m_parent->rect());
    setFixedSize(m_parent->rect().size());

    m_animation->setEasingCurve(QEasingCurve::Linear);
    connect(m_animation, &QVariantAnimation::valueChanged, [this](const QVariant &value){
        m_item->setRotation(value.toReal());
    });

    for (int i(0); i != 60; ++i)
        m_animation->setKeyValueAt(i / 59.0, Frames[i]);
}

void AppEffect::initJump() {

    m_item->setPos(QPointF(m_parent->rect().center()) - QPointF(m_icon.rect().center()) / m_parent->devicePixelRatioF());

    int width = m_parent->rect().width();
    int height = m_parent->rect().height();
    if(m_position == Left || m_position == Right)
        width *= 2;
    else
        height *= 2;

    m_itemScene->setSceneRect(0, 0, width, height);
    setSceneRect(0, 0, width, height);
    setFixedSize(QSize(width, height));

    m_animation->setEasingCurve(QEasingCurve::OutBounce);
    connect(m_animation, &QVariantAnimation::valueChanged, [this](const QVariant &value){
        m_item->setPos(value.toPointF());
    });

    if(m_position == Left) {
        m_animation->setStartValue(QPoint(0, 0));
        m_animation->setKeyValueAt(.5, QPoint(width/2, 0));
        m_animation->setEndValue(QPoint(0, 0));
    } else if(m_position == Right) {
        m_animation->setStartValue(QPoint(width/2, 0));
        m_animation->setKeyValueAt(.5, QPoint(0, 0));
        m_animation->setEndValue(QPoint(width/2, 0));
    } else {
        m_animation->setStartValue(QPoint(0, height/2));
        m_animation->setKeyValueAt(.5, QPoint(0, 0));
        m_animation->setEndValue(QPoint(0, height/2));
    }

    if (m_position == Bottom)
        move(m_parent->mapToGlobal(QPoint(0, 0)) - QPoint{0, this->height()-m_parent->height()});
    else if (m_position == Right)
        move(m_parent->mapToGlobal(QPoint(0, 0)) - QPoint{this->width()-m_parent->width(), 0});
    else if(m_position == Left)
        move(m_parent->mapToGlobal(QPoint(0, 0)));
}

void AppEffect::initScale() {
    setWindowFlag(Qt::WindowStaysOnBottomHint, true);

    const qreal scale = .2;
    const int oldWidth = m_parent->width();
    const int newWidth = oldWidth*(1 + scale);
    QPointF pos = m_parent->rect().center() - m_icon.rect().center() / m_parent->devicePixelRatioF();

    if (m_position == Bottom) {
        m_itemScene->setSceneRect(0, 0, oldWidth, newWidth);
        setFixedSize(QSize(oldWidth, newWidth));
        move(m_parent->mapToGlobal(QPoint(0, 0)) - QPoint(0, oldWidth*scale));
        m_item->setPos(QPoint(0, newWidth) + QPoint(pos.x(), -pos.y() - m_icon.height()/m_parent->devicePixelRatioF()));
    } else if (m_position == Right) {
        m_itemScene->setSceneRect(0, 0, newWidth, oldWidth);
        setFixedSize(QSize(newWidth, oldWidth));
        move(m_parent->mapToGlobal(QPoint(0, 0)) - QPoint(oldWidth*scale, 0));
        m_item->setPos(QPoint(newWidth, 0) + QPoint(-pos.x() - m_icon.width()/m_parent->devicePixelRatioF(), pos.y()));
    } else if(m_position == Left) {
        m_itemScene->setSceneRect(0, 0, newWidth, oldWidth);
        setFixedSize(QSize(newWidth, oldWidth));
        move(m_parent->mapToGlobal(QPoint(0, 0)));
        m_item->setPos(pos);
    }

    pos = m_item->pos();

    m_animation->setDuration(300);
    m_animation->setEasingCurve(QEasingCurve::Linear);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this, pos](const QVariant &value){
        if(m_position == Bottom)
            m_item->setPos(pos + QPointF(0, -value.toDouble()));
        else if(m_position == Right)
            m_item->setPos(pos + QPointF(-value.toDouble(), 0));
        else
            m_item->setPos(pos + QPoint(value.toDouble(), 0));
    });

    m_animation->setStartValue(0.);
    m_animation->setEndValue(oldWidth*scale);
}

void AppEffect::enterEvent(QEvent *event) {
    QGraphicsView::enterEvent(event);
    if(m_type == DockItemManager::Scale)
        m_animation->setDirection(QVariantAnimation::Forward);
}

void AppEffect::leaveEvent(QEvent *event) {
    QGraphicsView::leaveEvent(event);
    if(m_type == DockItemManager::Scale) {
        m_animation->setDirection(QVariantAnimation::Backward);
        m_animation->start();
    }
}