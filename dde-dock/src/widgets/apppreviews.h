#ifndef APPPREVIEWS_H
#define APPPREVIEWS_H

#include <QLabel>
#include <QDebug>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>

#include "windowpreview.h"
#include "abstractdockitem.h"
#include "dbus/dbusclientmanager.h"
#include "interfaces/dockconstants.h"

class AppPreviewFrame : public QWidget
{
    Q_OBJECT

public:
    explicit AppPreviewFrame(const QString &title,int xid, QWidget *parent=0);
    ~AppPreviewFrame();
    void addPreview(int xid);
    void setTitle(const QString &t);

protected:
    void mousePressEvent(QMouseEvent *);
    void enterEvent(QEvent *);
    void leaveEvent(QEvent *);

signals:
    void close(int xid);
    void activate(int xid);

private:
    void addCloseButton();
    void showCloseButton();
    void hideCloseButton();

private:
    WindowPreview * m_preview = NULL;
    QPushButton *m_cb = NULL;
    int xidValue;
    const int BUTTON_SIZE = Dock::APP_PREVIEW_CLOSEBUTTON_SIZE;
    const int TITLE_HEIGHT = 25;
    const int PREVIEW_BORDER_WIDTH = 3;
};

class AppPreviews : public QWidget
{
    Q_OBJECT
public:
    explicit AppPreviews(QWidget *parent = 0);
    ~AppPreviews();

    void addItem(const QString &title,int xid);

protected:
    void leaveEvent(QEvent *);

signals:
    void requestHide();
    void sizeChanged();

public slots:
    void removePreview(int xid);
    void activatePreview(int xid);
    void clearUpPreview();

private:
    DBusClientManager *m_clientManager = new DBusClientManager(this);
    QHBoxLayout *m_mainLayout = NULL;
    QList<int> m_xidList;
    bool m_isClosing = false;
};

#endif // APPPREVIEWS_H