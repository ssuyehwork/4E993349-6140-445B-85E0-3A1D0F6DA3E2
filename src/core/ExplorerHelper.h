#ifndef EXPLORERHELPER_H
#define EXPLORERHELPER_H

#include <QStringList>

class ExplorerHelper {
public:
    /**
     * @brief 获取当前活动资源管理器窗口中选中的文件/文件夹路径
     */
    static QStringList getSelectedPaths();

    /**
     * @brief 检查当前前台窗口是否为资源管理器
     */
    static bool isForegroundExplorer();
};

#endif // EXPLORERHELPER_H
