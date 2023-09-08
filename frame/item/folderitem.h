#ifndef FOLDERITEM_H
#define FOLDERITEM_H

#include "dockitem.h"

class FolderWidget;

class FolderItem : public DockItem {
    Q_OBJECT
public:
    FolderItem(QString path, QWidget *parent=nullptr);
    ~FolderItem();
    inline QString getPath() const { return m_path; }
    inline ItemType itemType() const override {return Plugins;}

    void showDirPopupWindow();
    void hideDirpopupWindow();

protected:
    void leaveEvent(QEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

    inline QString popupTips() override { return m_path; }
    void invokedMenuItem(const QString &itemId, const bool checked) Q_DECL_OVERRIDE;
    const QString contextMenu() const Q_DECL_OVERRIDE;

signals:
    void undocked();

private:
    const QString m_path;
    FolderWidget *m_popupGrid;
};

#endif