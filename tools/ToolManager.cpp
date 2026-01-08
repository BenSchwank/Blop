#include "ToolManager.h"

ToolManager& ToolManager::instance() {
    static ToolManager _instance;
    return _instance;
}

void ToolManager::registerTool(AbstractTool* tool) {
    if(tool) {
        m_tools[tool->mode()] = tool;
        tool->setParent(this);
    }
}

void ToolManager::selectTool(ToolMode mode) {
    // Wenn das Tool schon aktiv ist -> Menü öffnen via Public Method
    if (m_activeTool && m_activeTool->mode() == mode) {
        m_activeTool->triggerSettingsMenu();
        return;
    }

    // Altes Tool deaktivieren
    if (m_activeTool) {
        m_activeTool->onDeactivated();
    }

    // Neues Tool suchen und aktivieren
    if (m_tools.contains(mode)) {
        m_activeTool = m_tools[mode];
        m_activeTool->setConfig(m_globalConfig);
        m_activeTool->onActivated();
        emit toolChanged(m_activeTool);
    } else {
        m_activeTool = nullptr;
    }
}

AbstractTool* ToolManager::activeTool() const {
    return m_activeTool;
}

void ToolManager::updateConfig(const ToolConfig& config) {
    m_globalConfig = config;
    if (m_activeTool) {
        m_activeTool->setConfig(config);
    }
}

ToolConfig ToolManager::config() const {
    return m_globalConfig;
}
