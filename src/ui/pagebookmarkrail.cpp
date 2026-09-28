#include "pagebookmarkrail.h"

#include "blop_dialogs.h"
#include "blop_inwindow_menu.h"
#include "multipagenoteview.h"
#include "notechrome.h"
#include "uiscale.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

namespace {

int tabW() { return UiScale::dp(44); }
int tabH() { return UiScale::dp(34); }
int addTabH() { return UiScale::dp(30); }
int tabGap() { return UiScale::dp(6); }
int hoverPop() { return UiScale::dp(4); }
int maxTabW() { return UiScale::dp(150); }
int tabRadius() { return UiScale::dp(10); }

QFont numberFont() {
  QFont f;
  f.setPixelSize(UiScale::dp(12));
  f.setWeight(QFont::DemiBold);
  return f;
}

QFont nameFont() {
  QFont f;
  f.setPixelSize(UiScale::dp(11));
  f.setWeight(QFont::Medium);
  return f;
}

/// Tab shape flush with the canvas edge: square on the left, round on the right.
QPainterPath registerTabPath(const QRectF &r, qreal radius) {
  QPainterPath path;
  path.moveTo(r.left(), r.top());
  path.lineTo(r.right() - radius, r.top());
  path.quadTo(r.right(), r.top(), r.right(), r.top() + radius);
  path.lineTo(r.right(), r.bottom() - radius);
  path.quadTo(r.right(), r.bottom(), r.right() - radius, r.bottom());
  path.lineTo(r.left(), r.bottom());
  path.closeSubpath();
  return path;
}

class BookmarkPreviewCard : public QWidget {
public:
  explicit BookmarkPreviewCard(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    hide();
  }

  static QSize thumbSize() {
    const int w = UiScale::dp(150);
    return QSize(w, qRound(w * 1.4142));
  }

  void setContent(const QString &title, const QPixmap &pm) {
    m_title = title;
    m_pixmap = pm;
    const QSize t = thumbSize();
    const int pad = UiScale::dp(8);
    resize(t.width() + 2 * pad, t.height() + 2 * pad + UiScale::dp(24));
    update();
  }

  void setPixmap(const QPixmap &pm) {
    m_pixmap = pm;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(card, UiScale::dp(12), UiScale::dp(12));
    p.fillPath(path, NoteChrome::panelElevated());
    p.setPen(QPen(NoteChrome::borderSoft(), 1));
    p.drawPath(path);

    const int pad = UiScale::dp(8);
    const QSize t = thumbSize();
    const QRect thumb(pad, pad, t.width(), t.height());
    QPainterPath clip;
    clip.addRoundedRect(QRectF(thumb), UiScale::dp(6), UiScale::dp(6));
    p.save();
    p.setClipPath(clip);
    p.fillRect(thumb, NoteChrome::canvasBg());
    if (!m_pixmap.isNull()) {
      const QSize s = m_pixmap.size().scaled(t, Qt::KeepAspectRatio);
      const QRect target(thumb.x() + (t.width() - s.width()) / 2,
                         thumb.y() + (t.height() - s.height()) / 2, s.width(),
                         s.height());
      p.drawPixmap(target, m_pixmap);
    }
    p.restore();

    QFont f = nameFont();
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(NoteChrome::textPrimary());
    const QRect label(pad, thumb.bottom() + UiScale::dp(4), t.width(),
                      UiScale::dp(20));
    p.drawText(label, Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(f).elidedText(m_title, Qt::ElideRight,
                                          label.width()));
  }

private:
  QString m_title;
  QPixmap m_pixmap;
};

} // namespace

PageBookmarkRail::PageBookmarkRail(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("PageBookmarkRail"));
  setAttribute(Qt::WA_TranslucentBackground, true);
  setMouseTracking(true);
  setFocusPolicy(Qt::ClickFocus);
  setToolTip(QString());

  m_preview = new BookmarkPreviewCard(parent);

  m_previewTimer = new QTimer(this);
  m_previewTimer->setSingleShot(true);
  m_previewTimer->setInterval(260);
  connect(m_previewTimer, &QTimer::timeout, this, [this]() {
    if (m_hover >= 0)
      showPreview(m_hover);
  });

  m_syncTimer = new QTimer(this);
  m_syncTimer->setSingleShot(true);
  m_syncTimer->setInterval(60);
  connect(m_syncTimer, &QTimer::timeout, this,
          &PageBookmarkRail::syncCurrentPage);
  hide();
}

void PageBookmarkRail::setNoteView(MultiPageNoteView *view) {
  if (m_view == view)
    return;
  if (m_view) {
    disconnect(m_view, nullptr, this, nullptr);
    if (auto *sb = m_view->verticalScrollBar())
      disconnect(sb, nullptr, this, nullptr);
  }
  m_view = view;
  if (m_view) {
    connect(m_view, &MultiPageNoteView::pagesChanged, this,
            &PageBookmarkRail::rebuild);
    if (auto *sb = m_view->verticalScrollBar())
      connect(sb, &QScrollBar::valueChanged, m_syncTimer,
              qOverload<>(&QTimer::start));
  }
  m_scroll = 0;
  hidePreview();
  rebuild();
  syncCurrentPage();
}

void PageBookmarkRail::refreshTheme() {
  update();
  if (m_preview)
    m_preview->update();
}

void PageBookmarkRail::rebuild() {
  m_tabs.clear();
  if (m_view) {
    const int n = m_view->pageCount();
    for (int i = 0; i < n; ++i) {
      Tab t;
      t.page = i;
      t.bookmarked = m_view->isPageBookmarked(i);
      if (t.bookmarked)
        t.label = m_view->pageTitle(i).trimmed();
      m_tabs.push_back(t);
    }
    Tab add;
    add.page = -1;
    m_tabs.push_back(add);
  }
  m_current = m_view ? qBound(0, m_current, qMax(0, m_view->pageCount() - 1))
                     : 0;
  layoutTabs();
  update();
}

void PageBookmarkRail::layoutTabs() {
  const QFontMetrics fmName(nameFont());
  int y = 0;
  for (Tab &t : m_tabs) {
    int w = tabW();
    int h = t.page < 0 ? addTabH() : tabH();
    if (!t.label.isEmpty())
      w = qMin(maxTabW(), tabW() + fmName.horizontalAdvance(t.label) +
                              UiScale::dp(10));
    t.rect = QRect(0, y, w, h);
    y += h + tabGap();
  }
  m_contentH = qMax(0, y - tabGap());
  placeIn(m_x, m_bandTop, m_bandBottom);
}

void PageBookmarkRail::placeIn(int x, int top, int bottom) {
  m_x = x;
  m_bandTop = top;
  m_bandBottom = bottom;
  int widest = tabW();
  for (const Tab &t : m_tabs)
    widest = qMax(widest, t.rect.width());
  const int avail = qMax(tabH(), bottom - top);
  const int h = qMin(avail, m_contentH);
  setGeometry(x, top, widest + hoverPop() + UiScale::dp(2), qMax(1, h));
  setScroll(m_scroll);
}

QRect PageBookmarkRail::visualRect(const Tab &t) const {
  return t.rect.translated(0, -m_scroll);
}

int PageBookmarkRail::tabAt(const QPoint &pos) const {
  for (int i = 0; i < m_tabs.size(); ++i) {
    QRect r = visualRect(m_tabs[i]);
    r.setRight(r.right() + hoverPop());
    if (r.contains(pos))
      return i;
  }
  return -1;
}

void PageBookmarkRail::setScroll(int y) {
  const int maxScroll = qMax(0, m_contentH - height());
  const int clamped = qBound(0, y, maxScroll);
  if (clamped == m_scroll)
    return;
  m_scroll = clamped;
  update();
}

void PageBookmarkRail::ensureVisible(int page) {
  for (const Tab &t : m_tabs) {
    if (t.page != page)
      continue;
    const int pad = tabGap();
    if (t.rect.top() - pad < m_scroll)
      setScroll(t.rect.top() - pad);
    else if (t.rect.bottom() + pad > m_scroll + height())
      setScroll(t.rect.bottom() + pad - height());
    return;
  }
}

void PageBookmarkRail::syncCurrentPage() {
  if (!m_view)
    return;
  const int cur = m_view->currentPageIndex();
  if (cur == m_current)
    return;
  m_current = cur;
  ensureVisible(cur);
  update();
}

void PageBookmarkRail::activate(int page) {
  if (!m_view || page < 0 || page >= m_view->pageCount())
    return;
  m_current = page;
  ensureVisible(page);
  update();
  m_view->scrollToPage(page, true);
  emit pageActivated(page);
}

void PageBookmarkRail::stepPage(int delta) {
  if (!m_view)
    return;
  const int n = m_view->pageCount();
  if (n <= 0)
    return;
  activate(qBound(0, m_current + delta, n - 1));
}

void PageBookmarkRail::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QColor accent = NoteChrome::accent();
  const QColor idleBg = NoteChrome::panelElevated();
  const QColor border = NoteChrome::borderSoft();
  const QColor fg = NoteChrome::textSecondary();
  const QFont fNum = numberFont();
  const QFont fName = nameFont();

  for (int i = 0; i < m_tabs.size(); ++i) {
    const Tab &t = m_tabs[i];
    QRect r = visualRect(t);
    if (r.bottom() < 0 || r.top() > height())
      continue;
    const bool hovered = (i == m_hover);
    const bool active = (t.page >= 0 && t.page == m_current);
    if (hovered || active)
      r.setRight(r.right() + hoverPop());
    const QRectF rf = QRectF(r).adjusted(0, 0.5, -0.5, -0.5);
    const QPainterPath path = registerTabPath(rf, tabRadius());

    if (t.page < 0) {
      QColor bg = idleBg;
      bg.setAlpha(hovered ? 255 : 150);
      p.fillPath(path, bg);
      QPen pen(hovered ? accent : border, 1, Qt::DashLine);
      p.setPen(pen);
      p.drawPath(path);
      p.setPen(QPen(hovered ? accent : fg, UiScale::dp(2) * 0.8,
                    Qt::SolidLine, Qt::RoundCap));
      const QPointF c(r.left() + tabW() / 2.0, r.center().y() + 0.5);
      const qreal a = UiScale::dp(5);
      p.drawLine(QPointF(c.x() - a, c.y()), QPointF(c.x() + a, c.y()));
      p.drawLine(QPointF(c.x(), c.y() - a), QPointF(c.x(), c.y() + a));
      continue;
    }

    if (active) {
      p.fillPath(path, accent);
    } else {
      QColor bg = idleBg;
      if (hovered)
        bg = bg.lighter(NoteChrome::isDark() ? 125 : 97);
      p.fillPath(path, bg);
      QColor edge = hovered ? accent : border;
      if (hovered)
        edge.setAlpha(140);
      p.setPen(QPen(edge, 1));
      p.drawPath(path);
    }

    const QColor ink = active ? QColor(Qt::white)
                              : (hovered ? NoteChrome::textPrimary() : fg);
    const QRect numRect(r.left(), r.top(), tabW(), r.height());
    p.setFont(fNum);
    p.setPen(ink);
    p.drawText(numRect, Qt::AlignCenter, QString::number(t.page + 1));

    if (!t.label.isEmpty()) {
      p.setFont(fName);
      QColor nameInk = ink;
      if (!active)
        nameInk.setAlphaF(0.85);
      p.setPen(nameInk);
      const QRect nameRect(r.left() + tabW() - UiScale::dp(6), r.top(),
                           r.width() - tabW(), r.height());
      p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                 QFontMetrics(fName).elidedText(t.label, Qt::ElideRight,
                                                nameRect.width()));
    } else if (t.bookmarked) {
      // Unnamed bookmark: small ribbon notch on the tab's right edge.
      const qreal rw = UiScale::dp(5);
      const qreal rh = UiScale::dp(9);
      const qreal x0 = r.right() - UiScale::dp(9);
      const qreal y0 = r.top() + UiScale::dp(4);
      QPainterPath ribbon;
      ribbon.moveTo(x0, y0);
      ribbon.lineTo(x0 + rw, y0);
      ribbon.lineTo(x0 + rw, y0 + rh);
      ribbon.lineTo(x0 + rw / 2.0, y0 + rh - UiScale::dp(2));
      ribbon.lineTo(x0, y0 + rh);
      ribbon.closeSubpath();
      p.fillPath(ribbon, active ? QColor(255, 255, 255, 220) : accent);
    }
  }
}

void PageBookmarkRail::mouseMoveEvent(QMouseEvent *event) {
  const int hit = tabAt(event->position().toPoint());
  if (hit == m_hover)
    return;
  m_hover = hit;
  setCursor(hit >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
  update();
  if (hit >= 0 && m_tabs[hit].page >= 0) {
    if (m_preview && m_preview->isVisible())
      showPreview(hit);
    else
      m_previewTimer->start();
  } else {
    m_previewTimer->stop();
    hidePreview();
  }
}

void PageBookmarkRail::leaveEvent(QEvent *event) {
  QWidget::leaveEvent(event);
  m_hover = -1;
  m_previewTimer->stop();
  hidePreview();
  update();
}

void PageBookmarkRail::mousePressEvent(QMouseEvent *event) {
  const int hit = tabAt(event->position().toPoint());
  if (hit < 0) {
    QWidget::mousePressEvent(event);
    return;
  }
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
  hidePreview();
  const Tab &t = m_tabs[hit];
  if (t.page < 0) {
    if (!m_view)
      return;
    m_view->addNewPage();
    const int last = m_view->pageCount() - 1;
    rebuild();
    activate(last);
    emit pagesMutated();
    return;
  }
  activate(t.page);
  event->accept();
}

void PageBookmarkRail::wheelEvent(QWheelEvent *event) {
  static int accum = 0;
  accum += event->angleDelta().y();
  while (accum >= 120) {
    accum -= 120;
    stepPage(-1);
  }
  while (accum <= -120) {
    accum += 120;
    stepPage(1);
  }
  event->accept();
}

void PageBookmarkRail::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Up:
  case Qt::Key_Left:
  case Qt::Key_PageUp:
    stepPage(-1);
    break;
  case Qt::Key_Down:
  case Qt::Key_Right:
  case Qt::Key_PageDown:
    stepPage(1);
    break;
  case Qt::Key_Home:
    activate(0);
    break;
  case Qt::Key_End:
    if (m_view)
      activate(m_view->pageCount() - 1);
    break;
  default:
    QWidget::keyPressEvent(event);
    return;
  }
  event->accept();
}

void PageBookmarkRail::contextMenuEvent(QContextMenuEvent *event) {
  const int hit = tabAt(event->pos());
  if (hit < 0 || m_tabs[hit].page < 0)
    return;
  hidePreview();
  showTabMenu(m_tabs[hit].page, event->globalPos());
  event->accept();
}

void PageBookmarkRail::showPreview(int tabIndex) {
  if (!m_view || !m_preview || tabIndex < 0 || tabIndex >= m_tabs.size())
    return;
  const Tab &t = m_tabs[tabIndex];
  if (t.page < 0)
    return;
  auto *card = static_cast<BookmarkPreviewCard *>(m_preview);
  const QString title =
      t.label.isEmpty() ? QStringLiteral("Seite %1").arg(t.page + 1)
                        : QStringLiteral("%1 · %2").arg(t.page + 1).arg(t.label);
  card->setContent(title, QPixmap());

  QWidget *host = parentWidget();
  const QRect tabR = visualRect(t).translated(pos());
  int x = geometry().right() + UiScale::dp(8);
  int y = tabR.center().y() - card->height() / 2;
  if (host)
    y = qBound(UiScale::dp(8), y, host->height() - card->height() - UiScale::dp(8));
  card->move(x, y);
  card->show();
  card->raise();
  raise();

  const int epoch = ++m_previewEpoch;
  QPointer<PageBookmarkRail> self(this);
  m_view->generateThumbnailAsync(
      t.page, BookmarkPreviewCard::thumbSize() * devicePixelRatioF(),
      [self, epoch](QPixmap pm) {
        if (!self || self->m_previewEpoch != epoch || !self->m_preview)
          return;
        static_cast<BookmarkPreviewCard *>(self->m_preview)->setPixmap(pm);
      });
}

void PageBookmarkRail::hidePreview() {
  ++m_previewEpoch;
  if (m_preview)
    m_preview->hide();
}

void PageBookmarkRail::showTabMenu(int page, const QPoint &globalPos) {
  if (!m_view)
    return;
  QPointer<MultiPageNoteView> view = m_view;
  QList<BlopInWindowMenu::Item> items;
  items.push_back({QStringLiteral("Lesezeichen benennen…"), QIcon(),
                   [this, view, page]() {
                     if (!view)
                       return;
                     const QString cur = view->pageTitle(page);
                     const QString name = BlopDialogs::promptText(
                         this, QStringLiteral("Lesezeichen benennen"),
                         QStringLiteral("Name für Seite %1").arg(page + 1),
                         cur);
                     if (name.trimmed().isEmpty())
                       return;
                     view->renamePage(page, name.trimmed());
                     view->setPageBookmarked(page, true);
                     rebuild();
                     emit pagesMutated();
                   }});
  if (m_view->isPageBookmarked(page)) {
    items.push_back({QStringLiteral("Lesezeichen entfernen"), QIcon(),
                     [this, view, page]() {
                       if (!view)
                         return;
                       view->setPageBookmarked(page, false);
                       rebuild();
                       emit pagesMutated();
                     }});
  } else {
    items.push_back({QStringLiteral("Als Lesezeichen markieren"), QIcon(),
                     [this, view, page]() {
                       if (!view)
                         return;
                       view->setPageBookmarked(page, true);
                       rebuild();
                       emit pagesMutated();
                     }});
  }
  items.push_back({QStringLiteral("Seite duplizieren"), QIcon(),
                   [this, view, page]() {
                     if (!view)
                       return;
                     view->duplicatePage(page);
                     rebuild();
                     activate(page + 1);
                     emit pagesMutated();
                   }});
  BlopInWindowMenu::Item sep;
  sep.separator = true;
  items.push_back(sep);
  items.push_back({QStringLiteral("Seite löschen"), QIcon(),
                   [this, view, page]() {
                     if (!view)
                       return;
                     if (view->pageCount() <= 1) {
                       BlopDialogs::notify(
                           this, QStringLiteral("Seite löschen"),
                           QStringLiteral("Mindestens eine Seite muss bleiben."));
                       return;
                     }
                     if (!BlopDialogs::confirm(
                             this, QStringLiteral("Seite löschen"),
                             QStringLiteral("Seite %1 wirklich löschen?")
                                 .arg(page + 1),
                             QStringLiteral("Löschen"),
                             QStringLiteral("Abbrechen")))
                       return;
                     view->deletePage(page);
                     rebuild();
                     emit pagesMutated();
                   },
                   true});
  BlopInWindowMenu::show(this, globalPos, items);
}
