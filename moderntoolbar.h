#pragma once
#include <QWidget>
#include "ToolMode.h"

class QPushButton;
class QButtonGroup;

class ModernToolbar : public QWidget {
    Q_OBJECT
public:
    explicit ModernToolbar(QWidget* parent=nullptr);
    void setToolMode(ToolMode mode);
    ToolMode toolMode() const { return mode_; }

signals:
    void toolChanged(ToolMode mode);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

private:
    ToolMode mode_{ToolMode::Pen};
    QPoint dragStartPos_;
    QButtonGroup* group_{nullptr};
    void setupUi();
};
