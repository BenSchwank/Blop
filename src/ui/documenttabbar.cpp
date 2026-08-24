#include "documenttabbar.h"

#include <QFont>
#include <QTimer>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>

#include "blop_theme.h"
#include "blop_scroll.h"
#include "blopstyle.h"
#include "moderntoolbar.h"
#include "notechrome.h"
#include "uiscale.h"

namespace {
QIcon makeTabIcon(const QString &name, const QColor &color, int size) {
  QPixmap pm(size, size);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  const qreal g = size / 64.0;
  p.scale(g, g);
  // color doubles as the body stroke so tab glyphs work on light chrome.
  blopDrawToolbarGlyph64(&p, name, color, color);
  return QIcon(pm);
}

int tabMaxWidthPx() {
  // Narrow phones: leave room for home/menu/overflow in the Android header.
  if (UiScale::isAndroidPhoneUi())
    return UiScale::dp(120);
  return UiScale::dp(240);
}

QFont tabTitleFont(const QFont &base, bool compact) {
  QFont f = base;
  f.setPixelSize(compact ? UiScale::dp(12) : UiScale::dp(13));
  f.setWeight(QFont::DemiBold);
  return f;
}
} // namespace

// -----------------------------------------------------------------------------
// DocumentTab
// -----------------------------------------------------------------------------
DocumentTab::DocumentTab(const QString &title, const QString &iconName,
                         bool closable, QWidget *parent)
    : QWidget(parent), m_title(title), m_iconName(iconName),
      m_closable(closable) {
  setAttribute(Qt::WA_TranslucentBackground, true);
  setCursor(Qt::PointingHandCursor);
  setFixedHeight(UiScale::dp(36));
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  QHBoxLayout *lay = new QHBoxLayout(this);
  lay->setContentsMargins(UiScale::dp(10), 0, UiScale::dp(10), 0);
  lay->setSpacing(UiScale::dp(8));

  QLabel *iconLbl = new QLabel(this);
  iconLbl->setObjectName(QStringLiteral("DocumentTabIcon"));
  iconLbl->setPixmap(makeTabIcon(m_iconName, QColor(232, 228, 255), UiScale::dp(18))
                         .pixmap(UiScale::dp(18), UiScale::dp(18)));
  lay->addWidget(iconLbl);

  m_textLbl = new QLabel(this);
  m_textLbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  if (m_title.isEmpty() && !m_closable) {
    m_textLbl->hide();
  } else {
    refreshTitleLabel();
    lay->addWidget(m_textLbl);
  }

  if (m_closable) {
    QPushButton *closeBtn = new QPushButton(QStringLiteral("\u2715"), this);
    closeBtn->setObjectName(QStringLiteral("DocumentTabClose"));
    closeBtn->setFixedSize(UiScale::dp(18), UiScale::dp(18));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  background-color: transparent;"
                       "  border: none;"
                       "  color: rgba(255,255,255,0.45);"
                       "  font-size: 11px;"
                       "  border-radius: 9px;"
                       "}"
                       "QPushButton:hover {"
                       "  background-color: rgba(255,255,255,0.12);"
                       "  color: white;"
                       "}"));
    connect(closeBtn, &QPushButton::clicked, this, [this]() { emit closeClicked(); });
    lay->addWidget(closeBtn);
  }

  setMouseTracking(true);
  setFixedWidth(sizeHint().width());
  refreshChromeStyle();
}

void DocumentTab::setNoteChromeMode(bool on) {
  if (m_noteChromeMode == on)
    return;
  m_noteChromeMode = on;
  refreshChromeStyle();
  update();
}

void DocumentTab::setReadingMarkMode(bool on) {
  if (m_readingMarkMode == on)
    return;
  m_readingMarkMode = on;
  if (on) {
    setFixedHeight(UiScale::dp(28));
    if (auto *iconLbl = findChild<QLabel *>(QStringLiteral("DocumentTabIcon")))
      iconLbl->hide();
  } else if (auto *iconLbl = findChild<QLabel *>(QStringLiteral("DocumentTabIcon"))) {
    iconLbl->show();
    setFixedHeight(UiScale::dp(36));
  }
  refreshTitleLabel();
  refreshChromeStyle();
  update();
}

void DocumentTab::refreshChromeStyle() {
  const QColor fg = m_readingMarkMode
                        ? (m_active ? QColor(0x1C, 0x1E, 0x24)
                                    : QColor(0x6B, 0x72, 0x80))
                        : (m_noteChromeMode ? NoteChrome::textPrimary()
                                            : BlopTheme::textPrimary());
  if (auto *iconLbl = findChild<QLabel *>(QStringLiteral("DocumentTabIcon"))) {
    if (m_readingMarkMode)
      iconLbl->hide();
    else
      iconLbl->setPixmap(
          makeTabIcon(m_iconName, fg, UiScale::dp(18))
              .pixmap(UiScale::dp(18), UiScale::dp(18)));
  }
  if (m_textLbl) {
    m_textLbl->setStyleSheet(
        QStringLiteral("QLabel { background: transparent; color: %1; }")
            .arg(fg.name(QColor::HexRgb)));
  }
  if (auto *closeBtn =
          findChild<QPushButton *>(QStringLiteral("DocumentTabClose"))) {
    const QString idle = m_readingMarkMode
                             ? QStringLiteral("#9CA3AF")
                             : (m_noteChromeMode
                                    ? NoteChrome::textSecondary().name(QColor::HexRgb)
                                    : BlopTheme::textTertiary().name(QColor::HexRgb));
    const QString hoverFg =
        m_readingMarkMode
            ? QStringLiteral("#1C1E24")
            : (m_noteChromeMode ? NoteChrome::textPrimary().name(QColor::HexRgb)
                                : BlopTheme::textPrimary().name(QColor::HexRgb));
    const QString hoverBg = m_readingMarkMode
                                ? QStringLiteral("rgba(0,0,0,0.06)")
                                : (BlopTheme::instance().isDark()
                                       ? QStringLiteral("rgba(255,255,255,0.10)")
                                       : QStringLiteral("rgba(0,0,0,0.08)"));
    closeBtn->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  background-color: transparent; border: none;"
                       "  color: %1; font-size: 11px; border-radius: 9px;"
                       "}"
                       "QPushButton:hover {"
                       "  background-color: %3; color: %2;"
                       "}")
            .arg(idle, hoverFg, hoverBg));
  }
}

int DocumentTab::chromeWidthPx() const {
  // Everything around the title: paddings, glyph, spacing, close button.
  if (m_readingMarkMode)
    return UiScale::dp(24) + (m_closable ? UiScale::dp(24) : 0);
  return UiScale::dp(20) + UiScale::dp(18) + UiScale::dp(8) +
         (m_closable ? UiScale::dp(26) : 0);
}

void DocumentTab::refreshTitleLabel() {
  if (!m_textLbl)
    return;
  const QFont f = tabTitleFont(font(), m_readingMarkMode);
  m_textLbl->setFont(f);
  const QFontMetrics fm(f);
  const int avail = qMax(UiScale::dp(40), tabMaxWidthPx() - chromeWidthPx());
  const QString shown = fm.elidedText(m_title, Qt::ElideRight, avail);
  m_textLbl->setText(shown);
  m_textLbl->setToolTip(shown == m_title ? QString() : m_title);
  m_textLbl->setMinimumWidth(0);
  m_textLbl->setFixedWidth(fm.horizontalAdvance(shown));
  refreshChromeStyle();
  setFixedWidth(sizeHint().width());
  updateGeometry();
}

void DocumentTab::setTitle(const QString &title) {
  if (m_title == title)
    return;
  m_title = title;
  refreshTitleLabel();
}


void DocumentTab::setActive(bool active, const QColor &accent) {
  if (m_active == active && m_accentColor == accent)
    return;
  m_active = active;
  m_accentColor = accent;
  update();
}

void DocumentTab::setAccentColor(const QColor &color) {
  if (m_accentColor == color)
    return;
  m_accentColor = color;
  if (m_active)
    update();
}

QSize DocumentTab::iconTextSize() const {
  return sizeHint();
}

QSize DocumentTab::sizeHint() const {
  const QFontMetrics fm(tabTitleFont(font(), m_readingMarkMode));
  const QString shown = m_textLbl ? m_textLbl->text() : m_title;
  const int textW = shown.isEmpty() ? 0 : fm.horizontalAdvance(shown);
  if (m_readingMarkMode) {
    int w = chromeWidthPx() + textW;
    if (m_title.isEmpty() && !m_closable)
      w = UiScale::dp(28);
    return QSize(qMin(w, tabMaxWidthPx()), UiScale::dp(28));
  }
  int w = chromeWidthPx() + textW;
  if (m_title.isEmpty() && !m_closable) {
    // Home squircle: icon-only compact chip.
    w = UiScale::dp(36);
  }
  return QSize(qMin(w, tabMaxWidthPx()), UiScale::dp(36));
}

QSize DocumentTab::minimumSizeHint() const { return sizeHint(); }

void DocumentTab::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  if (m_readingMarkMode) {
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal rad = UiScale::dp(5);
    QPainterPath path;
    path.moveTo(r.left(), r.bottom());
    path.lineTo(r.left(), r.top() + rad);
    path.quadTo(r.left(), r.top(), r.left() + rad, r.top());
    path.lineTo(r.right() - rad, r.top());
    path.quadTo(r.right(), r.top(), r.right(), r.top() + rad);
    path.lineTo(r.right(), r.bottom());
    path.closeSubpath();

    const QColor accent =
        m_accentColor.isValid() ? m_accentColor : NoteChrome::accent();
    if (m_active) {
      p.fillPath(path, QColor(0xFF, 0xFF, 0xFF));
      p.setPen(QPen(QColor(0xE4, 0xE7, 0xEE), 1.0));
      p.drawPath(path);
      p.fillRect(QRectF(r.left(), r.top(), UiScale::dp(3), r.height()), accent);
    } else if (m_hovered) {
      p.fillPath(path, QColor(0xF3, 0xF4, 0xF7));
      p.setPen(QPen(QColor(0xD8, 0xDC, 0xE4), 1));
      p.drawPath(path);
    } else {
      p.fillPath(path, QColor(0xF8, 0xF9, 0xFB));
      p.setPen(QPen(QColor(0xE4, 0xE7, 0xEE), 1));
      p.drawPath(path);
    }
    return;
  }

  const qreal rad = m_noteChromeMode ? UiScale::dp(8) : UiScale::dp(14);
  QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  QPainterPath path;
  path.addRoundedRect(r, rad, rad);

  if (m_noteChromeMode) {
    const QColor accent =
        m_accentColor.isValid() ? m_accentColor : NoteChrome::accent();
    if (m_active) {
      p.fillPath(path, QColor(0xFF, 0xFF, 0xFF));
      p.setPen(QPen(accent, 2.0));
      p.drawPath(path);
    } else if (m_hovered) {
      p.fillPath(path, QColor(0xF8, 0xF9, 0xFB));
      p.setPen(QPen(QColor(0xE4, 0xE7, 0xEE), 1.0));
      p.drawPath(path);
    } else {
      p.fillPath(path, QColor(0xFF, 0xFF, 0xFF));
      p.setPen(QPen(QColor(0xE4, 0xE7, 0xEE), 1.0));
      p.drawPath(path);
    }
    return;
  }

  if (m_active) {
    QColor bg = m_accentColor;
    bg.setAlphaF(0.16);
    p.fillPath(path, bg);

    QColor border = m_accentColor;
    border.setAlphaF(0.42);
    p.setPen(QPen(border, 1.0));
    p.drawPath(path);
  } else if (m_hovered) {
    QColor bg = BlopTheme::instance().isDark() ? QColor(255, 255, 255, 18)
                                               : QColor(0, 0, 0, 18);
    p.fillPath(path, bg);
  } else {
    QColor bg = BlopTheme::instance().isDark() ? QColor(255, 255, 255, 8)
                                               : QColor(0, 0, 0, 10);
    p.fillPath(path, bg);
  }
}

void DocumentTab::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton)
    emit clicked();
}

void DocumentTab::enterEvent(QEnterEvent *event) {
  Q_UNUSED(event)
  m_hovered = true;
  update();
}

void DocumentTab::leaveEvent(QEvent *event) {
  Q_UNUSED(event)
  m_hovered = false;
  update();
}

// -----------------------------------------------------------------------------
// DocumentTabBar
// -----------------------------------------------------------------------------
DocumentTabBar::DocumentTabBar(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground, true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setFixedHeight(UiScale::dp(40));

  m_outerLayout = new QHBoxLayout(this);
  m_outerLayout->setContentsMargins(0, 0, 0, 0);
  m_outerLayout->setSpacing(UiScale::dp(8));

  m_homeTab = new DocumentTab(QString(), QStringLiteral("home"), false, this);
  m_homeTab->setActive(true, m_accentColor);
  connect(m_homeTab, &DocumentTab::clicked, this, [this]() { emit homeClicked(); });
  m_outerLayout->addWidget(m_homeTab, 0);

  m_scroll = new QScrollArea(this);
  m_scroll->setWidgetResizable(false);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setAttribute(Qt::WA_TranslucentBackground, true);
  m_scroll->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
  m_scroll->setStyleSheet(QStringLiteral(
      "QScrollArea { background: transparent; border: none; }"
      "QScrollArea > QWidget > QWidget { background: transparent; }"));
  m_scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_scroll->setFixedHeight(UiScale::dp(40));

  m_scrollContent = new QWidget(m_scroll);
  m_scrollContent->setAttribute(Qt::WA_TranslucentBackground, true);
  m_tabsLayout = new QHBoxLayout(m_scrollContent);
  m_tabsLayout->setContentsMargins(0, 2, 0, 2);
  m_tabsLayout->setSpacing(UiScale::dp(6));
  m_tabsLayout->addStretch(1);
  m_scrollContent->setFixedHeight(UiScale::dp(40));
  m_scroll->setWidget(m_scrollContent);

  BlopScroll::enableFingerScroll(m_scroll);

  m_outerLayout->addWidget(m_scroll, 1);

  m_indicator = new QWidget(m_scrollContent);
  m_indicator->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_indicator->setAttribute(Qt::WA_StyledBackground, true);
  m_indicator->resize(UiScale::dp(28), UiScale::dp(3));
  m_indicator->setStyleSheet(
      QStringLiteral("background-color: %1; border-radius: 2px;")
          .arg(m_accentColor.name(QColor::HexRgb)));
  m_indicator->hide();
}

int DocumentTabBar::tabsWidthPx() const {
  int w = 0;
  for (DocumentTab *t : m_tabs)
    w += t->sizeHint().width() + UiScale::dp(6);
  return w;
}

void DocumentTabBar::syncContentWidth() {
  if (!m_scrollContent || !m_scroll)
    return;
  const int natural = qMax(UiScale::dp(40), tabsWidthPx());
  m_scrollContent->setFixedWidth(qMax(m_scroll->viewport()->width(), natural));
  updateGeometry();
}

QSize DocumentTabBar::sizeHint() const {
  // Ask the title bar for the room the tabs actually need; without this the
  // scroll area only reports its own tiny hint and the strip gets squeezed.
  int w = tabsWidthPx();
  if (m_homeTab && !m_homeTab->isHidden())
    w += m_homeTab->sizeHint().width() + m_outerLayout->spacing();
  return QSize(qMax(w, UiScale::dp(48)), height());
}

QSize DocumentTabBar::minimumSizeHint() const {
  // Stay shrinkable on narrow windows — the strip scrolls horizontally.
  return QSize(qMin(sizeHint().width(), UiScale::dp(160)), height());
}

void DocumentTabBar::rebindTabSignals() {
  for (int i = 0; i < m_tabs.size(); ++i) {
    disconnect(m_tabs[i], &DocumentTab::clicked, nullptr, nullptr);
    disconnect(m_tabs[i], &DocumentTab::closeClicked, nullptr, nullptr);
    connect(m_tabs[i], &DocumentTab::clicked, this, [this, i]() { setCurrentIndex(i); });
    connect(m_tabs[i], &DocumentTab::closeClicked, this,
            [this, i]() { emit tabCloseRequested(i); });
  }
}

int DocumentTabBar::addTab(const QString &title, const QString &iconName) {
  DocumentTab *tab = new DocumentTab(title, iconName, true, m_scrollContent);
  tab->setNoteChromeMode(m_noteChromeMode);
  tab->setReadingMarkMode(m_readingMarkMode);
  int idx = m_tabs.size();
  m_tabs.append(tab);

  // Insert before the trailing stretch.
  m_tabsLayout->insertWidget(m_tabsLayout->count() - 1, tab);
  rebindTabSignals();

  // Grow scroll content to natural tab row width.
  syncContentWidth();

  if (m_currentIndex < 0)
    setCurrentIndex(idx);
  else
    updateIndicator(false);

  ensureTabVisible(idx);
  return idx;
}

void DocumentTabBar::removeTab(int index) {
  if (index < 0 || index >= m_tabs.size())
    return;
  DocumentTab *tab = m_tabs.takeAt(index);
  m_tabsLayout->removeWidget(tab);
  tab->deleteLater();
  rebindTabSignals();

  syncContentWidth();

  if (m_currentIndex == index) {
    m_currentIndex = m_tabs.isEmpty() ? -1 : qMin(index, m_tabs.size() - 1);
    if (m_currentIndex >= 0)
      m_tabs[m_currentIndex]->setActive(true, m_accentColor);
  } else if (m_currentIndex > index) {
    --m_currentIndex;
  }
  updateIndicator(false);
  emit currentChanged(m_currentIndex);
}

void DocumentTabBar::setTabText(int index, const QString &title) {
  if (index < 0 || index >= m_tabs.size())
    return;
  m_tabs[index]->setTitle(title);
  syncContentWidth();
  updateIndicator(false);
}

int DocumentTabBar::currentIndex() const { return m_currentIndex; }

int DocumentTabBar::count() const { return m_tabs.size(); }

void DocumentTabBar::setAccentColor(const QColor &color) {
  if (m_accentColor == color)
    return;
  m_accentColor = color;
  for (DocumentTab *tab : m_tabs)
    tab->setAccentColor(color);
  m_homeTab->setAccentColor(color);
  if (m_homeActive)
    m_homeTab->setActive(true, color);
  if (m_indicator)
    m_indicator->setStyleSheet(
        QStringLiteral("background-color: %1; border-radius: 2px;")
            .arg(m_accentColor.name(QColor::HexRgb)));
  updateIndicator(false);
}

void DocumentTabBar::setHomeActive(bool active) {
  if (m_homeActive == active)
    return;
  m_homeActive = active;
  m_homeTab->setActive(active, m_accentColor);
  for (DocumentTab *tab : m_tabs)
    tab->setActive(!active && (m_currentIndex >= 0 && m_tabs[m_currentIndex] == tab),
                   m_accentColor);
  updateIndicator(false);
}

void DocumentTabBar::setHomeVisible(bool visible) {
  if (!m_homeTab)
    return;
  m_homeTab->setVisible(visible);
  if (!visible)
    m_homeActive = false;
  syncContentWidth();
  updateIndicator(false);
}

void DocumentTabBar::setNoteChromeMode(bool on) {
  if (m_noteChromeMode == on)
    return;
  m_noteChromeMode = on;
  if (on)
    setAccentColor(NoteChrome::accent());
  if (m_homeTab)
    m_homeTab->setNoteChromeMode(on);
  for (DocumentTab *tab : m_tabs)
    tab->setNoteChromeMode(on);
  syncContentWidth();
  updateIndicator(false);
}

void DocumentTabBar::setReadingMarkMode(bool on) {
  if (m_readingMarkMode == on)
    return;
  m_readingMarkMode = on;
  setFixedHeight(on ? UiScale::dp(44) : UiScale::dp(40));
  if (m_scroll)
    m_scroll->setFixedHeight(on ? UiScale::dp(44) : UiScale::dp(40));
  if (m_scrollContent)
    m_scrollContent->setFixedHeight(on ? UiScale::dp(44) : UiScale::dp(40));
  if (m_homeTab) {
    m_homeTab->setReadingMarkMode(on);
    // Bookmark tabs hang from the title bar, where the app already owns a
    // home button — an extra empty squircle only crowds the strip.
    m_homeTab->setVisible(!on);
    if (on)
      m_homeActive = false;
  }
  for (DocumentTab *tab : m_tabs)
    tab->setReadingMarkMode(on);
  if (m_tabsLayout)
    m_tabsLayout->setContentsMargins(0, on ? UiScale::dp(16) : 2, 0, 0);
  if (m_indicator)
    m_indicator->setVisible(!on);
  syncContentWidth();
  updateIndicator(false);
}

void DocumentTabBar::refreshTheme() {
  if (m_homeTab)
    m_homeTab->refreshChromeStyle();
  for (DocumentTab *tab : m_tabs) {
    if (tab)
      tab->refreshChromeStyle();
  }
  update();
}

void DocumentTabBar::setCurrentIndex(int index) {
  if (index < -1 || index >= m_tabs.size())
    return;
  const bool leavingHome = m_homeActive;
  if (index == m_currentIndex && !leavingHome)
    return;

  m_homeActive = false;
  m_homeTab->setActive(false, m_accentColor);

  if (m_currentIndex >= 0 && m_currentIndex < m_tabs.size())
    m_tabs[m_currentIndex]->setActive(false, m_accentColor);
  m_currentIndex = index;
  if (m_currentIndex >= 0)
    m_tabs[m_currentIndex]->setActive(true, m_accentColor);
  updateIndicator(true);
  ensureTabVisible(m_currentIndex);
  emit currentChanged(m_currentIndex);
}

void DocumentTabBar::ensureTabVisible(int index) {
  if (!m_scroll || index < 0 || index >= m_tabs.size())
    return;
  DocumentTab *tab = m_tabs[index];
  if (!tab)
    return;
  m_scroll->ensureWidgetVisible(tab, 0, 0);
}

void DocumentTabBar::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (m_scrollContent) {
    syncContentWidth();
  }
  updateIndicator(false);
}

void DocumentTabBar::wheelEvent(QWheelEvent *event) {
  if (!m_scroll)
    return;
  QScrollBar *bar = m_scroll->horizontalScrollBar();
  if (!bar)
    return;
  // Prefer horizontal wheel; map vertical trackpad deltas to tab strip scroll.
  const int delta = event->angleDelta().x() != 0 ? event->angleDelta().x()
                                                 : event->angleDelta().y();
  bar->setValue(bar->value() - delta);
  event->accept();
}

void DocumentTabBar::updateIndicator(bool animate) {
  if (!m_indicator)
    return;

  // NoteChrome chips already encode selection via fill/stroke — no underline.
  if (m_noteChromeMode) {
    m_indicator->hide();
    return;
  }

  DocumentTab *activeTab =
      m_homeActive ? m_homeTab
                   : (m_currentIndex >= 0 ? m_tabs.value(m_currentIndex) : nullptr);
  if (!activeTab || !activeTab->isVisible()) {
    m_indicator->hide();
    return;
  }

  // Home lives outside the scroll content — underline sits under home in outer bar.
  if (activeTab == m_homeTab) {
    if (m_indicator->parentWidget() != this) {
      m_indicator->setParent(this);
      m_indicator->show();
    }
    const int indW = qMax(UiScale::dp(16), activeTab->width() - UiScale::dp(12));
    const int indH = UiScale::dp(3);
    const QPoint target(activeTab->x() + (activeTab->width() - indW) / 2,
                        activeTab->y() + activeTab->height() - UiScale::dp(4));
    m_indicator->resize(indW, indH);
    if (!animate || !m_indicator->isVisible()) {
      m_indicator->move(target);
      m_indicator->show();
      m_indicator->raise();
      return;
    }
    if (!m_indicatorAnim)
      m_indicatorAnim = new QPropertyAnimation(m_indicator, "pos", this);
    else
      m_indicatorAnim->stop();
    m_indicatorAnim->setDuration(BlopMotion::kStandard);
    m_indicatorAnim->setEasingCurve(BlopMotion::kEaseStandard);
    m_indicatorAnim->setStartValue(m_indicator->pos());
    m_indicatorAnim->setEndValue(target);
    m_indicatorAnim->start();
    m_indicator->raise();
    return;
  }

  // Note tabs: indicator lives in the scroll content so it scrolls with them.
  if (m_indicator->parentWidget() != m_scrollContent) {
    m_indicator->setParent(m_scrollContent);
  }

  const int indW = qMax(UiScale::dp(16), activeTab->width() - UiScale::dp(16));
  const int indH = UiScale::dp(3);
  const QPoint target(activeTab->x() + (activeTab->width() - indW) / 2,
                      activeTab->y() + activeTab->height() - UiScale::dp(5));

  m_indicator->resize(indW, indH);

  if (!m_indicator->isVisible()) {
    m_indicator->move(target);
    m_indicator->show();
    m_indicator->raise();
    return;
  }

  if (!animate) {
    m_indicator->move(target);
    m_indicator->raise();
    return;
  }

  if (!m_indicatorAnim) {
    m_indicatorAnim = new QPropertyAnimation(m_indicator, "pos", this);
  } else {
    m_indicatorAnim->stop();
  }
  m_indicatorAnim->setDuration(BlopMotion::kStandard);
  m_indicatorAnim->setEasingCurve(BlopMotion::kEaseStandard);
  m_indicatorAnim->setStartValue(m_indicator->pos());
  m_indicatorAnim->setEndValue(target);
  m_indicatorAnim->start();
  m_indicator->raise();
}

DocumentTab *DocumentTabBar::tabAt(int index) const {
  if (index < 0 || index >= m_tabs.size())
    return nullptr;
  return m_tabs[index];
}
