#include "librarytagspanel.h"

#include "blop_dialogs.h"
#include "blop_inwindow_menu.h"
#include "blop_scroll.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "librarytagstore.h"
#include "uiscale.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

class TagRowDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  QColor accent{QColor(QStringLiteral("#5B9DFF"))};

  QSize sizeHint(const QStyleOptionViewItem &,
                 const QModelIndex &) const override {
    return QSize(10, UiScale::dp(24));
  }

  void paint(QPainter *p, const QStyleOptionViewItem &opt,
             const QModelIndex &idx) const override {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    QRect r = opt.rect.adjusted(2, 1, -2, -1);
    const bool dark = BlopTheme::instance().isDark();
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered = opt.state & QStyle::State_MouseOver;

    const qreal rad = UiScale::dp(8);
    if (selected) {
      QColor fill = accent;
      fill.setAlpha(dark ? 56 : 40);
      p->setPen(QPen(accent, 1.0));
      p->setBrush(fill);
      p->drawRoundedRect(r, rad, rad);
    } else if (hovered) {
      p->setPen(Qt::NoPen);
      p->setBrush(dark ? QColor(255, 255, 255, 18) : BlopStyle::paperHover());
      p->drawRoundedRect(r, rad, rad);
    }

    const QString name = idx.data(Qt::DisplayRole).toString();
    const int count = idx.data(Qt::UserRole).toInt();
    const QString countStr = QString::number(count);

    QFont f = opt.font;
    f.setPixelSize(UiScale::sp(11));
    f.setWeight(QFont::DemiBold);
    p->setFont(f);
    QFontMetrics fm(f);
    const int countW = fm.horizontalAdvance(countStr) + UiScale::dp(10);
    const QColor ink =
        dark ? BlopTheme::textPrimary() : BlopStyle::paperInk();
    const QColor muted =
        dark ? BlopTheme::textTertiary() : BlopStyle::paperInkMuted();

    QRect nameR = r.adjusted(UiScale::dp(8), 0, -countW - UiScale::dp(8), 0);
    p->setPen(ink);
    p->drawText(nameR, Qt::AlignVCenter | Qt::AlignLeft,
                fm.elidedText(name, Qt::ElideRight, nameR.width()));

    QRect badge(r.right() - countW - UiScale::dp(4),
                r.center().y() - UiScale::dp(8), countW, UiScale::dp(16));
    p->setPen(Qt::NoPen);
    p->setBrush(dark ? QColor(255, 255, 255, 16) : BlopStyle::paperChipBg());
    p->drawRoundedRect(badge, UiScale::dp(8) / 2.0, UiScale::dp(8) / 2.0);
    QFont bf = f;
    bf.setPixelSize(UiScale::sp(10));
    bf.setWeight(QFont::Medium);
    p->setFont(bf);
    p->setPen(muted);
    p->drawText(badge, Qt::AlignCenter, countStr);
    p->restore();
  }
};

} // namespace

LibraryTagsPanel::LibraryTagsPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryTagsPanel"));
  setAttribute(Qt::WA_StyledBackground, true);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(UiScale::dp(6), UiScale::dp(4), UiScale::dp(6),
                           UiScale::dp(6));
  root->setSpacing(UiScale::dp(4));

  m_header = new QWidget(this);
  auto *headerLay = new QHBoxLayout(m_header);
  headerLay->setContentsMargins(0, 0, 0, 0);
  headerLay->setSpacing(UiScale::dp(4));

  m_btnToggle = new QToolButton(m_header);
  m_btnToggle->setObjectName(QStringLiteral("libraryTagsToggle"));
  m_btnToggle->setText(QStringLiteral("▾"));
  m_btnToggle->setFixedSize(UiScale::dp(18), UiScale::dp(18));
  m_btnToggle->setCursor(Qt::PointingHandCursor);
  m_btnToggle->setAutoRaise(true);
  connect(m_btnToggle, &QToolButton::clicked, this,
          &LibraryTagsPanel::onToggleCollapsed);
  headerLay->addWidget(m_btnToggle);

  m_title = new QLabel(QStringLiteral("TAGS"), m_header);
  m_title->setObjectName(QStringLiteral("libraryTagsTitle"));
  headerLay->addWidget(m_title, 1);
  root->addWidget(m_header);

  m_body = new QWidget(this);
  auto *bodyLay = new QVBoxLayout(m_body);
  bodyLay->setContentsMargins(0, 0, 0, 0);
  bodyLay->setSpacing(UiScale::dp(4));

  auto *addRow = new QHBoxLayout();
  addRow->setSpacing(UiScale::dp(4));
  m_input = new QLineEdit(m_body);
  m_input->setObjectName(QStringLiteral("libraryTagsInput"));
  m_input->setPlaceholderText(QStringLiteral("Tag hinzufügen…"));
  m_input->setClearButtonEnabled(true);
  m_input->setMinimumWidth(0);
  m_input->setFixedHeight(UiScale::dp(26));
  m_input->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  connect(m_input, &QLineEdit::returnPressed, this, &LibraryTagsPanel::onAddClicked);
  addRow->addWidget(m_input, 1);

  m_btnAdd = new QPushButton(QStringLiteral("+"), m_body);
  m_btnAdd->setObjectName(QStringLiteral("libraryTagsBtnAdd"));
  m_btnAdd->setFixedSize(UiScale::dp(24), UiScale::dp(24));
  m_btnAdd->setCursor(Qt::PointingHandCursor);
  m_btnAdd->setToolTip(QStringLiteral("Tag anlegen"));
  connect(m_btnAdd, &QPushButton::clicked, this, &LibraryTagsPanel::onAddClicked);
  addRow->addWidget(m_btnAdd);
  bodyLay->addLayout(addRow);

  m_list = new QListWidget(m_body);
  m_list->setObjectName(QStringLiteral("libraryTagsList"));
  m_list->setSelectionMode(QAbstractItemView::MultiSelection);
  m_list->setSpacing(0);
  m_list->setFrameShape(QFrame::NoFrame);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_list->setContextMenuPolicy(Qt::CustomContextMenu);
  m_list->setItemDelegate(new TagRowDelegate(m_list));
  connect(m_list, &QListWidget::itemSelectionChanged, this,
          &LibraryTagsPanel::onSelectionChanged);
  connect(m_list, &QWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            QListWidgetItem *it = m_list->itemAt(pos);
            if (!it)
              return;
            m_list->setCurrentItem(it);
            const QPoint globalPos = m_list->mapToGlobal(pos);
#ifdef Q_OS_ANDROID
            QList<BlopInWindowMenu::Item> items;
            items.append({QStringLiteral("Umbenennen"), QIcon(),
                          [this]() { onRenameClicked(); }, false, false});
            items.append({QStringLiteral("Löschen"), QIcon(),
                          [this]() { onDeleteClicked(); }, true, false});
            BlopInWindowMenu::show(window() ? window() : this, globalPos, items);
#else
            QMenu menu(this);
            menu.addAction(QStringLiteral("Umbenennen"), this,
                           &LibraryTagsPanel::onRenameClicked);
            menu.addAction(QStringLiteral("Löschen"), this,
                           &LibraryTagsPanel::onDeleteClicked);
            menu.exec(globalPos);
#endif
          });
  bodyLay->addWidget(m_list, 1);

  m_btnManage = new QPushButton(QStringLiteral("Auswahl leeren"), m_body);
  m_btnManage->setObjectName(QStringLiteral("libraryTagsBtnGhost"));
  m_btnManage->setCursor(Qt::PointingHandCursor);
  m_btnManage->setFlat(true);
  connect(m_btnManage, &QPushButton::clicked, this, [this]() {
    clearSelection();
    emit filterChanged(selectedTags());
  });
  bodyLay->addWidget(m_btnManage);

  root->addWidget(m_body, 1);
  m_collapsed = true;
  applyCollapsed();
  refreshTheme();
  reload();
}

void LibraryTagsPanel::setSidebarMode(bool on) {
  m_sidebarMode = on;
  setMinimumWidth(0);
  setMaximumWidth(QWIDGETSIZE_MAX);
  setSizePolicy(QSizePolicy::Expanding,
                on ? QSizePolicy::Maximum : QSizePolicy::Expanding);
  if (auto *root = qobject_cast<QVBoxLayout *>(layout())) {
    if (on)
      root->setContentsMargins(UiScale::dp(4), UiScale::dp(2), UiScale::dp(4),
                               UiScale::dp(4));
    else
      root->setContentsMargins(UiScale::dp(6), UiScale::dp(4), UiScale::dp(6),
                               UiScale::dp(6));
  }
  if (m_btnAdd)
    m_btnAdd->setFixedSize(on ? UiScale::dp(22) : UiScale::dp(24),
                           on ? UiScale::dp(22) : UiScale::dp(24));
  if (m_input) {
    m_input->setMinimumWidth(0);
    m_input->setFixedHeight(UiScale::dp(on ? 24 : 26));
    m_input->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  }
  if (m_list) {
    if (on)
      BlopScroll::makeListFitContents(m_list);
    else {
      m_list->setMinimumHeight(0);
      m_list->setMaximumHeight(QWIDGETSIZE_MAX);
      m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
  }
  refreshTheme();
}

void LibraryTagsPanel::setAccentColor(const QColor &color) {
  if (!color.isValid())
    return;
  m_accent = color;
  if (m_list) {
    if (auto *d = dynamic_cast<TagRowDelegate *>(m_list->itemDelegate()))
      d->accent = color;
  }
  refreshTheme();
}

void LibraryTagsPanel::reload() {
  if (LibraryTagStore::catalog().isEmpty()) {
    LibraryTagStore::addTagToCatalog(QStringLiteral("Projekt"));
    LibraryTagStore::addTagToCatalog(QStringLiteral("Entwurf"));
  }
  rebuildList(selectedTags());
}

QStringList LibraryTagsPanel::selectedTags() const {
  QStringList out;
  if (!m_list)
    return out;
  const auto items = m_list->selectedItems();
  for (const QListWidgetItem *it : items) {
    if (it)
      out.append(it->text());
  }
  return out;
}

void LibraryTagsPanel::clearSelection() {
  if (m_list)
    m_list->clearSelection();
}

void LibraryTagsPanel::onToggleCollapsed() {
  setCollapsed(!m_collapsed);
}

void LibraryTagsPanel::setCollapsed(bool on) {
  if (m_collapsed == on)
    return;
  m_collapsed = on;
  applyCollapsed();
}

void LibraryTagsPanel::applyCollapsed() {
  if (m_body)
    m_body->setVisible(!m_collapsed);
  if (m_btnToggle)
    m_btnToggle->setText(m_collapsed ? QStringLiteral("▸") : QStringLiteral("▾"));
}

void LibraryTagsPanel::onAddClicked() {
  const QString raw = m_input ? m_input->text() : QString();
  if (!LibraryTagStore::addTagToCatalog(raw))
    return;
  if (m_input)
    m_input->clear();
  rebuildList(selectedTags());
  emit catalogChanged();
}

void LibraryTagsPanel::onRenameClicked() {
  if (!m_list)
    return;
  const auto selected = m_list->selectedItems();
  if (selected.size() != 1)
    return;
  const QString from = selected.first()->text();
  const QString to = BlopDialogs::promptText(
      this, QStringLiteral("Tag umbenennen"), QStringLiteral("Neuer Name:"), from);
  if (to.isEmpty())
    return;
  if (!LibraryTagStore::renameTag(from, to))
    return;
  rebuildList({LibraryTagStore::normalize(to)});
  emit catalogChanged();
  emit filterChanged(selectedTags());
}

void LibraryTagsPanel::onDeleteClicked() {
  if (!m_list)
    return;
  const auto selected = m_list->selectedItems();
  if (selected.isEmpty())
    return;
  if (!BlopDialogs::confirm(
          this, QStringLiteral("Tags löschen"),
          QStringLiteral(
              "Ausgewählte Tags aus dem Katalog und allen Notizen entfernen?"),
          QStringLiteral("Löschen"), QStringLiteral("Abbrechen")))
    return;
  for (const QListWidgetItem *it : selected) {
    if (it)
      LibraryTagStore::removeTag(it->text());
  }
  rebuildList();
  emit catalogChanged();
  emit filterChanged(selectedTags());
}

void LibraryTagsPanel::onSelectionChanged() {
  emit filterChanged(selectedTags());
}

void LibraryTagsPanel::rebuildList(const QStringList &preserveSelection) {
  if (!m_list)
    return;
  const QStringList catalog = LibraryTagStore::catalog();
  const QHash<QString, int> counts = LibraryTagStore::usageCounts();
  m_list->clear();
  for (const QString &tag : catalog) {
    auto *item = new QListWidgetItem(tag, m_list);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(Qt::UserRole, counts.value(tag, 0));
    item->setToolTip(QStringLiteral("%1 · %2× verwendet")
                         .arg(tag)
                         .arg(counts.value(tag, 0)));
    for (const QString &sel : preserveSelection) {
      if (sel.compare(tag, Qt::CaseInsensitive) == 0) {
        item->setSelected(true);
        break;
      }
    }
  }
  if (m_sidebarMode)
    BlopScroll::makeListFitContents(m_list);
}

void LibraryTagsPanel::refreshTheme() {
  const bool dark = BlopTheme::instance().isDark();
  const QString text =
      dark ? BlopTheme::textPrimary().name(QColor::HexRgb)
           : BlopStyle::paperInk().name(QColor::HexRgb);
  const QString muted =
      dark ? BlopTheme::textSecondary().name(QColor::HexRgb)
           : BlopStyle::paperInkMuted().name(QColor::HexRgb);
  const QString border =
      dark ? QStringLiteral("rgba(255,255,255,0.10)")
           : BlopStyle::paperBorder().name(QColor::HexRgb);
  const QString accent = m_accent.name(QColor::HexRgb);
  const QString bg = m_sidebarMode ? QStringLiteral("transparent")
                                   : BlopTheme::surfaceMuted().name(QColor::HexRgb);
  const QString inputBg =
      dark ? QStringLiteral("rgba(255,255,255,0.06)")
           : BlopStyle::libraryMain().name(QColor::HexRgb);

  if (m_list) {
    if (auto *d = dynamic_cast<TagRowDelegate *>(m_list->itemDelegate()))
      d->accent = m_accent;
  }

  const int rad = UiScale::dp(8);
  setStyleSheet(QStringLiteral(
                    "QWidget#LibraryTagsPanel {"
                    "  background: %1;"
                    "  border: none;"
                    "}"
                    "QLabel#libraryTagsTitle {"
                    "  color: %3; font-size: 10px; font-weight: 700;"
                    "  letter-spacing: 0.8px;"
                    "  background: transparent;"
                    "}"
                    "QToolButton#libraryTagsToggle {"
                    "  background: transparent; border: none; color: %4; font-size: 11px;"
                    "}"
                    "QLineEdit#libraryTagsInput {"
                    "  background: %6; color: %3; border: 1px solid %2;"
                    "  border-radius: %10px; padding: 0 6px; font-size: 11px;"
                    "}"
                    "QLineEdit#libraryTagsInput:focus { border: 1px solid %5; }"
                    "QPushButton#libraryTagsBtnAdd {"
                    "  background: rgba(%7,%8,%9,0.18); color: %3; border: 1px solid %5;"
                    "  border-radius: %10px; font-size: 14px; font-weight: 700;"
                    "}"
                    "QPushButton#libraryTagsBtnGhost {"
                    "  background: transparent; color: %4; border: none;"
                    "  font-size: 10px; text-align: left; padding: 1px 0;"
                    "}"
                    "QListWidget#libraryTagsList {"
                    "  background: transparent; border: none; outline: none;"
                    "}")
                    .arg(bg, border, text, muted, accent, inputBg)
                    .arg(m_accent.red())
                    .arg(m_accent.green())
                    .arg(m_accent.blue())
                    .arg(rad));
}
