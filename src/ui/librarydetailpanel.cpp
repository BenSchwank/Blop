#include "librarydetailpanel.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "librarytagstore.h"
#include "notepreviewicon.h"
#include "uiscale.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString humanSize(qint64 bytes) {
  if (bytes < 1024)
    return QStringLiteral("%1 B").arg(bytes);
  const double kb = bytes / 1024.0;
  if (kb < 1024)
    return QStringLiteral("%1 KB").arg(kb, 0, 'f', kb < 10 ? 1 : 0);
  return QStringLiteral("%1 MB").arg(kb / 1024.0, 0, 'f', 1);
}

QString kindLabel(const QFileInfo &info) {
  if (info.isDir())
    return QStringLiteral("Ordner");
  const QString ext = info.suffix().toLower();
  if (ext == QLatin1String("blop"))
    return QStringLiteral("Unendlich");
  if (ext == QLatin1String("bnote"))
    return QStringLiteral("DIN A4");
  if (ext == QLatin1String("struct"))
    return QStringLiteral("Struktur");
  if (ext.isEmpty())
    return QStringLiteral("Datei");
  return ext.toUpper();
}
} // namespace

LibraryDetailPanel::LibraryDetailPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryDetailPanel"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedWidth(UiScale::dp(280));

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(UiScale::dp(16), UiScale::dp(18), UiScale::dp(16),
                           UiScale::dp(16));
  root->setSpacing(UiScale::dp(10));

  m_empty = new QLabel(QStringLiteral("Wähle eine Notiz oder einen Ordner."), this);
  m_empty->setWordWrap(true);
  m_empty->setAlignment(Qt::AlignTop);
  root->addWidget(m_empty);
  root->addStretch(1);

  m_body = new QWidget(this);
  m_body->hide();
  auto *lay = new QVBoxLayout(m_body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(10));

  m_preview = new QLabel(m_body);
  m_preview->setAlignment(Qt::AlignCenter);
  lay->addWidget(m_preview, 0, Qt::AlignHCenter);

  m_name = new QLabel(m_body);
  m_name->setWordWrap(true);
  m_name->setObjectName(QStringLiteral("LibraryDetailName"));
  lay->addWidget(m_name);

  m_meta = new QLabel(m_body);
  m_meta->setObjectName(QStringLiteral("LibraryDetailMeta"));
  lay->addWidget(m_meta);

  m_btnOpen = new QPushButton(QStringLiteral("Öffnen"), m_body);
  m_btnOpen->setCursor(Qt::PointingHandCursor);
  m_btnOpen->setMinimumHeight(UiScale::dp(36));
  connect(m_btnOpen, &QPushButton::clicked, this, [this]() {
    if (!m_path.isEmpty())
      emit openRequested(m_path);
  });
  lay->addWidget(m_btnOpen);

  auto *btnRename = new QPushButton(QStringLiteral("Umbenennen"), m_body);
  btnRename->setCursor(Qt::PointingHandCursor);
  btnRename->setFlat(true);
  connect(btnRename, &QPushButton::clicked, this, [this]() {
    if (!m_path.isEmpty())
      emit renameRequested(m_path);
  });
  lay->addWidget(btnRename, 0, Qt::AlignLeft);

  auto *btnDelete = new QPushButton(QStringLiteral("Löschen"), m_body);
  btnDelete->setCursor(Qt::PointingHandCursor);
  btnDelete->setFlat(true);
  connect(btnDelete, &QPushButton::clicked, this, [this]() {
    if (!m_path.isEmpty())
      emit deleteRequested(m_path);
  });
  lay->addWidget(btnDelete, 0, Qt::AlignLeft);

  auto *tagTitle = new QLabel(QStringLiteral("Tags"), m_body);
  tagTitle->setObjectName(QStringLiteral("LibraryDetailTagTitle"));
  lay->addWidget(tagTitle);

  m_tagHost = new QWidget(m_body);
  m_tagLay = new QVBoxLayout(m_tagHost);
  m_tagLay->setContentsMargins(0, 0, 0, 0);
  m_tagLay->setSpacing(UiScale::dp(6));
  lay->addWidget(m_tagHost);

  m_tagInput = new QLineEdit(m_body);
  m_tagInput->setPlaceholderText(QStringLiteral("Tag hinzufügen…"));
  m_tagInput->setMinimumHeight(UiScale::dp(32));
  connect(m_tagInput, &QLineEdit::returnPressed, this,
          &LibraryDetailPanel::addTagFromInput);
  lay->addWidget(m_tagInput);

  m_when = new QLabel(m_body);
  m_when->setWordWrap(true);
  m_when->setObjectName(QStringLiteral("LibraryDetailWhen"));
  lay->addWidget(m_when);
  lay->addStretch(1);

  root->addWidget(m_body, 1);
  applyChrome();
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
          [this]() { applyChrome(); });
}

void LibraryDetailPanel::setPath(const QString &absolutePath) {
  m_path = absolutePath;
  refresh();
}

void LibraryDetailPanel::refresh() {
  const QFileInfo info(m_path);
  const bool ok = !m_path.isEmpty() && info.exists();
  m_empty->setVisible(!ok);
  m_body->setVisible(ok);
  if (!ok)
    return;

  const int px = UiScale::dp(160);
  m_preview->setPixmap(NotePreviewIcon::pixmapForPath(m_path, info.isDir(), px));
  m_name->setText(info.fileName());
  if (info.isDir()) {
    m_meta->setText(QStringLiteral("Ordner"));
  } else {
    m_meta->setText(QStringLiteral("%1 · %2")
                        .arg(kindLabel(info), humanSize(info.size())));
  }
  m_btnOpen->setText(info.isDir() ? QStringLiteral("Öffnen")
                                  : QStringLiteral("Öffnen"));
  const QDateTime edited = info.lastModified();
  m_when->setText(edited.isValid()
                      ? QStringLiteral("Geändert %1")
                            .arg(edited.toString(QStringLiteral("dd.MM.yyyy HH:mm")))
                      : QString());
  rebuildTagChips();
  applyChrome();
}

void LibraryDetailPanel::addTagFromInput() {
  if (m_path.isEmpty() || !m_tagInput)
    return;
  const QString n = LibraryTagStore::normalize(m_tagInput->text());
  if (n.isEmpty())
    return;
  LibraryTagStore::addTagToCatalog(m_tagInput->text());
  QStringList tags = LibraryTagStore::tagsForPath(m_path);
  if (!tags.contains(n))
    tags.append(n);
  LibraryTagStore::setTagsForPath(m_path, tags);
  m_tagInput->clear();
  rebuildTagChips();
  applyChrome();
}

void LibraryDetailPanel::rebuildTagChips() {
  if (!m_tagLay)
    return;
  while (QLayoutItem *item = m_tagLay->takeAt(0)) {
    if (item->widget())
      item->widget()->deleteLater();
    delete item;
  }
  const QStringList tags = LibraryTagStore::tagsForPath(m_path);
  if (tags.isEmpty()) {
    auto *none = new QLabel(QStringLiteral("Noch keine Tags"), m_tagHost);
    none->setObjectName(QStringLiteral("LibraryDetailMeta"));
    m_tagLay->addWidget(none);
    return;
  }
  auto *row = new QWidget(m_tagHost);
  auto *rowLay = new QHBoxLayout(row);
  rowLay->setContentsMargins(0, 0, 0, 0);
  rowLay->setSpacing(UiScale::dp(6));
  for (const QString &tag : tags) {
    auto *chip = new QToolButton(row);
    chip->setText(tag);
    chip->setToolTip(QStringLiteral("Tag entfernen"));
    chip->setCursor(Qt::PointingHandCursor);
    chip->setAutoRaise(true);
    connect(chip, &QToolButton::clicked, this, [this, tag]() {
      QStringList next = LibraryTagStore::tagsForPath(m_path);
      next.removeAll(tag);
      LibraryTagStore::setTagsForPath(m_path, next);
      rebuildTagChips();
      applyChrome();
    });
    rowLay->addWidget(chip);
  }
  rowLay->addStretch(1);
  m_tagLay->addWidget(row);
}

void LibraryDetailPanel::applyChrome() {
  const bool dark = BlopTheme::instance().isDark();
  const QString bg = BlopStyle::librarySidebar().name(QColor::HexRgb);
  const QString ink = dark ? BlopStyle::obsidianText().name(QColor::HexRgb)
                           : QStringLiteral("#12141A");
  const QString muted = dark ? QStringLiteral("#9AA3B2") : QStringLiteral("#5C6370");
  const QString acc = BlopTheme::accentPrimary().name(QColor::HexRgb);
  const QString border = dark ? QStringLiteral("rgba(255,255,255,0.08)")
                              : QStringLiteral("rgba(20,24,40,0.08)");
  setStyleSheet(QStringLiteral(
      "QWidget#LibraryDetailPanel {"
      "  background: %1; border-left: 1px solid %2;"
      "}"
      "QLabel { color: %3; background: transparent; }"
      "QLabel#LibraryDetailName { font-size: 15px; font-weight: 700; }"
      "QLabel#LibraryDetailMeta, QLabel#LibraryDetailWhen,"
      "QLabel#LibraryDetailTagTitle { color: %4; font-size: 12px; font-weight: 600; }"
      "QLineEdit {"
      "  background: %5; color: %3; border: 1px solid %2;"
      "  border-radius: 8px; padding: 0 10px; font-size: 12px;"
      "}"
      "QPushButton {"
      "  background: transparent; color: %3; border: none;"
      "  text-align: left; font-size: 13px; font-weight: 600; padding: 4px 0;"
      "}"
      "QPushButton:hover { color: %6; }"
      "QToolButton {"
      "  background: %5; color: %3; border: 1px solid %2;"
      "  border-radius: 8px; padding: 4px 8px; font-size: 12px; font-weight: 600;"
      "}"
      "QToolButton:hover { border-color: %6; color: %6; }")
                    .arg(bg, border, ink, muted,
                         dark ? QStringLiteral("#242833") : QStringLiteral("#FFFFFF"),
                         acc));
  if (m_btnOpen) {
    m_btnOpen->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1; color: white; border: none; border-radius: 10px;"
        "  font-weight: 700; font-size: 13px; text-align: center;"
        "}"
        "QPushButton:hover { background: %2; }")
                                 .arg(acc, BlopTheme::accentHover().name(QColor::HexRgb)));
  }
}
