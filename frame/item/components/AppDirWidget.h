#ifndef APPDIRWIDGET_H
#define APPDIRWIDGET_H

#include "../appitem.h"

#include <QWidget>
#include <QLineEdit>
#include <QGridLayout>

class AppItem;
class AppDirWidget : public QWidget
{
Q_OBJECT

public:
    explicit AppDirWidget(QString title, QWidget *parent=nullptr);
    ~AppDirWidget() = default;

    void addAppItem(AppItem *item);
    void removeAppItem(AppItem *item);
    void prepareHide();

protected:
    void enterEvent(QEvent *e) override;
    void leaveEvent(QEvent *e) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void checkMouseLeave();

signals:
    void requestHidePopup();
    void updateTitle(QString title);

private:
    QGridLayout *m_Layout;
    QLineEdit *m_TextField;

    int m_row, m_column;

    QTimer *m_mouseLeaveTimer;

};

#endif