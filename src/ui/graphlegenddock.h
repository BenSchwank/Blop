#pragma once

#include <QWidget>

class GraphCanvasItem;
class QButtonGroup;
class QFrame;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

/// Floating card beside the graph: modern list of functions, footer actions.
class GraphLegendDock : public QWidget {
    Q_OBJECT
public:
    explicit GraphLegendDock(QWidget *parent = nullptr);

    void bind(GraphCanvasItem *item);

signals:
    void removeRequested(int index);
    void removeGraphWidgetRequested();
    void selectionRequested(int index);
    void entryBarRequested();

private:
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
