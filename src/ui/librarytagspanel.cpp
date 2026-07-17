#include "librarytagspanel.h"

#include "blop_dialogs.h"
#include "blop_theme.h"
#include "librarytagstore.h"
#include "uiscale.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

LibraryTagsPanel::LibraryTagsPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryTagsPanel"));
  setAttribute(Qt::WA_StyledBackground, true);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(UiScale::dp(8), UiScale::dp(6), UiScale::dp(8),
                           UiScale::dp(8));
  root->setSpacing(UiScale::dp(6));

  m_header = new QWidget(this);
  auto *headerLay = new QHBoxLayout(m_header);
  headerLay->setContentsMargins(0, 0, 0, 0);
  headerLay->setSpacing(UiScale::dp(4));

  m_btnToggle = new QToolButton(m_header);
  m_btnToggle->setObjectName(QStringLiteral("libraryTagsToggle"));
  m_btnToggle->setText(QStringLiteral("▾"));
  m_btnToggle->setFixedSize(UiScale::dp(22), UiScale::dp(22));
  m_btnToggle->setCursor(Qt::PointingHandCursor);
  m_btnToggle->setAutoRaise(true);
  connect(m_btnToggle, &QToolButton::clicked, this,
          &LibraryTagsPanel::onToggleCollapsed);
  headerLay->addWidget(m_btnToggle);

  m_title = new QLabel(QStringLiteral("Tags"), m_header);
  m_title->setObjectName(QStringLiteral("libraryTagsTitle"));
  headerLay->addWidget(m_title, 1);
  root->addWidget(m_header);

  m_body = new QWidget(this);
  auto *bodyLay = new QVBoxLayout(m_body);
  bodyLay->setContentsMargins(0, 0, 0, 0);
  bodyLay->setSpacing(UiScale::dp(6));

  auto *addRow = new QHBoxLayout();
  addRow->setSpacing(UiScale::dp(4));
  m_input = new QLineEdit(m_body);
  m_input->setObjectName(QStringLiteral("libraryTagsInput"));
  m_input->setPlaceholderText(QStringLiteral("Tag hinzufügen…"));
  m_input->setClearButtonEnabled(true);
  connect(m_input, &QLineEdit::returnPressed, this, &LibraryTagsPanel::onAddClicked);
  addRow->addWidget(m_input, 1);

  m_btnAdd = new QPushButton(QStringLiteral("+"), m_body);
  m_btnAdd->setObjectName(QStringLiteral("libraryTagsBtnAdd"));
  m_btnAdd->setFixedSize(UiScale::dp(30), UiScale::dp(30));
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
  m_list->setMaximumHeight(UiScale::dp(160));
  m_list->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_list, &QListWidget::itemSelectionChanged, this,
          &LibraryTagsPanel::onSelectionChanged);
  connect(m_list, &QWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            QListWidgetItem *it = m_list->itemAt(pos);
            if (!it)
              return;
            m_list->setCurrentItem(it);
            QMenu menu(this);
            menu.addAction(QStringLiteral("Umbenennen"), this,
                           &LibraryTagsPanel::onRenameClicked);
            menu.addAction(QStringLiteral("Löschen"), this,
                           &LibraryTagsPanel::onDeleteClicked);
            menu.exec(m_list->mapToGlobal(pos));
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
  refreshTheme();
  reload();
}

void LibraryTagsPanel::setSidebarMode(bool on) {
  m_sidebarMode = on;
  setMinimumWidth(0);
  setMaximumWidth(QWIDGETSIZE_MAX);
  setSizePolicy(QSizePolicy::Expanding,
                on ? QSizePolicy::Maximum : QSizePolicy::Expanding);
  if (m_list)
    m_list->setMaximumHeight(on ? UiScale::dp(140) : QWIDGETSIZE_MAX);
  refreshTheme();
}

void LibraryTagsPanel::setAccentColor(const QColor &color) {
  if (!color.isValid())
    return;
  m_accent = color;
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
  m_collapsed = !m_collapsed;
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
  m_list->clear();
  for (const QString &tag : catalog) {
    auto *item = new QListWidgetItem(tag, m_list);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    for (const QString &sel : preserveSelection) {
      if (sel.compare(tag, Qt::CaseInsensitive) == 0) {
        item->setSelected(true);
        break;
      }
    }
  }
}

void LibraryTagsPanel::refreshTheme() {
  const QString text = BlopTheme::textPrimary().name(QColor::HexRgb);
  const QString muted = BlopTheme::textSecondary().name(QColor::HexRgb);
  const QString border = BlopTheme::borderSubtle().name(QColor::HexArgb);
  const QString accent = m_accent.name(QColor::HexRgb);
  const QString bg = m_sidebarMode ? QStringLiteral("transparent")
                                   : BlopTheme::surfaceMuted().name(QColor::HexRgb);

  setStyleSheet(QStringLiteral(
      "QWidget#LibraryTagsPanel {"
      "  background: %1;"
      "  border-top: 1px solid %2;"
      "}"
      "QLabel#libraryTagsTitle {"
      "  color: %3; font-size: 11px; font-weight: 700;"
      "  letter-spacing: 0.6px; text-transform: uppercase;"
      "  background: transparent;"
      "}"
      "QToolButton#libraryTagsToggle {"
      "  background: transparent; border: none; color: %4; font-size: 12px;"
      "}"
      "QLineEdit#libraryTagsInput {"
      "  background: rgba(255,255,255,0.04); color: %3; border: 1px solid %2;"
      "  border-radius: 8px; padding: 0 8px; min-height: 28px; font-size: 11px;"
      "}"
      "QLineEdit#libraryTagsInput:focus { border: 1px solid %5; }"
      "QPushButton#libraryTagsBtnAdd {"
      "  background: rgba(%6,%7,%8,0.20); color: %3; border: 1px solid %5;"
      "  border-radius: 8px; font-size: 16px; font-weight: 700;"
      "}"
      "QPushButton#libraryTagsBtnGhost {"
      "  background: transparent; color: %4; border: none;"
      "  font-size: 11px; text-align: left; padding: 2px 0;"
      "}"
      "QListWidget#libraryTagsList {"
      "  background: transparent; border: none; outline: none; color: %3;"
      "  font-size: 12px;"
      "}"
      "QListWidget#libraryTagsList::item {"
      "  background: transparent; border: none; border-radius: 8px;"
      "  padding: 6px 8px; margin: 1px 0;"
      "}"
      "QListWidget#libraryTagsList::item:selected {"
      "  background: rgba(%6,%7,%8,0.20); color: %3;"
      "}"
      "QListWidget#libraryTagsList::item:hover:!selected {"
      "  background: rgba(255,255,255,0.05);"
      "}")
                    .arg(bg, border, text, muted, accent)
                    .arg(m_accent.red())
                    .arg(m_accent.green())
                    .arg(m_accent.blue()));
}
