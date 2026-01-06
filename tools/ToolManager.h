#pragma once

#include <QObject>
#include <QMap>
#include "AbstractTool.h"
#include "../ToolMode.h"
#include "../ToolSettings.h"

// Forward Declaration, damit wir den Include Loop vermeiden
class ToolFactory;

class ToolManager : public QObject {
    Q_OBJECT

public:
    static ToolManager& instance() {
        static ToolManager _instance;
        return _instance;
    }

    void registerTool(AbstractTool* tool) {
        if(tool) {
            m_tools[tool->mode()] = tool;
            tool->setParent(this);
        }
    }

    void selectTool(ToolMode mode) {
        if (m_activeTool && m_activeTool->mode() == mode) {
            emit m_activeTool->requestSettingsMenu();
            return;
        }

        if (m_activeTool) {
            m_activeTool->onDeactivated();
        }

        if (m_tools.contains(mode)) {
            m_activeTool = m_tools[mode];
            m_activeTool->setConfig(m_globalConfig);
            m_activeTool->onActivated();
            emit toolChanged(m_activeTool);
        } else {
            m_activeTool = nullptr;
        }
    }

    AbstractTool* activeTool() const { return m_activeTool; }

    void updateConfig(const ToolConfig& config) {
        m_globalConfig = config;
        if (m_activeTool) {
            m_activeTool->setConfig(config);
        }
    }

    ToolConfig config() const { return m_globalConfig; }

signals:
    void toolChanged(AbstractTool* newTool);

private:
    ToolManager() = default;
    ~ToolManager() = default;
    ToolManager(const ToolManager&) = delete;
    ToolManager& operator=(const ToolManager&) = delete;

    QMap<ToolMode, AbstractTool*> m_tools;
    AbstractTool* m_activeTool{nullptr};
    ToolConfig m_globalConfig;
};
