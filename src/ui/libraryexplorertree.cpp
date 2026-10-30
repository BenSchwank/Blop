#include "libraryexplorertree.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "uiscale.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QSortFilterProxyModel>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>

namespace {

bool dotNamed(const QString &name) {
  return name.startsWith(QLatin1Char('.'));
}

class ExplorerFilterProxy : public QSortFilterProxyModel {
public:
  explicit ExplorerFilterProxy(QObject *parent = nullptr)
      : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
  }

  void setNeedle(const QString &text) {
    const QString next = text.trimmed();
    if (next == m_needle)
      return;
    beginFilterChange();
    m_needle = next;
    endFilterChange();
  }

  void setIcons(const QIcon &folder, const QIcon &file) {
    m_folder = folder;
    m_file = file;
  }

  QVariant data(const QModelIndex &index, int role) const override {
    if (role == Qt::DecorationRole && index.column() == 0) {
      const auto *fs = qobject_cast<const QFileSystemModel *>(sourceModel());
      const QModelIndex src = mapToSource(index);
      if (fs && src.isValid())
        return fs->isDir(src) ? m_folder : m_file;
    }
    if (role == Qt::DisplayRole && index.column() == 0) {
      const QString name =
          QSortFilterProxyModel::data(index, Qt::DisplayRole).toString();
      if (name.endsWith(QLatin1String(".bnote"), Qt::CaseInsensitive))
        return name.left(name.size() - 6);
    }
    return QSortFilterProxyModel::data(index, role);
  }

protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex &sourceParent) const override {
    if (!sourceModel())
      return false;
    QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!idx.isValid())
      return false;
    for (QModelIndex walk = idx; walk.isValid(); walk = walk.parent()) {
      if (dotNamed(walk.data(Qt::DisplayRole).toString()))
        return false;
    }
    const QString name = idx.data(Qt::DisplayRole).toString();
    if (!nameMatches(name)) {
      if (m_needle.isEmpty() || !subtreeMatches(idx, 0))
        return false;
    }
    return true;
  }

private:
  bool nameMatches(const QString &name) const {
    if (m_needle.isEmpty())
      return true;
    if (name.contains(m_needle, Qt::CaseInsensitive))
      return true;
    return name.endsWith(QLatin1String(".bnote"), Qt::CaseInsensitive) &&
           name.left(name.size() - 6).contains(m_needle, Qt::CaseInsensitive);
  }

  bool subtreeMatches(const QModelIndex &idx, int depth) const {
    if (depth > 6 || !sourceModel())
      return false;
    const int rows = sourceModel()->rowCount(idx);
    for (int row = 0; row < rows; ++row) {
      const QModelIndex child = sourceModel()->index(row, 0, idx);
      if (!child.isValid())
        continue;
      const QString childName = child.data(Qt::DisplayRole).toString();
      if (dotNamed(childName))
        continue;
      if (nameMatches(childName))
        return true;
      if (subtreeMatches(child, depth + 1))
        return true;
    }
    return false;
  }
  QString m_needle;
  QIcon m_folder;
  QIcon m_file;
};

QIcon strokeIcon(bool folder, const QColor &ink) {
  QPixmap pm(32, 32);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(ink, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  if (folder) {
    p.drawRoundedRect(QRectF(4, 12, 24, 14), 2.5, 2.5);
    QPainterPath tab;
    tab.moveTo(4, 14);
    tab.lineTo(4, 10);
    tab.lineTo(12, 10);
    tab.lineTo(15, 14);
    p.drawPath(tab);
  } else {
    p.drawRoundedRect(QRectF(8, 4, 16, 24), 2, 2);
    p.drawLine(QPointF(12, 12), QPointF(20, 12));
    p.drawLine(QPointF(12, 16), QPointF(20, 16));
    p.drawLine(QPointF(12, 20), QPointF(17, 20));
  }
  return QIcon(pm);
}

QIcon actionIcon(bool folder, const QColor &ink) {
  QPixmap pm(32, 32);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(ink, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  if (folder) {
    p.drawRoundedRect(QRectF(2, 12, 16, 12), 2, 2);
    p.drawLine(QPointF(2, 14), QPointF(8, 9));
    p.drawLine(QPointF(8, 9), QPointF(12, 14));
  } else {
    p.drawRoundedRect(QRectF(3, 4, 14, 18), 2, 2);
  }
  p.drawLine(QPointF(22, 16), QPointF(30, 16));
  p.drawLine(QPointF(26, 12), QPointF(26, 20));
  return QIcon(pm);
}

class ExplorerTreeView : public QTreeView {
public:
  using QTreeView::QTreeView;

  std::function<QString(const QModelIndex &)> pathOf;
  std::function<void(const QStringList &, const QString &)> onMoveDrop;
  std::function<void(const QString &, const QPoint &)> onContextMenu;
  QStringList dragPaths;

protected:
  void startDrag(Qt::DropActions supportedActions) override {
    dragPaths.clear();
    if (pathOf && selectionModel()) {
      for (const QModelIndex &s : selectionModel()->selectedRows(0))
        dragPaths.append(pathOf(s));
    }
    QTreeView::startDrag(supportedActions);
  }

  void drawBranches(QPainter *painter, const QRect &rect,
                    const QModelIndex &index) const override {
    if (!painter || !model() || !model()->hasChildren(index))
      return;
    const bool open = isExpanded(index);
    const bool dark = BlopTheme::instance().isDark();
    const QColor ink = dark ? QColor(0x9A, 0xA3, 0xB2)
                            : BlopStyle::paperInkMuted();
    const int s = 12;
    const QRectF box(rect.center().x() - s / 2.0, rect.center().y() - s / 2.0, s,
                     s);
    QPainterPath path;
    if (open) {
      path.moveTo(box.left() + 2, box.top() + 4);
      path.lineTo(box.center().x(), box.bottom() - 3);
      path.lineTo(box.right() - 2, box.top() + 4);
    } else {
      path.moveTo(box.left() + 4, box.top() + 2);
      path.lineTo(box.right() - 3, box.center().y());
      path.lineTo(box.left() + 4, box.bottom() - 2);
    }
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
    painter->restore();
  }

  void contextMenuEvent(QContextMenuEvent *event) override {
    if (!event || !pathOf || !onContextMenu)
      return;
    const QModelIndex idx = indexAt(event->pos());
    if (!idx.isValid())
      return;
    setCurrentIndex(idx);
    onContextMenu(pathOf(idx), event->globalPos());
    event->accept();
  }

  void dragEnterEvent(QDragEnterEvent *event) override {
    if (!event)
      return;
    event->setDropAction(Qt::MoveAction);
    event->accept();
  }

  void dragMoveEvent(QDragMoveEvent *event) override {
    if (!event || !pathOf)
      return;
    const QModelIndex idx = indexAt(event->position().toPoint());
    if (!idx.isValid()) {
      event->ignore();
      return;
    }
    QString dest = pathOf(idx);
    if (!QFileInfo(dest).isDir())
      dest = QFileInfo(dest).absolutePath();
    if (dest.isEmpty()) {
      event->ignore();
      return;
    }
    event->setDropAction(Qt::MoveAction);
    event->accept();
  }

  void dropEvent(QDropEvent *event) override {
    if (!event || !pathOf || !onMoveDrop)
      return;
    const QModelIndex idx = indexAt(event->position().toPoint());
    if (!idx.isValid()) {
      event->ignore();
      return;
    }
    QString dest = pathOf(idx);
    if (!QFileInfo(dest).isDir())
      dest = QFileInfo(dest).absolutePath();
    QStringList sources = dragPaths;
    if (sources.isEmpty() && event->mimeData() && event->mimeData()->hasUrls()) {
      for (const QUrl &u : event->mimeData()->urls()) {
        if (u.isLocalFile())
          sources.append(u.toLocalFile());
      }
    }
    sources.removeDuplicates();
    if (sources.isEmpty() || dest.isEmpty()) {
      event->ignore();
      return;
    }
    onMoveDrop(sources, dest);
    dragPaths.clear();
    event->setDropAction(Qt::MoveAction);
    event->accept();
  }
};

QToolButton *makeHeaderButton(QWidget *parent, const QString &tip) {
  auto *b = new QToolButton(parent);
  b->setToolTip(tip);
  b->setCursor(Qt::PointingHandCursor);
  b->setAutoRaise(true);
  b->setFocusPolicy(Qt::NoFocus);
  b->setFixedSize(UiScale::dp(24), UiScale::dp(24));
  b->setIconSize(QSize(UiScale::dp(14), UiScale::dp(14)));
  return b;
}

} // namespace

LibraryExplorerTree::LibraryExplorerTree(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryExplorerTree"));
  setAttribute(Qt::WA_StyledBackground, true);
  setToolTip(QStringLiteral("Ordnerstruktur"));
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

  m_accent = BlopTheme::accentPrimary();

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(UiScale::dp(BlopStyle::controlGapDp()),
                           UiScale::dp(BlopStyle::controlGapDp()),
                           UiScale::dp(BlopStyle::controlGapDp()),
                           UiScale::dp(BlopStyle::controlGapDp()));
  root->setSpacing(UiScale::dp(2));

  auto *bar = new QWidget(this);
  bar->setObjectName(QStringLiteral("LibraryExplorerSection"));
  auto *barLay = new QHBoxLayout(bar);
  barLay->setContentsMargins(0, 0, 0, 0);
  barLay->setSpacing(UiScale::dp(4));

  m_sectionLabel = new QLabel(QStringLiteral("ORDNER"), bar);
  m_sectionLabel->setObjectName(QStringLiteral("LibraryExplorerSectionLabel"));
  barLay->addWidget(m_sectionLabel, 0, Qt::AlignVCenter);
  barLay->addStretch(1);

  m_btnNewFolder = makeHeaderButton(bar, QStringLiteral("Neuer Ordner"));
  m_btnNewFolder->setObjectName(QStringLiteral("LibraryExplorerNewFolder"));
  connect(m_btnNewFolder, &QToolButton::clicked, this,
          &LibraryExplorerTree::newFolderRequested);
  barLay->addWidget(m_btnNewFolder, 0, Qt::AlignVCenter);

  m_btnNewNote = makeHeaderButton(bar, QStringLiteral("Neue Notiz"));
  m_btnNewNote->setObjectName(QStringLiteral("LibraryExplorerNewNote"));
  connect(m_btnNewNote, &QToolButton::clicked, this, [this]() {
    QString dir = currentPath();
    const QFileInfo fi(dir);
    if (dir.isEmpty() || !fi.exists())
      dir = rootPath();
    else if (!fi.isDir())
      dir = fi.absolutePath();
    emit newNoteInFolderRequested(dir);
  });
  barLay->addWidget(m_btnNewNote, 0, Qt::AlignVCenter);
  root->addWidget(bar);

  m_model = new QFileSystemModel(this);
  m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
  m_model->setNameFilters({QStringLiteral("*.bnote"), QStringLiteral("*.blop"),
                           QStringLiteral("*.struct")});
  m_model->setNameFilterDisables(false);
  m_model->setReadOnly(false);

  m_proxy = new ExplorerFilterProxy(this);
  m_proxy->setSourceModel(m_model);

  auto *tree = new ExplorerTreeView(this);
  m_tree = tree;
  tree->pathOf = [this](const QModelIndex &viewIndex) {
    return pathFor(viewIndex);
  };
  tree->setObjectName(QStringLiteral("LibraryExplorerView"));
  tree->setModel(m_proxy);
  tree->setHeaderHidden(true);
  tree->setAnimated(false);
  tree->setIndentation(UiScale::dp(18));
  tree->setIconSize(QSize(UiScale::dp(16), UiScale::dp(16)));
  tree->setExpandsOnDoubleClick(false);
  tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tree->setDragEnabled(true);
  tree->setAcceptDrops(true);
  tree->setDropIndicatorShown(true);
  tree->setDefaultDropAction(Qt::MoveAction);
  tree->setDragDropMode(QAbstractItemView::DragDrop);
  tree->setContextMenuPolicy(Qt::DefaultContextMenu);
  tree->setUniformRowHeights(true);
  tree->setRootIsDecorated(true);
  for (int c = 1; c < m_model->columnCount(); ++c)
    tree->hideColumn(c);

  tree->onMoveDrop = [this](const QStringList &sources, const QString &dest) {
    emit movePathsRequested(sources, dest);
  };
  tree->onContextMenu = [this](const QString &path, const QPoint &globalPos) {
    emit contextMenuRequested(path, globalPos);
  };

  connect(tree, &QTreeView::clicked, this, [this](const QModelIndex &idx) {
    const QString path = pathFor(idx);
    if (!path.isEmpty())
      emit pathSelected(path);
  });
  connect(tree, &QTreeView::doubleClicked, this,
          [this](const QModelIndex &idx) {
            const QString path = pathFor(idx);
            if (!path.isEmpty())
              emit pathActivated(path);
          });

  root->addWidget(tree, 1);
  m_model->sort(0, Qt::AscendingOrder);
  applyChrome();
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
          [this]() { applyChrome(); });
}

void LibraryExplorerTree::setRootPath(const QString &absolutePath) {
  if (!m_model || !m_tree || !m_proxy)
    return;
  const QModelIndex root = m_model->setRootPath(absolutePath);
  m_tree->setRootIndex(m_proxy->mapFromSource(root));
}

void LibraryExplorerTree::setAccentColor(const QColor &color) {
  if (!color.isValid() || m_accent == color)
    return;
  m_accent = color;
  applyChrome();
}

void LibraryExplorerTree::setFilterText(const QString &text) {
  if (auto *proxy = static_cast<ExplorerFilterProxy *>(m_proxy))
    proxy->setNeedle(text);
}

QString LibraryExplorerTree::rootPath() const {
  return m_model ? m_model->rootPath() : QString();
}

QString LibraryExplorerTree::currentPath() const {
  if (!m_tree)
    return QString();
  return pathFor(m_tree->currentIndex());
}

QString LibraryExplorerTree::pathFor(const QModelIndex &viewIndex) const {
  if (!m_model || !viewIndex.isValid())
    return QString();
  const QModelIndex src =
      m_proxy ? m_proxy->mapToSource(viewIndex) : viewIndex;
  if (!src.isValid())
    return QString();
  return m_model->filePath(src);
}

void LibraryExplorerTree::applyChrome() {
  const bool dark = BlopTheme::instance().isDark();
  const QString bg = BlopStyle::librarySidebar().name(QColor::HexRgb);
  const QString ink = dark ? QStringLiteral("#E8EAF0")
                           : BlopStyle::paperInk().name(QColor::HexRgb);
  const QString muted =
      dark ? QStringLiteral("#9AA3B2")
           : BlopStyle::paperInkMuted().name(QColor::HexRgb);
  const QColor accColor =
      m_accent.isValid() ? m_accent : BlopTheme::accentPrimary();
  const QString acc = accColor.name(QColor::HexRgb);
  const QString hover =
      dark ? QStringLiteral("rgba(255,255,255,0.10)")
           : BlopStyle::paperHover().name(QColor::HexRgb);
  const QString btnHover =
      dark ? QStringLiteral("rgba(255,255,255,0.10)")
           : QStringLiteral("rgba(0,0,0,0.06)");
  const QString sel = dark ? QStringLiteral("rgba(%1,%2,%3,0.28)")
                           : QStringLiteral("rgba(%1,%2,%3,0.16)");
  const int rowH = UiScale::dp(32);
  const int rad = UiScale::dp(4);
  const QString selFill =
      sel.arg(accColor.red()).arg(accColor.green()).arg(accColor.blue());
  setStyleSheet(
      QStringLiteral(
          "QWidget#LibraryExplorerTree { background: %1; border: none; }"
          "QLabel#LibraryExplorerSectionLabel {"
          "  color: %6; font-size: 10px; font-weight: 700;"
          "  letter-spacing: 0.8px; background: transparent;"
          "}"
          "QToolButton#LibraryExplorerNewFolder,"
          "QToolButton#LibraryExplorerNewNote {"
          "  background: transparent; border: none; border-radius: %8px;"
          "}"
          "QToolButton#LibraryExplorerNewFolder:hover,"
          "QToolButton#LibraryExplorerNewNote:hover {"
          "  background: %9; border: none;"
          "}"
          "QTreeView#LibraryExplorerView {"
          "  background: transparent; border: none; color: %2; outline: 0;"
          "  font-size: 12px; show-decoration-selected: 1;"
          "}"
          "QTreeView#LibraryExplorerView::item {"
          "  min-height: %4px; padding: 2px 8px; border-radius: %8px;"
          "  margin: 1px 0;"
          "}"
          "QTreeView#LibraryExplorerView::item:hover { background: %3; }"
          "QTreeView#LibraryExplorerView::item:selected {"
          "  background: %5; color: %7;"
          "}"
          "QTreeView#LibraryExplorerView::branch {"
          "  background: transparent; border-image: none;"
          "}")
          .arg(bg, ink, hover, QString::number(rowH), selFill, muted, acc,
               QString::number(rad), btnHover));
  const QColor iconInk = dark ? QColor(0xC8, 0xCD, 0xD8) : BlopStyle::paperInk();
  if (m_btnNewFolder)
    m_btnNewFolder->setIcon(actionIcon(true, iconInk));
  if (m_btnNewNote)
    m_btnNewNote->setIcon(actionIcon(false, iconInk));
  if (auto *proxy = static_cast<ExplorerFilterProxy *>(m_proxy))
    proxy->setIcons(strokeIcon(true, iconInk), strokeIcon(false, iconInk));
  if (m_tree)
    m_tree->viewport()->update();
}
