#pragma once
#include "ToolManager.h"
#include "WritingTools.h"
#include "EraserTool.h"
#include "LassoTool.h"
#include "ShapeTool.h"
#include "ImageTool.h"
#include "RulerTool.h"

class ToolFactory {
public:
    static void registerAllTools() {
        auto& mgr = ToolManager::instance();
        mgr.registerTool(new PenTool(&mgr));
        mgr.registerTool(new PencilTool(&mgr));
        mgr.registerTool(new HighlighterTool(&mgr));
        mgr.registerTool(new EraserTool(&mgr));
        mgr.registerTool(new LassoTool(&mgr));
        mgr.registerTool(new ShapeTool(&mgr));
        mgr.registerTool(new ImageTool(&mgr));
        mgr.registerTool(new RulerTool(&mgr));
    }
};
