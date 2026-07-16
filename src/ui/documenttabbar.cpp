#include "documenttabbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>

#include "blop_theme.h"
#include "blopstyle.h"
#include "moderntoolbar.h"
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

  QHBoxLayout *lay = new QHBoxLayout(this);
  lay->setContentsMargins(UiScale::dp(10), 0, UiScale::dp(6), 0);
  lay->setSpacing(UiScale::dp(6));

  QLabel *iconLbl = new QLabel(this);
  iconLbl->setPixmap(makeTabIcon(m_iconName, QColor(232, 228, 255), UiScale::dp(18))
                         .pixmap(UiScale::dp(18), UiScale::dp(18)));
  lay->addWidget(iconLbl);

  QLabel *textLbl = new QLabel(title, this);
  textLbl->setStyleSheet(
      QStringLiteral("QLabel { color: #E8E8FF; font-size: 14px; font-weight: 600; }"));
  lay->addWidget(textLbl);

  if (m_closable) {
    QPushButton *closeBtn = new QPushButton(QStringLiteral("\u2715"), this);
    closeBtn->setFixedSize(UiScale::dp(20), UiScale::dp(20));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  background-color: transparent;"
                       "  border: none;"
                       "  color: rgba(255,255,255,0.50);"
                       "  font-size: 12px;"
                       "  border-radius: 10px;"
                       "}"
                       "QPushButton:hover {"
                       "  background-color: rgba(255,255,255,0.15);"
                       "  color: white;"
                       "}"));
    connect(closeBtn, &QPushButton::clicked, this, [this]() { emit closeClicked(); });
    lay->addWidget(closeBtn);
  }

  setMouseTracking(true);
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
  QFontMetrics fm(font());
  int textW = fm.horizontalAdvance(m_title);
  int iconW = UiScale::dp(18);
  int spacing = UiScale::dp(6);
  return QSize(iconW + spacing + textW, UiScale::dp(36));
}

void DocumentTab::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const qreal rad = UiScale::dp(14);
  QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  QPainterPath path;
  path.addRoundedRect(r, rad, rad);

  if (m_active) {
    QColor bg = m_accentColor;
    bg.setAlphaF(0.22);
    p.fillPath(path, bg);

    QColor border = m_accentColor;
    border.setAlphaF(0.55);
    p.setPen(QPen(border, 1.5));
    p.drawPath(path);
  } else if (m_hovered) {
    QColor bg = BlopStyle::surfaceBg();
    bg.setAlphaF(0.5);
    p.fillPath(path, bg);

    QColor border = BlopStyle::surfaceBorder();
    p.setPen(QPen(border, 1.0));
    p.drawPath(path);
  } else {
    QColor bg = BlopStyle::surfaceBg();
    bg.setAlphaF(0.22);
    p.fillPath(path, bg);

    QColor border = BlopStyle::surfaceBorder();
    border.setAlphaF(0.5);
    p.setPen(QPen(border, 1.0));
    p.drawPath(path);
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
  m_layout = new QHBoxLayout(this);
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(UiScale::dp(8));

  m_homeTab = new DocumentTab(QString(), QStringLiteral("home"), false, this);
  m_homeTab->setActive(true, m_accentColor);
  connect(m_homeTab, &DocumentTab::clicked, this, [this]() { emit homeClicked(); });
  m_layout->addWidget(m_homeTab);

  m_indicator = new QWidget(this);
  m_indicator->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_indicator->setAttribute(Qt::WA_NoSystemBackground, true);
  m_indicator->resize(UiScale::dp(28), UiScale::dp(3));
  m_indicator->hide();
}

int DocumentTabBar::addTab(const QString &title, const QString &iconName) {
  DocumentTab *tab = new DocumentTab(title, iconName, true, this);
  int idx = m_tabs.size();
  m_tabs.append(tab);

  m_layout->addWidget(tab);
  connect(tab, &DocumentTab::clicked, this, [this, idx]() { setCurrentIndex(idx); });
  connect(tab, &DocumentTab::closeClicked, this, [this, idx]() { emit tabCloseRequested(idx); });

  if (m_currentIndex < 0)
    setCurrentIndex(idx);
  else
    updateIndicator(false);
  return idx;
}

void DocumentTabBar::removeTab(int index) {
  if (index < 0 || index >= m_tabs.size())
    return;
  DocumentTab *tab = m_tabs.takeAt(index);
  tab->deleteLater();

  for (int i = 0; i < m_tabs.size(); ++i) {
    disconnect(m_tabs[i], &DocumentTab::clicked, nullptr, nullptr);
    connect(m_tabs[i], &DocumentTab::clicked, this, [this, i]() { setCurrentIndex(i); });
  }

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
  QList<QLabel *> labels = m_tabs[index]->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly);
  if (labels.size() >= 2)
    labels[1]->setText(title);
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
  updateIndicator(false);
}

void DocumentTabBar::setHomeActive(bool active) {
  if (m_homeActive == active)
    return;
  m_homeActive = active;
  m_homeTab->setActive(active, m_accentColor);
  for (DocumentTab *tab : m_tabs)
    tab->setActive(!active && (m_currentIndex >= 0 && m_tabs[m_currentIndex] == tab), m_accentColor);
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
  emit currentChanged(m_currentIndex);
}

void DocumentTabBar::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateIndicator(false);
}

void DocumentTabBar::updateIndicator(bool animate) {
  if (!m_indicator)
    return;

  DocumentTab *activeTab = m_homeActive ? m_homeTab : (m_currentIndex >= 0 ? m_tabs[m_currentIndex] : nullptr);
  if (!activeTab || !activeTab->isVisible()) {
    m_indicator->hide();
    return;
  }

  const int indW = activeTab->width() - UiScale::dp(20);
  const int indH = UiScale::dp(3);
  const QPoint target(activeTab->x() + UiScale::dp(10), activeTab->y() + activeTab->height() - UiScale::dp(6));

  m_indicator->resize(qMax(0, indW), indH);

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
  m_indicatorAnim->setDuration(240);
  m_indicatorAnim->setEasingCurve(QEasingCurve::OutCubic);
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

