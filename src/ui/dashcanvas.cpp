#include "dashcanvas.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "dashpagescroll.h"
#include "dashwidget.h"
#include "phonechrome.h"
#include "uiscale.h"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>
#include <QtMath>
#include <QWheelEvent>
#include <algorithm>
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
  // Desk matches shell content plate — cards (obsidianSheet) float above.
  const QColor desk =
      dark ? BlopStyle::obsidianContent() : BlopStyle::paperBg();
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
  // Leaving Anpassen must never leave the page scroller frozen.
  if (!on)
    setScrollLocked(false);
  applyPositions();
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
int DashCanvas::hGap() const { return UiScale::dp(m_phone ? 10 : 12); }
int DashCanvas::vGap() const { return UiScale::dp(m_phone ? 8 : 8); }

QMargins DashCanvas::boardMargins() const {
  const int left = UiScale::dp(m_phone ? 16 : 40);
  // When a right rail is present, keep only a quiet gutter — avoids a
  // Windows-looking dead strip + scrollbar gutter before the rail.
  const int right =
      m_phone ? left
              : (m_besideRail ? UiScale::dp(8) : UiScale::dp(40));
  // Tight top so cover sits close under the title bar / Anpassen overlay.
  const int top = UiScale::dp(m_phone ? 2 : 4);
  // Extra bottom pad so the last board row can clear the window edge and the
  // page always has a little rubber-band room under the cards.
  const int bot =
      m_phone ? PhoneChrome::contentBottomInsetPx(const_cast<DashCanvas *>(this))
              : UiScale::dp(72);
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

int DashCanvas::contentBottomOf(
    const QVector<DashboardWidgetSpec> &specs) const {
  if (m_phone) {
    int y = boardMargins().top();
    for (const auto &s : specs) {
      if (!s.visible)
        continue;
      const int rs = DashboardLayoutStore::rowSpanFor(s.sizeClass);
      y += rs * rowUnit() + (rs - 1) * vGap() + vGap();
    }
    return y + boardMargins().bottom();
  }
  int maxBottom = boardMargins().top();
  for (const auto &s : specs) {
    if (!s.visible)
      continue;
    const int rs = DashboardLayoutStore::rowSpanFor(s.sizeClass);
    maxBottom = qMax(maxBottom, boardMargins().top() +
                                    (s.row + rs) * (rowUnit() + vGap()));
  }
  return maxBottom + boardMargins().bottom();
}

int DashCanvas::contentBottom() const { return contentBottomOf(m_specs); }

QSize DashCanvas::sizeHint() const {
  return QSize(width() > 0 ? width() : UiScale::dp(720),
               qMax(contentBottom(), UiScale::dp(480)));
}

QSize DashCanvas::minimumSizeHint() const {
  return QSize(UiScale::dp(280), contentBottom());
}

void DashCanvas::layoutPhoneStack() {
  int y = boardMargins().top();
  const int x = boardMargins().left();
  const int wAvail =
      qMax(12, width() - boardMargins().left() - boardMargins().right());
  for (const auto &s : m_specs) {
    if (!s.visible)
      continue;
    DashWidget *w = m_widgets.value(s.id);
    if (!w)
      continue;
    const int rs = DashboardLayoutStore::rowSpanFor(s.sizeClass);
    const int cellH = rs * rowUnit() + (rs - 1) * vGap();
    int h = cellH;
    if (!m_editMode && s.id == QLatin1String("intro")) {
      w->setMinimumHeight(0);
      w->setMaximumHeight(QWIDGETSIZE_MAX);
      w->setFixedWidth(wAvail);
      // Never clip greeting/CTAs — grow past the Title cell if needed.
      h = qMax(UiScale::dp(176), w->sizeHint().height());
    }
    w->setGeometry(x, y, wAvail, h);
    w->setFixedHeight(h);
    w->raise();
    y += h + vGap();
  }
  setMinimumHeight(y + boardMargins().bottom());
}

void DashCanvas::layoutDesktopGrid() {
  // Intro sizes to content. Positive slack = reclaim empty Title space;
  // negative slack = grow past Title and push cards down (no clipped greeting).
  int introRow = -1;
  int introSlack = 0;
  if (!m_editMode) {
    for (const auto &s : m_specs) {
      if (!s.visible || s.id != QLatin1String("intro"))
        continue;
      DashWidget *intro = m_widgets.value(s.id);
      if (!intro)
        break;
      const int cs = DashboardLayoutStore::colSpanFor(s.sizeClass);
      const int rs = DashboardLayoutStore::rowSpanFor(s.sizeClass);
      const QRect full = cellRect(s.row, s.col, rs, cs);
      // Drop prior fixedHeight so sizeHint isn't capped to the Title cell.
      intro->setMinimumHeight(0);
      intro->setMaximumHeight(QWIDGETSIZE_MAX);
      intro->setFixedWidth(full.width());
      if (intro->layout())
        intro->layout()->activate();
      const int hug = qMax(UiScale::dp(176), intro->sizeHint().height());
      introRow = s.row;
      introSlack = full.height() - hug;
      break;
    }
  }

  for (const auto &s : m_specs) {
    if (!s.visible)
      continue;
    DashWidget *w = m_widgets.value(s.id);
    if (!w || (m_dragging && s.id == m_dragId))
      continue;
    const int cs = DashboardLayoutStore::colSpanFor(s.sizeClass);
    const int rs = DashboardLayoutStore::rowSpanFor(s.sizeClass);
    QRect r = cellRect(s.row, s.col, rs, cs);
    if (introSlack != 0 && introRow >= 0 && s.row > introRow)
      r.translate(0, -introSlack);
    if (!m_editMode && s.id == QLatin1String("intro"))
      r.setHeight(r.height() - introSlack);
    w->setGeometry(r);
    w->setFixedHeight(r.height());
  }
  const int slack = (!m_editMode) ? introSlack : 0;
  if (m_editMode)
    setMinimumHeight(contentBottom() + 3 * (rowUnit() + vGap()));
  else
    setMinimumHeight(qMax(0, contentBottom() - slack));
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
  return findSpecIn(m_specs, id);
}

DashboardWidgetSpec *DashCanvas::findSpecIn(QVector<DashboardWidgetSpec> &specs,
                                            const QString &id) {
  for (auto &s : specs) {
    if (s.id == id)
      return &s;
  }
  return nullptr;
}

DashSizeClass DashCanvas::nearestSizeClass(int colSpan, int rowSpan) const {
  return DashboardLayoutStore::fromSpans(colSpan, rowSpan);
}

bool DashCanvas::specsLayoutEqual(const QVector<DashboardWidgetSpec> &a,
                                  const QVector<DashboardWidgetSpec> &b) {
  if (a.size() != b.size())
    return false;
  for (int i = 0; i < a.size(); ++i) {
    if (a[i].id != b[i].id || a[i].visible != b[i].visible ||
        a[i].row != b[i].row || a[i].col != b[i].col ||
        a[i].sizeClass != b[i].sizeClass)
      return false;
  }
  return true;
}

void DashCanvas::reflowPushDown(QVector<DashboardWidgetSpec> &specs,
                                const QString &anchorId, bool floatUp) {
  for (auto &s : specs)
    clampSpec(s);

  QVector<int> order;
  int pinned = -1;
  for (int i = 0; i < specs.size(); ++i) {
    if (!specs[i].visible)
      continue;
    if (specs[i].id == anchorId)
      pinned = i;
    else
      order.append(i);
  }
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    if (specs[a].row != specs[b].row)
      return specs[a].row < specs[b].row;
    return specs[a].col < specs[b].col;
  });
  if (pinned >= 0)
    order.prepend(pinned);

  // Push-down: later cards move below anything they collide with (col stays).
  QVector<int> placed;
  for (int idx : order) {
    DashboardWidgetSpec &it = specs[idx];
    bool moved = true;
    while (moved) {
      moved = false;
      for (int p : placed) {
        if (overlaps(it, specs[p])) {
          it.row = specs[p].row +
                   DashboardLayoutStore::rowSpanFor(specs[p].sizeClass);
          clampSpec(it);
          moved = true;
        }
      }
    }
    placed.append(idx);
  }

  if (floatUp) {
    QVector<int> byRow;
    byRow.reserve(specs.size());
    for (int i = 0; i < specs.size(); ++i) {
      if (specs[i].visible)
        byRow.append(i);
    }
    std::stable_sort(byRow.begin(), byRow.end(), [&](int a, int b) {
      if (specs[a].row != specs[b].row)
        return specs[a].row < specs[b].row;
      return specs[a].col < specs[b].col;
    });
    QVector<int> settled;
    for (int idx : byRow) {
      DashboardWidgetSpec &it = specs[idx];
      while (it.row > 0) {
        DashboardWidgetSpec probe = it;
        --probe.row;
        bool blocked = false;
        for (int s : settled) {
          if (overlaps(probe, specs[s])) {
            blocked = true;
            break;
          }
        }
        if (blocked)
          break;
        it.row = probe.row;
      }
      settled.append(idx);
    }
  }

  for (auto &s : specs)
    clampSpec(s);
}

void DashCanvas::reflowKeeping(const QString &anchorId) {
  reflowPushDown(m_specs, anchorId, /*floatUp=*/true);
}

void DashCanvas::stopNeighborAnims() {
  for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it) {
    DashWidget *w = it.value();
    if (!w)
      continue;
    const auto anims = w->findChildren<QPropertyAnimation *>(
        QStringLiteral("dashMove"), Qt::FindDirectChildrenOnly);
    for (QPropertyAnimation *a : anims) {
      a->setObjectName(QString());
      a->stop();
    }
  }
}

void DashCanvas::applySpecGeometries(const QVector<DashboardWidgetSpec> &specs,
                                     bool animate, const QString &skipId) {
  const int bottom = contentBottomOf(specs);
  if (m_editMode)
    setMinimumHeight(bottom + 3 * (rowUnit() + vGap()));
  else
    setMinimumHeight(bottom);

  for (const auto &s : specs) {
    if (!s.visible || s.id == skipId)
      continue;
    DashWidget *w = m_widgets.value(s.id);
    if (!w)
      continue;
    const int cs = DashboardLayoutStore::colSpanFor(s.sizeClass);
    const int rs = DashboardLayoutStore::rowSpanFor(s.sizeClass);
    const QRect target = cellRect(s.row, s.col, rs, cs);

    QPropertyAnimation *running = nullptr;
    const auto anims = w->findChildren<QPropertyAnimation *>(
        QStringLiteral("dashMove"), Qt::FindDirectChildrenOnly);
    for (QPropertyAnimation *a : anims) {
      if (a->state() == QAbstractAnimation::Running)
        running = a;
    }
    if (running && running->endValue().toRect() == target && animate)
      continue;
    if (running) {
      running->setObjectName(QString());
      running->stop();
    }
    if (!animate || !isVisible()) {
      w->setGeometry(target);
      w->setFixedHeight(target.height());
      continue;
    }
    if (w->geometry() == target) {
      w->setFixedHeight(target.height());
      continue;
    }
    // Allow geometry animation (fixedHeight would freeze the rect).
    w->setMinimumHeight(0);
    w->setMaximumHeight(QWIDGETSIZE_MAX);
    auto *anim = new QPropertyAnimation(w, "geometry", w);
    anim->setObjectName(QStringLiteral("dashMove"));
    anim->setDuration(BlopMotion::kFast);
    anim->setEasingCurve(BlopMotion::kEaseStandard);
    anim->setStartValue(w->geometry());
    anim->setEndValue(target);
    connect(anim, &QPropertyAnimation::finished, w, [w, target]() {
      if (w)
        w->setFixedHeight(target.height());
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
  }
}

void DashCanvas::syncGesturePreview(bool animate) {
  m_previewSpecs = m_specs;
  if (DashboardWidgetSpec *anchor = findSpecIn(m_previewSpecs, m_dragId)) {
    anchor->row = m_previewRow;
    anchor->col = m_previewCol;
    anchor->sizeClass = m_previewSize;
    clampSpec(*anchor);
  }
  // Live gesture: push only — float-up on commit closes holes after shrink.
  reflowPushDown(m_previewSpecs, m_dragId, /*floatUp=*/false);
  applySpecGeometries(m_previewSpecs, animate, m_dragId);

  if (auto *anchor = findSpecIn(m_previewSpecs, m_dragId)) {
    const int cs = DashboardLayoutStore::colSpanFor(anchor->sizeClass);
    const int rs = DashboardLayoutStore::rowSpanFor(anchor->sizeClass);
    if (m_ghost)
      m_ghost->showAt(cellRect(anchor->row, anchor->col, rs, cs));
  }
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
  m_previewSpecs = m_specs;

  setScrollLocked(true);
  w->setLifted(true);
  w->raise();
  w->clearFocus();
  syncGesturePreview(/*animate=*/false);
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
  m_previewSpecs = m_specs;

  setScrollLocked(true);
  w->setLifted(true);
  w->raise();
  w->clearFocus();
  syncGesturePreview(/*animate=*/false);
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
    // Ghost-only anchor: never relocate the live card during drag — that made
    // QScrollArea chase the cursor until the next click. Neighbors push-down.
    const QPoint delta = globalPos - m_pressGlobal;
    const QPoint topLeft = m_originTopLeft + delta;
    const int cs = DashboardLayoutStore::colSpanFor(spec->sizeClass);
    int col = qRound(double(topLeft.x() - mg.left()) / double(pitchX));
    int row = qRound(double(topLeft.y() - mg.top()) / double(pitchY));
    col = qBound(0, col, 12 - cs);
    row = qMax(0, row);
    m_previewRow = row;
    m_previewCol = col;
    m_previewSize = spec->sizeClass;
    syncGesturePreview(/*animate=*/true);
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
  // Neighbors animate down as the ghost grows; anchor stays put (scroll-safe).
  syncGesturePreview(/*animate=*/true);
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
  const QString dragId = m_dragId;
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

  auto *spec = findSpec(dragId);
  if (commit && (finished == Gesture::Move || finished == Gesture::Resize) &&
      !m_previewSpecs.isEmpty()) {
    // Commit already-reflowed preview, then float-up to close holes after shrink.
    QVector<DashboardWidgetSpec> next = m_previewSpecs;
    reflowPushDown(next, dragId, /*floatUp=*/true);
    const bool changed = !specsLayoutEqual(m_specs, next);
    if (changed)
      emit layoutAboutToChange();
    m_specs = next;
    if (changed)
      emit specsChanged(m_specs);
  } else if (spec) {
    // Esc / cancel: restore footprint, do not persist preview.
    spec->row = m_startRow;
    spec->col = m_startCol;
    spec->sizeClass = m_startSize;
    clampSpec(*spec);
  }

  stopNeighborAnims();
  clearGhost();
  if (m_dragWidget && (spec = findSpec(dragId)))
    m_dragWidget->setSizeClass(spec->sizeClass);

  m_dragWidget = nullptr;
  m_dragId.clear();
  m_previewSpecs.clear();

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
