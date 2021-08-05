#ifndef DIR_ITEM_H
#define DIR_ITEM_H

#include "dockitem.h"
#include "appitem.h"
#include "tipswidget.h"
#include "components/AppDirWidget.h"

class AppItem;
class AppDirWidget;

class DirItem : public DockItem
{
    Q_OBJECT

public:
    explicit DirItem(QString title = nullptr, QWidget *parent=nullptr);
    ~DirItem();

    inline ItemType itemType() const override { return DirApp; }

    inline int maxCount() { return 9; }
    int currentCount();

    QString getTitle();
    void setTitle(QString title);

    void setIds(QStringList ids);
    bool hasId(QString id);

    int getIndex();
    void setIndex(int index);

    void addItem(AppItem *appItem);
    void removeItem(AppItem *appItem);

    void rerender(AppItem *appItem);

    bool isEmpty();
    QList<AppItem *> getAppList();
    AppItem *firstItem();
    AppItem *lastItem();

    Place getPlace() override { return DockPlace; }

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;

    QWidget *popupTips() override;

    void showDirPopupWindow();

public slots:
    void hideDirpopupWindow();

signals:
    void updateContent();

private:
    AppDirWidget *m_popupGrid;

    int m_index;

    QString m_title;
    QList<QString> m_ids;

    TipsWidget *m_dirTips;
    QList<AppItem *> m_appList;
};

#endif