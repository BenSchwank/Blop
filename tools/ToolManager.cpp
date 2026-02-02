#include "ToolManager.h"
#include "AbstractTool.h"

ToolManager& ToolManager::instance() {
    static ToolManager _instance;
    return _instance;
}

ToolManager::ToolManager(QObject* parent) : QObject(parent) {
}

ToolManager::~ToolManager() {
    qDeleteAll(m_tools);
}

void ToolManager::registerTool(AbstractTool* tool) {
    if (!tool) return;
    tool->setParent(this);
    m_tools.insert(tool->mode(), tool);

    // Erstes Tool als aktiv setzen, falls noch keines gewählt ist
    if (!m_activeTool) {
        selectTool(tool->mode());
    }
}

AbstractTool* ToolManager::tool(ToolMode mode) const {
    return m_tools.value(mode, nullptr);
}

// HIER WAR DER FEHLER: Es heißt jetzt selectTool
void ToolManager::selectTool(ToolMode mode) {
    auto it = m_tools.find(mode);
    if (it != m_tools.end()) {
        if (m_activeTool != it.value()) {
            if (m_activeTool) m_activeTool->onDeactivated();
            m_activeTool = it.value();
            m_activeTool->setConfig(m_config); // Config syncen
            m_activeTool->onActivated();
            emit toolChanged(m_activeTool);
        }
    }
}

AbstractTool* ToolManager::activeTool() const {
    return m_activeTool;
}

// HIER WAR DER FEHLER: Rückgabetyp ist ToolMode
ToolMode ToolManager::activeToolMode() const {
    if (m_activeTool) return m_activeTool->mode();
    return ToolMode::Pen; // Fallback
}

const ToolConfig& ToolManager::config() const {
    return m_config;
}

void ToolManager::setConfig(const ToolConfig& config) {
    m_config = config;
    if (m_activeTool) {
        m_activeTool->setConfig(m_config);
    }
    emit configChanged(m_config);
}

void ToolManager::updateConfig(const ToolConfig& config) {
    setConfig(config);
}
