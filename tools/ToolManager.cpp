#include "ToolManager.h"
#include <QDebug>

ToolManager& ToolManager::instance() {
    static ToolManager _instance;
    return _instance;
}

ToolManager::ToolManager(QObject* parent) : QObject(parent) {
    // Basic defaults
    m_fallbackConfig.penColor = Qt::black;
    m_fallbackConfig.penWidth = 3.0;

    // Pre-populate some mode-specific defaults
    ToolConfig highlighterCfg;
    highlighterCfg.penWidth = 15;
    highlighterCfg.opacity = 0.5;
    highlighterCfg.penColor = Qt::yellow;
    m_configs[ToolMode::Highlighter] = highlighterCfg;

    ToolConfig eraserCfg;
    eraserCfg.penWidth = 20;
    m_configs[ToolMode::Eraser] = eraserCfg;
    
    ToolConfig pencilCfg;
    pencilCfg.penColor = QColor(40, 40, 40);
    pencilCfg.penWidth = 2;
    m_configs[ToolMode::Pencil] = pencilCfg;
}

ToolManager::~ToolManager() {
    qDeleteAll(m_tools);
    m_tools.clear();
}

void ToolManager::registerTool(AbstractTool* tool) {
    if (!tool) return;
    tool->setParent(this);
    m_tools.insert(tool->mode(), tool);

    // Initialize config if missing
    if (!m_configs.contains(tool->mode())) {
        m_configs[tool->mode()] = m_fallbackConfig;
    }

    // Erstes registriertes Tool als aktiv setzen, falls noch keines gewählt ist
    if (!m_activeTool) {
        selectTool(tool->mode());
    }
}

AbstractTool* ToolManager::tool(ToolMode mode) const {
    return m_tools.value(mode, nullptr);
}

void ToolManager::selectTool(ToolMode mode) {
    auto it = m_tools.find(mode);
    if (it != m_tools.end()) {
        if (m_activeTool != it.value()) {
            if (m_activeTool) {
                m_activeTool->onDeactivated();
            }

            m_activeTool = it.value();
            m_activeTool->setConfig(m_configs[mode]); // Config syncen
            m_activeTool->onActivated();

            emit toolChanged(m_activeTool);
        }
    }
}

AbstractTool* ToolManager::activeTool() const {
    return m_activeTool;
}

ToolMode ToolManager::activeToolMode() const {
    if (m_activeTool) return m_activeTool->mode();
    return ToolMode::Pen; // Fallback
}

const ToolConfig& ToolManager::config() const {
    if (m_activeTool) {
        auto it = m_configs.find(m_activeTool->mode());
        if (it != m_configs.end()) {
            return it.value();
        }
    }
    return m_fallbackConfig;
}

void ToolManager::setConfig(const ToolConfig& config) {
    if (m_activeTool) {
        m_configs[m_activeTool->mode()] = config;
        m_activeTool->setConfig(config);
        emit configChanged(config);
    } else {
        m_fallbackConfig = config;
    }
}

void ToolManager::updateConfig(const ToolConfig& config) {
    setConfig(config);
}

ToolConfig& ToolManager::configFor(ToolMode mode) {
    if (!m_configs.contains(mode)) {
        m_configs[mode] = m_fallbackConfig;
    }
    return m_configs[mode];
}
