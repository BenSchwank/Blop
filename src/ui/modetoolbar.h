#ifndef MODETOOLBAR_H
#define MODETOOLBAR_H

#include <QToolBar>
#include <QPaintEvent>
#include "ToolMode.h" // Einbindung von ToolMode

class ModeToolBar : public QToolBar {
    Q_OBJECT

public:
    explicit ModeToolBar(QWidget* parent = nullptr);
    ~ModeToolBar();

    void setMode(ToolMode mode);
    ToolMode currentMode() const { return m_mode; }

signals:
    void modeChanged(ToolMode mode);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ToolMode m_mode{ToolMode::Pen};
};

#endif // MODETOOLBAR_H
