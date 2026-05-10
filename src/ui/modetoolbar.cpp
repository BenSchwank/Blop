#include "modetoolbar.h"
#include <QPainter>
#include <QAction>
#include "tools/ToolManager.h"

ModeToolBar::ModeToolBar(QWidget* parent)
    : QToolBar(parent)
{
    // Alle Tools hinzufügen
    auto addTool = [this](const QString& name, ToolMode m) {
        QAction* act = addAction(name);
        connect(act, &QAction::triggered, this, [this, m]() {
            setMode(m);
            ToolManager::instance().selectTool(m);
        });
    };

    addTool("Füller", ToolMode::Pen);
    addTool("Bleistift", ToolMode::Pencil);
    addTool("Marker", ToolMode::Highlighter);
    addTool("Radierer", ToolMode::Eraser);
    addTool("Lasso", ToolMode::Lasso);
    addTool("Lineal", ToolMode::Ruler);
    addTool("Formen", ToolMode::Shape);
    addTool("Notiz", ToolMode::StickyNote);
    addTool("Text", ToolMode::Text);
    addTool("Bild", ToolMode::Image);
    addTool("Hand", ToolMode::Hand);
}

ModeToolBar::~ModeToolBar() {}

void ModeToolBar::setMode(ToolMode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged(mode);
        update();
    }
}

void ModeToolBar::paintEvent(QPaintEvent* event) {
    QToolBar::paintEvent(event);
}
