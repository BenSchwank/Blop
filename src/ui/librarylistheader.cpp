#include "librarylistheader.h"

#include "blop_theme.h"
#include "librarylistcolumns.h"
#include "uiscale.h"

#include <QAbstractButton>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>

namespace {

/// Master select control with explicit check / dash glyphs (QSS alone leaves
/// a bare blue square for checked/indeterminate).
class MasterCheckButton : public QAbstractButton {
public:
  explicit MasterCheckButton(QWidget *parent = nullptr)
      : QAbstractButton(parent) {
    setObjectName(QStringLiteral("LibraryListMasterCheck"));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setCheckable(true);
    setToolTip(QStringLiteral("Alle auswählen"));
  }

  void setTriState(Qt::CheckState state) {
    m_state = state;
    setChecked(state == Qt::Checked);
    update();
  }

  Qt::CheckState triState() const { return m_state; }

protected:
  void nextCheckState() override {
    // Click always toggles between unchecked and checked (clear partial).
    const bool on = m_state != Qt::Checked;
    m_state = on ? Qt::Checked : Qt::Unchecked;
    setChecked(on);
    emit clicked(on);
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const bool dark = BlopTheme::instance().isDark();
    const QRectF box = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);
    const qreal rad = 4.0;

    if (m_state == Qt::Unchecked) {
      p.setPen(QPen(dark ? QColor(0x47, 0x55, 0x69) : QColor(0xCB, 0xD5, 0xE1),
                    1.5));
      p.setBrush(dark ? Qt::transparent : QColor(0xFF, 0xFF, 0xFF));
      p.drawRoundedRect(box, rad, rad);
      return;
    }

    p.setPen(QPen(QColor(0x25, 0x63, 0xEB), 1.5));
    p.setBrush(QColor(0x25, 0x63, 0xEB));
    p.drawRoundedRect(box, rad, rad);

    p.setPen(QPen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (m_state == Qt::PartiallyChecked) {
      const qreal cy = box.center().y();
      p.drawLine(QPointF(box.left() + 3.5, cy), QPointF(box.right() - 3.5, cy));
    } else {
      QPainterPath tick;
      tick.moveTo(box.left() + 3.5, box.center().y());
      tick.lineTo(box.center().x() - 0.5, box.bottom() - 4.0);
      tick.lineTo(box.right() - 3.0, box.top() + 3.5);
      p.drawPath(tick);
    }
  }

private:
  Qt::CheckState m_state{Qt::Unchecked};
};

} // namespace

LibraryListHeader::LibraryListHeader(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryListHeader"));
  setFixedHeight(UiScale::dp(32));
  setMouseTracking(true);
  setAttribute(Qt::WA_StyledBackground, true);

  auto *master = new MasterCheckButton(this);
  m_master = master;
  connect(master, &QAbstractButton::clicked, this, [this](bool on) {
    emit selectAllRequested(on);
  });
  syncTheme();
  layoutCheck();
}

void LibraryListHeader::setSort(SortColumn col, Qt::SortOrder order) {
  if (m_col == col && m_order == order)
    return;
  m_col = col;
  m_order = order;
  update();
}

void LibraryListHeader::setMasterChecked(bool on) {
  if (!m_master)
    return;
  static_cast<MasterCheckButton *>(m_master)->setTriState(
      on ? Qt::Checked : Qt::Unchecked);
}

void LibraryListHeader::setMasterTristate(Qt::CheckState state) {
  if (!m_master)
    return;
  static_cast<MasterCheckButton *>(m_master)->setTriState(state);
}

void LibraryListHeader::syncTheme() {
  const bool dark = BlopTheme::instance().isDark();
  const QString bg =
      dark ? QStringLiteral("#181B22") : QStringLiteral("#F9FAFB");
  const QString bd = dark ? QStringLiteral("rgba(255,255,255,0.10)")
                          : QStringLiteral("#E5E7EB");
  setStyleSheet(QStringLiteral(
                    "QWidget#LibraryListHeader {"
                    "  background: %1;"
                    "  border-bottom: 1px solid %2;"
                    "}"
                    "QAbstractButton#LibraryListMasterCheck {"
                    "  background: transparent; border: none;"
                    "}")
                    .arg(bg, bd));
  update();
}

void LibraryListHeader::layoutCheck() {
  if (!m_master)
    return;
  const auto cols = LibraryListColumns::columnRects(rect());
  const QRect box = LibraryListColumns::checkboxRect(cols[LibraryListColumns::Select]);
  m_master->setGeometry(box);
}

void LibraryListHeader::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  layoutCheck();
}

QRect LibraryListHeader::labelRect(int col) const {
  const auto cols = LibraryListColumns::columnRects(rect());
  if (col < 0 || col >= LibraryListColumns::Count)
    return {};
  if (col == LibraryListColumns::Select)
    return {};
  return cols[col].adjusted(UiScale::dp(2), 0, -UiScale::dp(2), 0);
}

LibraryListHeader::SortColumn
LibraryListHeader::columnAt(const QPoint &pos) const {
  const auto cols = LibraryListColumns::columnRects(rect());
  if (cols[LibraryListColumns::Name].contains(pos))
    return SortColumn::Name;
  if (cols[LibraryListColumns::Place].contains(pos))
    return SortColumn::Place;
  if (cols[LibraryListColumns::Extent].contains(pos))
    return SortColumn::Extent;
  if (cols[LibraryListColumns::Modified].contains(pos))
    return SortColumn::Modified;
  return SortColumn::Name;
}

void LibraryListHeader::paintEvent(QPaintEvent *event) {
  QWidget::paintEvent(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const bool dark = BlopTheme::instance().isDark();
  const QColor muted = dark ? QColor(0x9C, 0xA3, 0xAF)
                            : QColor(0x6B, 0x72, 0x80);
  const QColor hover =
      dark ? QColor(255, 255, 255, 18) : QColor(0xF3, 0xF4, 0xF6);

  QFont f = font();
  f.setPixelSize(UiScale::sp(11));
  f.setWeight(QFont::DemiBold);
  f.setCapitalization(QFont::AllUppercase);
  f.setLetterSpacing(QFont::AbsoluteSpacing, UiScale::dp(0.5));
  p.setFont(f);

  struct Label {
    int col;
    SortColumn sort;
    QString text;
    bool sortable;
    Qt::Alignment align;
  };
  const Label labels[] = {
      {LibraryListColumns::Name, SortColumn::Name, QStringLiteral("Name"), true,
       Qt::AlignLeft | Qt::AlignVCenter},
      {LibraryListColumns::Tags, SortColumn::Name, QStringLiteral("Tags"), false,
       Qt::AlignLeft | Qt::AlignVCenter},
      {LibraryListColumns::Place, SortColumn::Place,
       QStringLiteral("Speicherort"), true, Qt::AlignLeft | Qt::AlignVCenter},
      {LibraryListColumns::Extent, SortColumn::Extent, QStringLiteral("Umfang"),
       true, Qt::AlignLeft | Qt::AlignVCenter},
      {LibraryListColumns::Modified, SortColumn::Modified,
       QStringLiteral("Geändert"), true, Qt::AlignRight | Qt::AlignVCenter},
  };

  const auto cols = LibraryListColumns::columnRects(rect());
  for (const Label &lb : labels) {
    QRect r = cols[lb.col].adjusted(UiScale::dp(2), 0, -UiScale::dp(2), 0);
    if (m_hoverCol == lb.col && lb.sortable) {
      p.setPen(Qt::NoPen);
      p.setBrush(hover);
      p.drawRoundedRect(r.adjusted(0, 2, 0, -2), 4, 4);
    }
    QString text = lb.text;
    if (lb.sortable && m_col == lb.sort) {
      text += m_order == Qt::AscendingOrder ? QStringLiteral(" ↑")
                                            : QStringLiteral(" ↓");
    } else if (lb.sortable) {
      text += QStringLiteral(" ↕");
    }
    p.setPen(muted);
    p.drawText(r, lb.align, text);
  }
  Q_UNUSED(event);
}

void LibraryListHeader::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
  // Ignore clicks on the master checkbox (it handles itself).
  if (m_master && m_master->geometry().contains(event->pos())) {
    QWidget::mousePressEvent(event);
    return;
  }
  const auto cols = LibraryListColumns::columnRects(rect());
  if (cols[LibraryListColumns::Tags].contains(event->pos()))
    return;

  const SortColumn col = columnAt(event->pos());
  Qt::SortOrder order = Qt::AscendingOrder;
  if (col == SortColumn::Modified || col == SortColumn::Extent)
    order = Qt::DescendingOrder;
  if (m_col == col)
    order = (m_order == Qt::AscendingOrder) ? Qt::DescendingOrder
                                            : Qt::AscendingOrder;
  m_col = col;
  m_order = order;
  update();
  emit sortRequested(col, order);
}

void LibraryListHeader::mouseMoveEvent(QMouseEvent *event) {
  const auto cols = LibraryListColumns::columnRects(rect());
  int hover = -1;
  if (cols[LibraryListColumns::Name].contains(event->pos()))
    hover = LibraryListColumns::Name;
  else if (cols[LibraryListColumns::Place].contains(event->pos()))
    hover = LibraryListColumns::Place;
  else if (cols[LibraryListColumns::Extent].contains(event->pos()))
    hover = LibraryListColumns::Extent;
  else if (cols[LibraryListColumns::Modified].contains(event->pos()))
    hover = LibraryListColumns::Modified;
  if (hover != m_hoverCol) {
    m_hoverCol = hover;
    update();
  }
  QWidget::mouseMoveEvent(event);
}

void LibraryListHeader::leaveEvent(QEvent *event) {
  if (m_hoverCol != -1) {
    m_hoverCol = -1;
    update();
  }
  QWidget::leaveEvent(event);
}
