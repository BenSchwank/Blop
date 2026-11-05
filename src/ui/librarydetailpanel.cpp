#include "librarydetailpanel.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "libraryorgstore.h"
#include "librarytagstore.h"
#include "notepreviewicon.h"
#include "uiscale.h"

#include <QDateTime>
#include <QEnterEvent>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
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

QString displayName(const QFileInfo &info) {
  const QString ext = info.suffix().toLower();
  if (ext == QLatin1String("bnote") || ext == QLatin1String("blop") ||
      ext == QLatin1String("struct"))
    return info.completeBaseName();
  return info.fileName();
}

QColor tagDotColor(const QString &name) {
  return LibraryTagStore::tagColor(name);
}

class TagChipButton : public QPushButton {
public:
  TagChipButton(const QString &tag, QWidget *parent)
      : QPushButton(parent), m_tag(tag), m_dot(tagDotColor(tag)) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedHeight(28);
    setMouseTracking(true);
  }

  bool overRemove() const { return m_overX; }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool dark = BlopTheme::instance().isDark();
    p.setPen(QPen(dark ? QColor(255, 255, 255, 28) : QColor(0xE5, 0xE7, 0xEB), 1));
    p.setBrush(dark ? QColor(255, 255, 255, 16) : QColor(0xF3, 0xF4, 0xF6));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 14, 14);
    p.setPen(Qt::NoPen);
    p.setBrush(m_dot);
    p.drawEllipse(QRect(8, (height() - 8) / 2, 8, 8));
    p.setPen(dark ? QColor(0xF4, 0xF6, 0xFA) : QColor(0x11, 0x18, 0x27));
    QFont f = font();
    f.setPixelSize(12);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    const QString shown =
        QFontMetrics(f).elidedText(m_tag, Qt::ElideRight, qMax(8, width() - 40));
    p.drawText(QRect(22, 0, width() - 40, height()), Qt::AlignVCenter | Qt::AlignLeft,
               shown);
    if (m_hover) {
      p.setPen(QColor(0x9C, 0xA3, 0xAF));
      p.drawText(QRect(width() - 18, 0, 14, height()), Qt::AlignCenter,
                 QStringLiteral("✕"));
    }
  }
  void enterEvent(QEnterEvent *event) override {
    m_hover = true;
    m_overX = event->position().x() > width() - 20;
    update();
    QPushButton::enterEvent(event);
  }
  void leaveEvent(QEvent *event) override {
    m_hover = false;
    m_overX = false;
    update();
    QPushButton::leaveEvent(event);
  }
  void mouseMoveEvent(QMouseEvent *event) override {
    const bool over = event->position().x() > width() - 20;
    if (over != m_overX) {
      m_overX = over;
      update();
    }
    QPushButton::mouseMoveEvent(event);
  }

private:
  QString m_tag;
  QColor m_dot;
  bool m_hover{false};
  bool m_overX{false};
};

QIcon actionIcon(const QString &kind, const QColor &color) {
  QPixmap pm(16, 16);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  if (kind == QLatin1String("pen")) {
    p.drawLine(3, 13, 12, 4);
    p.drawLine(10, 3, 13, 6);
  } else {
    p.drawRoundedRect(QRectF(4, 5, 8, 8), 1, 1);
    p.drawLine(3, 5, 13, 5);
    p.drawLine(6, 5, 6, 3);
    p.drawLine(10, 5, 10, 3);
  }
  return QIcon(pm);
}

QString cloudKindLabel(const QString &name, const QString &mime, bool isFolder) {
  if (isFolder || mime == QLatin1String("application/vnd.google-apps.folder"))
    return QStringLiteral("Ordner");
  if (mime.contains(QLatin1String("google-apps.document")))
    return QStringLiteral("Google Doc");
  if (mime.contains(QLatin1String("google-apps.spreadsheet")))
    return QStringLiteral("Google Tabelle");
  if (mime.contains(QLatin1String("google-apps.presentation")))
    return QStringLiteral("Google Präsentation");
  if (mime.contains(QLatin1String("google-apps.form")))
    return QStringLiteral("Google Formular");
  if (mime == QLatin1String("application/pdf"))
    return QStringLiteral("PDF");
  if (mime.startsWith(QLatin1String("image/")))
    return QStringLiteral("Bild");
  if (mime.startsWith(QLatin1String("video/")))
    return QStringLiteral("Video");
  if (mime.startsWith(QLatin1String("audio/")))
    return QStringLiteral("Audio");
  const QString ext = QFileInfo(name).suffix().toUpper();
  if (!ext.isEmpty())
    return ext;
  if (!mime.isEmpty())
    return mime.section(QLatin1Char('/'), -1);
  return QStringLiteral("Datei");
}

QIcon paintChevron(bool pointLeft, const QColor &color) {
  const int s = 32;
  QPixmap pm(s, s);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  if (pointLeft) {
    p.drawLine(QPointF(19, 10), QPointF(13, 16));
    p.drawLine(QPointF(13, 16), QPointF(19, 22));
  } else {
    p.drawLine(QPointF(13, 10), QPointF(19, 16));
    p.drawLine(QPointF(19, 16), QPointF(13, 22));
  }
  return QIcon(pm);
}

QPixmap emptyPlaceholderPixmap(const QColor &border, const QColor &fill) {
  const int s = UiScale::dp(48);
  QPixmap pm(s, s);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(border, 1.2));
  p.setBrush(fill);
  const qreal inset = s * 0.08;
  const qreal rad = UiScale::dp(BlopStyle::surfaceRadiusDp());
  p.drawRoundedRect(QRectF(inset, inset, s - 2 * inset, s - 2 * inset), rad,
                    rad);
  // Quiet inner document lines.
  p.setPen(QPen(border, 1.0));
  const qreal x0 = s * 0.28;
  const qreal x1 = s * 0.72;
  p.drawLine(QPointF(x0, s * 0.38), QPointF(x1, s * 0.38));
  p.drawLine(QPointF(x0, s * 0.50), QPointF(x1 * 0.85, s * 0.50));
  p.drawLine(QPointF(x0, s * 0.62), QPointF(x1 * 0.7, s * 0.62));
  return pm;
}
} // namespace

LibraryDetailPanel::LibraryDetailPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryDetailPanel"));
  setAttribute(Qt::WA_StyledBackground, true);
  m_expandedWidth = UiScale::dp(280);
  setFixedWidth(m_expandedWidth);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  m_chrome = new QWidget(this);
  m_chrome->setObjectName(QStringLiteral("LibraryDetailChrome"));
  auto *chromeLay = new QHBoxLayout(m_chrome);
  chromeLay->setContentsMargins(UiScale::dp(BlopStyle::edgeInsetDp()),
                                UiScale::dp(20),
                                UiScale::dp(BlopStyle::controlGapDp()),
                                UiScale::dp(BlopStyle::controlGapDp()));
  chromeLay->setSpacing(UiScale::dp(6));

  m_title = new QLabel(QStringLiteral("Details"), m_chrome);
  m_title->setObjectName(QStringLiteral("LibraryDetailTitle"));
  chromeLay->addWidget(m_title, 0, Qt::AlignVCenter);
  chromeLay->addStretch(1);

  m_btnCollapse = new QToolButton(m_chrome);
  m_btnCollapse->setObjectName(QStringLiteral("LibraryDetailCollapse"));
  m_btnCollapse->setCursor(Qt::PointingHandCursor);
  m_btnCollapse->setAutoRaise(true);
  m_btnCollapse->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  m_btnCollapse->setIconSize(QSize(UiScale::dp(14), UiScale::dp(14)));
  m_btnCollapse->setToolTip(QStringLiteral("Detailspalte einklappen"));
  connect(m_btnCollapse, &QToolButton::clicked, this, [this]() {
    setCollapsed(!m_collapsed);
  });
  chromeLay->addWidget(m_btnCollapse, 0, Qt::AlignVCenter);
  root->addWidget(m_chrome, 0);

  m_contentHost = new QWidget(this);
  auto *contentLay = new QVBoxLayout(m_contentHost);
  contentLay->setContentsMargins(UiScale::dp(14), UiScale::dp(8),
                                 UiScale::dp(14), UiScale::dp(14));
  contentLay->setSpacing(UiScale::dp(10));

  m_emptyHost = new QWidget(m_contentHost);
  m_emptyHost->setObjectName(QStringLiteral("LibraryDetailEmpty"));
  auto *emptyLay = new QVBoxLayout(m_emptyHost);
  emptyLay->setContentsMargins(0, 0, 0, 0);
  emptyLay->setSpacing(UiScale::dp(8));
  emptyLay->addStretch(2);
  m_emptyIcon = new QLabel(m_emptyHost);
  m_emptyIcon->setAlignment(Qt::AlignCenter);
  m_emptyIcon->setObjectName(QStringLiteral("LibraryDetailEmptyIcon"));
  emptyLay->addWidget(m_emptyIcon, 0, Qt::AlignHCenter);
  m_emptyTitle = new QLabel(QStringLiteral("Keine Auswahl"), m_emptyHost);
  m_emptyTitle->setObjectName(QStringLiteral("LibraryDetailEmptyTitle"));
  m_emptyTitle->setAlignment(Qt::AlignHCenter);
  emptyLay->addWidget(m_emptyTitle);
  m_emptyHint = new QLabel(
      QStringLiteral(
          "Wähle eine Notiz oder einen Ordner aus, um Eigenschaften, "
          "Tags und Statistiken einzusehen."),
      m_emptyHost);
  m_emptyHint->setObjectName(QStringLiteral("LibraryDetailEmptyHint"));
  m_emptyHint->setWordWrap(true);
  m_emptyHint->setAlignment(Qt::AlignHCenter);
  m_emptyHint->setMaximumWidth(UiScale::dp(200));
  emptyLay->addWidget(m_emptyHint);
  emptyLay->addStretch(3);
  contentLay->addWidget(m_emptyHost, 1);

  m_body = new QWidget(m_contentHost);
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

  m_provider = new QLabel(m_body);
  m_provider->setObjectName(QStringLiteral("LibraryDetailWhen"));
  m_provider->setWordWrap(true);
  m_provider->hide();
  lay->addWidget(m_provider);

  m_when = new QLabel(m_body);
  m_when->setWordWrap(true);
  m_when->setObjectName(QStringLiteral("LibraryDetailWhen"));
  lay->addWidget(m_when);

  m_btnFavorite = new QPushButton(m_body);
  m_btnFavorite->setObjectName(QStringLiteral("LibraryDetailFavorite"));
  m_btnFavorite->setCursor(Qt::PointingHandCursor);
  m_btnFavorite->setFlat(true);
  m_btnFavorite->setMinimumHeight(UiScale::dp(28));
  connect(m_btnFavorite, &QPushButton::clicked, this, [this]() {
    if (m_path.isEmpty())
      return;
    LibraryOrgStore::toggleFavorite(m_path);
    const bool on = LibraryOrgStore::isFavorite(m_path);
    updateFavoriteButton();
    emit favoriteChanged(m_path, on);
  });
  lay->addWidget(m_btnFavorite, 0, Qt::AlignLeft);

  m_btnOpen = new QPushButton(QStringLiteral("Öffnen"), m_body);
  m_btnOpen->setObjectName(QStringLiteral("LibraryDetailOpen"));
  m_btnOpen->setCursor(Qt::PointingHandCursor);
  m_btnOpen->setFixedHeight(36);
  connect(m_btnOpen, &QPushButton::clicked, this, [this]() {
    if (hasCloudItem())
      emit cloudOpenRequested();
    else if (!m_path.isEmpty())
      emit openRequested(m_path);
  });
  lay->addWidget(m_btnOpen);

  auto *actionRow = new QHBoxLayout();
  actionRow->setContentsMargins(0, 0, 0, 0);
  actionRow->setSpacing(UiScale::dp(12));
  m_btnRename = new QPushButton(QStringLiteral("Umbenennen"), m_body);
  m_btnRename->setObjectName(QStringLiteral("LibraryDetailQuietAction"));
  m_btnRename->setCursor(Qt::PointingHandCursor);
  m_btnRename->setFlat(true);
  connect(m_btnRename, &QPushButton::clicked, this, [this]() {
    if (hasCloudItem())
      emit cloudRenameRequested();
    else if (!m_path.isEmpty())
      emit renameRequested(m_path);
  });
  m_btnRename->setFixedHeight(32);
  m_btnRename->setIcon(actionIcon(QStringLiteral("pen"), QColor(0xE5, 0xE7, 0xEB)));
  m_btnRename->setIconSize(QSize(14, 14));
  actionRow->addWidget(m_btnRename, 1);

  m_btnDelete = new QPushButton(QStringLiteral("Löschen"), m_body);
  m_btnDelete->setObjectName(QStringLiteral("LibraryDetailQuietAction"));
  m_btnDelete->setCursor(Qt::PointingHandCursor);
  m_btnDelete->setFlat(true);
  connect(m_btnDelete, &QPushButton::clicked, this, [this]() {
    if (hasCloudItem())
      emit cloudDeleteRequested();
    else if (!m_path.isEmpty())
      emit deleteRequested(m_path);
  });
  m_btnDelete->setFixedHeight(32);
  m_btnDelete->setIcon(actionIcon(QStringLiteral("trash"), QColor(0xF8, 0x71, 0x71)));
  m_btnDelete->setIconSize(QSize(14, 14));
  actionRow->addWidget(m_btnDelete, 1);
  lay->addLayout(actionRow);

  m_tagSection = new QWidget(m_body);
  auto *tagLay = new QVBoxLayout(m_tagSection);
  tagLay->setContentsMargins(0, 0, 0, 0);
  tagLay->setSpacing(UiScale::dp(8));
  auto *tagTitle = new QLabel(QStringLiteral("Tags"), m_tagSection);
  tagTitle->setObjectName(QStringLiteral("LibraryDetailTagTitle"));
  tagLay->addWidget(tagTitle);

  m_tagHost = new QWidget(m_tagSection);
  m_tagHost->setObjectName(QStringLiteral("LibraryDetailTagHost"));
  m_tagLay = new QGridLayout(m_tagHost);
  m_tagLay->setContentsMargins(0, 0, 0, 0);
  m_tagLay->setHorizontalSpacing(UiScale::dp(6));
  m_tagLay->setVerticalSpacing(UiScale::dp(6));
  tagLay->addWidget(m_tagHost);

  auto *tagRow = new QHBoxLayout();
  tagRow->setSpacing(UiScale::dp(8));
  m_tagInput = new QLineEdit(m_tagSection);
  m_tagInput->setPlaceholderText(QStringLiteral("Tag hinzufügen…"));
  m_tagInput->setMinimumHeight(UiScale::dp(32));
  m_btnAddTag = new QPushButton(QStringLiteral("+"), m_tagSection);
  m_btnAddTag->setObjectName(QStringLiteral("LibraryDetailAddTag"));
  m_btnAddTag->setFixedSize(UiScale::dp(28), UiScale::dp(28));
  m_btnAddTag->setCursor(Qt::PointingHandCursor);
  m_btnAddTag->setAutoDefault(false);
  connect(m_tagInput, &QLineEdit::returnPressed, this,
          &LibraryDetailPanel::addTagFromInput);
  connect(m_btnAddTag, &QPushButton::clicked, this,
          &LibraryDetailPanel::addTagFromInput);
  tagRow->addWidget(m_tagInput, 1);
  tagRow->addWidget(m_btnAddTag);
  tagLay->addLayout(tagRow);
  lay->addWidget(m_tagSection);

  lay->addStretch(1);

  contentLay->addWidget(m_body, 1);
  root->addWidget(m_contentHost, 1);

  const bool saved =
      QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
          .value(QStringLiteral("ui/libraryDetailCollapsed"), false)
          .toBool();
  m_collapsed = saved;
  applyCollapsed();
  applyChrome();
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
          [this]() { applyChrome(); });
}

void LibraryDetailPanel::setCollapsed(bool on) {
  if (m_collapsed == on)
    return;
  m_collapsed = on;
  QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
      .setValue(QStringLiteral("ui/libraryDetailCollapsed"), m_collapsed);
  applyCollapsed();
  applyChrome();
  emit collapsedChanged(m_collapsed);
}

void LibraryDetailPanel::updateCollapseIcon() {
  if (!m_btnCollapse)
    return;
  const bool dark = BlopTheme::instance().isDark();
  const QColor ink =
      dark ? BlopStyle::obsidianText() : BlopStyle::paperInkMuted();
  // Expanded only: › folds the pane away completely (no collapsed strip).
  m_btnCollapse->setIcon(paintChevron(/*pointLeft=*/false, ink));
  m_btnCollapse->setText(QString());
  m_btnCollapse->setToolTip(QStringLiteral("Detailspalte ausblenden"));
}

void LibraryDetailPanel::applyCollapsed() {
  if (m_contentHost)
    m_contentHost->setVisible(!m_collapsed);
  if (m_title)
    m_title->setVisible(!m_collapsed);
  if (m_chrome) {
    auto *lay = qobject_cast<QHBoxLayout *>(m_chrome->layout());
    if (lay) {
      lay->setContentsMargins(UiScale::dp(14), UiScale::dp(20), UiScale::dp(8),
                              UiScale::dp(6));
    }
  }
  updateCollapseIcon();
  if (m_collapsed) {
    setFixedWidth(0);
    setMinimumWidth(0);
    setMaximumWidth(0);
    hide();
  } else {
    setFixedWidth(m_expandedWidth);
    setMinimumWidth(m_expandedWidth);
    setMaximumWidth(m_expandedWidth);
    show();
  }
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void LibraryDetailPanel::setPath(const QString &absolutePath) {
  m_cloud = CloudInfo{};
  m_path = absolutePath;
  refresh();
}

void LibraryDetailPanel::setCloudItem(const CloudInfo &info) {
  m_path.clear();
  m_cloud = info;
  refresh();
}

void LibraryDetailPanel::clearSelection() {
  m_path.clear();
  m_cloud = CloudInfo{};
  refresh();
}

void LibraryDetailPanel::setCloudUiVisible(bool cloud) {
  if (m_btnFavorite)
    m_btnFavorite->setVisible(!cloud);
  if (m_tagSection)
    m_tagSection->setVisible(!cloud);
  if (m_provider)
    m_provider->setVisible(cloud && !m_cloud.providerLabel.isEmpty());
}

void LibraryDetailPanel::refresh() {
  const bool cloud = hasCloudItem();
  const QFileInfo info(m_path);
  const bool localOk = !cloud && !m_path.isEmpty() && info.exists();
  const bool ok = cloud || localOk;
  if (m_emptyHost)
    m_emptyHost->setVisible(!ok && !m_collapsed);
  if (m_body)
    m_body->setVisible(ok && !m_collapsed);
  if (!ok || m_collapsed)
    return;

  setCloudUiVisible(cloud);
  const int px = UiScale::dp(128);
  if (cloud) {
    m_preview->setPixmap(NotePreviewIcon::pixmapForCloud(
        m_cloud.name, m_cloud.mimeType, m_cloud.isFolder, px));
    m_name->setText(m_cloud.name);
    QStringList metaBits;
    metaBits << cloudKindLabel(m_cloud.name, m_cloud.mimeType, m_cloud.isFolder);
    if (!m_cloud.isFolder && m_cloud.sizeBytes >= 0)
      metaBits << humanSize(m_cloud.sizeBytes);
    m_meta->setText(metaBits.join(QStringLiteral(" · ")));
    if (m_provider) {
      m_provider->setVisible(!m_cloud.providerLabel.isEmpty());
      m_provider->setText(m_cloud.providerLabel.isEmpty()
                              ? QString()
                              : QStringLiteral("Quelle · %1")
                                    .arg(m_cloud.providerLabel));
    }
    m_btnOpen->setText(m_cloud.isFolder ? QStringLiteral("Öffnen")
                                        : QStringLiteral("Öffnen / Bearbeiten"));
    QStringList whenLines;
    if (m_cloud.created.isValid()) {
      whenLines << QStringLiteral("Erstellt %1").arg(
          m_cloud.created.toLocalTime().toString(
              QStringLiteral("dd.MM.yyyy HH:mm")));
    }
    if (m_cloud.modified.isValid()) {
      whenLines << QStringLiteral("Geändert %1").arg(
          m_cloud.modified.toLocalTime().toString(
              QStringLiteral("dd.MM.yyyy HH:mm")));
    }
    if (whenLines.isEmpty())
      whenLines << QStringLiteral("Keine Datumsangabe vom Anbieter");
    m_when->setText(whenLines.join(QLatin1Char('\n')));
  } else {
    m_preview->setPixmap(
        NotePreviewIcon::pixmapForPath(m_path, info.isDir(), px));
    m_name->setText(displayName(info));
    if (info.isDir()) {
      m_meta->setText(QStringLiteral("Ordner"));
    } else {
      const QString ext = info.suffix().toUpper();
      if (ext == QLatin1String("BNOTE") || ext == QLatin1String("BLOP") ||
          ext == QLatin1String("STRUCT")) {
        m_meta->setText(QStringLiteral("%1 · %2 · %3")
                            .arg(kindLabel(info), ext, humanSize(info.size())));
      } else {
        m_meta->setText(QStringLiteral("%1 · %2")
                            .arg(kindLabel(info), humanSize(info.size())));
      }
    }
    m_btnOpen->setText(QStringLiteral("Öffnen"));
    updateFavoriteButton();
    const QDateTime edited = info.lastModified();
    m_when->setText(
        edited.isValid()
            ? QStringLiteral("Geändert %1")
                  .arg(edited.toString(QStringLiteral("dd.MM.yyyy HH:mm")))
            : QString());
    rebuildTagChips();
  }
  applyChrome();
}

void LibraryDetailPanel::updateFavoriteButton() {
  if (!m_btnFavorite)
    return;
  const bool on =
      !m_path.isEmpty() && LibraryOrgStore::isFavorite(m_path);
  m_btnFavorite->setText(on ? QStringLiteral("★ Favorit")
                            : QStringLiteral("☆ Zu Favoriten"));
  m_btnFavorite->setToolTip(on ? QStringLiteral("Aus Favoriten entfernen")
                               : QStringLiteral("Zu Favoriten hinzufügen"));
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
  QStringList catalog = LibraryTagStore::catalog();
  if (catalog.isEmpty()) {
    LibraryTagStore::addTagToCatalog(QStringLiteral("Projekt"));
    LibraryTagStore::addTagToCatalog(QStringLiteral("Entwurf"));
    catalog = LibraryTagStore::catalog();
  }
  const QStringList assigned = LibraryTagStore::tagsForPath(m_path);
  if (assigned.isEmpty()) {
    auto *empty = new QLabel(QStringLiteral("Keine Tags zugewiesen"), m_tagHost);
    empty->setObjectName(QStringLiteral("LibraryDetailTagEmpty"));
    empty->setStyleSheet(
        QStringLiteral("QLabel#LibraryDetailTagEmpty {"
                       "  color: #94A3B8; font-size: 12px; font-weight: 500;"
                       "  background: transparent; border: none;"
                       "}"));
    m_tagLay->addWidget(empty, 0, 0, 1, 2);
    Q_UNUSED(catalog);
    return;
  }
  int i = 0;
  constexpr int kCols = 2;
  QFont chipFont;
  chipFont.setPixelSize(12);
  chipFont.setWeight(QFont::DemiBold);
  const QFontMetrics fm(chipFont);
  const int hostW = m_tagHost->width() > 0 ? m_tagHost->width() : 240;
  const int colW = qMax(72, (hostW - UiScale::dp(6)) / 2);
  for (const QString &tag : assigned) {
    auto *chip = new TagChipButton(tag, m_tagHost);
    const int w = fm.horizontalAdvance(tag) + 44;
    chip->setFixedWidth(qBound(72, w, colW));
    connect(chip, &QPushButton::clicked, this, [this, tag, chip]() {
      if (!chip->overRemove() || m_path.isEmpty())
        return;
      QStringList next = LibraryTagStore::tagsForPath(m_path);
      next.removeAll(tag);
      LibraryTagStore::setTagsForPath(m_path, next);
      rebuildTagChips();
    });
    m_tagLay->addWidget(chip, i / kCols, i % kCols);
    ++i;
  }
  Q_UNUSED(catalog);
}

void LibraryDetailPanel::applyChrome() {
  const bool dark = BlopTheme::instance().isDark();
  const QString bg = BlopStyle::librarySidebar().name(QColor::HexRgb);
  const QString ink = dark ? BlopStyle::obsidianText().name(QColor::HexRgb)
                           : BlopStyle::paperInk().name(QColor::HexRgb);
  const QString muted =
      dark ? QStringLiteral("#9AA3B2")
           : BlopStyle::paperInkMuted().name(QColor::HexRgb);
  const QString acc = BlopTheme::accentPrimary().name(QColor::HexRgb);
  const QString border =
      dark ? QStringLiteral("rgba(255,255,255,0.10)")
           : BlopStyle::paperBorder().name(QColor::HexRgb);
  const QString surface =
      dark ? QStringLiteral("#242833")
           : BlopStyle::libraryMain().name(QColor::HexRgb);
  // Flush docked pane: hairline to Main; only outer (window) corners soft.
  const int outerRad = UiScale::dp(BlopStyle::radiusLgDp());
  const int ctrlRad = UiScale::dp(BlopStyle::radiusLgDp());
  setStyleSheet(QStringLiteral(
      "QWidget#LibraryDetailPanel {"
      "  background: %1; border: none; border-left: 1px solid %2;"
      "  border-top-left-radius: 0; border-bottom-left-radius: 0;"
      "  border-top-right-radius: %7px; border-bottom-right-radius: %7px;"
      "}"
      "QWidget#LibraryDetailChrome { background: transparent; border: none; }"
      "QLabel#LibraryDetailTitle {"
      "  color: %3; font-size: 14px; font-weight: 650; letter-spacing: 0.1px;"
      "}"
      "QLabel { color: %3; background: transparent; }"
      "QLabel#LibraryDetailEmptyTitle {"
      "  color: %3; font-size: 14px; font-weight: 700;"
      "}"
      "QLabel#LibraryDetailEmptyHint {"
      "  color: %4; font-size: 12px; font-weight: 500;"
      "}"
      "QLabel#LibraryDetailName { font-size: 15px; font-weight: 700; }"
      "QLabel#LibraryDetailMeta, QLabel#LibraryDetailWhen,"
      "QLabel#LibraryDetailTagTitle { color: %4; font-size: 12px; font-weight: 600; }"
      "QLineEdit {"
      "  background: %5; color: %3; border: 1px solid %2;"
      "  border-radius: %8px; padding: 0 10px; font-size: 12px;"
      "}"
      "QPushButton#LibraryDetailQuietAction,"
      "QPushButton#LibraryDetailFavorite {"
      "  background: transparent; color: %4; border: none;"
      "  text-align: left; font-size: 12px; font-weight: 600; padding: 2px 0;"
      "}"
      "QPushButton#LibraryDetailQuietAction:hover,"
      "QPushButton#LibraryDetailFavorite:hover { color: %6; }")
                    .arg(bg, border, ink, muted, surface, acc)
                    .arg(outerRad)
                    .arg(ctrlRad));

  if (m_btnCollapse)
    m_btnCollapse->setStyleSheet(
        BlopStyle::quietIconButtonQss(BlopStyle::radiusMdDp()));
  updateCollapseIcon();

  if (m_emptyIcon) {
    const QColor fill =
        dark ? QColor(0x24, 0x28, 0x33) : BlopStyle::libraryMain();
    const QColor bd =
        dark ? QColor(255, 255, 255, 40) : BlopStyle::paperBorder();
    m_emptyIcon->setPixmap(emptyPlaceholderPixmap(bd, fill));
  }

  if (m_btnOpen) {
    m_btnOpen->setFixedHeight(36);
    m_btnOpen->setStyleSheet(QStringLiteral(
        "QPushButton#LibraryDetailOpen {"
        "  background: %1; color: white; border: none; border-radius: 8px;"
        "  font-weight: 600; font-size: 13px; padding: 0 14px;"
        "  min-height: 36px; max-height: 36px;"
        "}"
        "QPushButton#LibraryDetailOpen:hover { background: %2; }")
                                 .arg(BlopTheme::accentPrimary().name(QColor::HexRgb),
                                      BlopTheme::accentPrimary().lighter(112).name(
                                          QColor::HexRgb)));
  }
  const QString secondary = QStringLiteral(
      "QPushButton#LibraryDetailQuietAction {"
      "  background: transparent; color: %1;"
      "  border: 1px solid %2; border-radius: 6px;"
      "  font-size: 12px; font-weight: 600; padding: 0 8px;"
      "  min-height: 32px; max-height: 32px;"
      "}"
      "QPushButton#LibraryDetailQuietAction:hover { background: %3; }")
                                .arg(ink, border,
                                     dark ? QStringLiteral("rgba(255,255,255,0.06)")
                                          : QStringLiteral("#F3F4F6"));
  if (m_btnRename) {
    m_btnRename->setStyleSheet(secondary);
    m_btnRename->setIcon(actionIcon(QStringLiteral("pen"),
                                    dark ? QColor(0xE5, 0xE7, 0xEB)
                                         : QColor(0x37, 0x41, 0x51)));
  }
  if (m_btnDelete) {
    m_btnDelete->setStyleSheet(QStringLiteral(
        "QPushButton#LibraryDetailQuietAction {"
        "  background: transparent; color: %1;"
        "  border: 1px solid %2; border-radius: 6px;"
        "  font-size: 12px; font-weight: 600; padding: 0 8px;"
        "  min-height: 32px; max-height: 32px;"
        "}"
        "QPushButton#LibraryDetailQuietAction:hover {"
        "  background: rgba(239,68,68,0.15); color: #F87171;"
        "}")
                                   .arg(ink, border));
    m_btnDelete->setIcon(actionIcon(QStringLiteral("trash"), QColor(0xF8, 0x71, 0x71)));
  }
  if (m_btnAddTag) {
    m_btnAddTag->setFixedSize(UiScale::dp(28), UiScale::dp(28));
    m_btnAddTag->setStyleSheet(BlopStyle::framedChromeIconButtonQss(
        dark, BlopStyle::radiusMdDp(), 28));
  }
}
