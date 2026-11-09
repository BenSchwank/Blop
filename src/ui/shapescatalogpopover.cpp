#include "shapescatalogpopover.h"

#include "ToolSettings.h"
#include "notechrome.h"
#include "uiscale.h"

#include <QButtonGroup>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

void paintPreview(QPainter *p, ShapeToolKind kind, const QRectF &r) {
  p->setRenderHint(QPainter::Antialiasing, true);
  QPen pen(QColor(0xE2, 0xE8, 0xF0), 1.4);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  p->setPen(pen);
  p->setBrush(Qt::NoBrush);
  const QPointF c = r.center();
  switch (kind) {
  case ShapeToolKind::Line:
  case ShapeToolKind::DashedLine:
    if (kind == ShapeToolKind::DashedLine)
      pen.setStyle(Qt::DashLine);
    p->setPen(pen);
    p->drawLine(QPointF(r.left(), c.y()), QPointF(r.right(), c.y()));
    break;
  case ShapeToolKind::Arrow:
  case ShapeToolKind::DoubleArrow: {
    p->drawLine(QPointF(r.left(), c.y()), QPointF(r.right() - 2, c.y()));
    QPainterPath tip;
    tip.moveTo(r.right(), c.y());
    tip.lineTo(r.right() - 4, c.y() - 2.5);
    tip.lineTo(r.right() - 4, c.y() + 2.5);
    tip.closeSubpath();
    p->setBrush(QColor(0xE2, 0xE8, 0xF0));
    p->drawPath(tip);
    if (kind == ShapeToolKind::DoubleArrow) {
      QPainterPath tip2;
      tip2.moveTo(r.left(), c.y());
      tip2.lineTo(r.left() + 4, c.y() - 2.5);
      tip2.lineTo(r.left() + 4, c.y() + 2.5);
      tip2.closeSubpath();
      p->drawPath(tip2);
    }
    break;
  }
  case ShapeToolKind::ElbowConnector:
    p->drawLine(QPointF(r.left(), r.top() + 2), QPointF(c.x(), r.top() + 2));
    p->drawLine(QPointF(c.x(), r.top() + 2), QPointF(c.x(), r.bottom() - 2));
    p->drawLine(QPointF(c.x(), r.bottom() - 2),
                QPointF(r.right(), r.bottom() - 2));
    break;
  case ShapeToolKind::Rectangle:
  case ShapeToolKind::RoundedRect:
    if (kind == ShapeToolKind::RoundedRect)
      p->drawRoundedRect(r.adjusted(1, 1, -1, -1), 2.5, 2.5);
    else
      p->drawRect(r.adjusted(1, 1, -1, -1));
    break;
  case ShapeToolKind::Ellipse:
  case ShapeToolKind::Circle:
  case ShapeToolKind::UnitCircle:
    p->drawEllipse(r.adjusted(1, 1, -1, -1));
    if (kind == ShapeToolKind::UnitCircle) {
      p->drawLine(QPointF(r.left(), c.y()), QPointF(r.right(), c.y()));
      p->drawLine(QPointF(c.x(), r.top()), QPointF(c.x(), r.bottom()));
    }
    break;
  case ShapeToolKind::Triangle: {
    QPainterPath t;
    t.moveTo(c.x(), r.top() + 1);
    t.lineTo(r.right() - 1, r.bottom() - 1);
    t.lineTo(r.left() + 1, r.bottom() - 1);
    t.closeSubpath();
    p->drawPath(t);
    break;
  }
  case ShapeToolKind::Polyline:
    p->drawPolyline(QPolygonF{QPointF(r.left(), r.bottom() - 2),
                              QPointF(r.left() + r.width() * 0.35, r.top() + 2),
                              QPointF(r.left() + r.width() * 0.65, r.bottom() - 3),
                              QPointF(r.right(), r.top() + 3)});
    break;
  case ShapeToolKind::Brace:
    p->drawArc(QRectF(r.left(), r.top(), r.width() * 0.55, r.height()), 90 * 16,
               180 * 16);
    break;
  case ShapeToolKind::Axes2D:
  case ShapeToolKind::AxesPositive:
  case ShapeToolKind::ComplexPlane:
  case ShapeToolKind::Axes3D: {
    const qreal mx =
        kind == ShapeToolKind::AxesPositive ? r.left() + 1 : c.x();
    const qreal my =
        kind == ShapeToolKind::AxesPositive ? r.bottom() - 1 : c.y();
    p->drawLine(QPointF(r.left(), my), QPointF(r.right(), my));
    p->drawLine(QPointF(mx, r.top()), QPointF(mx, r.bottom()));
    if (kind == ShapeToolKind::Axes3D) {
      p->drawLine(c, QPointF(r.left() + 1, r.bottom() - 1));
      p->drawLine(c, QPointF(r.right() - 1, r.bottom() - 1));
    }
    break;
  }
  case ShapeToolKind::StampResistor: {
    p->drawLine(QPointF(r.left(), c.y()), QPointF(r.left() + 2, c.y()));
    QPainterPath zig;
    zig.moveTo(r.left() + 2, c.y());
    const qreal step = (r.width() - 4) / 6.0;
    for (int i = 0; i < 6; ++i) {
      const qreal x = r.left() + 2 + (i + 1) * step;
      zig.lineTo(x, c.y() + ((i % 2) ? 2.5 : -2.5));
    }
    p->drawPath(zig);
    break;
  }
  case ShapeToolKind::StampCapacitor:
    p->drawLine(QPointF(r.left(), c.y()), QPointF(c.x() - 1.5, c.y()));
    p->drawLine(QPointF(c.x() - 1.5, r.top() + 1),
                QPointF(c.x() - 1.5, r.bottom() - 1));
    p->drawLine(QPointF(c.x() + 1.5, r.top() + 1),
                QPointF(c.x() + 1.5, r.bottom() - 1));
    p->drawLine(QPointF(c.x() + 1.5, c.y()), QPointF(r.right(), c.y()));
    break;
  case ShapeToolKind::StampInductor:
    for (int i = 0; i < 3; ++i) {
      const qreal x = r.left() + 2 + i * (r.width() - 4) / 3.0;
      p->drawArc(QRectF(x, c.y() - 3, (r.width() - 4) / 3.0, 6), 0, 180 * 16);
    }
    break;
  case ShapeToolKind::StampVoltageSrc:
  case ShapeToolKind::StampCurrentSrc:
    p->drawEllipse(r.adjusted(1, 1, -1, -1));
    p->drawLine(QPointF(c.x(), r.top() + 3), QPointF(c.x(), r.bottom() - 3));
    break;
  case ShapeToolKind::StampGnd:
    p->drawLine(QPointF(c.x(), r.top() + 1), QPointF(c.x(), c.y()));
    p->drawLine(QPointF(r.left() + 1, c.y()), QPointF(r.right() - 1, c.y()));
    p->drawLine(QPointF(r.left() + 3, c.y() + 2),
                QPointF(r.right() - 3, c.y() + 2));
    p->drawLine(QPointF(r.left() + 5, c.y() + 4),
                QPointF(r.right() - 5, c.y() + 4));
    break;
  case ShapeToolKind::StampNode:
    p->setBrush(QColor(0xE2, 0xE8, 0xF0));
    p->drawEllipse(c, 2.5, 2.5);
    break;
  case ShapeToolKind::FlowDecision: {
    QPolygonF dia;
    dia << QPointF(c.x(), r.top() + 1) << QPointF(r.right() - 1, c.y())
        << QPointF(c.x(), r.bottom() - 1) << QPointF(r.left() + 1, c.y());
    p->drawPolygon(dia);
    break;
  }
  case ShapeToolKind::FlowProcess:
    p->drawRoundedRect(r.adjusted(1, 2, -1, -2), 2, 2);
    break;
  case ShapeToolKind::FlowIO: {
    QPolygonF para;
    para << QPointF(r.left() + 3, r.top() + 1)
         << QPointF(r.right(), r.top() + 1)
         << QPointF(r.right() - 3, r.bottom() - 1)
         << QPointF(r.left(), r.bottom() - 1);
    p->drawPolygon(para);
    break;
  }
  default:
    p->drawRect(r.adjusted(1, 1, -1, -1));
    break;
  }
}

QString tabButtonStyle() {
  return QStringLiteral(
      "QPushButton {"
      "  background: transparent;"
      "  border: none;"
      "  border-bottom: 2px solid transparent;"
      "  border-radius: 0;"
      "  color: #94A3B8;"
      "  padding: 6px 10px;"
      "  font-size: 11px;"
      "  font-weight: 600;"
      "}"
      "QPushButton:checked {"
      "  background: transparent;"
      "  border: none;"
      "  border-bottom: 2px solid #3B82F6;"
      "  color: #F8FAFC;"
      "}"
      "QPushButton:hover {"
      "  color: #E2E8F0;"
      "}");
}

} // namespace

ShapesCatalogPopover::ShapesCatalogPopover(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("ShapesCatalogPopover"));
  setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setFixedWidth(UiScale::dp(280));
  setStyleSheet(NoteChrome::studioPopoverStyle(QStringLiteral("ShapesCatalogPopover")));

  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(32);
  shadow->setOffset(0, 12);
  shadow->setColor(QColor(0, 0, 0, 115));
  setGraphicsEffect(shadow);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(UiScale::dp(12), UiScale::dp(12), UiScale::dp(12),
                           UiScale::dp(12));
  root->setSpacing(UiScale::dp(8));

  m_tabLay = new QHBoxLayout;
  m_tabLay->setContentsMargins(0, 0, 0, 0);
  m_tabLay->setSpacing(UiScale::dp(2));
  m_tabGroup = new QButtonGroup(this);
  m_tabGroup->setExclusive(true);

  auto makeTab = [&](const QString &title) {
    auto *b = new QPushButton(title, this);
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFlat(true);
    b->setStyleSheet(tabButtonStyle());
    m_tabGroup->addButton(b);
    m_tabLay->addWidget(b);
    return b;
  };
  m_tabFormen = makeTab(QStringLiteral("Formen"));
  m_tabKoordinaten = makeTab(QStringLiteral("Koordinaten"));
  m_tabET = makeTab(QStringLiteral("ET-Symbole"));
  m_tabLay->addStretch(1);
  root->addLayout(m_tabLay);

  m_stack = new QStackedWidget(this);
  m_stack->setObjectName(QStringLiteral("ShapesCatalogStack"));
  root->addWidget(m_stack, 1);

  connect(m_tabFormen, &QPushButton::clicked, this, [this]() { selectTab(0); });
  connect(m_tabKoordinaten, &QPushButton::clicked, this,
          [this]() { selectTab(1); });
  connect(m_tabET, &QPushButton::clicked, this, [this]() { selectTab(2); });
}

QIcon ShapesCatalogPopover::shapePreviewIcon(int kind, int px) {
  QPixmap pm(px, px);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  paintPreview(&p, static_cast<ShapeToolKind>(kind),
               QRectF(0.5, 0.5, px - 1.0, px - 1.0));
  return QIcon(pm);
}

void ShapesCatalogPopover::selectTab(int index) {
  if (!m_stack)
    return;
  const int clamped = qBound(0, index, qMax(0, m_stack->count() - 1));
  m_stack->setCurrentIndex(clamped);
  if (m_tabFormen)
    m_tabFormen->setChecked(clamped == 0);
  if (m_tabKoordinaten)
    m_tabKoordinaten->setChecked(clamped == 1);
  if (m_tabET)
    m_tabET->setChecked(clamped == 2);
}

QGridLayout *ShapesCatalogPopover::addPage(QPushButton *tab,
                                           const QString &title) {
  Q_UNUSED(title);
  auto *page = new QWidget; // no parent until addWidget
  page->setObjectName(QStringLiteral("ShapesCatalogPage"));
  auto *grid = new QGridLayout(page);
  grid->setContentsMargins(0, UiScale::dp(4), 0, 0);
  grid->setHorizontalSpacing(UiScale::dp(8));
  grid->setVerticalSpacing(UiScale::dp(8));
  grid->setColumnStretch(0, 1);
  grid->setColumnStretch(1, 1);
  m_stack->addWidget(page);
  if (tab)
    tab->setVisible(true);
  return grid;
}

void ShapesCatalogPopover::rebuild(bool mintTools) {
  if (!m_stack || !m_tabFormen)
    return;

  const QSignalBlocker blockStack(m_stack);
  while (m_stack->count() > 0) {
    QWidget *w = m_stack->widget(0);
    m_stack->removeWidget(w);
    delete w;
  }

  m_tabKoordinaten->setVisible(mintTools);
  m_tabET->setVisible(mintTools);

  auto *g1 = addPage(m_tabFormen, QStringLiteral("Formen"));
  auto *page1 = g1->parentWidget();
  const struct {
    const char *label;
    ShapeToolKind kind;
  } basic[] = {
      {"Linie", ShapeToolKind::Line},
      {"Pfeil", ShapeToolKind::Arrow},
      {"Doppelpfeil", ShapeToolKind::DoubleArrow},
      {"Elbow", ShapeToolKind::ElbowConnector},
      {"Rechteck", ShapeToolKind::Rectangle},
      {"Rund-Rect", ShapeToolKind::RoundedRect},
      {"Ellipse", ShapeToolKind::Ellipse},
      {"Dreieck", ShapeToolKind::Triangle},
      {"Polyline", ShapeToolKind::Polyline},
      {"Klammer {", ShapeToolKind::Brace},
      {"Gestrichelt", ShapeToolKind::DashedLine},
  };
  for (int i = 0; i < int(sizeof(basic) / sizeof(basic[0])); ++i)
    addShapeButton(page1, g1, QString::fromUtf8(basic[i].label),
                   int(basic[i].kind));

  if (mintTools) {
    auto *g2 = addPage(m_tabKoordinaten, QStringLiteral("Koordinaten"));
    auto *page2 = g2->parentWidget();
    const struct {
      const char *label;
      ShapeToolKind kind;
    } axes[] = {
        {"2D Achsen (4Q)", ShapeToolKind::Axes2D},
        {"1. Quadrant", ShapeToolKind::AxesPositive},
        {"Gauß Re/Im", ShapeToolKind::ComplexPlane},
        {"Einheitskreis", ShapeToolKind::UnitCircle},
        {"3D Achsen", ShapeToolKind::Axes3D},
    };
    for (int i = 0; i < int(sizeof(axes) / sizeof(axes[0])); ++i)
      addShapeButton(page2, g2, QString::fromUtf8(axes[i].label),
                     int(axes[i].kind));

    auto *g3 = addPage(m_tabET, QStringLiteral("ET-Symbole"));
    auto *page3 = g3->parentWidget();
    const struct {
      const char *label;
      ShapeToolKind kind;
    } et[] = {
        {"Widerstand R", ShapeToolKind::StampResistor},
        {"Kondensator C", ShapeToolKind::StampCapacitor},
        {"Spule L", ShapeToolKind::StampInductor},
        {"Spannung Uq", ShapeToolKind::StampVoltageSrc},
        {"Strom Iq", ShapeToolKind::StampCurrentSrc},
        {"Masse GND", ShapeToolKind::StampGnd},
        {"Knoten", ShapeToolKind::StampNode},
        {"Verzweigung", ShapeToolKind::FlowDecision},
        {"Prozess", ShapeToolKind::FlowProcess},
        {"Ein-/Ausgabe", ShapeToolKind::FlowIO},
    };
    for (int i = 0; i < int(sizeof(et) / sizeof(et[0])); ++i)
      addShapeButton(page3, g3, QString::fromUtf8(et[i].label),
                     int(et[i].kind));
  }

  selectTab(0);
}

void ShapesCatalogPopover::addShapeButton(QWidget *page, QGridLayout *grid,
                                          const QString &label, int kind) {
  auto *btn = new QPushButton(label, page);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setMinimumHeight(UiScale::dp(34));
  btn->setIcon(shapePreviewIcon(kind, UiScale::dp(14)));
  btn->setIconSize(QSize(UiScale::dp(14), UiScale::dp(14)));
  btn->setStyleSheet(
      QStringLiteral("QPushButton { text-align: left; padding: 6px 8px; }"));
  connect(btn, &QPushButton::clicked, this, [this, kind]() {
    emit shapeChosen(kind);
    close();
  });
  const int n = grid->count();
  grid->addWidget(btn, n / 2, n % 2);
}

void ShapesCatalogPopover::present(const QPoint &globalAnchor, bool mintTools) {
  rebuild(mintTools);
  adjustSize();
  NoteChrome::clampPopupToHost(this, globalAnchor + QPoint(0, 8),
                               parentWidget() ? parentWidget() : window());
  show();
  raise();
}
