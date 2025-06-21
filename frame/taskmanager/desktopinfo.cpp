// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopinfo.h"
#include "locale.h"
#include "unistd.h"

#include <QDebug>

#include <algorithm>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QVector>
#include <qlocale.h>

static QString desktopFileSuffix = ".desktop";

DesktopInfo::DesktopInfo(const QString &desktopfile)
{
    if(desktopfile.isEmpty()) {
        m_isValid = false;
        return;
    }
    QString desktopfilepath(desktopfile);
    if (!(desktopfilepath.endsWith(desktopFileSuffix)))
        desktopfilepath = desktopfilepath + desktopFileSuffix;

    QFileInfo desktopFileInfo(desktopfilepath);

    if (!desktopFileInfo.isAbsolute()) {
        for (auto dir: QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
            QString path = dir.append("/").append(desktopfilepath);
            if (QFile::exists(path)) {
                desktopFileInfo.setFile(path);
                break;
            }
        }
    }

    m_isValid = desktopFileInfo.isAbsolute() && desktopFileInfo.exists();

    if(m_isValid) {
        m_desktopFilePath = desktopFileInfo.absoluteFilePath();
        m_baseFileName = desktopFileInfo.completeBaseName();
        m_desktopFile.reset(new QSettings(m_desktopFilePath, QSettings::IniFormat));
        m_desktopFile->setIniCodec("utf-8");

        // check DesktopInfo valid
        QStringList mainKeys = m_desktopFile->childGroups();
        if (mainKeys.size() == 0)
            m_isValid = false;
        else {
            bool found = std::any_of(mainKeys.begin(), mainKeys.end(), [](const auto &key) {return key == MainSection;});

            if (!found)
                m_isValid = false;
            else if (m_desktopFile->value(MainSection + '/' + KeyType).toString() != TypeApplication)
                m_isValid = false;
        }
    }

    if(m_isValid) {
        m_name = getLocaleStr(MainSection, KeyName);
        m_icon = m_desktopFile->value(MainSection + '/' + KeyIcon).toString();
    }
}

DesktopInfo::DesktopInfo(const DesktopInfo &other) {
    m_isValid = other.m_isValid;

    m_name = other.m_name;
    m_icon = other.m_icon;
    m_desktopFilePath = other.m_desktopFilePath;
    m_baseFileName = other.m_baseFileName;

    // Desktopfile ini format
    m_desktopFile.reset(other.m_desktopFile.data());
    const_cast<DesktopInfo&>(other).m_desktopFile.take();
}

DesktopInfo &DesktopInfo::operator=(DesktopInfo &&other) {
    if(this != &other) {
        m_isValid = other.m_isValid;

        m_name = other.m_name;
        m_icon = other.m_icon;
        m_desktopFilePath = other.m_desktopFilePath;
        m_baseFileName = other.m_baseFileName;

        // Desktopfile ini format
        m_desktopFile.swap(other.m_desktopFile);
    }
    return *this;
}

QString DesktopInfo::getDesktopFilePath()
{
    return m_desktopFilePath;
}

bool DesktopInfo::isValidDesktop()
{
    return m_isValid;
}

bool DesktopInfo::isInstalled()
{
    QFileInfo desktopFileInfo(m_desktopFilePath);
    QString desktopFileName = desktopFileInfo.fileName();
    static auto applicationDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    return std::any_of(applicationDirs.begin(), applicationDirs.end(), [&desktopFileName](auto applicationDir){
        return QFile::exists(applicationDir+ '/' + desktopFileName);
    });
}

/** if return true, item is shown
 * @brief DesktopInfo::shouldShow
 * @return
 */
bool DesktopInfo::shouldShow()
{
    if (getNoDisplay() || getIsHidden()) {
        qDebug() << "hidden desktop file path: " << m_desktopFilePath;
        return false;
    }

    return getShowIn();
}

bool DesktopInfo::getNoDisplay()
{
    return m_desktopFile->value(MainSection + '/' +  KeyNoDisplay).toBool();
}

bool DesktopInfo::getIsHidden()
{
    return m_desktopFile->value(MainSection + '/' + KeyHidden).toBool();
}

bool DesktopInfo::getShowIn(QStringList desktopEnvs)
{
    if (desktopEnvs.size() == 0) {
        static const auto currentDesktops = QString(getenv("XDG_CURRENT_DESKTOP")).split(":");
        desktopEnvs = currentDesktops;
    }

    QStringList onlyShowIn = m_desktopFile->value(MainSection + '/' + KeyOnlyShowIn).toStringList();
    QStringList notShowIn = m_desktopFile->value(MainSection + '/' + KeyNotShowIn).toStringList();

    for (const auto &desktop : desktopEnvs) {
        bool ret = std::any_of(onlyShowIn.begin(), onlyShowIn.end(), [&desktop](const auto &d) {return d == desktop;});

        if (ret)
            return true;

        ret = std::any_of(notShowIn.begin(), notShowIn.end(), [&desktop](const auto &d) {return d == desktop;});

        if (ret)
            return false;
    }

    return onlyShowIn.size() == 0;
}

QString DesktopInfo::getExecutable()
{
    return m_desktopFile->value(MainSection + '/' + KeyExec).toString();
}

QList<DesktopAction> DesktopInfo::getActions()
{
    QList<DesktopAction> actions;
    for (const auto &mainKey : m_desktopFile->childGroups()) {
        if (mainKey.startsWith("Desktop Action")
                || mainKey.endsWith("Shortcut Group")) {
            DesktopAction action;
            action.name = getLocaleStr(mainKey, KeyName);
            action.exec = m_desktopFile->value(mainKey + '/' + KeyExec).toString();
            action.section = mainKey;
            actions.push_back(action);
        }
    }

    return actions;
}

bool DesktopInfo::getTerminal()
{
    return m_desktopFile->value(MainSection + '/' + KeyTerminal).toBool();
}

// TryExec is Path to an executable file on disk used to determine if the program is actually installed
QString DesktopInfo::getTryExec()
{
    return m_desktopFile->value(MainSection + '/' + KeyTryExec).toString();
}

// 按$PATH路径查找执行文件
bool DesktopInfo::findExecutable(const QString &exec)
{
    static QStringList paths = QString(getenv("PATH")).split(':');
    return std::any_of(paths.begin(), paths.end(), [&exec](QString path) {return QFile::exists(path + '/' + exec);});
}

QString DesktopInfo::getGenericName()
{
    return getLocaleStr(MainSection, KeyGenericName);
}

QString DesktopInfo::getName()
{
    return m_name;
}

QString DesktopInfo::getIcon()
{
    return m_icon;
}

QString DesktopInfo::getCommandLine()
{
    return m_desktopFile->value(MainSection + '/' + KeyExec).toString();
}

QStringList DesktopInfo::getKeywords()
{
    return m_desktopFile->value(MainSection + '/' + KeyKeywords).toStringList();
}

QStringList DesktopInfo::getCategories()
{
    return m_desktopFile->value(MainSection + '/' + KeyCategories).toStringList();
}

QSettings *DesktopInfo::getDesktopFile()
{
    return m_desktopFile.data();
}

QString DesktopInfo::getLocaleStr(const QString &section, const QString &key)
{
    QString currentLanguageCode = QLocale::system().name();
    QString res = m_desktopFile->value(section + '/' + key + QString("[%1]").arg(currentLanguageCode)).toString();
    if (res.isEmpty()) res = m_desktopFile->value(section + '/' + key).toString();
    return res;
}
