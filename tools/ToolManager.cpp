#include "ToolManager.h"

ToolManager& ToolManager::instance() {
    static ToolManager _instance;
    return _instance;
}

ToolManager::ToolManager() = default;

void ToolManager::registerTool(AbstractTool* tool) {
    if(tool) {
        m_tools[tool->mode()] = tool;
        tool->setParent(this);
    }
}

void ToolManager::selectTool(ToolMode mode) {
    // Wenn das Tool schon aktiv ist, Toggle-Menü anfordern
    if (m_activeTool && m_activeTool->mode() == mode) {
        emit m_activeTool->requestSettingsMenu();
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
        // Fallback, wenn Tool nicht gefunden wurde
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
