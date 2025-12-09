#ifndef MODETOOLBAR_H
#define MODETOOLBAR_H

#include <QToolBar>
#include <QPaintEvent>

enum class Mode {
    Pen,
    Eraser,
    Lasso
};

class ModeToolBar : public QToolBar {
    Q_OBJECT

public:
    explicit ModeToolBar(QWidget* parent = nullptr);
    ~ModeToolBar();

    void setMode(Mode mode);
    Mode currentMode() const { return m_mode; }

signals:
    void modeChanged(Mode mode);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Mode m_mode{Mode::Pen};
};

#endif // MODETOOLBAR_H
