#pragma once

#include <QPointF>
#include <QWidget>

class GraphCanvasItem;
class QDoubleSpinBox;
class QMenu;
class QToolButton;

/// Compact icon row + x0 row at tap position on the graph plot.
class GraphQuickActionPopup : public QWidget {
    Q_OBJECT
public:
    explicit GraphQuickActionPopup(QWidget *parent = nullptr);

    void bind(GraphCanvasItem *item);
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

private:
    void rebuildAnalyseMenu();

    QToolButton *m_btnAxes{nullptr};
    QToolButton *m_btnAnalyse{nullptr};
    QToolButton *m_btnTangent{nullptr};
    QToolButton *m_btnDelFn{nullptr};
    QToolButton *m_btnDelGraph{nullptr};
    QDoubleSpinBox *m_x0{nullptr};
    QToolButton *m_btnTangentAtRoot{nullptr};
    QMenu *m_menuAnalyse{nullptr};
    QPointF m_anchorScene;
};
