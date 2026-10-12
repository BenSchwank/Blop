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
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>

namespace {

class ExplorerTreeView : public QTreeView {
public:
  using QTreeView::QTreeView;

  std::function<void(const QStringList &, const QString &)> onMoveDrop;
  std::function<void(const QString &, const QPoint &)> onContextMenu;
  QFileSystemModel *fsModel{nullptr};
  QStringList dragPaths;

protected:
  void startDrag(Qt::DropActions supportedActions) override {
    dragPaths.clear();
    if (fsModel && selectionModel()) {
      for (const QModelIndex &s : selectionModel()->selectedRows(0))
        dragPaths.append(fsModel->filePath(s));
    }
    QTreeView::startDrag(supportedActions);
  }

  void contextMenuEvent(QContextMenuEvent *event) override {
    if (!event || !fsModel || !onContextMenu)
      return;
    const QModelIndex idx = indexAt(event->pos());
    if (!idx.isValid())
      return;
    setCurrentIndex(idx);
    onContextMenu(fsModel->filePath(idx), event->globalPos());
    event->accept();
  }

  void dragEnterEvent(QDragEnterEvent *event) override {
    if (!event)
      return;
    event->setDropAction(Qt::MoveAction);
    event->accept();
  }

  void dragMoveEvent(QDragMoveEvent *event) override {
    if (!event || !fsModel)
      return;
    const QModelIndex idx = indexAt(event->position().toPoint());
    if (!idx.isValid()) {
      event->ignore();
      return;
    }
    QString dest = fsModel->filePath(idx);
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
    if (!event || !fsModel || !onMoveDrop)
      return;
    const QModelIndex idx = indexAt(event->position().toPoint());
    if (!idx.isValid()) {
      event->ignore();
      return;
    }
    QString dest = fsModel->filePath(idx);
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

} // namespace

LibraryExplorerTree::LibraryExplorerTree(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryExplorerTree"));
  setAttribute(Qt::WA_StyledBackground, true);
  setToolTip(QStringLiteral("Ordnerstruktur"));
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

  m_accent = BlopTheme::accentPrimary();

  auto *root = new QVBoxLayout(this);
  // 8dp inset rhythm — matches sidebar search / mode switch / nav.
  root->setContentsMargins(UiScale::dp(8), UiScale::dp(4), UiScale::dp(8),
                           UiScale::dp(4));
  root->setSpacing(UiScale::dp(2));

  auto *bar = new QWidget(this);
  bar->setObjectName(QStringLiteral("LibraryExplorerSection"));
  auto *barLay = new QHBoxLayout(bar);
  barLay->setContentsMargins(0, UiScale::dp(2), 0, UiScale::dp(2));
  barLay->setSpacing(UiScale::dp(4));

  m_sectionLabel = new QLabel(QStringLiteral("ORDNER"), bar);
  m_sectionLabel->setObjectName(QStringLiteral("LibraryExplorerSectionLabel"));
  barLay->addWidget(m_sectionLabel, 0, Qt::AlignVCenter);
  barLay->addStretch(1);

  m_btnNewFolder = new QToolButton(bar);
  m_btnNewFolder->setObjectName(QStringLiteral("LibraryExplorerNewFolder"));
  m_btnNewFolder->setText(QStringLiteral("+"));
  m_btnNewFolder->setToolTip(QStringLiteral("Neuer Ordner"));
  m_btnNewFolder->setCursor(Qt::PointingHandCursor);
  m_btnNewFolder->setAutoRaise(true);
  m_btnNewFolder->setFixedSize(UiScale::dp(28), UiScale::dp(28));
  m_btnNewFolder->setFocusPolicy(Qt::NoFocus);
  connect(m_btnNewFolder, &QToolButton::clicked, this,
          &LibraryExplorerTree::newFolderRequested);
  barLay->addWidget(m_btnNewFolder, 0, Qt::AlignVCenter);
  root->addWidget(bar);

  m_model = new QFileSystemModel(this);
  m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
  m_model->setNameFilters({QStringLiteral("*.bnote"), QStringLiteral("*.blop"),
                           QStringLiteral("*.struct")});
  m_model->setNameFilterDisables(false);
  m_model->setReadOnly(false);

  auto *tree = new ExplorerTreeView(this);
  m_tree = tree;
  tree->fsModel = m_model;
  tree->setObjectName(QStringLiteral("LibraryExplorerView"));
  tree->setModel(m_model);
  tree->setHeaderHidden(true);
  tree->setAnimated(false);
  tree->setIndentation(UiScale::dp(12));
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
    if (!idx.isValid() || !m_model)
      return;
    emit pathSelected(m_model->filePath(idx));
  });
  connect(tree, &QTreeView::doubleClicked, this,
          [this](const QModelIndex &idx) {
            if (!idx.isValid() || !m_model)
              return;
            emit pathActivated(m_model->filePath(idx));
          });

  root->addWidget(tree, 1);
  m_model->sort(0, Qt::AscendingOrder);
  applyChrome();
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
          [this]() { applyChrome(); });
}

void LibraryExplorerTree::setRootPath(const QString &absolutePath) {
  if (!m_model || !m_tree)
    return;
  const QModelIndex root = m_model->setRootPath(absolutePath);
  m_tree->setRootIndex(root);
}

void LibraryExplorerTree::setAccentColor(const QColor &color) {
  if (!color.isValid() || m_accent == color)
    return;
  m_accent = color;
  applyChrome();
}

QString LibraryExplorerTree::rootPath() const {
  return m_model ? m_model->rootPath() : QString();
}

QString LibraryExplorerTree::currentPath() const {
  if (!m_tree || !m_model)
    return QString();
  const QModelIndex idx = m_tree->currentIndex();
  if (!idx.isValid())
    return QString();
  return m_model->filePath(idx);
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
  const QString sel = dark ? QStringLiteral("rgba(%1,%2,%3,0.28)")
                           : QStringLiteral("rgba(%1,%2,%3,0.16)");
  // Match SidebarNavDelegate desktop row height (ROW_HEIGHT_ITEM ≈ 32).
  const int rowH = UiScale::dp(32);
  const int rad = UiScale::dp(BlopStyle::radiusMdDp());
  const QString selFill =
      sel.arg(accColor.red()).arg(accColor.green()).arg(accColor.blue());
  const bool darkChrome = BlopStyle::librarySidebar().lightness() < 148;
  setStyleSheet(
      QStringLiteral(
          "QWidget#LibraryExplorerTree { background: %1; border: none; }"
          "QLabel#LibraryExplorerSectionLabel {"
          "  color: %6; font-size: 10px; font-weight: 700;"
          "  letter-spacing: 0.8px; background: transparent;"
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
          "  background: transparent;"
          "}")
          .arg(bg, ink, hover, QString::number(rowH), selFill, muted, acc,
               QString::number(rad)));
  if (m_btnNewFolder)
    m_btnNewFolder->setStyleSheet(
        BlopStyle::framedChromeIconButtonQss(darkChrome, 8, 28));
}
