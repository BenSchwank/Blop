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
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

namespace {
struct RailMetrics {
  int width;
  int thumbW;
  int thumbH;
  int itemH;
  int pad;
};

RailMetrics railMetrics(QWidget *ref, bool twoCol) {
#ifdef Q_OS_ANDROID
  if (!UiScale::isAndroidTablet(ref)) {
    return {UiScale::dp(68), UiScale::dp(52), UiScale::dp(72), UiScale::dp(90),
            UiScale::dp(6)};
  }
#else
  Q_UNUSED(ref);
#endif
  if (twoCol) {
    return {UiScale::dp(196), UiScale::dp(78), UiScale::dp(104),
            UiScale::dp(124), UiScale::dp(8)};
  }
  return {UiScale::dp(120), UiScale::dp(92), UiScale::dp(120), UiScale::dp(142),
          UiScale::dp(10)};
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
} // namespace

PageThumbnailSidebar::PageThumbnailSidebar(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("PageThumbnailSidebar"));
  setAttribute(Qt::WA_StyledBackground, true);
  const RailMetrics m = railMetrics(this, m_twoColumn);
  m_expandedWidth = m.width;
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
  lay->setContentsMargins(m.pad, UiScale::dp(8), m.pad, UiScale::dp(10));
  lay->setSpacing(UiScale::dp(8));

  m_list = new QListWidget(m_railBody);
  m_list->setFrameStyle(QFrame::NoFrame);
  m_list->setSpacing(UiScale::dp(8));
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
            rebuild();
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
  m_btnAddPage->setIcon(
      railGlyph(QStringLiteral("add"), NoteChrome::textSecondary(),
                UiScale::dp(16)));
  m_btnAddPage->setIconSize(QSize(UiScale::dp(14), UiScale::dp(14)));
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
  lay->addLayout(footer);

  root->addWidget(m_railBody, 1);
  applyViewMode();
  refreshListStyle();
}

void PageThumbnailSidebar::setCollapsed(bool collapsed) {
  if (m_floatingMode)
    collapsed = false;
  if (m_collapsed == collapsed)
    return;
  m_collapsed = collapsed;
  applyCollapsedState();
  emit collapsedChanged(m_collapsed);
}

void PageThumbnailSidebar::toggleCollapsed() {
  if (m_floatingMode)
    return;
  setCollapsed(!m_collapsed);
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

void PageThumbnailSidebar::applyViewMode() {
  const RailMetrics m = railMetrics(this, m_twoColumn);
  m_expandedWidth = m.width;
  if (!m_collapsed)
    setFixedWidth(m_expandedWidth);
  if (!m_list)
    return;
  m_list->setIconSize(QSize(m.thumbW, m.thumbH));
  if (m_twoColumn) {
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(true);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(UiScale::dp(6));
  } else {
    m_list->setViewMode(QListView::ListMode);
    m_list->setFlow(QListView::TopToBottom);
    m_list->setWrapping(false);
    m_list->setSpacing(UiScale::dp(8));
  }
}

void PageThumbnailSidebar::applyCollapsedState() {
  if (m_railBody)
    m_railBody->setVisible(!m_collapsed);
  if (m_collapsed) {
    setFixedWidth(UiScale::dp(28));
    if (m_btnToggle) {
      m_btnToggle->setIcon(
          railGlyph(QStringLiteral("chevron_right"), NoteChrome::textSecondary(),
                    UiScale::dp(16)));
      m_btnToggle->setToolTip(QStringLiteral("Seitenmanager aufklappen"));
    }
  } else {
    if (m_expandedWidth <= 0)
      m_expandedWidth = railMetrics(this, m_twoColumn).width;
    setFixedWidth(m_expandedWidth);
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
  setStyleSheet(QStringLiteral(
      "QWidget#PageThumbnailSidebar {"
      "  background: %1; border: none; border-right: 1px solid %2;"
      "}"
      "QWidget#PageRailBody { background: transparent; border: none; }"
      "QPushButton#PageRailToggleBtn {"
      "  background: transparent; border: none; color: %3;"
      "  border-bottom: 1px solid %2;"
      "}"
      "QPushButton#PageRailToggleBtn:hover { background: rgba(127,127,127,0.16); }"
      "QPushButton#PageRailAddBtn, QPushButton#PageRailColsBtn {"
      "  background: %4; color: %3; border: 1px solid %2; border-radius: 4px;"
      "  font-size: 12px; font-weight: 600;"
      "}"
      "QPushButton#PageRailAddBtn:hover, QPushButton#PageRailColsBtn:hover {"
      "  border-color: %5; color: %6; background: rgba(127,127,127,0.14);"
      "}")
                    .arg(NoteChrome::toolbarFill().name(),
                         NoteChrome::borderSoft().name(),
                         NoteChrome::textSecondary().name(),
                         NoteChrome::panelElevated().name(), accent.name(),
                         NoteChrome::textPrimary().name()));

  if (m_btnAddPage) {
    m_btnAddPage->setIcon(
        railGlyph(QStringLiteral("add"), NoteChrome::textSecondary(),
                  UiScale::dp(16)));
  }
  if (m_btnColumns) {
    m_btnColumns->setIcon(
        railGlyph(m_twoColumn ? QStringLiteral("layout_single")
                              : QStringLiteral("layout_rows"),
                  NoteChrome::textSecondary(), UiScale::dp(16)));
  }

  if (m_list) {
    m_list->setStyleSheet(
        QStringLiteral(
            "QListWidget { background: transparent; border: none; outline: 0; color: %1; }"
            "QListWidget::item {"
            "  border: 1px solid %2; border-radius: 4px; padding: 2px;"
            "  background: %3; color: %1;"
            "}"
            "QListWidget::item:selected {"
            "  border: 1px solid %4; background: rgba(%5,%6,%7,0.16);"
            "}"
            "QListWidget::item:hover { background: rgba(255,255,255,0.06); }")
            .arg(NoteChrome::textSecondary().name(),
                 NoteChrome::borderSoft().name(),
                 NoteChrome::panelElevated().name(), accent.name(),
                 QString::number(accent.red()), QString::number(accent.green()),
                 QString::number(accent.blue())));
  }
}

void PageThumbnailSidebar::setNoteView(MultiPageNoteView *view) {
  m_view = view;
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
  if (on) {
    setCollapsed(false);
    if (m_btnToggle)
      m_btnToggle->hide();
    if (m_railBody)
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
  const RailMetrics m = railMetrics(this, m_twoColumn);
  const Note *note = m_view->note();
  const int count = note->pages.size();
  for (int i = 0; i < count; ++i) {
    auto *item = new QListWidgetItem(m_list);
    item->setTextAlignment(Qt::AlignCenter | Qt::AlignBottom);
    item->setSizeHint(QSize(m.thumbW + (m_twoColumn ? 0 : 0), m.itemH));
    QString label = QString::number(i + 1);
    item->setText(label);
    if (note->pages[i].bookmarked) {
      item->setForeground(NoteChrome::accent());
      item->setToolTip(QStringLiteral("Seite %1 · Lesezeichen").arg(i + 1));
      // Tiny bookmark ribbon as decoration overlay on the right of the label.
      item->setData(Qt::UserRole + 1, true);
    } else {
      item->setForeground(NoteChrome::textSecondary());
      item->setToolTip(QStringLiteral("Seite %1").arg(i + 1));
      item->setData(Qt::UserRole + 1, false);
    }
    item->setData(Qt::UserRole, i);
    m_list->addItem(item);
    requestThumbnail(i, item, epoch);
  }
  if (m_currentPage >= 0 && m_currentPage < count)
    m_list->setCurrentRow(m_currentPage);
}

void PageThumbnailSidebar::requestThumbnail(int pageIndex, QListWidgetItem *item,
                                            int epoch) {
  if (!m_view || !item)
    return;
  const RailMetrics m = railMetrics(this, m_twoColumn);
  m_view->generateThumbnailAsync(
      pageIndex, QSize(m.thumbW, m.thumbH),
      [this, item, epoch](const QPixmap &pm) {
        if (!item || m_rebuildEpoch != epoch)
          return;
        if (!pm.isNull())
          item->setIcon(QIcon(pm));
      });
}

void PageThumbnailSidebar::onCurrentPageChanged(int pageIndex) {
  m_currentPage = pageIndex;
  if (m_list && pageIndex >= 0 && pageIndex < m_list->count())
    m_list->setCurrentRow(pageIndex);
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
