#pragma once

#include <QDialog>

class GraphCanvasItem;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QWidget;

class GraphAxisSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit GraphAxisSettingsDialog(GraphCanvasItem *graph, QWidget *parent = nullptr);

private slots:
    void apply();
    void onXTickModeChanged(int index);
    void onYTickModeChanged(int index);

private:
    GraphCanvasItem *m_graph{nullptr};
    QDoubleSpinBox *m_xMin{nullptr};
    QDoubleSpinBox *m_xMax{nullptr};
    QDoubleSpinBox *m_yMin{nullptr};
    QDoubleSpinBox *m_yMax{nullptr};

    QComboBox *m_xTickMode{nullptr};
    QComboBox *m_yTickMode{nullptr};
    QDoubleSpinBox *m_xTickStep{nullptr};
    QDoubleSpinBox *m_yTickStep{nullptr};
    QSpinBox *m_xTickCount{nullptr};
    QSpinBox *m_yTickCount{nullptr};
    QWidget *m_xStepRow{nullptr};
    QWidget *m_yStepRow{nullptr};
    QWidget *m_xCountRow{nullptr};
    QWidget *m_yCountRow{nullptr};
};
