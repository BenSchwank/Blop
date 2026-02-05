#pragma once
#include <QObject>
#include <QMap>
#include "AbstractTool.h"
#include "../ToolSettings.h"
#include "../ToolMode.h"

class ToolManager : public QObject {
    Q_OBJECT
public:
    // Singleton Zugriff
    static ToolManager& instance();

    // Werkzeuge registrieren und abrufen
    void registerTool(AbstractTool* tool);
    AbstractTool* tool(ToolMode mode) const;

    // Aktives Werkzeug steuern
    void selectTool(ToolMode mode);
    AbstractTool* activeTool() const;
    ToolMode activeToolMode() const;

    // Konfiguration (Farbe, Größe, etc.)
    const ToolConfig& config() const;
    void setConfig(const ToolConfig& config);
    void updateConfig(const ToolConfig& config);

signals:
    void toolChanged(AbstractTool* tool);
    void configChanged(const ToolConfig& config);

private:
    explicit ToolManager(QObject* parent = nullptr);
    ~ToolManager();

    // Kopieren verhindern (Singleton Pattern)
    ToolManager(const ToolManager&) = delete;
    ToolManager& operator=(const ToolManager&) = delete;

    QMap<ToolMode, AbstractTool*> m_tools;
    AbstractTool* m_activeTool = nullptr;
    ToolConfig m_config;
};
