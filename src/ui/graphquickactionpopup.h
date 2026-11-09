#pragma once

#include <QPointF>
#include <QWidget>

class GraphCanvasItem;
class QDoubleSpinBox;
class QLabel;
class QMenu;
class QSlider;
class QToolButton;
class QWidget;

/// Compact icon row + x0 row at tap position on the graph plot.
class GraphQuickActionPopup : public QWidget {
    Q_OBJECT
public:
    explicit GraphQuickActionPopup(QWidget *parent = nullptr);

    void bind(GraphCanvasItem *item);
    void refreshChrome();
    void setAnchorScenePos(const QPointF &scenePos) { m_anchorScene = scenePos; }
    QPointF anchorScenePos() const { return m_anchorScene; }

signals:
    void axisSettingsRequested();
    void toggleRequested(const QString &what);
    void tangentXRequested(double x);
    void tangentAtFirstRootRequested();
    void tangentManualRequested();
    void removeSelectedFunctionRequested();
    void removeGraphRequested();
    void paramValueRequested(double value);

private:
    void applyCardStyle();
    void rebuildAnalyseMenu();
    void syncParamRow(GraphCanvasItem *item);

    QToolButton *m_btnAxes{nullptr};
    QToolButton *m_btnAnalyse{nullptr};
    QToolButton *m_btnTangent{nullptr};
    QToolButton *m_btnDelFn{nullptr};
    QToolButton *m_btnDelGraph{nullptr};
    QDoubleSpinBox *m_x0{nullptr};
    QToolButton *m_btnTangentAtRoot{nullptr};
    QMenu *m_menuAnalyse{nullptr};
    QWidget *m_paramRow{nullptr};
    QLabel *m_paramLabel{nullptr};
    QSlider *m_paramSlider{nullptr};
    QLabel *m_paramValue{nullptr};
    QPointF m_anchorScene;
    bool m_syncingParam{false};
};
