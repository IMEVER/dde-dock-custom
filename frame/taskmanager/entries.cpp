// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "entries.h"
#include "taskmanager.h"
#include "../util/docksettings.h"
#include "taskmanager/windowinfobase.h"

#include <QList>
#include <algorithm>
#include <iterator>

Entries::Entries(TaskManager *_taskmanager)
 : m_taskmanager(_taskmanager)
{
}

QVector<Entry *> Entries::filterDockedEntries()
{
    QVector<Entry *> ret;
    for (auto entry : m_items) {
        if (entry->isValid() && entry->getIsDocked()) ret.push_back(entry);
    }
    return ret;
}

Entry *Entries::getByInnerId(QString innerId)
{
    Entry *ret = nullptr;
    for (auto &entry : m_items) {
        if (entry->getInnerId() == innerId)
            ret = entry;
    }

    return ret;
}

void Entries::append(Entry *entry, bool beforeRecent)
{
    int index = -1;
    if(beforeRecent)
        for(int i=m_items.size()-1; i >= 0; i--) {
            if(m_items.at(i)->hasWindow() || m_items.at(i)->getIsDocked()) {
                index = i+1;
                break;
            }
        }

    insert(entry, index);
}

void Entries::insert(Entry *entry, int index)
{
    if(index != -1 and m_items.indexOf(entry) == index) return;

    m_items.removeAll(entry);

    if (index < 0 || index >= m_items.size()) {
        // append
        index = m_items.size();
        m_items.push_back(entry);
    } else {
        // insert
        m_items.insert(index, entry);
    }

    Q_EMIT m_taskmanager->entryAdded(entry, index);
}

void Entries::move(int oldIndex, int newIndex)
{
    if (oldIndex == newIndex || oldIndex < 0 || newIndex < 0 || oldIndex >= m_items.size() || newIndex >= m_items.size())
        return;

    m_items.swapItemsAt(oldIndex, newIndex);
}

void Entries::updateOrder(QStringList &apps) {
    std::sort(m_items.begin(), m_items.end(), [&apps](Entry *first, Entry *second) {
        return apps.indexOf(first->getId()) <= apps.indexOf(second->getId());
    });
}

Entry *Entries::getByWindowPid(int pid)
{
    Entry *ret = nullptr;
    for (auto &entry : m_items) {
        if (entry->getWindowInfoByPid(pid)) {
            ret = entry;
            break;
        }
    }

    return ret;
}

Entry *Entries::getByWindowId(XWindow windowId)
{
    Entry *ret = nullptr;
    for (auto &entry : m_items) {
        if (entry->getWindowInfoByWinId(windowId)) {
            ret = entry;
            break;
        }
    }

    return ret;
}

QList<Entry*> Entries::getEntries()
{
    QList<Entry*> list;
    auto showRec = DockSettings::instance()->showRecent();
    // 如果是高效模式或者没有开启显示最近应用的功能，那么未驻留并且没有子窗口的就不显示
    // 换句话说，只显示已经驻留或者有子窗口的应用
    for (auto item : m_items)
        if (showRec or item->getIsDocked() or item->hasWindow())
            list << item;

    return list;
}

Entry *Entries::getEntryById(const QString &appId, bool needDocked)
{
    Entry *ret = nullptr;
    for (auto entry : m_items) {
        if(appId == entry->getId()) {
            if (!needDocked or (entry->isValid() and entry->getIsDocked())) {
                ret = entry;
                break;
            }
        }
    }

    return ret;
}

QString Entries::queryWindowIdentifyMethod(XWindow windowId)
{
    QString ret;
    for (auto entry : m_items) {
        auto window = entry->getWindowInfoByWinId(windowId);
        if (window) {
            auto app = window->getAppInfo();
            ret = app ? app->getIdentifyMethod() : "Failed";
            break;
        }
    }

    return ret;
}

void Entries::handleActiveWindowChanged(XWindow activeWindId)
{
    for (auto entry : m_items) {
        if(auto windowInfo = entry->getWindowInfoByWinId(activeWindId)) {
            entry->setCurrentWindowInfo(windowInfo);
            entry->setPropIsActive(true);
            entry->updateName();
            entry->updateIcon();
        } else {
            entry->setPropIsActive(false);
        }
    }
}

void Entries::updateEntriesMenu()
{
    for (auto entry : m_items)
        entry->updateMenu();
}

const QList<Entry *> Entries::unDockedEntries() const
{
    QList<Entry *> entrys;
    for (Entry *entry : m_items) {
        if (!entry->isValid() || entry->getIsDocked())
            continue;

        entrys << entry;
    }

    return entrys;
}

void Entries::moveEntryToLast(Entry *entry)
{
    if (m_items.contains(entry)) {
        m_items.removeOne(entry);
        m_items << entry;
    }
}

void Entries::remove(Entry *entry)
{
    m_items.removeAll(entry);
    QTimer::singleShot(1000, entry, &Entry::deleteLater);

    Q_EMIT m_taskmanager->entryRemoved(entry->getId());
}

bool Entries::shouldInRecent()
{
    // 如果当前移除的应用是未驻留应用，则判断未驻留应用的数量是否小于等于3，则让其始终显示
    int count = 0;
    for (Entry *entry : m_items)
        if (entry->isValid() && !entry->getIsDocked() && !entry->hasWindow())
            count++;

    // 如果当前未驻留应用的数量小于3个，则认为后续的应用应该显示到最近打开应用
    return count <= MAX_UNOPEN_RECENT_COUNT;
}

void Entries::removeLastRecent()
{
    // 先查找最近使用的应用，删除没有使用的
    int unDockCount = 0;
    Entry *unDockEntry = nullptr;
    QList<Entry *> removeEntrys;

    for (auto entry : m_items) {
        // 此处只移除没有子窗口的图标
        if (!entry->getIsDocked() and !entry->hasWindow()) {
            if (!entry->isValid())
                removeEntrys << entry; // 如果应用已经被卸载，那么需要删除
            else if(!unDockEntry || unDockEntry->lastUpdateTime() > entry->lastUpdateTime())
                unDockEntry = entry;
            unDockCount++;
        }
    }
    if (unDockCount > MAX_UNOPEN_RECENT_COUNT && unDockEntry)
        // 只有当最近使用区域的图标大于等于某个数值（3）的时候，并且存在没有子窗口的Entry，那么就移除该Entry
        removeEntrys << unDockEntry;

    for (Entry *entry : removeEntrys)
        remove(entry);
}

void Entries::updateShowRecent()
{
    if(DockSettings::instance()->showRecent() == false) {
        // 如果是隐藏最近打开的应用，则发送移除的信号
        QList<Entry*> list;
        for (Entry *entry : m_items)
            // 已经驻留的或者有子窗口的本来就在任务栏上面，无需发送信号
            if (!entry->getIsDocked() and entry->hasWindow())
                list << entry;

        for(auto entry : list)
            remove(entry);
    }
}
