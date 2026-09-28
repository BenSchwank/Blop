#include "dashcanvas.h"

#include "blop_theme.h"
#include "dashpagescroll.h"
#include "dashwidget.h"
#include "phonechrome.h"
#include "uiscale.h"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>
#include <QtMath>
#include <QWheelEvent>
#include <climits>

class DashSnapGhost : public QWidget {
public:
  explicit DashSnapGhost(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    hide();
  }
  void showAt(const QRect &r) {
    setGeometry(r);
    show();
    raise();
    update();
  }
  void clear() { hide(); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor pen = BlopTheme::accentPrimary();
    pen.setAlpha(170);
    QColor brush = BlopTheme::accentPrimary();
    brush.setAlpha(42);
    p.setPen(QPen(pen, 2));
    p.setBrush(brush);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), UiScale::dp(16),
                      UiScale::dp(16));
  }
};

DashCanvas::DashCanvas(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("DashCanvas"));
  setAttribute(Qt::WA_StyledBackground, true);
  // Base fill comes from paintEvent (desk + soft vignette).
  setStyleSheet(QStringLiteral("QWidget#DashCanvas { background: transparent; }"));
  m_ghost = new DashSnapGhost(this);
  m_dragWatch = new QTimer(this);
  m_dragWatch->setInterval(16);
  connect(m_dragWatch, &QTimer::timeout, this, [this]() {
    if (!m_dragging)
      return;
    pinScrollNow();
    // Windows sometimes drops MouseButtonRelease during app-wide filters.
    if (!(QGuiApplication::mouseButtons() & Qt::LeftButton))
      endGesture(true);
  });
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
          [this]() { update(); });
}

void DashCanvas::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  const bool dark = BlopTheme::instance().isDark();
  // Desk sits slightly below card sheets so elevation reads without drop-shadows.
  const QColor desk =
      dark ? QColor(0x12, 0x14, 0x1A) : QColor(0xF4, 0xF5, 0xF7);
  p.fillRect(rect(), desk);

  QRadialGradient vig(rect().center(),
                      qMax(rect().width(), rect().height()) * 0.72);
  if (dark) {
    vig.setColorAt(0.0, QColor(255, 255, 255, 10));
    vig.setColorAt(0.55, QColor(255, 255, 255, 0));
    vig.setColorAt(1.0, QColor(0, 0, 0, 55));
  } else {
    vig.setColorAt(0.0, QColor(255, 255, 255, 110));
    vig.setColorAt(0.55, QColor(255, 255, 255, 0));
    vig.setColorAt(1.0, QColor(15, 23, 42, 10));
  }
  p.fillRect(rect(), vig);
}

void DashCanvas::pinScrollNow() {
  if (QScrollArea *sa = scrollArea()) {
    auto *bar = sa->verticalScrollBar();
    if (auto *dash = qobject_cast<DashPageScroll *>(sa))
      dash->setScrollFrozen(true, m_frozenScrollY);
    if (bar->value() != m_frozenScrollY) {
      QSignalBlocker block(bar);
      bar->setValue(m_frozenScrollY);
    }
  }
}

void DashCanvas::startDragWatch() {
  pinScrollNow();
  if (m_dragWatch && !m_dragWatch->isActive())
    m_dragWatch->start();
}

void DashCanvas::stopDragWatch() {
  if (m_dragWatch)
    m_dragWatch->stop();
}

void DashCanvas::setPhoneMode(bool phone) {
  if (m_phone == phone)
    return;
  m_phone = phone;
  for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it) {
    if (it.value())
      it.value()->setPhoneMode(phone);
  }
  applyPositions();
}

void DashCanvas::setBesideRail(bool on) {
  if (m_besideRail == on)
    return;
  m_besideRail = on;
  applyPositions();
}

void DashCanvas::setEditMode(bool on) {
  m_editMode = on;
  for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it) {
    if (it.value())
      it.value()->setEditMode(on);
  }
  if (!on && m_dragging)
    cancelGesture();
}

void DashCanvas::setSpecs(const QVector<DashboardWidgetSpec> &specs) {
  m_specs = specs;
}

void DashCanvas::restoreSpecs(const QVector<DashboardWidgetSpec> &specs) {
  m_specs = specs;
  // Drop widgets that are no longer visible / present, then rebuild.
  syncWidgets();
  // Force geometry even when size class did not change.
  for (const auto &s : m_specs) {
    if (!s.visible)
      continue;
    if (DashWidget *w = m_widgets.value(s.id)) {
      w->setSizeClass(s.sizeClass);
      w->applyChromePrefs(s.bgEnabled, s.borderEnabled, s.bgColor,
                          s.borderColor);
      w->setEditMode(m_editMode);
      w->setPhoneMode(m_phone);
    }
  }
  applyPositions();
  emit specsChanged(m_specs);
}

DashWidget *DashCanvas::widgetFor(const QString &id) const {
  return m_widgets.value(id, nullptr);
}

void DashCanvas::wireWidget(DashWidget *w) {
  connect(w, &DashWidget::openNotePath, this, &DashCanvas::openNotePath);
  connect(w, &DashWidget::newNoteRequested, this, &DashCanvas::newNoteRequested);
  connect(w, &DashWidget::snapToNotesRequested, this,
          &DashCanvas::snapToNotesRequested);
  connect(w, &DashWidget::studyRequested, this, &DashCanvas::studyRequested);
  connect(w, &DashWidget::searchLibrary, this, &DashCanvas::searchLibrary);
  connect(w, &DashWidget::maximizeCalendarRequested, this,
          &DashCanvas::maximizeCalendarRequested);
  connect(w, &DashWidget::dragHandlePressed, this,
          [this, w](const QPoint &gp) {
            if (m_editMode)
              beginMove(w, gp);
          });
  connect(w, &DashWidget::resizeHandlePressed, this,
          [this, w](const QPoint &gp) {
            if (m_editMode)
              beginResize(w, gp);
          });
  connect(w, &DashWidget::sizeClassPicked, this,
          [this, w](DashSizeClass sc) { resizeBlock(w->blockId(), sc); });
  connect(w, &DashWidget::contentChanged, this, [this]() {
    refreshTodoDependent();
    emit boardContentChanged();
  });
  connect(w, &DashWidget::removeBannerRequested, this,
          &DashCanvas::removeBanner);
  connect(w, &DashWidget::chromePrefsChanged, this,
          [this, w](bool bg, bool border, const QString &bgColor,
                   const QString &borderColor) {
            if (auto *spec = findSpec(w->blockId())) {
              spec->bgEnabled = bg;
              spec->borderEnabled = border;
              spec->bgColor = bgColor;
              spec->borderColor = borderColor;
              emit specsChanged(m_specs);
            }
          });
}

void DashCanvas::syncWidgets() {
  QSet<QString> want;
  for (const auto &s : m_specs) {
    if (s.visible)
      want.insert(s.id);
  }

  const QStringList keys = m_widgets.keys();
  for (const QString &id : keys) {
    if (!want.contains(id)) {
      if (DashWidget *w = m_widgets.take(id))
        delete w;
    }
  }

  for (const auto &s : m_specs) {
    if (!s.visible)
      continue;
    DashWidget *w = m_widgets.value(s.id, nullptr);
    if (!w) {
      w = DashWidget::create(s.id, this);
      m_widgets.insert(s.id, w);
      wireWidget(w);
      w->setPhoneMode(m_phone);
      w->setEditMode(m_editMode);
      w->show();
    }
    w->setSizeClass(s.sizeClass);
    w->applyChromePrefs(s.bgEnabled, s.borderEnabled, s.bgColor, s.borderColor);
  }
  applyPositions();
}

void DashCanvas::refreshAll() {
  for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it) {
    if (it.value())
      it.value()->refreshContent();
  }
}

void DashCanvas::refreshTodoDependent() {
  static const QStringList kIds = {QStringLiteral("today"),
                                   QStringLiteral("todos")};
  for (const QString &id : kIds) {
    if (DashWidget *w = m_widgets.value(id))
      w->refreshContent();
  }
}

void DashCanvas::setBlockVisible(const QString &id, bool visible) {
  auto *spec = findSpec(id);
  if (!spec || spec->visible == visible)
    return;
  emit layoutAboutToChange();
  spec->visible = visible;
  if (visible) {
    int maxBottom = 0;
    for (const auto &s : m_specs) {
      if (!s.visible || s.id == id)
        continue;
      maxBottom = qMax(maxBottom,
                       s.row + DashboardLayoutStore::rowSpanFor(s.sizeClass));
    }
    spec->row = maxBottom;
    spec->col = 0;
    clampSpec(*spec);
    reflowKeeping(id);
  }
  syncWidgets();
  emit specsChanged(m_specs);
}

void DashCanvas::addBanner() {
  emit layoutAboutToChange();
  DashboardWidgetSpec s;
  s.id = DashboardLayoutStore::allocateBannerId(m_specs);
  s.visible = true;
  s.sizeClass = DashSizeClass::L;
  int maxBottom = 0;
  for (const auto &x : m_specs) {
    if (!x.visible)
      continue;
    maxBottom = qMax(maxBottom,
                     x.row + DashboardLayoutStore::rowSpanFor(x.sizeClass));
  }
  s.row = maxBottom;
  s.col = 0;
  s.order = m_specs.size();
  clampSpec(s);
  m_specs.append(s);
  reflowKeeping(s.id);
  syncWidgets();
  emit specsChanged(m_specs);
}

void DashCanvas::removeBanner(const QString &id) {
  if (!DashboardLayoutStore::isBannerId(id))
    return;
  int idx = -1;
  for (int i = 0; i < m_specs.size(); ++i) {
    if (m_specs[i].id == id) {
      idx = i;
      break;
    }
  }
  if (idx < 0)
    return;
  emit layoutAboutToChange();
  m_specs.removeAt(idx);
  if (DashWidget *w = m_widgets.take(id))
    delete w;
  syncWidgets();
  emit specsChanged(m_specs);
}

int DashCanvas::rowUnit() const { return UiScale::dp(92); }
int DashCanvas::hGap() const { return UiScale::dp(m_phone ? 12 : 14); }
int DashCanvas::vGap() const { return UiScale::dp(m_phone ? 12 : 14); }

QMargins DashCanvas::boardMargins() const {
  const int left = UiScale::dp(m_phone ? 16 : 48);
  // When a right rail is present, keep only a quiet gutter — avoids a
  // Windows-looking dead strip + scrollbar gutter before the rail.
  const int right =
      m_phone ? left
              : (m_besideRail ? UiScale::dp(8) : UiScale::dp(48));
  const int top = UiScale::dp(m_phone ? 4 : 8);
  const int bot =
      m_phone ? PhoneChrome::contentBottomInsetPx(const_cast<DashCanvas *>(this))
              : UiScale::dp(48);
  return QMargins(left, top, right, bot);
}

QRect DashCanvas::cellRect(int row, int col, int rowSpan, int colSpan) const {
  const QMargins mg = boardMargins();
  const int availW = qMax(12, width() - mg.left() - mg.right());
  const int gap = hGap();
  const int colW = qMax(1, (availW - 11 * gap) / 12);
  const int x = mg.left() + col * (colW + gap);
  const int w = colSpan * colW + (colSpan - 1) * gap;
  const int y = mg.top() + row * (rowUnit() + vGap());
  const int h = rowSpan * rowUnit() + (rowSpan - 1) * vGap();
  return QRect(x, y, w, h);
}

int DashCanvas::contentBottom() const {
  if (m_phone) {
    int y = boardMargins().top();
    for (const auto &s : m_specs) {
      if (!s.visible)
        continue;
      const int rs = DashboardLayoutStore::rowSpanFor(s.sizeClass);
      y += rs * rowUnit() + (rs - 1) * vGap() + vGap();
    }
    return y + boardMargins().bottom();
  }
  int maxBottom = boardMargins().top();
  for (const auto &s : m_specs) {
    if (!s.visible)
      continue;
    const int rs = DashboardLayoutStore::rowSpanFor(s.sizeClass);
    maxBottom = qMax(maxBottom, boardMargins().top() +
                                    (s.row + rs) * (rowUnit() + vGap()));
  }
  return maxBottom + boardMargins().bottom();
}

void DashCanvas::layoutPhoneStack() {
  int row = 0;
  for (const auto &s : m_specs) {
    if (!s.visible)
      continue;
    DashWidget *w = m_widgets.value(s.id);
    if (!w)
      continue;
    const int rs = DashboardLayoutStore::rowSpanFor(s.sizeClass);
    const QRect r = cellRect(row, 0, rs, 12);
    w->setGeometry(r);
    w->setFixedHeight(r.height());
    w->raise();
    row += rs;
  }
  setMinimumHeight(contentBottom());
}

void DashCanvas::layoutDesktopGrid() {
  for (const auto &s : m_specs) {
    if (!s.visible)
      continue;
    DashWidget *w = m_widgets.value(s.id);
    if (!w || (m_dragging && s.id == m_dragId))
      continue;
    const int cs = DashboardLayoutStore::colSpanFor(s.sizeClass);
    const int rs = DashboardLayoutStore::rowSpanFor(s.sizeClass);
    const QRect r = cellRect(s.row, s.col, rs, cs);
    w->setGeometry(r);
    w->setFixedHeight(r.height());
  }
  if (m_editMode)
    setMinimumHeight(contentBottom() + 3 * (rowUnit() + vGap()));
  else
    setMinimumHeight(contentBottom());
}

void DashCanvas::applyPositions() {
  if (m_phone)
    layoutPhoneStack();
  else
    layoutDesktopGrid();
  if (m_ghost)
    m_ghost->raise();
}

void DashCanvas::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  applyPositions();
}

bool DashCanvas::overlaps(const DashboardWidgetSpec &a,
                          const DashboardWidgetSpec &b) const {
  if (a.id == b.id || !a.visible || !b.visible)
    return false;
  const int aR = a.row + DashboardLayoutStore::rowSpanFor(a.sizeClass);
  const int aC = a.col + DashboardLayoutStore::colSpanFor(a.sizeClass);
  const int bR = b.row + DashboardLayoutStore::rowSpanFor(b.sizeClass);
  const int bC = b.col + DashboardLayoutStore::colSpanFor(b.sizeClass);
  return a.row < bR && b.row < aR && a.col < bC && b.col < aC;
}

bool DashCanvas::canPlace(const DashboardWidgetSpec &candidate,
                          const QString &ignoreId) const {
  const int cs = DashboardLayoutStore::colSpanFor(candidate.sizeClass);
  if (candidate.col < 0 || candidate.row < 0 || candidate.col + cs > 12)
    return false;
  for (const auto &s : m_specs) {
    if (!s.visible || s.id == ignoreId)
      continue;
    if (overlaps(candidate, s))
      return false;
  }
  return true;
}

void DashCanvas::clampSpec(DashboardWidgetSpec &s) const {
  const int cs = DashboardLayoutStore::colSpanFor(s.sizeClass);
  s.col = qBound(0, s.col, 12 - cs);
  s.row = qMax(0, s.row);
}

DashboardWidgetSpec *DashCanvas::findSpec(const QString &id) {
  for (auto &s : m_specs) {
    if (s.id == id)
      return &s;
  }
  return nullptr;
}

DashSizeClass DashCanvas::nearestSizeClass(int colSpan, int rowSpan) const {
  return DashboardLayoutStore::fromSpans(colSpan, rowSpan);
}

void DashCanvas::reflowKeeping(const QString &anchorId) {
  if (auto *anchor = findSpec(anchorId))
    clampSpec(*anchor);

  for (int pass = 0; pass < 32; ++pass) {
    bool changed = false;
    for (int i = 0; i < m_specs.size(); ++i) {
      if (!m_specs[i].visible)
        continue;
      for (int j = 0; j < m_specs.size(); ++j) {
        if (i == j || !m_specs[j].visible)
          continue;
        if (!overlaps(m_specs[i], m_specs[j]))
          continue;

        DashboardWidgetSpec *stay = &m_specs[i];
        DashboardWidgetSpec *move = &m_specs[j];
        if (move->id == anchorId) {
          stay = &m_specs[j];
          move = &m_specs[i];
        } else if (stay->id != anchorId && move->id != anchorId) {
          // Prefer moving the one further down / right.
          if (m_specs[i].row > m_specs[j].row ||
              (m_specs[i].row == m_specs[j].row &&
               m_specs[i].col >= m_specs[j].col)) {
            stay = &m_specs[j];
            move = &m_specs[i];
          }
        }

        const int stayRight =
            stay->col + DashboardLayoutStore::colSpanFor(stay->sizeClass);
        const int stayBottom =
            stay->row + DashboardLayoutStore::rowSpanFor(stay->sizeClass);
        const int moveCs = DashboardLayoutStore::colSpanFor(move->sizeClass);
        const int oldRow = move->row;
        const int oldCol = move->col;

        // 1) Try east of the staying block.
        if (stayRight + moveCs <= 12) {
          move->col = stayRight;
          move->row = stay->row;
          clampSpec(*move);
          if (!overlaps(*stay, *move)) {
            // Check vs all others briefly — if still overlapping someone, keep trying.
            bool ok = true;
            for (const auto &o : m_specs) {
              if (!o.visible || o.id == move->id)
                continue;
              if (overlaps(*move, o)) {
                ok = false;
                break;
              }
            }
            if (ok) {
              changed = true;
              continue;
            }
          }
          move->row = oldRow;
          move->col = oldCol;
        }

        // 2) Push south — always keeps the board valid.
        move->row = stayBottom;
        move->col = qBound(0, move->col, 12 - moveCs);
        if (move->row != oldRow || move->col != oldCol)
          changed = true;
      }
    }
    if (!changed)
      break;
  }

  // Final clamp all.
  for (auto &s : m_specs)
    clampSpec(s);
}

void DashCanvas::resizeBlock(const QString &id, DashSizeClass sizeClass) {
  auto *spec = findSpec(id);
  if (!spec)
    return;
  if (spec->sizeClass == sizeClass)
    return;
  emit layoutAboutToChange();
  spec->sizeClass = sizeClass;
  clampSpec(*spec);
  reflowKeeping(id);
  if (DashWidget *w = m_widgets.value(id))
    w->setSizeClass(sizeClass);
  emit specsChanged(m_specs);
  applyPositions();
}

QScrollArea *DashCanvas::scrollArea() const {
  for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
    if (auto *sa = qobject_cast<QScrollArea *>(w))
      return sa;
  }
  return nullptr;
}

void DashCanvas::setScrollLocked(bool locked) {
  QScrollArea *sa = scrollArea();
  if (!sa)
    return;
  auto *bar = sa->verticalScrollBar();
  auto *dashScroll = qobject_cast<DashPageScroll *>(sa);
  if (locked) {
    m_frozenScrollY = bar->value();
    m_savedVScrollPolicy = sa->verticalScrollBarPolicy();
    if (dashScroll)
      dashScroll->setScrollFrozen(true, m_frozenScrollY);
    // Hide + disable the bar so Qt cannot auto-scroll while the finger grows
    // a card past the viewport edge.
    sa->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bar->setEnabled(false);
    if (sa->viewport() && !m_viewportFilterInstalled) {
      sa->viewport()->installEventFilter(this);
      m_viewportFilterInstalled = true;
    }
    QObject::disconnect(m_scrollFreezeConn);
    QObject::disconnect(m_scrollRangeConn);
    m_scrollFreezeConn =
        connect(bar, &QScrollBar::valueChanged, this, [this, bar](int) {
          if (!m_dragging)
            return;
          QSignalBlocker block(bar);
          bar->setValue(m_frozenScrollY);
        });
    m_scrollRangeConn =
        connect(bar, &QScrollBar::rangeChanged, this, [this, bar](int, int) {
          if (!m_dragging)
            return;
          QSignalBlocker block(bar);
          bar->setValue(m_frozenScrollY);
        });
  } else {
    QObject::disconnect(m_scrollFreezeConn);
    QObject::disconnect(m_scrollRangeConn);
    if (sa->viewport() && m_viewportFilterInstalled) {
      sa->viewport()->removeEventFilter(this);
      m_viewportFilterInstalled = false;
    }
    if (dashScroll)
      dashScroll->setScrollFrozen(false);
    bar->setEnabled(true);
    // Keep native chrome off — OverlayScrollIndicator owns the affordance.
    sa->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  }
}

void DashCanvas::beginMove(DashWidget *w, const QPoint &globalPos) {
  if (!w || m_phone || !m_editMode)
    return;
  auto *spec = findSpec(w->blockId());
  if (!spec)
    return;
  // Never stack gestures — sticky scroll/chase comes from a half-dead drag.
  if (m_dragging)
    endGesture(false);

  m_dragging = true;
  m_gesture = Gesture::Move;
  m_dragWidget = w;
  m_dragId = w->blockId();
  m_pressGlobal = globalPos;
  m_originTopLeft = w->pos();
  m_originSize = w->size();
  m_startSize = spec->sizeClass;
  m_startRow = spec->row;
  m_startCol = spec->col;
  m_previewSize = spec->sizeClass;
  m_previewRow = spec->row;
  m_previewCol = spec->col;

  setScrollLocked(true);
  w->setLifted(true);
  w->raise();
  w->clearFocus();
  const int cs = DashboardLayoutStore::colSpanFor(spec->sizeClass);
  const int rs = DashboardLayoutStore::rowSpanFor(spec->sizeClass);
  if (m_ghost)
    m_ghost->showAt(cellRect(m_previewRow, m_previewCol, rs, cs));
  // App-wide filter only — no mouse grab. Grabbing inside a QScrollArea makes
  // the viewport chase the cursor while/after resize.
  qApp->installEventFilter(this);
  startDragWatch();
}

void DashCanvas::beginResize(DashWidget *w, const QPoint &globalPos) {
  if (!w || m_phone || !m_editMode)
    return;
  auto *spec = findSpec(w->blockId());
  if (!spec)
    return;
  if (m_dragging)
    endGesture(false);

  m_dragging = true;
  m_gesture = Gesture::Resize;
  m_dragWidget = w;
  m_dragId = w->blockId();
  m_pressGlobal = globalPos;
  m_originTopLeft = w->pos();
  m_originSize = w->size();
  m_startSize = spec->sizeClass;
  m_startRow = spec->row;
  m_startCol = spec->col;
  m_previewSize = spec->sizeClass;
  m_previewRow = spec->row;
  m_previewCol = spec->col;

  setScrollLocked(true);
  w->setLifted(true);
  w->raise();
  w->clearFocus();
  const int cs = DashboardLayoutStore::colSpanFor(spec->sizeClass);
  const int rs = DashboardLayoutStore::rowSpanFor(spec->sizeClass);
  if (m_ghost)
    m_ghost->showAt(cellRect(m_previewRow, m_previewCol, rs, cs));
  qApp->installEventFilter(this);
  startDragWatch();
}

void DashCanvas::updateGesture(const QPoint &globalPos) {
  if (!m_dragging || !m_dragWidget)
    return;
  auto *spec = findSpec(m_dragId);
  if (!spec)
    return;

  // Keep scroll pinned for the whole gesture (Qt likes to chase the grabber).
  pinScrollNow();

  const QMargins mg = boardMargins();
  const int availW = qMax(12, width() - mg.left() - mg.right());
  const int gap = hGap();
  const int colW = qMax(1, (availW - 11 * gap) / 12);
  const int pitchX = colW + gap;
  const int pitchY = rowUnit() + vGap();

  if (m_gesture == Gesture::Move) {
    // Ghost-only move: never relocate the live card during drag — that made
    // QScrollArea chase the cursor until the next click.
    const QPoint delta = globalPos - m_pressGlobal;
    const QPoint topLeft = m_originTopLeft + delta;
    const int cs = DashboardLayoutStore::colSpanFor(spec->sizeClass);
    const int rs = DashboardLayoutStore::rowSpanFor(spec->sizeClass);
    int col = qRound(double(topLeft.x() - mg.left()) / double(pitchX));
    int row = qRound(double(topLeft.y() - mg.top()) / double(pitchY));
    col = qBound(0, col, 12 - cs);
    row = qMax(0, row);
    m_previewRow = row;
    m_previewCol = col;
    m_previewSize = spec->sizeClass;
    if (m_ghost)
      m_ghost->showAt(cellRect(m_previewRow, m_previewCol, rs, cs));
    pinScrollNow();
    return;
  }

  // Resize: grow down without stealing full width. Width only changes when
  // the pointer clearly moves sideways from the press point.
  const QPoint local = mapFromGlobal(globalPos);
  const int startCs = DashboardLayoutStore::colSpanFor(m_startSize);
  const int dx = globalPos.x() - m_pressGlobal.x();
  const int dy = globalPos.y() - m_pressGlobal.y();
  const bool widenIntent =
      qAbs(dx) > pitchX * 0.7 && qAbs(dx) >= qAbs(dy) * 0.45;

  int rightCol = qRound(double(local.x() - mg.left() + colW * 0.25) /
                        double(pitchX));
  int bottomRow = qRound(double(local.y() - mg.top() + rowUnit() * 0.25) /
                         double(pitchY));
  rightCol = qBound(m_previewCol + 1, rightCol, 12);
  bottomRow = qBound(m_previewRow + 1, bottomRow, m_previewRow + 10);

  const int wantCs =
      widenIntent ? qMax(1, rightCol - m_previewCol) : startCs;
  const int wantRs = qMax(1, bottomRow - m_previewRow);
  DashSizeClass next = nearestSizeClass(wantCs, wantRs);

  int cs = DashboardLayoutStore::colSpanFor(next);
  // Hard lock: never wider than start unless widenIntent.
  if (!widenIntent && cs > startCs) {
    next = nearestSizeClass(startCs, wantRs);
    cs = DashboardLayoutStore::colSpanFor(next);
  }
  if (m_previewCol + cs > 12) {
    const int rsWant = DashboardLayoutStore::rowSpanFor(next);
    if (m_previewCol + 9 <= 12)
      next = nearestSizeClass(9, rsWant);
    else if (m_previewCol + 6 <= 12)
      next = nearestSizeClass(6, rsWant);
    else
      next = nearestSizeClass(3, rsWant);
    cs = DashboardLayoutStore::colSpanFor(next);
    if (m_previewCol + cs > 12)
      m_previewCol = qMax(0, 12 - cs);
  }

  m_previewSize = next;
  const int rs = DashboardLayoutStore::rowSpanFor(next);
  const QRect target = cellRect(m_previewRow, m_previewCol, rs, cs);
  // Ghost-only resize preview — live geometry stays put until commit so the
  // scroll area cannot chase a growing child under the cursor.
  if (m_ghost)
    m_ghost->showAt(target);
  pinScrollNow();
}

void DashCanvas::cancelGesture() { endGesture(false); }

void DashCanvas::endGesture(bool commit) {
  if (!m_dragging)
    return;
  // Mark idle FIRST so nested filters / timers cannot re-enter updateGesture.
  m_dragging = false;
  stopDragWatch();
  const int keepScrollY = m_frozenScrollY;
  const Gesture finished = m_gesture;
  m_gesture = Gesture::None;

  // Tear down input capture before layout commits.
  qApp->removeEventFilter(this);
  if (QWidget *g = QWidget::mouseGrabber())
    g->releaseMouse();
  // Belt-and-suspenders: any grabber left on children of this board.
  if (m_dragWidget) {
    m_dragWidget->releaseMouse();
    m_dragWidget->clearFocus();
    m_dragWidget->setLifted(false);
  }
  setScrollLocked(false);

  auto *spec = findSpec(m_dragId);
  if (spec) {
    if (commit) {
      const bool changed =
          (finished == Gesture::Move &&
           (spec->row != m_previewRow || spec->col != m_previewCol)) ||
          (finished == Gesture::Resize &&
           (spec->sizeClass != m_previewSize || spec->row != m_previewRow ||
            spec->col != m_previewCol));
      if (changed)
        emit layoutAboutToChange();
      if (finished == Gesture::Move) {
        spec->row = m_previewRow;
        spec->col = m_previewCol;
        clampSpec(*spec);
        reflowKeeping(m_dragId);
      } else if (finished == Gesture::Resize) {
        spec->sizeClass = m_previewSize;
        spec->row = m_previewRow;
        spec->col = m_previewCol;
        clampSpec(*spec);
        reflowKeeping(m_dragId);
      }
      if (changed)
        emit specsChanged(m_specs);
    } else {
      // Esc / cancel: restore footprint, do not persist preview.
      spec->row = m_startRow;
      spec->col = m_startCol;
      spec->sizeClass = m_startSize;
      clampSpec(*spec);
    }
  }

  clearGhost();
  if (m_dragWidget && spec)
    m_dragWidget->setSizeClass(spec->sizeClass);

  m_dragWidget = nullptr;
  m_dragId.clear();

  if (QScrollArea *sa = scrollArea()) {
    auto *bar = sa->verticalScrollBar();
    QSignalBlocker block(bar);
    bar->setValue(keepScrollY);
    if (auto *dash = qobject_cast<DashPageScroll *>(sa))
      dash->setScrollFrozen(false);
  }

  applyPositions();

  const auto pin = [this, keepScrollY]() {
    if (m_dragging)
      return;
    if (QScrollArea *sa = scrollArea()) {
      auto *bar = sa->verticalScrollBar();
      if (auto *dash = qobject_cast<DashPageScroll *>(sa))
        dash->setScrollFrozen(false);
      if (bar->value() != keepScrollY) {
        QSignalBlocker block(bar);
        bar->setValue(keepScrollY);
      }
    }
  };
  QTimer::singleShot(0, this, pin);
  QTimer::singleShot(32, this, pin);
}

void DashCanvas::clearGhost() {
  if (m_ghost)
    m_ghost->clear();
}

bool DashCanvas::eventFilter(QObject *watched, QEvent *event) {
  if (!m_dragging)
    return QWidget::eventFilter(watched, event);

  const auto leftIsDown = [](const QMouseEvent *me) {
    return me && (me->buttons() & Qt::LeftButton);
  };

  switch (event->type()) {
  case QEvent::MouseMove:
  case QEvent::HoverMove:
  case QEvent::NonClientAreaMouseMove: {
    const auto *me = dynamic_cast<QMouseEvent *>(event);
    // Missed mouse-up (common on Windows) — HoverMove alone must also end.
    if (!leftIsDown(me) &&
        !(QGuiApplication::mouseButtons() & Qt::LeftButton)) {
      endGesture(true);
      // Let the event continue so Qt can clear any sticky button/scroll state.
      return false;
    }
    if (me)
      updateGesture(me->globalPosition().toPoint());
    else
      updateGesture(QCursor::pos());
    return true;
  }
  case QEvent::MouseButtonRelease:
  case QEvent::NonClientAreaMouseButtonRelease:
  case QEvent::TouchEnd:
  case QEvent::TouchCancel: {
    if (event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::NonClientAreaMouseButtonRelease) {
      const auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() != Qt::LeftButton)
        break;
    }
    endGesture(true);
    // Critical: do NOT swallow the release — consuming it left Windows with a
    // sticky "drag scroll" until the next click.
    return false;
  }
  case QEvent::WindowDeactivate:
  case QEvent::FocusOut:
  case QEvent::ApplicationStateChange:
    endGesture(true);
    return false;
  case QEvent::KeyPress: {
    const auto *ke = static_cast<QKeyEvent *>(event);
    if (ke->key() == Qt::Key_Escape) {
      cancelGesture();
      return true;
    }
    break;
  }
  case QEvent::Wheel:
  case QEvent::Scroll:
    // Keep scroll frozen for the gesture; don't let the board nudge.
    return true;
  default:
    break;
  }
  Q_UNUSED(watched);
  return QWidget::eventFilter(watched, event);
}
