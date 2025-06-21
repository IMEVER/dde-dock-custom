// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOCKSETTINGS_H
#define DOCKSETTINGS_H

#include "../interfaces/constants.h"

#include <QObject>

using namespace Dock;

class HideModeHandler {
    HideMode modeEnum;
    QString modeStr;

public:
    HideModeHandler(HideMode mode) : modeEnum(mode), modeStr("") {}
    HideModeHandler(QString mode) : modeEnum(HideMode::KeepShowing), modeStr(mode) {}

    bool equal(HideModeHandler hideMode) {
        return toString() == hideMode.toString() || toEnum() == hideMode.toEnum();
    }

    QString toString() {
        switch (modeEnum) {
        case HideMode::KeepShowing:
            return "keep-showing";
        case HideMode::KeepHidden:
            return "keep-hidden";
        case HideMode::SmartHide:
            return "smart-hide";
        default:
            return "keep-showing";
        }
        // 默认保持始终显示
    }

    HideMode toEnum() {
        if (modeStr == "keep-hidden")
            return HideMode::KeepHidden;
        if (modeStr == "smart-hide")
            return HideMode::SmartHide;

        return HideMode::KeepShowing;
    }
};

class PositionModeHandler {
    Position modeEnum;
    QString modeStr;

public:
    PositionModeHandler(Position mode) : modeEnum(mode), modeStr("") {}
    PositionModeHandler(QString mode) : modeEnum(Position::Bottom), modeStr(mode) {}

    bool equal(PositionModeHandler displayMode) {
        return toString() == displayMode.toString() || toEnum() == displayMode.toEnum();
    }

    QString toString() {
        switch (modeEnum) {
        case Position::Top:
            return "top";
        case Position::Right:
            return "right";
        case Position::Left:
            return "left";
        case Position::Bottom:
            return "bottom";
        default:
            return "bottom";
        }
    }

    Position toEnum() {
        if (modeStr == "top")
            return Position::Top;
        if (modeStr == "right")
            return Position::Right;
        if (modeStr == "bottom")
            return Position::Bottom;
        if (modeStr == "left")
            return Position::Left;

        return Position::Bottom;
    }
};

struct DirData {
    QString title;
    int index;
    QSet<QString> ids;

    DirData(QString title, int index, QSet<QString> ids) {
        this->title = title;
        this->index = index;
        this->ids = ids;
    }
};

class Settings;
namespace Dtk {
namespace Core {
class DConfig;
}
}

using namespace Dtk::Core;

// 任务栏组策略配置类
class DockSettings: public QObject
{
    Q_OBJECT
public:
    enum ActivateAnimationType {
        Swing = 0,
        Jump = 1,
        Scale = 2,
        Popup = 3,
        No = 4
    };

    static inline DockSettings *instance() {
        static DockSettings instance;
        return &instance;
    }

    HideMode getHideMode();
    void setHideMode(HideMode mode);
    Position getPositionMode();
    void setPositionMode(Position mode);
    uint getIconSize();
    void setIconSize(uint size);
    bool showInPrimary();
    void setShowInPrimary(bool show);
    uint getShowTimeout();
    void setShowTimeout(uint time);
    uint getHideTimeout();
    void setHideTimeout(uint time);
    uint getWindowSizeFashion();
    void setWindowSizeFashion(uint size);
    QStringList getDockedApps();
    void setDockedApps(const QStringList &apps);
    QStringList getRecentApps() const;
    void setRecentApps(const QStringList &apps);
    void setShowRecent(bool visible);
    bool showRecent() const;

    int  getWindowNameShowMode();
    void setWindowNameShowMode(int value);

    void setShowMultiWindow(bool showMultiWindow);
    bool showMultiWindow() const;

    QList<DirData> loadDirDatas() const;
    void setDirDatas(QList<DirData> &dirDatas);
    void addFolder(const QString &path, int index);
    void updateFolders(const QStringList &paths);
    QStringList loadLoaders();

    bool isEnableHoverScaleAnimation();
    bool isEnableInOutAnimation();
    bool isEnableDragAnimation();
    bool isEnableHoverHighlight();
    void setHoverScaleAnimation(bool enable);
    void setInOutAnimation(bool enable);
    void setDragAnimation(bool enable);
    void setHoverHighlight(bool enable);

    MergeMode getDockMergeMode();
    void saveDockMergeMode(MergeMode mode);
    ActivateAnimationType animationType();
    void setAnimationType(ActivateAnimationType type);

Q_SIGNALS:
    // 隐藏模式改变
    void hideModeChanged(HideMode mode);
    // 显示位置改变
    void positionModeChanged(Position mode);
    // 是否显示最近打开应用改变
    void showRecentChanged(bool);
    // 是否显示多开应用改变
    void showMultiWindowChanged(bool);
    // 小窗口显示窗口名称模式改变
    void windowNameShowModeChanged(int mode);
    // 时尚模式下，dock尺寸信息改变
    void windowSizeFashionChanged(uint size);

    void mergeModeChanged(MergeMode mode);
    void hoverHighlighted(bool enabled);

private:
    DockSettings(QObject *paret = nullptr);
    DockSettings(const DockSettings &);
    DockSettings& operator= (const DockSettings &);
    void saveStringList(const QString &key, const QStringList &values);
    QStringList loadStringList(const QString &key) const;

private:
    DConfig *m_dockSettings;
    DConfig *m_dockApps;
    QSettings *m_qsettings;
};

Q_DECLARE_METATYPE(DockSettings::ActivateAnimationType);
#endif // DOCKSETTINGS_H
