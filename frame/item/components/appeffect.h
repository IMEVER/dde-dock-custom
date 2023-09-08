#ifndef SWINGEFFECT
#define SWINGEFFECT

#include "interfaces/constants.h"
#include "window/dockitemmanager.h"

#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QVariantAnimation>

class AppEffect : public QGraphicsView {
    Q_OBJECT
    public:
        static QVariantAnimation* SwingEffect(QWidget *parent, const QPixmap &icon)
        {
            AppEffect *view = new AppEffect(parent, icon, DockItemManager::Swing);
            return view->m_animation;
        }

        static QVariantAnimation* JumpEffect(QWidget *parent, const QPixmap &icon, Position position)
        {
            AppEffect *view = new AppEffect(parent, icon, DockItemManager::Jump, position);
            return view->m_animation;
        }

        static QVariantAnimation* ScaleEffect(QWidget *parent, const QPixmap &icon, Position position)
        {
            AppEffect *view = new AppEffect(parent, icon, DockItemManager::Scale, position);
            return view->m_animation;
        }

        static QVariantAnimation* PopupEffect(QWidget *parent, const QPixmap &icon, Position position)
        {
            AppEffect *view = new AppEffect(parent, icon, DockItemManager::Popup, position);
            return view->m_animation;
        }
    protected:
        void enterEvent(QEvent *event) override;
        void leaveEvent(QEvent *event) override;
        bool eventFilter(QObject *object, QEvent *event) override;

    private:
        AppEffect(QWidget *parent, const QPixmap &icon, DockItemManager::ActivateAnimationType type, Position position=Bottom);

        void initSwing();
        void initJump();
        void initScale();
        void initPopup();

private:
    QWidget *m_parent;
    const QPixmap m_icon;
    const Position m_position;
    const DockItemManager::ActivateAnimationType m_type;
    QGraphicsScene *m_itemScene;
    QGraphicsPixmapItem *m_item;
    QVariantAnimation *m_animation;
};
#endif /* ifndef SWINGEFFECT */
