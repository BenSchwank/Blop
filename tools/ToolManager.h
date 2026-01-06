#pragma once

#include <QObject>
#include <QMap>
#include "AbstractTool.h"
#include "../ToolMode.h"
#include "../ToolSettings.h"

class ToolManager : public QObject {
    Q_OBJECT

public:
    static ToolManager& instance() {
        static ToolManager _instance;
        return _instance;
    }

    // Registriert ein Tool (wird in Schritt 3+ genutzt)
    void registerTool(AbstractTool* tool) {
        if(tool) {
            m_tools[tool->mode()] = tool;
            tool->setParent(this);
        }
    }

    // Wechselt das aktive Werkzeug
    void selectTool(ToolMode mode) {
        if (m_activeTool && m_activeTool->mode() == mode) {
            // Second Tap Event -> Settings öffnen
            emit m_activeTool->requestSettingsMenu();
            return;
        }

        if (m_activeTool) {
            m_activeTool->onDeactivated();
        }

        if (m_tools.contains(mode)) {
            m_activeTool = m_tools[mode];
            m_activeTool->setConfig(m_globalConfig); // Aktuelle Config anwenden
            m_activeTool->onActivated();
            emit toolChanged(m_activeTool);
        } else {
            // Fallback oder Fehlerbehandlung
            m_activeTool = nullptr;
        }
    }

    AbstractTool* activeTool() const { return m_activeTool; }

    // Globale Konfiguration aktualisieren
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
