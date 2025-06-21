// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WINDOWIDENTIFY_H
#define WINDOWIDENTIFY_H

#include "taskmanager/entry.h"
#include "windowpatterns.h"
#include "windowinfok.h"
#include "windowinfox.h"

#include <QObject>
#include <QVector>
#include <QMap>

class AppInfo;
class TaskManager;

typedef AppInfo *(*IdentifyFunc)(TaskManager *, WindowInfoX*);

// 应用窗口识别类
class WindowIdentify : public QObject
{
    Q_OBJECT

public:
    explicit WindowIdentify(TaskManager *_taskmanager, QObject *parent = nullptr);

    AppInfo *identifyWindow(WindowInfoBase *winInfo);
    AppInfo *identifyWindowX11(WindowInfoX *winInfo);
    AppInfo *identifyWindowWayland(WindowInfoK *winInfo);

    static AppInfo *identifyWindowByPidEnv(TaskManager *_dock, WindowInfoX *winInfo);
    static AppInfo *identifyWindowByCmdlineTurboBooster(TaskManager *_dock, WindowInfoX *winInfo);
    static AppInfo *identifyWindowByCmdlineXWalk(TaskManager *_dock, WindowInfoX *winInfo);
    static AppInfo *identifyWindowByFlatpakAppID(TaskManager *_dock, WindowInfoX *winInfo);
    static AppInfo *identifyWindowByCrxId(TaskManager *_dock, WindowInfoX *winInfo);
    static AppInfo *identifyWindowByRule(TaskManager *_dock, WindowInfoX *winInfo);
    static AppInfo *identifyWindowByBamf(TaskManager *_dock, WindowInfoX *winInfo);
    static AppInfo *identifyWindowByPid(TaskManager *_dock, WindowInfoX *winInfo);
    static AppInfo *identifyWindowByScratch(TaskManager *_dock, WindowInfoX *winInfo);
    static AppInfo *identifyWindowByGtkAppId(TaskManager *_dock, WindowInfoX *winInfo);
    static AppInfo *identifyWindowByWmClass(TaskManager *_dock, WindowInfoX *winInfo);

private:
    AppInfo *fixAutostartAppInfo(QString fileName);

private:
    TaskManager *m_taskmanager;
    QList<QPair<QString, IdentifyFunc>> m_identifyWindowFuns;
};

#endif // IDENTIFYWINDOW_H
