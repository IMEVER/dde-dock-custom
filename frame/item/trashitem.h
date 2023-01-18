#ifndef TRASHITEM_H
#define TRASHITEM_H

#include "dockitem.h"
#include "tipswidget.h"

class TrashItem : public DockItem
{
    Q_OBJECT

public:
    explicit TrashItem(QWidget *parent = nullptr);

    inline ItemType itemType() const override {return Plugins;}
    void refershIcon() override;

private:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

    QWidget *popupTips() override;

    void invokedMenuItem(const QString &itemId, const bool checked) Q_DECL_OVERRIDE;
    const QString contextMenu() const Q_DECL_OVERRIDE;

private:
    QPixmap m_icon;
    TipsWidget *m_tips;
};

#endif // TRASHITEM_H