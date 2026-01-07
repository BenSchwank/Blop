#pragma once

#include <QObject>
#include <QMap>
#include "AbstractTool.h"
#include "../ToolMode.h"
#include "../ToolSettings.h"

class ToolManager : public QObject {
    Q_OBJECT

public:
    static ToolManager& instance();

    void registerTool(AbstractTool* tool);
    void selectTool(ToolMode mode);
    AbstractTool* activeTool() const;

    void updateConfig(const ToolConfig& config);
    ToolConfig config() const;

signals:
    void toolChanged(AbstractTool* newTool);

private:
    ToolManager();
    ~ToolManager() override = default;
    ToolManager(const ToolManager&) = delete;
    ToolManager& operator=(const ToolManager&) = delete;

    QMap<ToolMode, AbstractTool*> m_tools;
    AbstractTool* m_activeTool{nullptr};
    ToolConfig m_globalConfig;
};
