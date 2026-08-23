#include "pagethumbnailsidebar.h"

#include "Note.h"
#include "blop_dialogs.h"
#include "blop_inwindow_menu.h"
#include "moderntoolbar.h"
#include "multipagenoteview.h"
#include "notechrome.h"
#include "uiscale.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QEasingCurve>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QSize>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace {
struct RailMetrics {
  int width;
  int thumbW;
  int thumbH;
  int itemH;
  int pad;
};

RailMetrics railMetrics(QWidget *ref, bool twoCol, bool horizontal) {
  if (horizontal) {
    // J mockup: larger paper thumbs in the top strip.
    return {0, UiScale::dp(64), UiScale::dp(90), UiScale::dp(118),
            UiScale::dp(8)};
  }
  if (UiScale::isAndroidPhoneUi(ref)) {
    return {UiScale::dp(68), UiScale::dp(52), UiScale::dp(72), UiScale::dp(90),
            UiScale::dp(6)};
  }
  if (twoCol) {
    return {UiScale::dp(176), UiScale::dp(70), UiScale::dp(94),
            UiScale::dp(112), UiScale::dp(6)};
  }
  return {UiScale::dp(108), UiScale::dp(80), UiScale::dp(104), UiScale::dp(124),
          UiScale::dp(8)};
}

QIcon railGlyph(const QString &name, const QColor &fg, int px) {
  QPixmap pm(px, px);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  const qreal s = px / 64.0;
  p.scale(s, s);
  blopDrawToolbarGlyph64(&p, name, fg);
  return QIcon(pm);
}

class PageStripDelegate : public QStyledItemDelegate {
public:
  explicit PageStripDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void paint(QPainter *p, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;
    opt.state &= ~QStyle::State_Selected;

    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    const QRectF box = QRectF(option.rect).adjusted(1.5, 1.5, -1.5, -1.5);
    const QColor accent = NoteChrome::accent();
    if (selected) {
      p->setPen(QPen(accent, 2.0));
      p->setBrush(QColor(0xFF, 0xFF, 0xFF));
    } else if (hover) {
      p->setPen(QPen(QColor(0xE4, 0xE7, 0xEE), 1.0));
      p->setBrush(QColor(0xF8, 0xF9, 0xFB));
    } else {
      p->setPen(QPen(QColor(0xE4, 0xE7, 0xEE), 1.0));
      p->setBrush(QColor(0xFF, 0xFF, 0xFF));
    }
    p->drawRoundedRect(box, 8.0, 8.0);

    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    const QString text = index.data(Qt::DisplayRole).toString();
    const QFont f = opt.font;
    QFontMetrics fm(f);
    const int textH = fm.height();
    const int pad = UiScale::dp(4);
    const int iconH = qMax(UiScale::dp(48),
                           option.rect.height() - textH - pad * 3);
    const int iconW = option.rect.width() - pad * 2;
    const QRect iconRect(option.rect.left() + pad, option.rect.top() + pad,
                         iconW, iconH);
    if (!icon.isNull())
      icon.paint(p, iconRect, Qt::AlignCenter);

    const QColor textColor =
        selected ? QColor(0x1C, 0x1E, 0x24)
                 : (index.data(Qt::ForegroundRole).canConvert<QColor>()
                        ? index.data(Qt::ForegroundRole).value<QColor>()
                        : QColor(0x5A, 0x60, 0x70));
    p->setPen(textColor);
    p->setFont(f);
    const QRect textRect(option.rect.left(), iconRect.bottom() + pad / 2,
                         option.rect.width(), textH + pad);
    p->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, text);

    p->restore();
  }
};
} // namespace

PageThumbnailSidebar::PageThumbnailSidebar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("PageThumbnailSidebar"));
  setAttribute(Qt::WA_StyledBackground, true);
  const RailMetrics m = railMetrics(this, m_twoColumn, m_horizontalStrip);
  m_expandedWidth = m.width > 0 ? m.width : UiScale::dp(108);
  m_expandedHeight = m_horizontalStrip ? UiScale::dp(136) : UiScale::dp(118);
  if (!m_horizontalStrip)
    setFixedWidth(m_expandedWidth);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  m_btnToggle = new QPushButton(this);
  m_btnToggle->setObjectName(QStringLiteral("PageRailToggleBtn"));
  m_btnToggle->setFixedHeight(UiScale::dp(28));
  m_btnToggle->setCursor(Qt::PointingHandCursor);
  m_btnToggle->setIcon(
      railGlyph(QStringLiteral("chevron_left"), NoteChrome::textSecondary(),
                UiScale::dp(16)));
  m_btnToggle->setIconSize(QSize(UiScale::dp(14), UiScale::dp(14)));
  connect(m_btnToggle, &QPushButton::clicked, this,
          &PageThumbnailSidebar::toggleCollapsed);
  root->addWidget(m_btnToggle);

  m_railBody = new QWidget(this);
  m_railBody->setObjectName(QStringLiteral("PageRailBody"));
  auto *lay = new QVBoxLayout(m_railBody);
  lay->setContentsMargins(m.pad, UiScale::dp(6), m.pad, UiScale::dp(8));
  lay->setSpacing(UiScale::dp(6));

  m_list = new QListWidget(m_railBody);
  m_list->setFrameStyle(QFrame::NoFrame);
  m_list->setSpacing(UiScale::dp(6));
  m_list->setIconSize(QSize(m.thumbW, m.thumbH));
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_list->setContextMenuPolicy(Qt::CustomContextMenu);
  m_list->setDragDropMode(QAbstractItemView::InternalMove);
  m_list->setDefaultDropAction(Qt::MoveAction);
  m_list->setMovement(QListView::Snap);
  connect(m_list, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) { onItemClicked(m_list->row(item)); });
  connect(m_list, &QWidget::customContextMenuRequested, this,
          &PageThumbnailSidebar::showItemContextMenu);
  connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this]() { requestVisibleThumbnails(); });
  if (m_list->horizontalScrollBar()) {
    connect(m_list->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            [this]() {
              requestVisibleThumbnails();
              updateHorizontalScrollAffordance();
            });
    connect(m_list->horizontalScrollBar(), &QScrollBar::rangeChanged, this,
            [this](int, int) { updateHorizontalScrollAffordance(); });
  }
  m_list->setFocusPolicy(Qt::NoFocus);
  m_list->viewport()->installEventFilter(this);
  connect(m_list->model(), &QAbstractItemModel::rowsMoved, this,
          [this](const QModelIndex &, int start, int end,
                 const QModelIndex &, int dest) {
            Q_UNUSED(end);
            if (!m_view || start < 0)
              return;
            int to = dest;
            if (to > start)
              --to;
            if (to == start)
              return;
            m_view->movePage(start, to);
            // pagesChanged emitted by movePage triggers rebuild() automatically.
            emit pagesMutated();
            emit pageSelected(to);
          });
  lay->addWidget(m_list, 1);

  auto *footer = new QHBoxLayout;
  footer->setSpacing(UiScale::dp(6));
  m_btnAddPage = new QPushButton(m_railBody);
  m_btnAddPage->setObjectName(QStringLiteral("PageRailAddBtn"));
  m_btnAddPage->setFixedHeight(UiScale::dp(34));
  m_btnAddPage->setCursor(Qt::PointingHandCursor);
  m_btnAddPage->setToolTip(QStringLiteral("Seite hinzufügen"));
      m_btnAddPage->setText(QStringLiteral("+"));
      m_btnAddPage->setIcon(QIcon());
  m_btnAddPage->setIcon(
      railGlyph(QStringLiteral("add"), NoteChrome::textSecondary(),
                UiScale::dp(16)));
  m_btnAddPage->setIconSize(QSize(UiScale::dp(14), UiScale::dp(14)));
  // Narrow Android phone rail: icon-only — " Seite" truncates to "Sei…".
  if (UiScale::isAndroidPhoneUi(this))
    m_btnAddPage->setText(QString());
  else
    m_btnAddPage->setText(QStringLiteral(" Seite"));
  connect(m_btnAddPage, &QPushButton::clicked, this,
          &PageThumbnailSidebar::addPageRequested);
  footer->addWidget(m_btnAddPage, 1);

  m_btnColumns = new QPushButton(m_railBody);
  m_btnColumns->setObjectName(QStringLiteral("PageRailColsBtn"));
  m_btnColumns->setFixedSize(UiScale::dp(34), UiScale::dp(34));
  m_btnColumns->setCursor(Qt::PointingHandCursor);
  m_btnColumns->setToolTip(QStringLiteral("Ein-/Zweispaltig"));
  m_btnColumns->setIcon(
      railGlyph(QStringLiteral("layout_rows"), NoteChrome::textSecondary(),
                UiScale::dp(16)));
  m_btnColumns->setIconSize(QSize(UiScale::dp(14), UiScale::dp(14)));
  connect(m_btnColumns, &QPushButton::clicked, this,
          &PageThumbnailSidebar::toggleTwoColumnMode);
  footer->addWidget(m_btnColumns);
  if (UiScale::isAndroidPhoneUi(this))
    m_btnColumns->hide();
  lay->addLayout(footer);

  root->addWidget(m_railBody, 1);
  applyViewMode();
  refreshListStyle();
}

void PageThumbnailSidebar::setCollapsed(bool collapsed) {
  if (m_collapsed == collapsed)
    return;
  m_collapsed = collapsed;
  applyCollapsedState();
  emit collapsedChanged(m_collapsed);
}

void PageThumbnailSidebar::toggleCollapsed() {
  setCollapsed(!m_collapsed);
}

void PageThumbnailSidebar::mouseDoubleClickEvent(QMouseEvent *event) {
  if (m_horizontalStrip && event->button() == Qt::LeftButton) {
    toggleCollapsed();
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}

bool PageThumbnailSidebar::eventFilter(QObject *watched, QEvent *event) {
  if (m_horizontalStrip && m_list && watched == m_list->viewport() &&
      event->type() == QEvent::MouseButtonDblClick) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
      // Double-click anywhere on the J strip (incl. thumbnails) collapses.
      toggleCollapsed();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void PageThumbnailSidebar::setTwoColumnMode(bool on) {
  if (m_twoColumn == on)
    return;
  m_twoColumn = on;
  applyViewMode();
  rebuild();
  emit pagesMutated(); // triggers MainWindow reposition for width change
}

void PageThumbnailSidebar::toggleTwoColumnMode() {
  setTwoColumnMode(!m_twoColumn);
}

int PageThumbnailSidebar::expandedHeight() const {
  if (m_expandedHeight > 0)
    return m_expandedHeight;
  return UiScale::dp(118);
}

int PageThumbnailSidebar::collapsedHandleHeight() const {
  return UiScale::dp(22);
}

void PageThumbnailSidebar::setHorizontalStrip(bool on) {
  if (m_horizontalStrip == on)
    return;
  m_horizontalStrip = on;
  const RailMetrics m = railMetrics(this, m_twoColumn, m_horizontalStrip);
  m_expandedHeight = m.itemH + UiScale::dp(20);

  setMinimumWidth(0);
  setMaximumWidth(QWIDGETSIZE_MAX);
  setMinimumHeight(0);
  setMaximumHeight(QWIDGETSIZE_MAX);

  if (on) {
    m_floatingMode = false;
    if (m_btnToggle)
      m_btnToggle->show();
    if (m_btnColumns)
      m_btnColumns->hide();
    if (m_btnAddPage) {
      m_btnAddPage->setText(QString());
      m_btnAddPage->setFixedSize(UiScale::dp(34), UiScale::dp(34));
      m_btnAddPage->setToolTip(QStringLiteral("Seite hinzufügen"));
      m_btnAddPage->setText(QStringLiteral("+"));
      m_btnAddPage->setIcon(QIcon());
    }

    // Flatten into a single horizontal row: thumbs · add · collapse.
    if (QLayout *old = layout()) {
      while (old->count() > 0) {
        old->takeAt(0);
      }
      delete old;
    }
    if (m_railBody) {
      if (QLayout *bodyLay = m_railBody->layout()) {
        while (bodyLay->count() > 0)
          bodyLay->takeAt(0);
      }
      m_railBody->hide();
    }
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(UiScale::dp(8), UiScale::dp(6), UiScale::dp(8),
                            UiScale::dp(6));
    row->setSpacing(UiScale::dp(8));
    if (m_list) {
      m_list->setParent(this);
      row->addWidget(m_list, 1);
    }
    if (!m_btnScrollRight) {
      m_btnScrollRight = new QPushButton(this);
      m_btnScrollRight->setObjectName(QStringLiteral("PageRailScrollBtn"));
      m_btnScrollRight->setFixedSize(UiScale::dp(34), UiScale::dp(34));
      m_btnScrollRight->setCursor(Qt::PointingHandCursor);
      m_btnScrollRight->setToolTip(QStringLiteral("Weitere Seiten"));
      m_btnScrollRight->setText(QStringLiteral("›"));
      m_btnScrollRight->setIcon(QIcon());
      connect(m_btnScrollRight, &QPushButton::clicked, this, [this]() {
        if (!m_list || !m_list->horizontalScrollBar())
          return;
        auto *bar = m_list->horizontalScrollBar();
        bar->setValue(bar->value() + UiScale::dp(120));
        requestVisibleThumbnails();
        updateHorizontalScrollAffordance();
      });
    }
    if (m_btnScrollRight) {
      m_btnScrollRight->setParent(this);
      row->addWidget(m_btnScrollRight, 0, Qt::AlignVCenter);
    }
    if (m_btnAddPage) {
      m_btnAddPage->setParent(this);
      row->addWidget(m_btnAddPage, 0, Qt::AlignVCenter);
    }
    if (m_btnToggle) {
      m_btnToggle->setParent(this);
      m_btnToggle->setObjectName(QStringLiteral("PageRailToggleBtn"));
      m_btnToggle->setFixedSize(UiScale::dp(44), UiScale::dp(44));
      m_btnToggle->setToolTip(QStringLiteral("Seitenleiste einklappen"));
      m_btnToggle->setText(QStringLiteral("▴"));
      m_btnToggle->setIcon(QIcon());
      m_btnToggle->setStyleSheet(QStringLiteral(
          "QPushButton#PageRailToggleBtn {"
          "  background: #F8F9FB; border: 1px solid #E4E7EE; border-radius: 10px;"
          "  color: #5A6070; font-size: 16px; font-weight: 700;"
          "}"
          "QPushButton#PageRailToggleBtn:hover {"
          "  border-color: #5B9DFF; background: rgba(91,157,255,0.10);"
          "}"));
      row->addWidget(m_btnToggle, 0, Qt::AlignVCenter);
    }
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (!m_collapsed)
      setFixedHeight(m_expandedHeight);
  } else {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    if (!m_collapsed)
      setFixedWidth(m.width > 0 ? m.width : m_expandedWidth);
  }
  applyViewMode();
  applyCollapsedState();
  refreshListStyle();
  updateHorizontalScrollAffordance();
}

void PageThumbnailSidebar::updateHorizontalScrollAffordance() {
  if (!m_horizontalStrip || !m_btnScrollRight || !m_list)
    return;
  auto *bar = m_list->horizontalScrollBar();
  const bool canScroll =
      bar && bar->maximum() > 0 && bar->value() < bar->maximum();
  m_btnScrollRight->setVisible(!m_collapsed && canScroll);
}

void PageThumbnailSidebar::applyViewMode() {
  const RailMetrics m = railMetrics(this, m_twoColumn, m_horizontalStrip);
  m_expandedWidth = m.width > 0 ? m.width : m_expandedWidth;
  m_expandedHeight = m.itemH + UiScale::dp(20);
  if (!m_collapsed) {
    if (m_horizontalStrip)
      setFixedHeight(m_expandedHeight);
    else if (m_expandedWidth > 0)
      setFixedWidth(m_expandedWidth);
  }
  if (!m_list)
    return;
  m_list->setIconSize(QSize(m.thumbW, m.thumbH));
  if (m_horizontalStrip) {
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(false);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(UiScale::dp(8));
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  } else if (m_twoColumn) {
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(true);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(UiScale::dp(5));
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  } else {
    m_list->setViewMode(QListView::ListMode);
    m_list->setFlow(QListView::TopToBottom);
    m_list->setWrapping(false);
    m_list->setSpacing(UiScale::dp(6));
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  }
}

void PageThumbnailSidebar::applyCollapsedState() {
  if (m_horizontalStrip) {
    const int handle = collapsedHandleHeight();
    const int expanded = qMax(expandedHeight(), UiScale::dp(96));
    const int target = m_collapsed ? handle : expanded;
    if (m_list)
      m_list->setVisible(!m_collapsed);
    if (m_btnAddPage)
      m_btnAddPage->setVisible(!m_collapsed);
    updateHorizontalScrollAffordance();
    if (m_btnToggle) {
      m_btnToggle->show();
      m_btnToggle->setIcon(QIcon());
      m_btnToggle->setText(m_collapsed ? QStringLiteral("▾")
                                       : QStringLiteral("▴"));
      m_btnToggle->setToolTip(m_collapsed
                                  ? QStringLiteral("Seitenleiste aufklappen")
                                  : QStringLiteral("Seitenleiste einklappen"));
      if (m_collapsed) {
        m_btnToggle->setFixedSize(UiScale::dp(36), handle);
        m_btnToggle->setMinimumWidth(UiScale::dp(36));
      } else {
        m_btnToggle->setFixedSize(UiScale::dp(44), UiScale::dp(44));
      }
    }
    show();
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    if (m_heightAnim)
      m_heightAnim->stop();
    if (!m_heightAnim) {
      m_heightAnim = new QVariantAnimation(this);
      m_heightAnim->setDuration(220);
      m_heightAnim->setEasingCurve(QEasingCurve::InOutCubic);
      connect(m_heightAnim, &QVariantAnimation::valueChanged, this,
              [this](const QVariant &v) {
                setFixedHeight(v.toInt());
                emit pagesMutated();
              });
    }
    const int from = height() > 0 ? height() : target;
    if (qAbs(from - target) < 2) {
      setFixedHeight(target);
    } else {
      m_heightAnim->setStartValue(from);
      m_heightAnim->setEndValue(target);
      m_heightAnim->start();
    }
    refreshListStyle();
    return;
  }

  if (m_railBody)
    m_railBody->setVisible(!m_collapsed);
  if (m_collapsed) {
    // Fully fold away — no 28dp stub that looked "half cut off".
    setFixedWidth(0);
    hide();
    if (m_btnToggle) {
      m_btnToggle->setIcon(
          railGlyph(QStringLiteral("chevron_right"), NoteChrome::textSecondary(),
                    UiScale::dp(16)));
      m_btnToggle->setToolTip(QStringLiteral("Seitenmanager aufklappen"));
    }
  } else {
    if (m_expandedWidth <= 0)
      m_expandedWidth = railMetrics(this, m_twoColumn, m_horizontalStrip).width;
    setFixedWidth(m_expandedWidth);
    show();
    if (m_btnToggle) {
      m_btnToggle->setIcon(
          railGlyph(QStringLiteral("chevron_left"), NoteChrome::textSecondary(),
                    UiScale::dp(16)));
      m_btnToggle->setToolTip(QStringLiteral("Seitenmanager einklappen"));
    }
  }
  refreshListStyle();
}

void PageThumbnailSidebar::refreshListStyle() {
  const QColor accent = NoteChrome::accent();
  const QString bg =
      m_horizontalStrip ? QStringLiteral("#FFFFFF")
                        : NoteChrome::toolbarFill().name();
  const QString edge = m_horizontalStrip
                           ? QStringLiteral("border-bottom: 1px solid #DDE1E8;")
                           : QStringLiteral("border-right: 1px solid %1;")
                                 .arg(NoteChrome::borderSoft().name());
  const QColor toggleFg =
      m_horizontalStrip ? QColor(0x5A, 0x60, 0x70) : NoteChrome::textSecondary();
  setStyleSheet(QStringLiteral(
      "QWidget#PageThumbnailSidebar {"
      "  background: %1; border: none; %2"
      "}"
      "QWidget#PageRailBody { background: transparent; border: none; }"
      "QPushButton#PageRailToggleBtn {"
      "  background: transparent; border: none; color: %3;"
      "  border-radius: 6px;"
      "}"
      "QPushButton#PageRailToggleBtn:hover { background: rgba(0,0,0,0.06); }"
      "QPushButton#PageRailAddBtn, QPushButton#PageRailColsBtn {"
      "  background: %4; color: %3; border: 1px solid %5; border-radius: 6px;"
      "  font-size: 12px; font-weight: 600;"
      "}"
      "QPushButton#PageRailAddBtn:hover, QPushButton#PageRailColsBtn:hover,"
      "QPushButton#PageRailScrollBtn:hover {"
      "  border-color: %6; color: %7; background: rgba(91,157,255,0.10);"
      "}"
      "QPushButton#PageRailScrollBtn {"
      "  background: %4; color: %3; border: 1px solid %5; border-radius: 10px;"
      "  font-size: 18px; font-weight: 700;"
      "}")
                    .arg(bg, edge, toggleFg.name(),
                         m_horizontalStrip ? QStringLiteral("#F8F9FB")
                                           : NoteChrome::panelElevated().name(),
                         m_horizontalStrip ? QStringLiteral("#E4E7EE")
                                           : NoteChrome::borderSoft().name(),
                         accent.name(),
                         m_horizontalStrip ? QStringLiteral("#1C1E24")
                                           : NoteChrome::textPrimary().name()));

  if (m_btnAddPage) {
    m_btnAddPage->setIcon(
        railGlyph(QStringLiteral("add"),
                  m_horizontalStrip ? toggleFg : NoteChrome::textSecondary(),
                  UiScale::dp(16)));
  }
  if (m_btnColumns) {
    m_btnColumns->setIcon(
        railGlyph(m_twoColumn ? QStringLiteral("layout_single")
                              : QStringLiteral("layout_rows"),
                  m_horizontalStrip ? toggleFg : NoteChrome::textSecondary(),
                  UiScale::dp(16)));
  }

  if (m_list) {
    const QString itemBg =
        m_horizontalStrip ? QStringLiteral("#FFFFFF") : NoteChrome::panelElevated().name();
    const QString itemFg =
        m_horizontalStrip ? QStringLiteral("#6B7280") : NoteChrome::textSecondary().name();
    const QString itemBorder =
        m_horizontalStrip ? QStringLiteral("#E4E7EE") : NoteChrome::borderSoft().name();
    const QString hover = m_horizontalStrip ? QStringLiteral("rgba(91,157,255,0.08)")
                                            : QStringLiteral("rgba(91,157,255,0.12)");
    m_list->setStyleSheet(
        QStringLiteral(
            "QListWidget { background: transparent; border: none; outline: 0; color: %1;"
            "  show-decoration-selected: 0; }"
            "QListWidget::item {"
            "  border: 1px solid %2; border-radius: 8px; padding: 3px;"
            "  background: %3; color: %1;"
            "  font-size: 10px; font-weight: 600;"
            "}"
            "QListWidget::item:selected,"
            "QListWidget::item:selected:active,"
            "QListWidget::item:selected:!active {"
            "  border: 2px solid %4; background: #FFFFFF;"
            "  color: #1C1E24;"
            "}"
            "QListWidget::item:hover:!selected { background: %5; }")
            .arg(itemFg, itemBorder, itemBg, accent.name(), hover));
    if (m_horizontalStrip) {
      if (!m_stripDelegate)
        m_stripDelegate = new PageStripDelegate(m_list);
      if (m_list->itemDelegate() != m_stripDelegate)
        m_list->setItemDelegate(m_stripDelegate);
    } else if (m_stripDelegate && m_list->itemDelegate() == m_stripDelegate) {
      m_list->setItemDelegate(new QStyledItemDelegate(m_list));
    }
    QPalette pal = m_list->palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Highlight, QColor(255, 255, 255));
    pal.setColor(QPalette::HighlightedText, QColor(0x1C, 0x1E, 0x24));
    m_list->setPalette(pal);
  }
  updateHorizontalScrollAffordance();
}

void PageThumbnailSidebar::setNoteView(MultiPageNoteView *view) {
  if (m_view == view)
    return;
  if (m_view)
    disconnect(m_view, nullptr, this, nullptr);
  m_view = view;
  if (m_view) {
    connect(m_view, &MultiPageNoteView::pagesChanged, this,
            &PageThumbnailSidebar::rebuild, Qt::UniqueConnection);
  }
  rebuild();
}

void PageThumbnailSidebar::setAccentColor(const QColor &color) {
  if (m_accentColor == color)
    return;
  m_accentColor = color;
  refreshListStyle();
  rebuild();
}

void PageThumbnailSidebar::setFloatingMode(bool on) {
  m_floatingMode = on;
  setAttribute(Qt::WA_TranslucentBackground, false);
  if (m_horizontalStrip) {
    if (m_btnToggle)
      m_btnToggle->show();
    refreshListStyle();
    update();
    return;
  }
  if (on) {
    // Drawboard: fold/expand is owned by NoteLeftRail "Seitenleiste".
    // Keep the in-panel chevron hidden; do NOT force-expand here (that
    // fought the left-rail toggle and left a thin stub strip).
    if (m_btnToggle)
      m_btnToggle->hide();
    if (m_railBody && !m_collapsed)
      m_railBody->show();
  } else if (m_btnToggle) {
    m_btnToggle->show();
  }
  refreshListStyle();
  update();
}

void PageThumbnailSidebar::rebuild() {
  ++m_rebuildEpoch;
  const int epoch = m_rebuildEpoch;
  if (!m_list)
    return;
  m_list->clear();
  if (!m_view || !m_view->note())
    return;

  applyViewMode();
  const RailMetrics m = railMetrics(this, m_twoColumn, m_horizontalStrip);
  const Note *note = m_view->note();
  const int count = note->pages.size();
  for (int i = 0; i < count; ++i) {
    auto *item = new QListWidgetItem(m_list);
    item->setTextAlignment(Qt::AlignCenter | Qt::AlignBottom);
    item->setSizeHint(QSize(m.thumbW + UiScale::dp(m_horizontalStrip ? 36 : 12),
                            m.itemH));
    QString label;
    if (m_horizontalStrip && m_view) {
      label = m_view->pageTitle(i).trimmed();
      if (label.isEmpty())
        label = QStringLiteral("Seite %1").arg(i + 1);
    } else {
      label = QString::number(i + 1);
    }
    item->setText(label);
    if (note->pages[i].bookmarked) {
      item->setForeground(m_horizontalStrip ? QColor(QStringLiteral("#5B9DFF"))
                                            : NoteChrome::accent());
      item->setToolTip(QStringLiteral("%1 · Lesezeichen").arg(label));
      item->setData(Qt::UserRole + 1, true);
    } else {
      item->setForeground(m_horizontalStrip ? QColor(0x5A, 0x60, 0x70)
                                            : NoteChrome::textSecondary());
      item->setToolTip(label);
      item->setData(Qt::UserRole + 1, false);
    }
    item->setData(Qt::UserRole, i);
    item->setData(Qt::UserRole + 2, false); // thumbnail requested flag
    m_list->addItem(item);
  }
  if (m_currentPage >= 0 && m_currentPage < count)
    m_list->setCurrentRow(m_currentPage);

  // Only render the thumbnails that are currently visible. This keeps note
  // load and bulk page operations (delete/duplicate/layout) responsive for
  // notebooks with many pages. Defer until the list has laid out items.
  QTimer::singleShot(0, this, [this, epoch]() {
    requestVisibleThumbnails(epoch);
    updateHorizontalScrollAffordance();
  });
}

void PageThumbnailSidebar::requestThumbnail(int pageIndex, QListWidgetItem *item,
                                            int epoch) {
  if (!m_view || !item)
    return;
  const RailMetrics m = railMetrics(this, m_twoColumn, m_horizontalStrip);
  m_view->generateThumbnailAsync(
      pageIndex, QSize(m.thumbW, m.thumbH),
      [this, item, epoch](const QPixmap &pm) {
        if (!item || m_rebuildEpoch != epoch)
          return;
        if (!pm.isNull())
          item->setIcon(QIcon(pm));
      });
}

void PageThumbnailSidebar::requestVisibleThumbnails() {
  requestVisibleThumbnails(m_rebuildEpoch);
}

void PageThumbnailSidebar::requestVisibleThumbnails(int epoch) {
  if (!m_list || !m_view || !m_view->note())
    return;
  if (m_rebuildEpoch != epoch)
    return;

  const QRect viewport = m_list->viewport()->rect();
  QModelIndex topIdx = m_list->indexAt(viewport.topLeft());
  QModelIndex bottomIdx = m_list->indexAt(viewport.bottomRight());
  int first = topIdx.isValid() ? topIdx.row() : 0;
  int last = bottomIdx.isValid() ? bottomIdx.row() : m_list->count() - 1;
  if (first < 0)
    first = 0;
  if (last < first)
    last = first;
  if (last >= m_list->count())
    last = m_list->count() - 1;

  constexpr int kPrefetch = 2;
  first = qMax(0, first - kPrefetch);
  last = qMin(m_list->count() - 1, last + kPrefetch);

  // Always include the current page even if it scrolled out of view.
  if (m_currentPage >= 0 && m_currentPage < m_list->count()) {
    first = qMin(first, m_currentPage);
    last = qMax(last, m_currentPage);
  }

  for (int i = first; i <= last; ++i) {
    auto *item = m_list->item(i);
    if (!item || item->data(Qt::UserRole + 2).toBool())
      continue;
    item->setData(Qt::UserRole + 2, true);
    requestThumbnail(i, item, epoch);
  }
}

void PageThumbnailSidebar::onCurrentPageChanged(int pageIndex) {
  m_currentPage = pageIndex;
  if (m_list && pageIndex >= 0 && pageIndex < m_list->count()) {
    m_list->setCurrentRow(pageIndex);
    auto *item = m_list->item(pageIndex);
    if (item && !item->data(Qt::UserRole + 2).toBool()) {
      item->setData(Qt::UserRole + 2, true);
      requestThumbnail(pageIndex, item, m_rebuildEpoch);
    }
  }
}

void PageThumbnailSidebar::onItemClicked(int row) {
  m_currentPage = row;
  emit pageSelected(row);
}

void PageThumbnailSidebar::showItemContextMenu(const QPoint &pos) {
  auto *item = m_list->itemAt(pos);
  if (!item || !m_view)
    return;
  const int idx = item->data(Qt::UserRole).toInt();
  QList<BlopInWindowMenu::Item> items;
  items.push_back({QStringLiteral("Öffnen"), QIcon(), [this, idx]() {
                     emit pageSelected(idx);
                   }});
  items.push_back({m_view->isPageBookmarked(idx)
                       ? QStringLiteral("Lesezeichen entfernen")
                       : QStringLiteral("Lesezeichen setzen"),
                   QIcon(),
                   [this, idx]() {
                     m_view->togglePageBookmark(idx);
                     rebuild();
                     emit pagesMutated();
                   }});
  items.push_back({QStringLiteral("Duplizieren"), QIcon(), [this, idx]() {
                     m_view->duplicatePage(idx);
                     rebuild();
                     emit pagesMutated();
                   }});
  items.push_back({QStringLiteral("90° drehen"), QIcon(), [this, idx]() {
                     m_view->rotatePage(idx, 1);
                     rebuild();
                     emit pagesMutated();
                   }});
  items.push_back({QStringLiteral("Umbenennen…"), QIcon(), [this, idx]() {
                     const QString cur = m_view->pageTitle(idx);
                     const QString name = BlopDialogs::promptText(
                         this, QStringLiteral("Seite umbenennen"),
                         QStringLiteral("Name"), cur);
                     if (!name.isEmpty()) {
                       m_view->renamePage(idx, name);
                       rebuild();
                     }
                   }});
  items.push_back({QStringLiteral("Alle Seiten…"), QIcon(), [this]() {
                     emit openAllPagesRequested();
                   }});
  BlopInWindowMenu::Item sep;
  sep.separator = true;
  items.push_back(sep);
  items.push_back({QStringLiteral("Löschen"), QIcon(),
                   [this, idx]() {
                     if (m_view->pageCount() <= 1) {
                       BlopDialogs::notify(
                           this, QStringLiteral("Seite löschen"),
                           QStringLiteral("Mindestens eine Seite muss bleiben."));
                       return;
                     }
                     if (!BlopDialogs::confirm(
                             this, QStringLiteral("Seite löschen"),
                             QStringLiteral("Seite %1 wirklich löschen?")
                                 .arg(idx + 1),
                             QStringLiteral("Löschen"),
                             QStringLiteral("Abbrechen")))
                       return;
                     m_view->deletePage(idx);
                     rebuild();
                     emit pagesMutated();
                   },
                   true});
  BlopInWindowMenu::show(this, m_list->viewport()->mapToGlobal(pos), items);
}
