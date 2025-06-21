// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ENTRIES_H
#define ENTRIES_H

#include "entry.h"
#include "../interfaces/constants.h"
#include "windowinfobase.h"

#include <QVector>
#include <QWeakPointer>
#include <qlist.h>

#define MAX_UNOPEN_RECENT_COUNT 3

class TaskManager;

// 所有应用管理类
class Entries
{
public:
    Entries(TaskManager *_taskmanager);

    const QList<Entry *> unDockedEntries() const;

    bool shouldInRecent();

    void removeLastRecent();
    void updateShowRecent();
    void updateEntriesMenu();
    void append(Entry *entry, bool beforeRecent=false);
    void remove(Entry *entry);
    void moveEntryToLast(Entry *entry);
    void insert(Entry *entry, int index);
    void move(int oldIndex, int newIndex);
    void updateOrder(QStringList &apps);
    void handleActiveWindowChanged(XWindow activeWindId);

    QString queryWindowIdentifyMethod(XWindow windowId);

    Entry *getByWindowPid(int pid);
    Entry *getByInnerId(QString innerId);
    Entry *getByWindowId(XWindow windowId);
    Entry *getEntryById(const QString &appId, bool needDocked=false);

    QList<Entry*> getEntries();
    QVector<Entry *> filterDockedEntries();

private:
    QList<Entry *> m_items;
    TaskManager *m_taskmanager;
};

#endif // ENTRIES_H
