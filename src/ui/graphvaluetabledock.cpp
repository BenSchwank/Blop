#include "graphvaluetabledock.h"
#include "notechrome.h"
#include "tools/GraphCanvasItem.h"
#include "tools/math/MathEvaluator.h"
#include "tools/math/MathExpressionParser.h"
#include "tools/math/NumericAnalysis.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

GraphValueTableDock::GraphValueTableDock(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("GraphValueTableDock"));
  setAttribute(Qt::WA_StyledBackground, true);
  auto *lay = new QVBoxLayout(this);
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(6);
  m_title = new QLabel(QStringLiteral("Wertetabelle"), this);
  m_table = new QTableWidget(0, 2, this);
  m_table->setHorizontalHeaderLabels({QStringLiteral("x"), QStringLiteral("f(x)")});
  m_table->horizontalHeader()->setStretchLastSection(true);
  m_table->verticalHeader()->setVisible(false);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_table->setMaximumHeight(220);
  lay->addWidget(m_title);
  lay->addWidget(m_table);
  refreshChrome();
  hide();
}

void GraphValueTableDock::refreshChrome() {
  setStyleSheet(QStringLiteral(
                    "QWidget#GraphValueTableDock { background: %1; border: 1px solid %2;"
                    " border-radius: 12px; }"
                    "QLabel { color: %3; font-weight: 700; font-size: 12px; }"
                    "QTableWidget { background: %4; color: %3; border: 1px solid %2;"
                    " border-radius: 8px; gridline-color: %2; }"
                    "QHeaderView::section { background: %1; color: %3; padding: 4px;"
                    " border: none; font-weight: 600; }")
                    .arg(NoteChrome::panelElevated().name(QColor::HexRgb),
                         NoteChrome::rgbaCss(NoteChrome::border(), 90),
                         NoteChrome::textPrimary().name(QColor::HexRgb),
                         NoteChrome::panelBg().name(QColor::HexRgb)));
}

void GraphValueTableDock::bind(GraphCanvasItem *item) {
  if (!item) {
    hide();
    return;
  }
  const GraphObject d = item->data();
  if (!d.showValueTable) {
    hide();
    return;
  }
  rebuildTable(item);
  show();
  raise();
}

void GraphValueTableDock::rebuildTable(GraphCanvasItem *item) {
  if (!m_table || !item)
    return;
  const GraphObject d = item->data();
  if (d.selectedFunction < 0 || d.selectedFunction >= d.functions.size()) {
    m_table->setRowCount(0);
    return;
  }
  const GraphFunction &f = d.functions[d.selectedFunction];
  QString src = f.isDerivativeCurve ? f.sourceExpression : f.expression;
  if (!f.paramName.isEmpty())
    src = NumericAnalysis::substituteParam(src, f.paramName, f.paramValue);
  const ParsedExpression expr = MathExpressionParser::parseFunctionExpression(src);
  double x0 = d.tableX0;
  double x1 = d.tableX1;
  double dx = d.tableDx;
  if (!(dx > 1e-9))
    dx = 1.0;
  if (x1 < x0)
    std::swap(x0, x1);
  const int maxRows = 64;
  QVector<QPair<double, double>> rows;
  for (double x = x0; x <= x1 + dx * 1e-9 && rows.size() < maxRows; x += dx) {
    double y = qQNaN();
    if (expr.ok) {
      y = f.isDerivativeCurve ? NumericAnalysis::derivativeCentral(expr, x)
                              : MathEvaluator::evalAt(expr, x);
    }
    rows.append({x, y});
  }
  m_table->setRowCount(rows.size());
  for (int i = 0; i < rows.size(); ++i) {
    m_table->setItem(i, 0, new QTableWidgetItem(QString::number(rows[i].first, 'g', 5)));
    const QString ys = qIsFinite(rows[i].second)
                           ? QString::number(rows[i].second, 'g', 5)
                           : QStringLiteral("—");
    m_table->setItem(i, 1, new QTableWidgetItem(ys));
  }
  if (m_title) {
    m_title->setText(QStringLiteral("Wertetabelle  Δx=%1").arg(dx, 0, 'g', 4));
  }
  adjustSize();
}
