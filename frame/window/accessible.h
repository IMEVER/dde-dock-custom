#include "accessibledefine.h"

#include "mainwindow.h"
#include "../panel/mainpanelcontrol.h"
#include "../../widgets/tipswidget.h"
#include "../util/dockpopupwindow.h"

#include "../item/appitem.h"
#include "../item/components/previewcontainer.h"
#include "../item/placeholderitem.h"
#include "../item/components/appdragwidget.h"
#include "../item/components/appsnapshot.h"
#include "../item/components/floatingpreview.h"

#include <DImageButton>
#include <DSwitchButton>
#include <DPushButton>
#include <QWidget>
#include <QLabel>

DWIDGET_USE_NAMESPACE

// 添加accessible
SET_FORM_ACCESSIBLE(MainWindow, "mainwindow")
SET_BUTTON_ACCESSIBLE(MainPanelControl, "mainpanelcontrol")
SET_LABEL_ACCESSIBLE(TipsWidget, "tips")
SET_FORM_ACCESSIBLE(DockPopupWindow, "popupwindow")
SET_BUTTON_ACCESSIBLE(AppItem, "appitem")
SET_BUTTON_ACCESSIBLE(PreviewContainer, "previewcontainer")
SET_BUTTON_ACCESSIBLE(PlaceholderItem, "placeholderitem")
SET_BUTTON_ACCESSIBLE(AppDragWidget, "appdragwidget")
SET_BUTTON_ACCESSIBLE(AppSnapshot, "appsnapshot")
SET_BUTTON_ACCESSIBLE(FloatingPreview, "floatingpreview")
QAccessibleInterface *accessibleFactory(const QString &classname, QObject *object)
{
    QAccessibleInterface *interface = nullptr;

    USE_ACCESSIBLE(classname, MainWindow);
    USE_ACCESSIBLE(classname, MainPanelControl);
    USE_ACCESSIBLE(classname, TipsWidget);
    USE_ACCESSIBLE(classname, DockPopupWindow);
    USE_ACCESSIBLE(classname, AppItem);
    USE_ACCESSIBLE(classname, PreviewContainer);
    USE_ACCESSIBLE(classname, PlaceholderItem);
    USE_ACCESSIBLE(classname, AppDragWidget);
    USE_ACCESSIBLE(classname, AppSnapshot);
    USE_ACCESSIBLE(classname, FloatingPreview);
    USE_ACCESSIBLE(classname, QWidget);
    USE_ACCESSIBLE(classname, QLabel);
    USE_ACCESSIBLE_BY_OBJECTNAME(QString(classname).replace("Dtk::Widget::", ""), DImageButton, "closebutton-2d");
    USE_ACCESSIBLE_BY_OBJECTNAME(QString(classname).replace("Dtk::Widget::", ""), DImageButton, "closebutton-3d");
    USE_ACCESSIBLE_BY_OBJECTNAME(QString(classname).replace("Dtk::Widget::", ""), DSwitchButton, "");

    return interface;
}
