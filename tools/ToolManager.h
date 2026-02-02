#pragma once
#include <QObject>
#include <QMap>

// WICHTIG: Relative Pfade, damit ToolMode und Settings gefunden werden
#include "../ToolSettings.h"
#include "../ToolMode.h"

class AbstractTool;

class ToolManager : public QObject {
    Q_OBJECT
public:
    static ToolManager& instance();

    // Registrierung
    void registerTool(AbstractTool* tool);
    AbstractTool* tool(ToolMode mode) const;

    // Aktivierung (Hieß früher setActiveTool, jetzt selectTool)
    void selectTool(ToolMode mode);
    AbstractTool* activeTool() const;

    // Gibt den aktuellen Modus zurück (Hieß früher activeToolType)
    ToolMode activeToolMode() const;

    // Konfiguration
    const ToolConfig& config() const;
    void setConfig(const ToolConfig& config);
    void updateConfig(const ToolConfig& config); // Alias

signals:
    void toolChanged(AbstractTool* tool);
    void configChanged(const ToolConfig& config);

private:
    ToolManager(QObject* parent = nullptr);
    ~ToolManager();

    // Singleton Copy-Schutz
    ToolManager(const ToolManager&) = delete;
    void operator=(const ToolManager&) = delete;

    QMap<ToolMode, AbstractTool*> m_tools;
    AbstractTool* m_activeTool = nullptr;
    ToolConfig m_config;
};
