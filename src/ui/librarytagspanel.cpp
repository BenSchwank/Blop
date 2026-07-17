#include "librarytagspanel.h"

#include "blop_theme.h"
#include "librarytagstore.h"
#include "uiscale.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

LibraryTagsPanel::LibraryTagsPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryTagsPanel"));
  setAttribute(Qt::WA_StyledBackground, true);
  setMinimumWidth(UiScale::dp(200));
  setMaximumWidth(UiScale::dp(280));
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(UiScale::dp(14), UiScale::dp(12), UiScale::dp(14),
                           UiScale::dp(14));
  root->setSpacing(UiScale::dp(10));

  m_title = new QLabel(QStringLiteral("Tags"), this);
  m_title->setObjectName(QStringLiteral("libraryTagsTitle"));
  root->addWidget(m_title);

  m_hint = new QLabel(
      QStringLiteral("Ordne Notizen mit Tags. Mehrfachauswahl filtert die Bibliothek."),
      this);
  m_hint->setObjectName(QStringLiteral("libraryTagsHint"));
  m_hint->setWordWrap(true);
  root->addWidget(m_hint);

  m_input = new QLineEdit(this);
  m_input->setObjectName(QStringLiteral("libraryTagsInput"));
  m_input->setPlaceholderText(QStringLiteral("Neuer Tag…"));
  m_input->setClearButtonEnabled(true);
  connect(m_input, &QLineEdit::returnPressed, this, &LibraryTagsPanel::onAddClicked);
  root->addWidget(m_input);

  auto *actions = new QHBoxLayout();
  actions->setContentsMargins(0, 0, 0, 0);
  actions->setSpacing(UiScale::dp(6));

  m_btnAdd = new QPushButton(QStringLiteral("Hinzufügen"), this);
  m_btnAdd->setObjectName(QStringLiteral("libraryTagsBtnAdd"));
  m_btnAdd->setCursor(Qt::PointingHandCursor);
  connect(m_btnAdd, &QPushButton::clicked, this, &LibraryTagsPanel::onAddClicked);
  actions->addWidget(m_btnAdd);

  m_btnRename = new QPushButton(QStringLiteral("Umbenennen"), this);
  m_btnRename->setObjectName(QStringLiteral("libraryTagsBtnGhost"));
  m_btnRename->setCursor(Qt::PointingHandCursor);
  connect(m_btnRename, &QPushButton::clicked, this, &LibraryTagsPanel::onRenameClicked);
  actions->addWidget(m_btnRename);

  m_btnDelete = new QPushButton(QStringLiteral("Löschen"), this);
  m_btnDelete->setObjectName(QStringLiteral("libraryTagsBtnGhost"));
  m_btnDelete->setCursor(Qt::PointingHandCursor);
  connect(m_btnDelete, &QPushButton::clicked, this, &LibraryTagsPanel::onDeleteClicked);
  actions->addWidget(m_btnDelete);
  root->addLayout(actions);

  m_list = new QListWidget(this);
  m_list->setObjectName(QStringLiteral("libraryTagsList"));
  m_list->setSelectionMode(QAbstractItemView::MultiSelection);
  m_list->setSpacing(UiScale::dp(2));
  m_list->setFrameShape(QFrame::NoFrame);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  connect(m_list, &QListWidget::itemSelectionChanged, this,
          &LibraryTagsPanel::onSelectionChanged);
  root->addWidget(m_list, 1);

  refreshTheme();
  reload();
}

void LibraryTagsPanel::setAccentColor(const QColor &color) {
  if (!color.isValid())
    return;
  m_accent = color;
  refreshTheme();
}

void LibraryTagsPanel::reload() {
  // Seed a tiny starter catalog once so the shelf never looks unfinished.
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
  bool ok = false;
  const QString to = QInputDialog::getText(
      this, QStringLiteral("Tag umbenennen"), QStringLiteral("Neuer Name:"),
      QLineEdit::Normal, from, &ok);
  if (!ok)
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
  const auto answer = QMessageBox::question(
      this, QStringLiteral("Tags löschen"),
      QStringLiteral("Ausgewählte Tags aus dem Katalog und allen Notizen entfernen?"),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (answer != QMessageBox::Yes)
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
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    for (const QString &sel : preserveSelection) {
      if (sel.compare(tag, Qt::CaseInsensitive) == 0) {
        item->setSelected(true);
        break;
      }
    }
  }
}

void LibraryTagsPanel::refreshTheme() {
  const bool dark = BlopTheme::instance().isDark();
  const QString bg = BlopTheme::surfaceMuted().name(QColor::HexRgb);
  const QString panelBg = dark ? QStringLiteral("#12101C")
                               : BlopTheme::surfaceBase().name(QColor::HexRgb);
  const QString text = BlopTheme::textPrimary().name(QColor::HexRgb);
  const QString muted = BlopTheme::textSecondary().name(QColor::HexRgb);
  const QString border = BlopTheme::borderSubtle().name(QColor::HexRgb);
  const QString accent = m_accent.name(QColor::HexRgb);
  const int radius = UiScale::dp(BlopTheme::r16);

  setStyleSheet(QStringLiteral(
      "QWidget#LibraryTagsPanel {"
      "  background: %1;"
      "  border-left: 1px solid %2;"
      "}"
      "QLabel#libraryTagsTitle {"
      "  color: %3; font-size: 15px; font-weight: 700;"
      "  letter-spacing: -0.2px; background: transparent;"
      "}"
      "QLabel#libraryTagsHint {"
      "  color: %4; font-size: 11px; font-weight: 500;"
      "  background: transparent;"
      "}"
      "QLineEdit#libraryTagsInput {"
      "  background: %5; color: %3; border: 1px solid %2;"
      "  border-radius: 12px; padding: 0 12px; min-height: 34px;"
      "  font-size: 12px;"
      "}"
      "QLineEdit#libraryTagsInput:focus { border: 1px solid %6; }"
      "QPushButton#libraryTagsBtnAdd {"
      "  background: transparent; color: %3; border: 1px solid %6;"
      "  border-radius: 10px; padding: 6px 10px; font-size: 11px; font-weight: 600;"
      "}"
      "QPushButton#libraryTagsBtnAdd:hover { background: rgba(124,92,252,0.14); }"
      "QPushButton#libraryTagsBtnGhost {"
      "  background: transparent; color: %4; border: 1px solid %2;"
      "  border-radius: 10px; padding: 6px 8px; font-size: 11px;"
      "}"
      "QPushButton#libraryTagsBtnGhost:hover { color: %3; border-color: %6; }"
      "QListWidget#libraryTagsList {"
      "  background: transparent; border: none; outline: none; color: %3;"
      "  font-size: 12px;"
      "}"
      "QListWidget#libraryTagsList::item {"
      "  background: %5; border: 1px solid %2; border-radius: 12px;"
      "  padding: 8px 10px; margin: 2px 0;"
      "}"
      "QListWidget#libraryTagsList::item:selected {"
      "  background: rgba(124,92,252,0.18); border: 1px solid %6; color: %3;"
      "}"
      "QListWidget#libraryTagsList::item:hover:!selected {"
      "  border-color: %6;"
      "}")
                    .arg(panelBg, border, text, muted, bg, accent));
  Q_UNUSED(radius);
}
