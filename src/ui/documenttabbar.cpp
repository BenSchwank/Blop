#include "documenttabbar.h"

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
#include <QScroller>
#include <QWheelEvent>

#include "blop_theme.h"
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
  blopDrawToolbarGlyph64(&p, name, color);
  return QIcon(pm);
}

int tabMaxWidthPx() {
  // Narrow phones: leave room for home/menu/overflow in the Android header.
  if (UiScale::isAndroidPhoneUi())
    return UiScale::dp(120);
  return UiScale::dp(168);
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
  lay->setContentsMargins(UiScale::dp(10), 0, UiScale::dp(6), 0);
  lay->setSpacing(UiScale::dp(6));

  QLabel *iconLbl = new QLabel(this);
  iconLbl->setObjectName(QStringLiteral("DocumentTabIcon"));
  iconLbl->setPixmap(makeTabIcon(m_iconName, QColor(232, 228, 255), UiScale::dp(18))
                         .pixmap(UiScale::dp(18), UiScale::dp(18)));
  lay->addWidget(iconLbl);

  m_textLbl = new QLabel(this);
  m_textLbl->setStyleSheet(
      QStringLiteral("QLabel { color: #E8E8FF; font-size: 13px; font-weight: 600; }"));
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
}

void DocumentTab::setNoteChromeMode(bool on) {
  // Always refresh — light/dark chrome colors may have changed even when
  // the boolean mode flag is unchanged (applyNoteChromeTheme).
  m_noteChromeMode = on;
  refreshChromeStyle();
  update();
}

void DocumentTab::refreshChromeStyle() {
  const QColor fg =
      m_noteChromeMode ? NoteChrome::textPrimary() : QColor(232, 228, 255);
  if (auto *iconLbl = findChild<QLabel *>(QStringLiteral("DocumentTabIcon"))) {
    iconLbl->setPixmap(
        makeTabIcon(m_iconName, fg, UiScale::dp(18))
            .pixmap(UiScale::dp(18), UiScale::dp(18)));
  }
  if (m_textLbl) {
    m_textLbl->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 13px; font-weight: 600; }")
            .arg(fg.name(QColor::HexRgb)));
  }
  if (auto *closeBtn =
          findChild<QPushButton *>(QStringLiteral("DocumentTabClose"))) {
    const QString idle = m_noteChromeMode
                             ? NoteChrome::textSecondary().name(QColor::HexRgb)
                             : QStringLiteral("rgba(255,255,255,0.45)");
    const QString hoverFg =
        m_noteChromeMode ? NoteChrome::textPrimary().name(QColor::HexRgb)
                         : QStringLiteral("white");
    const QString hoverBg =
        (m_noteChromeMode && !NoteChrome::isDark())
            ? QStringLiteral("rgba(0,0,0,0.08)")
            : QStringLiteral("rgba(255,255,255,0.10)");
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

void DocumentTab::refreshTitleLabel() {
  if (!m_textLbl)
    return;
  QFontMetrics fm(m_textLbl->font());
  // Leave room for icon + close + padding inside max tab width.
  const int textBudget =
      tabMaxWidthPx() - UiScale::dp(10 + 18 + 6 + (m_closable ? 24 : 0) + 6);
  m_textLbl->setText(fm.elidedText(m_title, Qt::ElideRight, qMax(UiScale::dp(40), textBudget)));
  setFixedWidth(sizeHint().width());
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
  QFontMetrics fm(font());
  const QString shown = m_textLbl ? m_textLbl->text() : m_title;
  int textW = fm.horizontalAdvance(shown.isEmpty() ? QStringLiteral("W") : shown);
  int w = UiScale::dp(10) + UiScale::dp(18) + UiScale::dp(6) + textW +
          (m_closable ? UiScale::dp(24) : 0) + UiScale::dp(6);
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

  const qreal rad = m_noteChromeMode ? UiScale::dp(8) : UiScale::dp(14);
  QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  QPainterPath path;
  path.addRoundedRect(r, rad, rad);

  if (m_noteChromeMode) {
    if (m_active) {
      QColor bg = NoteChrome::panelElevated();
      p.fillPath(path, bg);
      p.setPen(QPen(m_accentColor.isValid() ? m_accentColor : NoteChrome::accent(),
                    1.2));
      p.drawPath(path);
    } else if (m_hovered) {
      QColor bg = NoteChrome::panelBg();
      bg.setAlpha(220);
      p.fillPath(path, bg);
    } else {
      QColor bg = NoteChrome::panelBg();
      bg.setAlpha(140);
      p.fillPath(path, bg);
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
    QColor bg(255, 255, 255, 18);
    p.fillPath(path, bg);
  } else {
    QColor bg(255, 255, 255, 8);
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

#ifdef Q_OS_ANDROID
  QScroller::grabGesture(m_scroll->viewport(), QScroller::LeftMouseButtonGesture);
#else
  QScroller::grabGesture(m_scroll->viewport(), QScroller::LeftMouseButtonGesture);
#endif

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
  int idx = m_tabs.size();
  m_tabs.append(tab);

  // Insert before the trailing stretch.
  m_tabsLayout->insertWidget(m_tabsLayout->count() - 1, tab);
  rebindTabSignals();

  // Grow scroll content to natural tab row width.
  int contentW = 0;
  for (DocumentTab *t : m_tabs)
    contentW += t->width() + UiScale::dp(6);
  contentW = qMax(contentW, m_scroll->viewport()->width());
  m_scrollContent->setFixedWidth(contentW);

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

  int contentW = 0;
  for (DocumentTab *t : m_tabs)
    contentW += t->width() + UiScale::dp(6);
  contentW = qMax(contentW, m_scroll->viewport()->width());
  m_scrollContent->setFixedWidth(qMax(UiScale::dp(40), contentW));

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
  int contentW = 0;
  for (DocumentTab *t : m_tabs)
    contentW += t->width() + UiScale::dp(6);
  m_scrollContent->setFixedWidth(qMax(m_scroll->viewport()->width(), contentW));
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
  updateIndicator(false);
}

void DocumentTabBar::setNoteChromeMode(bool on) {
  // Always refresh tab chrome so light/dark NoteChrome swaps apply even when
  // noteChromeMode was already true.
  m_noteChromeMode = on;
  if (on)
    setAccentColor(NoteChrome::accent());
  if (m_homeTab)
    m_homeTab->setNoteChromeMode(on);
  for (DocumentTab *tab : m_tabs)
    tab->setNoteChromeMode(on);
  updateIndicator(false);
}

void DocumentTabBar::setCurrentIndex(int index) {
  if (index < -1 || index >= m_tabs.size() || index == m_currentIndex)
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
  m_scroll->ensureWidgetVisible(tab, UiScale::dp(24), 0);
}

void DocumentTabBar::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (m_scrollContent) {
    int contentW = 0;
    for (DocumentTab *t : m_tabs)
      contentW += t->width() + UiScale::dp(6);
    m_scrollContent->setFixedWidth(
        qMax(m_scroll->viewport()->width(), qMax(UiScale::dp(40), contentW)));
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
