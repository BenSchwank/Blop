#pragma once
#include "ToolManager.h"
#include "WritingTools.h"
#include "EraserTool.h"
// NEU: Die neuen Header
#include "LassoTool.h"
#include "ShapeTool.h"
#include "ImageTool.h"

class ToolFactory {
public:
    static void registerAllTools() {
        auto& mgr = ToolManager::instance();

        // 1. Basis Tools (Reihenfolge hier ist egal für die Logik, aber wichtig für das Laden)
        mgr.registerTool(new PenTool(&mgr));
        mgr.registerTool(new PencilTool(&mgr));
        mgr.registerTool(new HighlighterTool(&mgr));
        mgr.registerTool(new EraserTool(&mgr));

        // 2. Komplexe Tools
        mgr.registerTool(new LassoTool(&mgr));
        mgr.registerTool(new ShapeTool(&mgr));
        mgr.registerTool(new ImageTool(&mgr));

        // Platzhalter für den Rest (Lineal, Text etc. kommen in Schritt 5)
    }
};
