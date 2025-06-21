// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "docksettings.h"

#include "../taskmanager/common.h"

#include <DConfig>
#include <QDebug>

DCORE_USE_NAMESPACE

DockSettings::DockSettings(QObject *parent)
 : QObject (parent)
 , m_dockSettings(DConfig::create(configAppName, configDock, QString(), parent))
 , m_dockApps(DConfig::create(configAppName, configDockApps, QString(), parent))
 , m_qsettings(new QSettings(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/setting.ini", QSettings::IniFormat, this))
{
    m_qsettings->setIniCodec(QTextCodec::codecForName("UTF-8"));
    // 绑定属性
    if (m_dockSettings) {
            connect(m_dockSettings, &DConfig::valueChanged, this, [&] (const QString &key) {
                if (key == keyHideMode) {
                    Q_EMIT hideModeChanged(HideModeHandler(m_dockSettings->value(keyHideMode).toString()).toEnum());
                } else if (key == keyPosition) {
                    Q_EMIT positionModeChanged(Position(PositionModeHandler(m_dockSettings->value(key).toString()).toEnum()));
                } else if (key == keyShowRecent) {
                    Q_EMIT showRecentChanged(m_dockSettings->value(key).toBool());
                } else if (key == keyShowMultiWindow) {
                    Q_EMIT showMultiWindowChanged(m_dockSettings->value(key).toBool());
                } else if ( key == keyShowWindowName) {
                    Q_EMIT windowNameShowModeChanged(m_dockSettings->value(keyShowWindowName).toInt());
                } else if ( key == keyIconSize) {
                    Q_EMIT windowSizeFashionChanged(m_dockSettings->value(keyIconSize).toUInt());
                }
            });
    }
}

HideMode DockSettings::getHideMode()
{
    if (m_dockSettings) {
        QString mode = m_dockSettings->value(keyHideMode).toString();
        HideModeHandler handler(mode);
        return handler.toEnum();
    }
    return HideMode::KeepShowing;
}

void DockSettings::setHideMode(HideMode mode)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyHideMode, HideModeHandler(mode).toString());
    }
}

Position DockSettings::getPositionMode()
{
    Position ret = Position::Bottom;
    if (m_dockSettings) {
        QString mode = m_dockSettings->value(keyPosition).toString();
        PositionModeHandler handler(mode);
        ret = handler.toEnum();
    }
    return ret;
}

void DockSettings::setPositionMode(Position mode)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyPosition, PositionModeHandler(mode).toString());
    }
}

uint DockSettings::getIconSize()
{
    uint size = 36;
    if (m_dockSettings) {
        size = m_dockSettings->value(keyIconSize).toUInt();
    }
    return size;
}

void DockSettings::setIconSize(uint size)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyIconSize, size);
    }
}

bool DockSettings::showInPrimary() {
    bool ret = true;
    if(m_dockSettings)
        ret = m_dockSettings->value(keyShowInPrimary, true).toBool();

    return ret;
}

void DockSettings::setShowInPrimary(bool show) {
    if(m_dockSettings)
        m_dockSettings->setValue(keyShowInPrimary, show);
}

uint DockSettings::getShowTimeout()
{
    uint time = 100;
    if (m_dockSettings) {
        time = m_dockSettings->value(keyShowTimeout).toUInt();
    }
    return time;
}

void DockSettings::setShowTimeout(uint time)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyShowTimeout, time);
    }
}

uint DockSettings::getHideTimeout()
{
    uint time = 0;
    if (m_dockSettings) {
        time = m_dockSettings->value(keyHideTimeout).toUInt();
    }
    return time;
}

void DockSettings::setHideTimeout(uint time)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyHideTimeout, time);
    }
}

uint DockSettings::getWindowSizeFashion()
{
    uint size = 48;
    if (m_dockSettings) {
        size = m_dockSettings->value(keyIconSize).toUInt();
    }
    return size;
}

void DockSettings::setWindowSizeFashion(uint size)
{
    if (m_dockSettings) {
        m_dockSettings->setValue(keyIconSize, size);
    }
}

void DockSettings::saveStringList(const QString &key, const QStringList &values)
{
    if (!m_dockApps)
        return;

    m_dockApps->setValue(key, values);
}

QStringList DockSettings::loadStringList(const QString &key) const
{
    QStringList ret;
    if (!m_dockApps)
        return ret;

    for(const auto &var : m_dockApps->value(key).toList()) {
        if (var.isValid())
            ret.push_back(var.toString());
    }

    return ret;
}

QStringList DockSettings::getDockedApps()
{
    QStringList apps;
    auto list = loadStringList(keyDockedApps);

    for(auto app : list) {
        // "id: dde-file-manager,type: amAPP"
        apps << app.leftRef(app.indexOf(',')).mid(4).toString();
    }
    // qInfo() << "Docked apps: " << apps;
    return apps;
}

void DockSettings::setDockedApps(const QStringList &apps)
{
    QStringList list;
    for(auto app : apps)
        list << QString("id: %1, type: %2").arg(app).arg("amAPP");
    saveStringList(keyDockedApps, list);
}

QStringList DockSettings::getRecentApps() const
{
    return m_qsettings->value("Recent/apps", QStringList()).toStringList();
}

void DockSettings::setRecentApps(const QStringList &apps)
{
    m_qsettings->setValue("Recent/apps", apps);
}

void DockSettings::setShowMultiWindow(bool showMultiWindow)
{
    if (!m_dockSettings)
        return;

    m_dockSettings->setValue(keyShowMultiWindow, showMultiWindow);
}

bool DockSettings::showMultiWindow() const
{
    if (!m_dockSettings)
        return false;

    return m_dockSettings->value(keyShowMultiWindow).toBool();
}

int DockSettings::getWindowNameShowMode()
{
    if (!m_dockSettings)
        return 0;
    return m_dockSettings->value(keyShowWindowName).toInt();
}

void DockSettings::setWindowNameShowMode(int value)
{
    if (!m_dockSettings)
        return;
    m_dockSettings->setValue(keyShowWindowName, value);
}

QList<DirData> DockSettings::loadDirDatas() const {
    QList<DirData> dirDatas;

    int count = m_qsettings->value("count", 0).toInt();

    while (count >=1)
    {
        QStringList desktopFiles = m_qsettings->value(QString("dir_%1/ids").arg(count), QStringList()).value<QStringList>();

        //item->setIds(QSet<QString>(desktopFiles.begin(), desktopFiles.end()));

        QSet<QString> ids;
        for(auto id : desktopFiles) {
            // QFileInfo fileInfo(id);
            // ids.insert(fileInfo.canonicalFilePath());
            ids.insert(id);
        }

        dirDatas << DirData(m_qsettings->value(QString("dir_%1/title").arg(count), "").toString(),
            m_qsettings->value(QString("dir_%1/index").arg(count), -1).toInt(), ids);

        count--;
    }

    return dirDatas;
}

void DockSettings::setDirDatas(QList<DirData> &dirDatas) {
    m_qsettings->setFallbacksEnabled(true);
    int index = 0, originCount = m_qsettings->value("count", 0).toInt();
    for(auto item : dirDatas)
    {
        index++;
        m_qsettings->setValue(QString("dir_%1/title").arg(index), item.title);
        m_qsettings->setValue(QString("dir_%1/index").arg(index), item.index);
        QStringList ids;
        for(auto id : item.ids) ids << id;
        m_qsettings->setValue(QString("dir_%1/ids").arg(index), ids);
    }

    while(index < originCount)
        m_qsettings->remove(QString("dir_%1").arg(originCount--));

    m_qsettings->setValue("count", index);
    m_qsettings->sync();
}

void DockSettings::addFolder(const QString &path, int index)
{
    m_qsettings->beginWriteArray("folder");
    m_qsettings->setArrayIndex(index);
    m_qsettings->setValue("path", path);
    m_qsettings->endArray();
    m_qsettings->sync();
}

void DockSettings::updateFolders(const QStringList &paths) {
        int index = paths.count();
        m_qsettings->beginWriteArray("folder", index);
        m_qsettings->setArrayIndex(index);
        m_qsettings->remove("");
        while(--index >= 0) {
            m_qsettings->setArrayIndex(index);
            m_qsettings->setValue("path", paths.at(index));
        }
        m_qsettings->endArray();
        m_qsettings->sync();
}

QStringList DockSettings::loadLoaders() {
    QStringList folders;
    int index = m_qsettings->beginReadArray("folder");
    while(--index >= 0) {
        m_qsettings->setArrayIndex(index);
        folders << m_qsettings->value("path").toString();
    }
    m_qsettings->endArray();

    return folders;
}

void DockSettings::setShowRecent(bool visible) {
    m_qsettings->setValue("Recent/enable", visible);
    m_qsettings->sync();
}

bool DockSettings::showRecent() const {
    return m_qsettings->value("Recent/enable", true).toBool();
}

bool DockSettings::isEnableHoverScaleAnimation()
{
    return m_qsettings->value("animation/hover", true).toBool();
}

bool DockSettings::isEnableInOutAnimation()
{
    return m_qsettings->value("animation/inout", true).toBool();
}

bool DockSettings::isEnableDragAnimation()
{
    return m_qsettings->value("animation/drag", true).toBool();
}

bool DockSettings::isEnableHoverHighlight()
{
    return m_qsettings->value("animation/highlight", true).toBool();
}

void DockSettings::setHoverScaleAnimation(bool enable)
{
    if(enable != isEnableHoverScaleAnimation())
        m_qsettings->setValue("animation/hover", enable);
}

void DockSettings::setInOutAnimation(bool enable)
{
    if(enable != isEnableInOutAnimation())
        m_qsettings->setValue("animation/inout", enable);
}

void DockSettings::setDragAnimation(bool enable)
{
    m_qsettings->setValue("animation/drag", enable);
}

void DockSettings::setHoverHighlight(bool enable)
{
    if(isEnableHoverHighlight() != enable) {
        m_qsettings->setValue("animation/highlight", enable);
        emit hoverHighlighted(enable);
    }
}

MergeMode DockSettings::getDockMergeMode()
{
    int i = m_qsettings->value("mergeMode", MergeDock).toInt();
    if(i < 0 || i > 1)
        i = 0;
    return MergeMode(i);
}

void DockSettings::saveDockMergeMode(MergeMode mode)
{
    if(mode != getDockMergeMode())
    {
        m_qsettings->setValue("mergeMode", mode);
        m_qsettings->sync();
        emit mergeModeChanged(mode);
    }
}

DockSettings::ActivateAnimationType DockSettings::animationType() {
    return m_qsettings->value("animation/activate", Jump).value<ActivateAnimationType>();
}

void DockSettings::setAnimationType(ActivateAnimationType type) {
    m_qsettings->setValue("animation/activate", type);
    m_qsettings->sync();
}