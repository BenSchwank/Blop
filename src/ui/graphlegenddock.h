#pragma once

#include <QWidget>

class GraphCanvasItem;
class QButtonGroup;
class QFrame;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

/// Compact chip list of functions for a graph. Tapping a chip selects it
/// so roots can be dragged on the plot.
class GraphLegendDock : public QWidget {
    Q_OBJECT
public:
    explicit GraphLegendDock(QWidget *parent = nullptr);

    void bind(GraphCanvasItem *item);
    void refreshChrome();

signals:
    void removeRequested(int index);
    void removeGraphWidgetRequested();
    void selectionRequested(int index);
    void entryBarRequested();

private:
    void applyCardStyle();
    void clearRowLayouts();
    void addFunctionRow(int index, const QString &expression, bool selected, const QColor &curveColor);

    QFrame *m_card{nullptr};
    QButtonGroup *m_fnGroup{nullptr};
    QLabel *m_lblActiveFn{nullptr};
    QVBoxLayout *m_rowLayout{nullptr};
    QWidget *m_listInset{nullptr};
    QWidget *m_footer{nullptr};
    QPushButton *m_btnAddFunction{nullptr};
    QPushButton *m_btnRemoveGraph{nullptr};
    int m_selectedIdx{-1};
};
