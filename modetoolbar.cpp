#include "modetoolbar.h"
#include <QPainter>
#include <QAction>

ModeToolBar::ModeToolBar(QWidget* parent)
    : QToolBar(parent)
{
    // Hier kannst du Buttons/Actions hinzufügen
    QAction* penAction = addAction("Stift");
    QAction* eraserAction = addAction("Radierer");
    QAction* lassoAction = addAction("Lasso");

    connect(penAction, &QAction::triggered, this, [this]() {
        setMode(Mode::Pen);
    });

    connect(eraserAction, &QAction::triggered, this, [this]() {
        setMode(Mode::Eraser);
    });

    connect(lassoAction, &QAction::triggered, this, [this]() {
        setMode(Mode::Lasso);
    });
}

ModeToolBar::~ModeToolBar() {
    // Destruktor
}

void ModeToolBar::setMode(Mode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged(mode);
        update();
    }
}

void ModeToolBar::paintEvent(QPaintEvent* event) {
    QToolBar::paintEvent(event);
    // Hier kannst du custom painting hinzufügen wenn nötig
}
