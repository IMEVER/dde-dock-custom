#ifndef TRASHITEM_H
#define TRASHITEM_H

#include "dockitem.h"

class TrashItem : public DockItem
{
    Q_OBJECT

public:
    explicit TrashItem(QWidget *parent = nullptr);

    inline ItemType itemType() const override {return Plugins;}
    void refershIcon();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

    QString popupTips() override;

    void invokedMenuItem(const QString &itemId, const bool checked) Q_DECL_OVERRIDE;
    const QString contextMenu() const Q_DECL_OVERRIDE;

private:
    QFileSystemWatcher *m_watcher;
    int m_count;
};

#endif // TRASHITEM_H