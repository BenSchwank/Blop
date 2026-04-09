#include "multipagenoteview.h"
#include "SelectionMenuIcons.h"
#include "TransformOverlay.h"
#include "editoroverlays.h"
#include "UIStyles.h"
#include "uiscale.h"
#include "tools/AbstractTool.h"
#include "tools/AbstractStrokeTool.h"
#include "tools/RulerTool.h" // NEU: Lineal-Werkzeug
#include "tools/ToolManager.h"
#include "tools/StrokeItem.h"
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPen>
#include <QPointingDevice>
#include <QPolygonF>
#include <QScrollBar>
#include <QClipboard>
#include <QGuiApplication>
#ifdef BLOP_HAS_PDF
#include <QPdfDocument>
#endif
#include <algorithm>
#include <cmath>
#include <QtGlobal>
#include <QPushButton>
#include <QSizePolicy>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QAction>
#include <QEvent>
#include <QPalette>
#include <QShowEvent>
#include <QUndoCommand>

class StrokeAddUndoCommand : public QUndoCommand {
public:
  StrokeAddUndoCommand(MultiPageNoteView *view, int pageIdx, Stroke stroke)
      : QUndoCommand(), m_view(view), m_page(pageIdx),
        m_stroke(std::move(stroke)), m_item(nullptr), m_index(-1) {}
  ~StrokeAddUndoCommand() override { delete m_item; }

  void undo() override {
    if (!m_view || !m_view->note_ || m_page < 0 ||
        m_page >= m_view->note_->pages.size())
      return;
    auto &strokes = m_view->note_->pages[m_page].strokes;
    if (m_index < 0 || m_index >= strokes.size())
      return;
    strokes.removeAt(m_index);
    delete m_item;
    m_item = nullptr;
    if (m_view->onSaveRequested)
      m_view->onSaveRequested(m_view->note_);
  }

  void redo() override {
    if (!m_view || !m_view->note_)
      return;
    m_view->note_->ensurePage(m_page);
    auto &strokes = m_view->note_->pages[m_page].strokes;
    if (m_index < 0) {
      m_index = strokes.size();
      strokes.append(m_stroke);
    } else {
      strokes.insert(m_index, m_stroke);
    }
    m_item = m_view->createStrokeGraphicsItem(m_stroke);
    if (m_page >= 0 && m_page < m_view->pageItems_.size() &&
        m_view->pageItems_[m_page])
      m_item->setParentItem(m_view->pageItems_[m_page]);
    if (m_view->onSaveRequested)
      m_view->onSaveRequested(m_view->note_);
  }

private:
  MultiPageNoteView *m_view;
  int m_page;
  Stroke m_stroke;
  QGraphicsPathItem *m_item;
  int m_index;
};

class NoteSelectionMenu : public QWidget {
  Q_OBJECT
public:
  explicit NoteSelectionMenu(QWidget *parent = nullptr) : QWidget(parent) {
    setStyleSheet(
        "QWidget { background-color: #252526; border-radius: 8px; border: 1px "
        "solid #444; }"
        "QPushButton { background: transparent; border: none; color: white; "
        "font-weight: bold; padding: 5px 8px; font-size: 14px; }"
        "QPushButton:hover { background-color: #3E3E42; border-radius: 4px; }");
    setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(2);
    auto addIconBtn = [&](const QIcon &ico, const QString &tip, auto signal) {
      auto *btn = new QPushButton(this);
      btn->setIcon(ico);
      btn->setIconSize(QSize(22, 22));
      btn->setToolTip(tip);
      btn->setFlat(true);
      connect(btn, &QPushButton::clicked, this, signal);
      layout->addWidget(btn);
    };
    addIconBtn(SelectionMenuIcons::cutIcon(), QStringLiteral("Ausschneiden"),
               &NoteSelectionMenu::cutRequested);
    addIconBtn(SelectionMenuIcons::copyIcon(), QStringLiteral("Kopieren"),
               &NoteSelectionMenu::copyRequested);
    addIconBtn(SelectionMenuIcons::colorIcon(), QStringLiteral("Farbe"),
               &NoteSelectionMenu::colorRequested);
    addIconBtn(SelectionMenuIcons::cropIcon(), QStringLiteral("Zuschneiden"),
               &NoteSelectionMenu::cropRequested);
    addIconBtn(SelectionMenuIcons::screenshotIcon(), QStringLiteral("Screenshot"),
               &NoteSelectionMenu::screenshotRequested);
    QFrame *line = new QFrame;
    line->setFrameShape(QFrame::VLine);
    line->setStyleSheet("color: #555;");
    layout->addWidget(line);
    QPushButton *btnDel = new QPushButton(this);
    btnDel->setIcon(SelectionMenuIcons::trashIcon());
    btnDel->setIconSize(QSize(22, 22));
    btnDel->setToolTip(QStringLiteral("Löschen"));
    btnDel->setFlat(true);
    btnDel->setStyleSheet("color: #FF5555;");
    connect(btnDel, &QPushButton::clicked, this,
            &NoteSelectionMenu::deleteRequested);
    layout->addWidget(btnDel);
    adjustSize();
    hide();
  }
signals:
  void deleteRequested();
  void duplicateRequested();
  void copyRequested();
  void cutRequested();
  void colorRequested();
  void screenshotRequested();
  void cropRequested();
};

static int a4wPx() {
#ifdef Q_OS_ANDROID
  return qRound(793 * (UiScale::dp(100) / 100.0));
#else
  return 793;
#endif
}
static int a4hPx() {
#ifdef Q_OS_ANDROID
  return qRound(1122 * (UiScale::dp(100) / 100.0));
#else
  return 1122;
#endif
}
static int pageSpacingPx() { return UiScale::dp(60); }
static float kBottomRevealPull() { return UiScale::dp(100); }
/// Abstand Seitenunterkante → Panel (Szenenkoordinaten)
static qreal kSheetGapBelowPage() { return UiScale::dp(40); }
/// Zusätzlicher Scrollbereich unter dem Inhalt (~eine A4-Höhe)
static qreal kExtraScrollBelowPages() { return static_cast<qreal>(a4hPx()); }
/// Szene: Leiste unter letzter Seite — schmaler als A4, zur Seite zentriert
static int kPagesBarStripHeight() { return UiScale::dp(380); }
/// Anteil der A4-Breite (Rest links/rechts frei = optisch zentriert zur Seite)
static constexpr qreal kPagesBarStripWidthRatio = 0.86;
/// Platz unter der Leiste zum bequemen Scrollen
static qreal kSceneReserveBelowPagesBar() { return UiScale::dp(200); }
/// Abstand Panel → untere Viewport-Kante (sonst wirkt es mit Taskbar/Fensterrand „verschmolzen“)
static int kBottomSheetViewportBottomInset() {
  return UiScale::dp(18) + UiScale::androidBottomInsetPx();
}
/// Leichter seitlicher Luftabstand zum Viewport-Rand (schwebende Karte)
static int kBottomSheetViewportSideInset() { return UiScale::dp(10); }
/// Zielhöhe des Panels (wirkt beim Scrollen stabil, nicht „zusammengedrückt“)
static int kBottomSheetPreferredHeight() { return UiScale::dp(300); }
/// Unter dieser Höhe zwischen letzter Seite und unterem Rand lieber ausblenden
static int kBottomSheetMinVisibleHeight() { return UiScale::dp(208); }

#ifdef Q_OS_ANDROID
static void applyGraphicsViewCanvasBackground(QGraphicsView *view) {
  if (!view)
    return;
  const QColor bg = UIStyles::SceneBackground;
  view->setBackgroundBrush(bg);
  view->setFrameShape(QFrame::NoFrame);
  view->setCacheMode(QGraphicsView::CacheNone);
  if (QWidget *vp = view->viewport()) {
    vp->setAutoFillBackground(true);
    QPalette pal = vp->palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base, bg);
    vp->setPalette(pal);
  }
}
#endif

MultiPageNoteView::MultiPageNoteView(QWidget *parent) : QGraphicsView(parent) {
  m_undoStack = new QUndoStack(this);

  setScene(&scene_);
  scene_.setItemIndexMethod(QGraphicsScene::NoIndex);
#ifdef Q_OS_ANDROID
  applyGraphicsViewCanvasBackground(this);
#else
  setBackgroundBrush(UIStyles::SceneBackground);
#endif

  // Gesten auf dem Viewport registrieren
  viewport()->grabGesture(Qt::PinchGesture);

  setOptimizationFlags(QGraphicsView::DontSavePainterState |
                       QGraphicsView::DontAdjustForAntialiasing);
#ifdef Q_OS_ANDROID
  setRenderHints(QPainter::Antialiasing);
#else
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
#endif
  setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);

  setDragMode(QGraphicsView::NoDrag);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setAcceptDrops(false);

  viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

  // NEU: Tool-Handling inkl. Lineal
  connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
          [this](AbstractTool *tool) {
            // Wenn Lineal gewählt ist, sicherstellen, dass es existiert
            if (tool && tool->mode() == ToolMode::Ruler) {
              RulerTool::ensureRulerExists(&scene_,
                                           ToolManager::instance().config());
            }
            viewport()->update();
          });

  // NEU: Config-Änderungen (z.B. Lineal-Einheiten)
  connect(&ToolManager::instance(), &ToolManager::configChanged, this,
          [this](const ToolConfig &config) {
            if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
              RulerTool::ensureRulerExists(&scene_, config);
            }
          });

  m_selectionMenu = new NoteSelectionMenu(this);
  connect(&scene_, &QGraphicsScene::selectionChanged, this, &MultiPageNoteView::onSelectionChanged);
  connect(m_selectionMenu, &NoteSelectionMenu::deleteRequested, this, &MultiPageNoteView::deleteSelection);
  connect(m_selectionMenu, &NoteSelectionMenu::copyRequested, this, &MultiPageNoteView::copySelection);
  connect(m_selectionMenu, &NoteSelectionMenu::cutRequested, this, &MultiPageNoteView::cutSelection);
  connect(m_selectionMenu, &NoteSelectionMenu::colorRequested, this, &MultiPageNoteView::changeSelectionColor);
  connect(m_selectionMenu, &NoteSelectionMenu::screenshotRequested, this, &MultiPageNoteView::screenshotSelection);

  connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this]() { syncPagesBarVisibility(); });
  connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
          [this]() { syncPagesBarVisibility(); });
  // Overlay über der Skeleton-Fläche — muss Maus-Events fangen (kein
  // WA_TransparentForMouseEvents, sonst gehen Klicks zur QGraphicsView durch).
  m_bottomSheet = new QWidget(this);

  m_pagesBarCard = new QFrame(m_bottomSheet);
  m_pagesBarCard->setObjectName(QStringLiteral("PagesBarStrip"));
  m_pagesBarCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_pagesBarCard->setStyleSheet(QStringLiteral(
      "#PagesBarStrip {"
      "  background-color: rgba(38, 40, 52, 0.96);"
      "  border: 1px solid rgba(120, 130, 160, 0.28);"
      "  border-radius: 20px;"
      "}"
      "#PagesBarStrip QPushButton#PagesBarPrimary {"
      "  background-color: rgba(58, 60, 72, 0.98);"
      "  border: 1px solid rgba(120, 130, 160, 0.35);"
      "  border-radius: 22px;"
      "  padding: 18px 16px;"
      "  min-height: 148px;"
      "  max-height: 196px;"
      "  text-align: center;"
      "}"
      "#PagesBarStrip QPushButton#PagesBarPrimary:hover {"
      "  background-color: rgba(68, 70, 86, 0.98);"
      "  border-color: rgba(140, 150, 185, 0.5);"
      "}"
      "#PagesBarStrip QLabel#PagesBarPrimaryIcon {"
      "  color: #6BA3F5;"
      "  font-size: 40px; font-weight: 500;"
      "  background: transparent;"
      "}"
      "#PagesBarStrip QLabel#PagesBarPrimaryCaption {"
      "  color: #6BA3F5;"
      "  font-size: 15px; font-weight: 600; letter-spacing: 0.15px;"
      "  background: transparent;"
      "}"
      "#PagesBarStrip QPushButton {"
      "  background-color: rgba(52, 54, 64, 0.98);"
      "  border: 1px solid rgba(110, 115, 140, 0.4);"
      "  border-radius: 16px;"
      "  color: rgba(220, 222, 232, 0.95);"
      "  font-size: 12px; font-weight: 600;"
      "  padding: 10px 8px; min-height: 48px; max-height: 56px;"
      "}"
      "#PagesBarStrip QPushButton:hover {"
      "  background-color: rgba(52, 54, 68, 0.98);"
      "  border-color: rgba(130, 115, 185, 0.45);"
      "}"
      "#PagesBarStrip QToolButton {"
      "  background-color: rgba(52, 54, 64, 0.98);"
      "  border: 1px solid rgba(110, 115, 140, 0.4);"
      "  border-radius: 16px;"
      "  color: rgba(220, 222, 232, 0.95);"
      "  font-size: 12px; font-weight: 600;"
      "  padding: 10px 8px; min-height: 48px; max-height: 56px;"
      "}"
      "#PagesBarStrip QToolButton:hover {"
      "  background-color: rgba(52, 54, 68, 0.98);"
      "  border-color: rgba(130, 115, 185, 0.45);"
      "}"));

  auto *outerLay = new QVBoxLayout(m_bottomSheet);
  outerLay->setContentsMargins(0, 0, 0, 0);
  outerLay->setSpacing(0);
  outerLay->addWidget(m_pagesBarCard, 1);

  auto *stripLay = new QVBoxLayout(m_pagesBarCard);
  stripLay->setContentsMargins(18, 16, 18, 16);
  stripLay->setSpacing(14);

  auto *primaryBtn = new QPushButton(m_pagesBarCard);
  primaryBtn->setObjectName(QStringLiteral("PagesBarPrimary"));
  primaryBtn->setCursor(Qt::PointingHandCursor);
  primaryBtn->setToolTip(QStringLiteral("Neue Seite"));
  primaryBtn->setAutoDefault(false);
  primaryBtn->setDefault(false);
  primaryBtn->setFocusPolicy(Qt::NoFocus);
  primaryBtn->setMinimumHeight(148);
  primaryBtn->setMaximumHeight(196);
  primaryBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  auto *primaryLay = new QVBoxLayout(primaryBtn);
  primaryLay->setContentsMargins(0, 0, 0, 0);
  primaryLay->setSpacing(6);
  primaryLay->setAlignment(Qt::AlignCenter);

  auto *lblPlus = new QLabel(QStringLiteral("＋"), primaryBtn);
  lblPlus->setObjectName(QStringLiteral("PagesBarPrimaryIcon"));
  lblPlus->setAlignment(Qt::AlignCenter);
  lblPlus->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  auto *lblCaption = new QLabel(QStringLiteral("Neue Seite"), primaryBtn);
  lblCaption->setObjectName(QStringLiteral("PagesBarPrimaryCaption"));
  lblCaption->setAlignment(Qt::AlignCenter);
  lblCaption->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  primaryLay->addWidget(lblPlus, 0, Qt::AlignHCenter);
  primaryLay->addWidget(lblCaption, 0, Qt::AlignHCenter);
  stripLay->addStretch(1);
  stripLay->addWidget(primaryBtn, 0);

  auto *bottomRow = new QHBoxLayout();
  bottomRow->setSpacing(10);
  auto *btnTpl = new QPushButton(QStringLiteral("Vorlagen"), m_pagesBarCard);
  btnTpl->setToolTip(QStringLiteral("Vorlagen"));
  auto *btnImport = new QToolButton(m_pagesBarCard);
  btnImport->setText(QStringLiteral("Importieren"));
  btnImport->setToolTip(QStringLiteral("PDF oder Bild importieren"));
  btnImport->setPopupMode(QToolButton::InstantPopup);
  btnImport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *importMenu = new QMenu(btnImport);
  QAction *actPdf = importMenu->addAction(QStringLiteral("PDF …"));
  QAction *actImg = importMenu->addAction(QStringLiteral("Bild …"));
  connect(actPdf, &QAction::triggered, this,
          [this]() { pickAndImportPdf(); });
  connect(actImg, &QAction::triggered, this,
          [this]() { pickAndAddImagePage(); });
  btnImport->setMenu(importMenu);
  auto *btnMore = new QPushButton(QStringLiteral("Mehr"), m_pagesBarCard);
  btnMore->setToolTip(QStringLiteral("Mehr"));
  btnTpl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  btnMore->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  bottomRow->addWidget(btnTpl, 1);
  bottomRow->addWidget(btnImport, 1);
  bottomRow->addWidget(btnMore, 1);
  stripLay->addLayout(bottomRow);

  connect(primaryBtn, &QPushButton::clicked, this, [this]() {
    addNewPage();
    if (note_)
      scrollToPage(note_->pages.size() - 1);
  });
  connect(btnTpl, &QPushButton::clicked, this, [this]() { openTemplatePageDialog(); });
  connect(btnMore, &QPushButton::clicked, this, [this]() { showBottomMoreMenu(); });
  m_bottomSheet->hide();
}

void MultiPageNoteView::setNote(Note *note) {
  note_ = note;
  if (m_undoStack)
    m_undoStack->clear();
  scene_.clear();
  pageItems_.clear();
  m_pagesBarAnchorStrip = nullptr;

  layoutPages();

  // Wenn wir beim Laden im Lineal-Modus sind, muss das Lineal wiederhergestellt
  // werden (da scene_.clear() es gelöscht hat)
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
    RulerTool::ensureRulerExists(&scene_, ToolManager::instance().config());
  }

  if (!note_)
    return;

  bool wasBlocked = scene_.blockSignals(true);

  for (int i = 0; i < note_->pages.size(); ++i) {
    for (const auto &s : note_->pages[i].strokes) {
      StrokeItem::StrokeStyle type = StrokeItem::Normal;
      QPen pen;
      if (s.isEraser) {
        pen = QPen(UIStyles::PageBackground, s.width);
        type = StrokeItem::Eraser;
      } else if (s.isHighlighter) {
        QColor c = s.color;
        c.setAlpha(80);
        pen = QPen(c, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        type = StrokeItem::Highlighter;
      } else {
        pen = QPen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        type = StrokeItem::Normal;
      }

      auto item = new StrokeItem(s.path, pen, QVector<StrokePoint>(), type);
      
      if (s.isEraser || (!s.isEraser && !s.isHighlighter)) {
          item->setZValue(1.0);
      } else if (s.isHighlighter) {
          item->setZValue(0.5);
      }
      
      item->setFlag(QGraphicsItem::ItemIsSelectable, true);
      item->setFlag(QGraphicsItem::ItemIsMovable, true);
      scene_.addItem(item);
      item->setPos(pageRect(i).topLeft());
    }
  }
  scene_.blockSignals(wasBlocked);
  syncPagesBarVisibility();
}

QGraphicsPathItem *MultiPageNoteView::createStrokeGraphicsItem(const Stroke &s) {
  auto *pathItem = new QGraphicsPathItem(s.path);
  QPen pen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  if (s.isEraser) {
    pen.setColor(Qt::white);
    pen.setWidthF(s.width * 2);
  } else if (s.isHighlighter) {
    pen.setColor(QColor(s.color.red(), s.color.green(), s.color.blue(), 100));
  }
  pathItem->setPen(pen);
  pathItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
  pathItem->setFlag(QGraphicsItem::ItemIsMovable, true);
  pathItem->setZValue(s.isHighlighter ? 0.5 : 1.0);
  return pathItem;
}

void MultiPageNoteView::pushStrokeUndoCommand(int pageIdx, Stroke stroke) {
  if (!m_undoStack || !note_)
    return;
  m_undoStack->push(new StrokeAddUndoCommand(this, pageIdx, std::move(stroke)));
}

void MultiPageNoteView::undo() {
  if (m_undoStack)
    m_undoStack->undo();
}

void MultiPageNoteView::redo() {
  if (m_undoStack)
    m_undoStack->redo();
}

bool MultiPageNoteView::canUndo() const {
  return m_undoStack && m_undoStack->canUndo();
}

bool MultiPageNoteView::canRedo() const {
  return m_undoStack && m_undoStack->canRedo();
}

void MultiPageNoteView::layoutPages() {
  if (m_undoStack)
    m_undoStack->clear();
  if (m_pagesBarAnchorStrip) {
    scene_.removeItem(m_pagesBarAnchorStrip);
    delete m_pagesBarAnchorStrip;
    m_pagesBarAnchorStrip = nullptr;
  }
  // Vorhandene Seiten-Items entfernen
  for (auto *item : pageItems_) {
    const QList<QGraphicsItem *> kids = item->childItems();
    for (QGraphicsItem *c : kids)
      c->setParentItem(nullptr);
    scene_.removeItem(item);
    delete item;
  }
  pageItems_.clear();

  if (!note_) {
    syncPagesBarVisibility();
    return;
  }
  int n = std::max(1, (int)note_->pages.size());
  qreal y = 0;

  for (int i = 0; i < n; ++i) {
    auto pageItem = new PageItem(0, 0, a4wPx(), a4hPx());
    // Seiten ganz unten (Z=0 oder negativ), damit Lineal drüber ist
    pageItem->setZValue(0);
    if (i < note_->pages.size()) {
      const NotePage &pg = note_->pages[i];
      const int bt = qBound(0, pg.backgroundType,
                            static_cast<int>(PageBackgroundType::Legal));
      pageItem->setType(static_cast<PageBackgroundType>(bt));
      pageItem->setPaperColor(pg.paperColor.isValid() ? pg.paperColor
                                                       : QColor(Qt::white));
      if (!pg.backgroundImage.isNull())
        pageItem->setBackgroundImage(pg.backgroundImage);
    }
    scene_.addItem(pageItem);
    pageItem->setPos(0, y);
    pageItems_.push_back(pageItem);
    y += a4hPx() + pageSpacingPx();
  }
  const qreal stripW =
      qMax(320.0, static_cast<qreal>(qRound(a4wPx() * kPagesBarStripWidthRatio)));
  const qreal stripX = (a4wPx() - stripW) / 2.0;
  m_pagesBarAnchorStrip =
      new SkeletonPageItem(stripX, y, stripW, kPagesBarStripHeight());
  scene_.addItem(m_pagesBarAnchorStrip);

  const qreal stripBottom = y + kPagesBarStripHeight();
  const qreal sceneBottom =
      stripBottom + kSheetGapBelowPage() + kSceneReserveBelowPagesBar();
  scene_.setSceneRect(QRectF(-100, -100, a4wPx() + 200, sceneBottom));
  ensureSceneRectCoversViewport();
  syncPagesBarVisibility();
}

void MultiPageNoteView::toggleRuler(bool active) {
    if (active) {
        ToolConfig config = ToolManager::instance().config();
        const QPointF spawn = mapToScene(viewport()->rect().center());
        RulerTool::ensureRulerExists(&scene_, config, spawn);
    } else {
        for (QGraphicsItem *item : scene_.items()) {
            if (item->type() == RulerItem::Type) {
                item->setVisible(false);
                break;
            }
        }
    }
}

QRectF MultiPageNoteView::pageRect(int idx) const {
  qreal y = idx * (a4hPx() + pageSpacingPx());
  return QRectF(0, y, a4wPx(), a4hPx());
}

int MultiPageNoteView::pageAt(const QPointF &scenePos) const {
  for (int i = 0; i < pageItems_.size(); ++i) {
    if (pageItems_[i]->sceneBoundingRect().contains(scenePos))
      return i;
  }
  return -1;
}

void MultiPageNoteView::addNewPage() {
  if (!note_)
    return;
  note_->ensurePage(note_->pages.size());
  layoutPages();

  // Auch hier: Sicherstellen, dass Lineal oben bleibt, falls layoutPages()
  // Z-Order ändert
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
    RulerTool::ensureRulerExists(&scene_, ToolManager::instance().config());
  }

  if (onSaveRequested)
    onSaveRequested(note_);
}

void MultiPageNoteView::addNewPageWithLayout(int backgroundType,
                                             const QColor &paperColor) {
  if (!note_)
    return;
  int idx = note_->pages.size();
  note_->ensurePage(idx);
  note_->pages[idx].backgroundType =
      qBound(0, backgroundType, static_cast<int>(PageBackgroundType::Legal));
  note_->pages[idx].paperColor =
      paperColor.isValid() ? paperColor : QColor(Qt::white);
  note_->pages[idx].backgroundImage = QImage();
  layoutPages();
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
    RulerTool::ensureRulerExists(&scene_, ToolManager::instance().config());
  }
  if (onSaveRequested)
    onSaveRequested(note_);
}

void MultiPageNoteView::showBottomSheetFromPull() {
  m_pullDistance = 0.f;
  syncPagesBarVisibility();
  if (m_pagesBarAnchorStrip) {
    QRectF r = m_pagesBarAnchorStrip->sceneBoundingRect();
    r.adjust(-40, -100, 40, kSceneReserveBelowPagesBar() + 160);
    ensureVisible(r, 32, 32);
  } else if (note_ && !pageItems_.isEmpty()) {
    const qreal lb = lastPageBottomSceneY();
    const QRectF reveal(0, lb - a4hPx() * 0.35, a4wPx(),
                        a4hPx() * 0.65 + kSheetGapBelowPage());
    ensureVisible(reveal, 24, 24);
  }
  syncPagesBarVisibility();
}

void MultiPageNoteView::hideBottomSheet() {
  if (m_bottomSheet)
    m_bottomSheet->hide();
  if (m_pagesBarAnchorStrip)
    m_pagesBarAnchorStrip->setVisible(true);
  m_pullDistance = 0.f;
}

void MultiPageNoteView::syncPagesBarVisibility() {
  if (!m_bottomSheet)
    return;
  if (!note_ || pageItems_.isEmpty() ||
      !isSkeletonStripIntersectingViewport()) {
    m_bottomSheet->hide();
    if (m_pagesBarAnchorStrip)
      m_pagesBarAnchorStrip->setVisible(true);
    m_pullDistance = 0.f;
    return;
  }
  if (m_pagesBarAnchorStrip)
    m_pagesBarAnchorStrip->setVisible(false);
  m_bottomSheet->show();
  m_bottomSheet->raise();
  updateBottomSheetGeometry();
}

qreal MultiPageNoteView::lastPageBottomSceneY() const {
  if (!pageItems_.isEmpty())
    return pageItems_.last()->sceneBoundingRect().bottom();
  return a4hPx();
}

bool MultiPageNoteView::isSkeletonStripIntersectingViewport() const {
  if (!viewport() || !m_pagesBarAnchorStrip)
    return false;
  const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
  return vis.intersects(m_pagesBarAnchorStrip->sceneBoundingRect());
}

void MultiPageNoteView::updateBottomSheetGeometry() {
  if (!m_bottomSheet || !m_bottomSheet->isVisible())
    return;
  if (!m_pagesBarAnchorStrip || !viewport()) {
    m_bottomSheet->hide();
    return;
  }
  const QRectF sr = m_pagesBarAnchorStrip->sceneBoundingRect();
  const QRect skView =
      mapFromScene(QPolygonF(sr)).boundingRect();

  if (m_pagesBarCard && m_pagesBarCard->layout())
    m_pagesBarCard->layout()->activate();
  if (QLayout *lay = m_bottomSheet->layout())
    lay->activate();
  m_bottomSheet->adjustSize();

  const int floorY = QGraphicsView::mapFromScene(
                         QPointF(a4wPx() * 0.5, lastPageBottomSceneY()))
                         .y();

  int x = skView.left() + kBottomSheetViewportSideInset();
  int w = skView.width() - 2 * kBottomSheetViewportSideInset();
  x = qBound(0, x, qMax(0, width() - 1));
  w = qMin(qMax(w, 0), width() - x);

  const int maxBottomY = height() - kBottomSheetViewportBottomInset();
  const int bottom = qMin(skView.bottom(), maxBottomY);
  const int floorYBound = qMax(floorY, 0);
  const int availableBelowPage = bottom - floorYBound;
  if (availableBelowPage < kBottomSheetMinVisibleHeight() || w < 120) {
    m_bottomSheet->hide();
    if (m_pagesBarAnchorStrip)
      m_pagesBarAnchorStrip->setVisible(true);
    return;
  }

  const int h = qMin(kBottomSheetPreferredHeight(), availableBelowPage);
  int y = bottom - h;
  y = qMax(y, floorY);
  y = qMax(y, 0);
  const int hClamped = bottom - y;
  if (hClamped < kBottomSheetMinVisibleHeight()) {
    m_bottomSheet->hide();
    if (m_pagesBarAnchorStrip)
      m_pagesBarAnchorStrip->setVisible(true);
    return;
  }

  m_bottomSheet->setGeometry(x, y, w, hClamped);
  m_bottomSheet->raise();
}

void MultiPageNoteView::openTemplatePageDialog() {
  A4LayoutDialogResult r;
  if (!showA4LayoutOverlay(this, QStringLiteral("Neue Seite von Vorlage"),
                           QString(), static_cast<int>(PageBackgroundType::Grid),
                           QColor(Qt::white), &r))
    return;
  addNewPageWithLayout(r.backgroundType, r.paperColor);
  if (note_)
    scrollToPage(note_->pages.size() - 1);
}

int MultiPageNoteView::visiblePageIndex() const {
  if (!note_ || note_->pages.isEmpty() || !viewport())
    return 0;
  const QPoint center(viewport()->width() / 2, viewport()->height() / 2);
  const QPointF scenePt = mapToScene(center);
  int p = pageAt(scenePt);
  if (p < 0)
    p = 0;
  return qBound(0, p, note_->pages.size() - 1);
}

void MultiPageNoteView::applyLayoutToPage(int pageIndex, int backgroundType,
                                          const QColor &paperColor) {
  if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size())
    return;
  NotePage &pg = note_->pages[pageIndex];
  pg.backgroundType =
      qBound(0, backgroundType, static_cast<int>(PageBackgroundType::Legal));
  pg.paperColor = paperColor.isValid() ? paperColor : QColor(Qt::white);
  pg.backgroundImage = QImage();
  if (pageIndex >= 0 && pageIndex < pageItems_.size() && pageItems_[pageIndex]) {
    PageItem *pi = pageItems_[pageIndex];
    pi->setType(static_cast<PageBackgroundType>(pg.backgroundType));
    pi->setPaperColor(pg.paperColor);
    pi->setBackgroundImage(QImage());
  }
  if (onSaveRequested)
    onSaveRequested(note_);
}

void MultiPageNoteView::openPageLayoutForVisiblePage() {
  if (!note_ || note_->pages.isEmpty())
    return;
  const int idx = visiblePageIndex();
  const NotePage &pg = note_->pages[idx];
  const int bt = qBound(0, pg.backgroundType,
                        static_cast<int>(PageBackgroundType::Legal));
  const QColor pc =
      pg.paperColor.isValid() ? pg.paperColor : QColor(Qt::white);
  const QString sub = QStringLiteral("Seite: %1").arg(pg.title);
  A4LayoutDialogResult r;
  if (!showA4LayoutOverlay(this, QStringLiteral("Seitenlayout"), sub, bt, pc,
                           &r))
    return;
  applyLayoutToPage(idx, r.backgroundType, r.paperColor);
}

void MultiPageNoteView::pickAndAddImagePage() {
  if (!note_)
    return;
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Bild wählen"), QString(),
      QStringLiteral("Bilder (*.png *.jpg *.jpeg *.webp *.bmp);;Alle Dateien (*)"));
  if (path.isEmpty())
    return;
  QImage img(path);
  if (img.isNull()) {
    QMessageBox::warning(this, QStringLiteral("Blop"),
                         QStringLiteral("Bild konnte nicht geladen werden."));
    return;
  }
  QImage scaled =
      img.scaled(a4wPx(), a4hPx(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QImage canvas(a4wPx(), a4hPx(), QImage::Format_ARGB32_Premultiplied);
  canvas.fill(Qt::white);
  {
    QPainter p(&canvas);
    p.drawImage((a4wPx() - scaled.width()) / 2, (a4hPx() - scaled.height()) / 2,
                scaled);
  }
  int idx = note_->pages.size();
  note_->ensurePage(idx);
  note_->pages[idx].backgroundImage = canvas;
  note_->pages[idx].backgroundType = static_cast<int>(PageBackgroundType::Blank);
  note_->pages[idx].paperColor = QColor(Qt::white);
  layoutPages();
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
    RulerTool::ensureRulerExists(&scene_, ToolManager::instance().config());
  }
  scrollToPage(idx);
  if (onSaveRequested)
    onSaveRequested(note_);
}

void MultiPageNoteView::pickAndImportPdf() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("PDF importieren"), QString(),
      QStringLiteral("PDF (*.pdf);;Alle Dateien (*)"));
  if (path.isEmpty())
    return;
  if (!importPdfPages(path)) {
    QMessageBox::information(
        this, QStringLiteral("PDF"),
        QStringLiteral(
            "PDF konnte nicht importiert werden (Datei ungültig oder PDF-Unterstützung "
            "nicht verfügbar)."));
  } else if (note_) {
    scrollToPage(note_->pages.size() - 1);
  }
}

void MultiPageNoteView::showBottomMoreMenu() {
  QMenu menu(this);
  if (note_ && !note_->pages.isEmpty()) {
    menu.addAction(QStringLiteral("Letzte Seite duplizieren"), [this]() {
      duplicatePage(note_->pages.size() - 1);
    });
  }
  menu.exec(QCursor::pos());
}

void MultiPageNoteView::ensureSceneRectCoversViewport() {
#ifdef Q_OS_ANDROID
  if (!viewport() || viewport()->width() <= 0 || viewport()->height() <= 0)
    return;
  const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
  if (vis.isEmpty())
    return;
  QRectF sr = scene_.sceneRect();
  scene_.setSceneRect(sr.united(vis.adjusted(-80, -80, 80, 80)));
#endif
}

void MultiPageNoteView::resizeEvent(QResizeEvent *e) {
  QGraphicsView::resizeEvent(e);
  syncPagesBarVisibility();
#ifdef Q_OS_ANDROID
  ensureSceneRectCoversViewport();
#endif
}

void MultiPageNoteView::showEvent(QShowEvent *e) {
  QGraphicsView::showEvent(e);
  syncPagesBarVisibility();
#ifdef Q_OS_ANDROID
  ensureSceneRectCoversViewport();
#endif
}

void MultiPageNoteView::wheelEvent(QWheelEvent *e) {
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
    const QPointF scenePos = mapToScene(e->position().toPoint());
    RulerItem *ruler = nullptr;
    for (QGraphicsItem *item : scene_.items()) {
      if (item->type() == RulerItem::Type) {
        ruler = static_cast<RulerItem *>(item);
        break;
      }
    }
    if (ruler && ruler->isVisible() &&
        ruler->canStartInteractionAtScenePos(scenePos)) {
      ruler->applyWheelDelta(e->angleDelta().y(), e->modifiers());
      e->accept();
      return;
    }
  }

  if (e->modifiers() & Qt::ControlModifier) {
    qreal delta = e->angleDelta().y() / 120.0;
    qreal factor = (delta > 0) ? 1.1 : 0.9;
    scale(factor, factor);
    zoom_ *= factor;
#ifdef Q_OS_ANDROID
    ensureSceneRectCoversViewport();
#endif
    syncPagesBarVisibility();
    e->accept();
  } else {
    QScrollBar *vb = verticalScrollBar();
    if (vb->value() >= vb->maximum() && e->angleDelta().y() < 0) {
      m_pullDistance += std::abs(e->angleDelta().y()) * 0.5f;
      if (m_pullDistance >= kBottomRevealPull()) {
        showBottomSheetFromPull();
      }
      e->accept();
      return;
    }

    if (m_pullDistance > 0.f && e->angleDelta().y() > 0)
      m_pullDistance = 0.f;

    QGraphicsView::wheelEvent(e);
  }
}

bool MultiPageNoteView::viewportEvent(QEvent *ev) {
  if (ev->type() == QEvent::Gesture) {
    gestureEvent(static_cast<QGestureEvent *>(ev));
    return true;
  }
  return QGraphicsView::viewportEvent(ev);
}

void MultiPageNoteView::gestureEvent(QGestureEvent *event) {
  bool handledByRuler = false;
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
    if (QGesture *pinch = event->gesture(Qt::PinchGesture)) {
      auto *pg = static_cast<QPinchGesture *>(pinch);
      const QPointF cScene = mapToScene(pg->centerPoint().toPoint());
      RulerItem *ruler = nullptr;
      for (QGraphicsItem *item : scene_.items()) {
        if (item->type() == RulerItem::Type) {
          ruler = static_cast<RulerItem *>(item);
          break;
        }
      }
      if (ruler && ruler->isVisible() && ruler->canStartInteractionAtScenePos(cScene)) {
        event->accept(pinch);
        event->accept();
        pinchTriggered(pg);
        handledByRuler = true;
      }
    }
  }
  if (handledByRuler)
    return;
  if (QGesture *pinch = event->gesture(Qt::PinchGesture)) {
    event->accept(pinch);
    pinchTriggered(static_cast<QPinchGesture *>(pinch));
  }
}

void MultiPageNoteView::pinchTriggered(QPinchGesture *gesture) {
  QPointF scenePos = mapToScene(gesture->centerPoint().toPoint());

  // Wenn das Ruler-Tool aktiv ist, sollen Pinch/Rotate-Gesten ausschließlich
  // das Lineal beeinflussen – die Seite selbst soll nicht zoomen oder scrollen.
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
    RulerItem *ruler = nullptr;
    for (QGraphicsItem *item : scene()->items()) {
      if (item->type() == RulerItem::Type) {
        ruler = static_cast<RulerItem *>(item);
        break;
      }
    }
    if (ruler && ruler->isVisible() &&
        ruler->canStartInteractionAtScenePos(scenePos)) {
      qreal rotationDelta =
          gesture->rotationAngle() - gesture->lastRotationAngle();
      ruler->applyRotationDelta(rotationDelta * 0.6);

      // Ruler leicht der Geste folgen lassen, ohne die Seite zu bewegen.
      QPointF diff = mapToScene(gesture->centerPoint().toPoint()) -
                     mapToScene(gesture->lastCenterPoint().toPoint());
      ruler->moveBy(diff.x(), diff.y());
      return;
    }
  }

  QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();

  if (gesture->state() == Qt::GestureStarted) {
    m_isZooming = true;
    m_isPanning = false;
    m_pullDistance = 0;
    viewport()->update();
  }

  if (changeFlags & QPinchGesture::ScaleFactorChanged) {
    qreal factor = gesture->scaleFactor();
    qreal currentScale = transform().m11();

    if (!((currentScale < 0.25 && factor < 1.0) ||
          (currentScale > 4.0 && factor > 1.0))) {
      scale(factor, factor);
      zoom_ *= factor;
#ifdef Q_OS_ANDROID
      ensureSceneRectCoversViewport();
#endif
      syncPagesBarVisibility();
    }
  }

  if (changeFlags & QPinchGesture::CenterPointChanged) {
    QPointF delta = gesture->centerPoint() - gesture->lastCenterPoint();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
  }

  if (gesture->state() == Qt::GestureFinished ||
      gesture->state() == Qt::GestureCanceled) {
    m_isZooming = false;
  }
}

void MultiPageNoteView::mousePressEvent(QMouseEvent *e) {
  if (m_isZooming) {
    e->accept();
    return;
  }

  const QInputDevice *dev = e->device();
  bool isTouch = (dev && dev->type() == QInputDevice::DeviceType::TouchScreen);
  if (!isTouch && e->source() == Qt::MouseEventSynthesizedBySystem)
    isTouch = true;

  const bool rulerMode =
      (ToolManager::instance().activeToolMode() == ToolMode::Ruler);

  if (isTouch) {
    if (rulerMode) {
      const QPointF scenePos = mapToScene(e->pos());
      RulerItem *ruler = nullptr;
      for (QGraphicsItem *item : scene_.items()) {
        if (item->type() == RulerItem::Type) {
          ruler = static_cast<RulerItem *>(item);
          break;
        }
      }
      m_isPanning = !(ruler && ruler->isVisible() &&
                      ruler->canStartInteractionAtScenePos(scenePos));
    } else if (m_penOnlyMode) {
      m_isPanning = true;
      m_lastPanPos = e->pos();
      setCursor(Qt::ClosedHandCursor);
      e->accept();
      return;
    } else {
      m_isPanning = false;
    }
  }

  // Auto-deselect if clicking explicitly outside the transform overlay
  // Or delegate to QGraphicsView if a handle is pressed
  if (m_transformOverlay) {
      if (!m_transformOverlay->isHandleAt(mapToScene(e->pos()))) {
          scene_.clearSelection();
          applyTransform();
      } else {
          QGraphicsView::mousePressEvent(e);
          return;
      }
  }

  // --- ToolManager & Ruler Logic ---
  AbstractTool *tool = ToolManager::instance().activeTool();
  if (tool && e->button() == Qt::LeftButton) {
    QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMousePress);
    scEvent.setScenePos(mapToScene(e->pos()));
    scEvent.setButton(e->button());
    scEvent.setButtons(e->buttons());
    scEvent.setModifiers(e->modifiers());

    // An scene_ weiterleiten, damit Lineal-Tool Events bekommt
    if (tool->handleMousePress(&scEvent, &scene_)) {
      e->accept();
      return;
    }
  }

  if (e->button() == Qt::LeftButton && tool &&
     (tool->mode() == ToolMode::Pen || tool->mode() == ToolMode::Eraser ||
      tool->mode() == ToolMode::Highlighter || tool->mode() == ToolMode::Pencil)) {
    QTabletEvent fake(QEvent::TabletPress,
                      QPointingDevice::primaryPointingDevice(), e->position(),
                      e->globalPosition(), 1.0, 0, 0, 0, 0, 0, e->modifiers(),
                      e->button(), e->buttons());
    tabletEvent(&fake);
  } else {
    QGraphicsView::mousePressEvent(e);
  }
}

void MultiPageNoteView::mouseMoveEvent(QMouseEvent *e) {
  if (m_isZooming) {
    e->accept();
    return;
  }

  if (m_isPanning) {
    QPoint delta = e->pos() - m_lastPanPos;
    m_lastPanPos = e->pos();

    QScrollBar *vb = verticalScrollBar();
    if (vb->value() >= vb->maximum() && delta.y() < 0) {
      m_pullDistance += std::abs(delta.y()) * 0.5f;
      if (m_pullDistance >= kBottomRevealPull())
        showBottomSheetFromPull();
    } else {
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                      delta.x());
      verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
      if (m_pullDistance > 0.f)
        m_pullDistance = 0.f;
    }

    e->accept();
    return;
  }

  const QInputDevice *dev = e->device();
  bool isTouch = (dev && dev->type() == QInputDevice::DeviceType::TouchScreen);
  if (!isTouch && e->source() == Qt::MouseEventSynthesizedBySystem)
    isTouch = true;

  if (m_penOnlyMode && isTouch && !m_isPanning) {
    e->accept();
    return;
  }

  // --- ToolManager Logic ---
  AbstractTool *tool = ToolManager::instance().activeTool();
  if (tool) {
    QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMouseMove);
    scEvent.setScenePos(mapToScene(e->pos()));
    scEvent.setButtons(e->buttons());
    scEvent.setModifiers(e->modifiers());

    if (tool->handleMouseMove(&scEvent, &scene_)) {
      e->accept();
      return;
    }
  }
  // -----------------------------------

  if (e->buttons() & Qt::LeftButton && drawing_) {
    QTabletEvent fake(QEvent::TabletMove,
                      QPointingDevice::primaryPointingDevice(), e->position(),
                      e->globalPosition(), 1.0, 0, 0, 0, 0, 0, e->modifiers(),
                      e->button(), e->buttons());
    tabletEvent(&fake);
  } else {
    QGraphicsView::mouseMoveEvent(e);
  }
}

void MultiPageNoteView::commitPendingStrokeItemsToNote(AbstractTool *tool) {
  if (!tool || !note_)
    return;
  QList<QGraphicsItem *> itemsToRemove;
  for (QGraphicsItem *item : scene_.items()) {
    if (!item->parentItem() && item->type() == QGraphicsItem::UserType + 1) {
      auto *strokeItem = static_cast<StrokeItem *>(item);
      auto points = strokeItem->points();
      if (!points.isEmpty()) {
        int pIdx = pageAt(points.first().pos);
        if (pIdx >= 0 && note_) {
          note_->ensurePage(pIdx);
          QPointF pageTopLeft = pageRect(pIdx).topLeft();

          Stroke s;
          s.pageIndex = pIdx;
          s.color = strokeItem->pen().color();
          s.width = strokeItem->pen().width();
          s.isEraser = (strokeItem->strokeStyle() == StrokeItem::Eraser);
          s.isHighlighter =
              (strokeItem->strokeStyle() == StrokeItem::Highlighter);

          QPainterPath localPath;
          for (int i = 0; i < points.size(); ++i) {
            QPointF localP = points[i].pos - pageTopLeft;
            s.points.append(localP);
            if (i == 0)
              localPath.moveTo(localP);
            else
              localPath.lineTo(localP);
          }
          s.path = localPath;
          pushStrokeUndoCommand(pIdx, std::move(s));
        }
      }
      itemsToRemove.append(item);
    }
  }

  for (auto *item : itemsToRemove) {
    scene_.removeItem(item);
    delete item;
  }

  if (tool->mode() == ToolMode::Lasso && !scene_.selectedItems().isEmpty())
    startTransformSession();
}

void MultiPageNoteView::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pullDistance > 0.f)
    m_pullDistance = 0.f;

  if (m_isPanning) {
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
    e->accept();
    return;
  }

  if (m_isZooming) {
    e->accept();
    return;
  }

  // --- ToolManager Logic ---
  AbstractTool *tool = ToolManager::instance().activeTool();
  if (tool && e->button() == Qt::LeftButton) {
    QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMouseRelease);
    scEvent.setScenePos(mapToScene(e->pos()));
    scEvent.setButton(e->button());

    if (tool->handleMouseRelease(&scEvent, &scene_)) {
      commitPendingStrokeItemsToNote(tool);
      e->accept();
      return;
    }
  }
  // --------------------------------------

  if (drawing_) {
    QTabletEvent fake(QEvent::TabletRelease,
                      QPointingDevice::primaryPointingDevice(), e->position(),
                      e->globalPosition(), 1.0, 0, 0, 0, 0, 0, e->modifiers(),
                      e->button(), e->buttons());
    tabletEvent(&fake);
  } else {
    QGraphicsView::mouseReleaseEvent(e);
  }
}

void MultiPageNoteView::tabletEvent(QTabletEvent *e) {
  AbstractTool *tool = ToolManager::instance().activeTool();
  if (tool && note_ && mode_ != ToolMode::Lasso) {
    QPointF scenePos = mapToScene(e->position().toPoint());
    if (auto *strokeTool = qobject_cast<AbstractStrokeTool *>(tool))
      strokeTool->setStrokeSceneForTablet(&scene_);
    if (tool->handleTabletEvent(e, scenePos)) {
      e->accept();
      if (e->type() == QEvent::TabletRelease)
        commitPendingStrokeItemsToNote(tool);
      return;
    }
  }

  if (!note_ || mode_ == ToolMode::Lasso) {
    e->ignore();
    return;
  }

  QPointF scenePos = mapToScene(e->position().toPoint());
  int p = pageAt(scenePos);
  if (p < 0) {
    e->ignore();
    return;
  }
  QPointF local = scenePos - pageRect(p).topLeft();

  if (e->type() == QEvent::TabletPress) {
    drawing_ = true;
    currentPage_ = p;
    currentStroke_ = Stroke{};
    currentStroke_.pageIndex = p;
    currentStroke_.isEraser = (mode_ == ToolMode::Eraser);
    currentStroke_.isHighlighter = (mode_ == ToolMode::Highlighter);

    if (currentStroke_.isEraser) {
      currentStroke_.width = 20.0;
      currentStroke_.color = UIStyles::PageBackground;
    } else if (currentStroke_.isHighlighter) {
      currentStroke_.width = 24.0;
      QColor c = penColor_;
      c.setAlpha(80);
      currentStroke_.color = c;
    } else {
      currentStroke_.width = std::max<qreal>(2.0, e->pressure() * 4.0);
      currentStroke_.color = penColor_;
    }

    currentStroke_.points.push_back(local);
    currentStroke_.path = QPainterPath(local);
    currentPathItem_ = new QGraphicsPathItem();
    QPen pen(currentStroke_.color, currentStroke_.width, Qt::SolidLine,
             Qt::RoundCap, Qt::RoundJoin);
    currentPathItem_->setPen(pen);
    currentPathItem_->setZValue(currentStroke_.isHighlighter ? 0.5 : 1.0);
    currentPathItem_->setFlag(QGraphicsItem::ItemIsSelectable, true);
    currentPathItem_->setFlag(QGraphicsItem::ItemIsMovable, true);
    scene_.addItem(currentPathItem_);
    currentPathItem_->setPos(pageRect(p).topLeft());
    e->accept();
  } else if (e->type() == QEvent::TabletMove && drawing_) {
    currentStroke_.points.push_back(local);
    currentStroke_.path.lineTo(local);
    currentPathItem_->setPath(currentStroke_.path);
    e->accept();
  } else if (e->type() == QEvent::TabletRelease && drawing_) {
    drawing_ = false;
    if (currentPathItem_) {
      scene_.removeItem(currentPathItem_);
      delete currentPathItem_;
      currentPathItem_ = nullptr;
    }
    if (note_)
      pushStrokeUndoCommand(currentPage_, std::move(currentStroke_));
    e->accept();
  } else {
    e->ignore();
  }
}

QPixmap MultiPageNoteView::generateThumbnail(int pageIndex, const QSize &size) {
  if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size()) {
    QPixmap empty(size);
    empty.fill(Qt::white);
    return empty;
  }
  QImage img(a4wPx(), a4hPx(), QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::white);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);

  for (const auto &s : note_->pages[pageIndex].strokes) {
    QColor c = s.color;
    if (s.isHighlighter)
      c.setAlpha(80);
    else if (!s.isEraser)
      c.setAlpha(255);
    QPen pen;
    if (s.isEraser)
      pen = QPen(Qt::white, s.width);
    else
      pen = QPen(c, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.drawPath(s.path);
  }
  p.end();
  return QPixmap::fromImage(
      img.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

bool MultiPageNoteView::exportPageToPng(int pageIndex, const QString &path) {
  QPixmap pm = generateThumbnail(pageIndex, QSize(a4wPx(), a4hPx()));
  return pm.save(path, "PNG");
}

bool MultiPageNoteView::exportPageToPdf(int pageIndex, const QString &path) {
  QPdfWriter pdf(path);
  pdf.setPageSize(QPageSize(QPageSize::A4));
  QPainter p(&pdf);
  p.fillRect(QRectF(0, 0, a4wPx(), a4hPx()), Qt::white);
  if (note_ && pageIndex >= 0 && pageIndex < note_->pages.size()) {
    for (const auto &s : note_->pages[pageIndex].strokes) {
      QColor c = s.color;
      if (s.isHighlighter)
        c.setAlpha(80);
      QPen pen;
      if (s.isEraser)
        pen = QPen(Qt::white, s.width);
      else
        pen = QPen(c, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
      p.setPen(pen);
      p.drawPath(s.path);
    }
  }
  p.end();
  return true;
}

void MultiPageNoteView::movePage(int fromIndex, int toIndex) {
  if (!note_ || fromIndex < 0 || fromIndex >= note_->pages.size() ||
      toIndex < 0 || toIndex >= note_->pages.size())
    return;
  note_->pages.move(fromIndex, toIndex);
  setNote(note_);
  if (onSaveRequested)
    onSaveRequested(note_);
}
void MultiPageNoteView::duplicatePage(int pageIndex) {
  if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size())
    return;
  NotePage newPage = note_->pages[pageIndex];
  note_->pages.insert(pageIndex + 1, newPage);
  setNote(note_);
  if (onSaveRequested)
    onSaveRequested(note_);
}
void MultiPageNoteView::deletePage(int pageIndex) {
  if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size())
    return;
  note_->pages.removeAt(pageIndex);
  setNote(note_);
  if (onSaveRequested)
    onSaveRequested(note_);
}
void MultiPageNoteView::scrollToPage(int pageIndex) {
  if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size())
    return;
  QRectF r = pageRect(pageIndex);
  verticalScrollBar()->setValue(r.top());
}

int MultiPageNoteView::pageCount() const {
  return note_ ? note_->pages.size() : 0;
}

QString MultiPageNoteView::pageTitle(int pageIndex) const {
  if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size())
    return QString();
  return note_->pages[pageIndex].title;
}

void MultiPageNoteView::renamePage(int pageIndex, const QString &title) {
  if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size())
    return;
  note_->pages[pageIndex].title = title;
  setNote(note_);
  if (onSaveRequested)
    onSaveRequested(note_);
}

void MultiPageNoteView::duplicatePages(const QList<int> &pageIndices) {
  if (!note_ || pageIndices.isEmpty())
    return;
  QList<int> sorted = pageIndices;
  std::sort(sorted.begin(), sorted.end());
  for (int i = sorted.size() - 1; i >= 0; --i) {
    const int idx = sorted[i];
    if (idx < 0 || idx >= note_->pages.size())
      continue;
    note_->pages.insert(idx + 1, note_->pages[idx]);
  }
  setNote(note_);
  if (onSaveRequested)
    onSaveRequested(note_);
}

void MultiPageNoteView::deletePages(const QList<int> &pageIndices) {
  if (!note_ || pageIndices.isEmpty())
    return;
  QList<int> sorted = pageIndices;
  std::sort(sorted.begin(), sorted.end(), std::greater<int>());
  for (int idx : sorted) {
    if (idx >= 0 && idx < note_->pages.size() && note_->pages.size() > 1)
      note_->pages.removeAt(idx);
  }
  setNote(note_);
  if (onSaveRequested)
    onSaveRequested(note_);
}

void MultiPageNoteView::applyLayoutToPages(const QList<int> &pageIndices,
                                           int backgroundType,
                                           const QColor &paperColor) {
  if (!note_ || pageIndices.isEmpty())
    return;
  for (int idx : pageIndices)
    applyLayoutToPage(idx, backgroundType, paperColor);
}

void MultiPageNoteView::drawForeground(QPainter *painter, const QRectF &rect) {
  QGraphicsView::drawForeground(painter, rect);

  AbstractTool *tool = ToolManager::instance().activeTool();
  if (tool) {
    painter->save();
    tool->drawOverlay(painter, rect);
    painter->restore();
  }
}

// ─── PDF Import ─────────────────────────────────────────────────────────────
bool MultiPageNoteView::importPdfPages(const QString &pdfPath) {
  if (!note_) return false;

#ifdef BLOP_HAS_PDF
  QPdfDocument doc;
  auto status = doc.load(pdfPath);
  if (status != QPdfDocument::Error::None) return false;

  int pageCount = doc.pageCount();
  if (pageCount == 0) return false;

  for (int i = 0; i < pageCount; ++i) {
    int pageIdx = note_->pages.size();
    note_->ensurePage(pageIdx);
    note_->pages[pageIdx].title = QString("PDF S.%1").arg(i + 1);

    // Render at A4 pixel resolution (793 x 1122)
    QImage img = doc.render(i, QSize(a4wPx(), a4hPx()));
    if (!img.isNull())
      note_->pages[pageIdx].backgroundImage = img;
  }
  setNote(note_);
  if (onSaveRequested) onSaveRequested(note_);
  return true;
#else
  Q_UNUSED(pdfPath);
  return false;
#endif

}

void MultiPageNoteView::onSelectionChanged() {
  QList<QGraphicsItem*> items = scene_.selectedItems();
  if (items.isEmpty() || !m_selectionMenu) {
      if (m_selectionMenu) m_selectionMenu->hide();
      applyTransform(); // Deselect and remove overlay if it exists
      return;
  }

  // Selection debug logs removed for smoother debug performance.
  // Find bounding rect of all selected items
  QRectF boundingRect;
  for (QGraphicsItem* item : items) {
      if (item->type() == QGraphicsPathItem::Type) { // StrokeItem is now PathItem
          boundingRect = boundingRect.isValid() ? boundingRect.united(item->sceneBoundingRect()) : item->sceneBoundingRect();
      }
  }

  if (!boundingRect.isValid()) {
      m_selectionMenu->hide();
      return;
  }

  // Calculate viewport position
  QPoint viewPos = mapFromScene(boundingRect.topLeft());
  m_selectionMenu->move(viewPos.x() + 10, qMax(0, viewPos.y() - m_selectionMenu->height() - 55));
  m_selectionMenu->show();
  m_selectionMenu->raise();
}

#include "multipagenoteview.moc"


void MultiPageNoteView::deleteSelection() {
    for (auto item : scene_.selectedItems()) {
        scene_.removeItem(item);
        delete item;
    }
    if (m_selectionMenu) m_selectionMenu->hide();
    if (onSaveRequested) onSaveRequested(note_);
}

void MultiPageNoteView::copySelection() {
    const QList<QGraphicsItem*> selected = scene_.selectedItems();
    if (selected.isEmpty()) return;
    QRectF selRect;
    for (auto *i : selected) selRect = selRect.united(i->sceneBoundingRect());
    if (!selRect.isEmpty()) {
        QImage img(selRect.size().toSize(), QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        scene_.render(&p, QRectF(0, 0, img.width(), img.height()), selRect);
        QGuiApplication::clipboard()->setImage(img);
    }
    if (m_selectionMenu) m_selectionMenu->hide();
}

void MultiPageNoteView::cutSelection() {
    copySelection();
    deleteSelection();
}

void MultiPageNoteView::duplicateSelection() {}

void MultiPageNoteView::changeSelectionColor() {
    QColor c = Qt::black;
    for (auto *item : scene_.selectedItems()) {
      if (auto *pi = dynamic_cast<QGraphicsPathItem *>(item)) {
        c = pi->pen().color();
        break;
      }
    }
    if (!showColorPickerOverlay(this, &c, QStringLiteral("Farbe ändern")))
      return;
    for (auto *item : scene_.selectedItems()) {
      if (auto *pi = dynamic_cast<QGraphicsPathItem *>(item)) {
        QPen p = pi->pen();
        p.setColor(c);
        pi->setPen(p);
      }
    }
    scene_.update();
    if (m_selectionMenu) m_selectionMenu->hide();
    if (onSaveRequested) onSaveRequested(note_);
}

void MultiPageNoteView::startTransformSession() {
  QList<QGraphicsItem *> items = scene_.selectedItems();
  if (items.isEmpty()) return;

  // Clean up previous session if still active
  if (m_transformOverlay) {
    scene_.removeItem(m_transformOverlay);
    delete m_transformOverlay;
    m_transformOverlay = nullptr;
  }
  if (m_transformGroup) {
    scene_.destroyItemGroup(m_transformGroup);
    m_transformGroup = nullptr;
  }

  if (items.count() == 1) {
    QGraphicsItem *singleItem = items.first();
    m_transformGroup = nullptr;
    QPointF oldCenter = singleItem->sceneBoundingRect().center();
    singleItem->setTransformOriginPoint(singleItem->boundingRect().center());
    QPointF newCenter = singleItem->sceneBoundingRect().center();
    singleItem->moveBy(oldCenter.x() - newCenter.x(), oldCenter.y() - newCenter.y());
    m_transformOverlay = new TransformOverlay(singleItem);
    connect(m_transformOverlay, &TransformOverlay::transformChanged, this, &MultiPageNoteView::onSelectionChanged);
    connect(m_transformOverlay, &TransformOverlay::interactionStarted, m_selectionMenu, &QWidget::hide);
    connect(m_transformOverlay, &TransformOverlay::interactionEnded, this, &MultiPageNoteView::onSelectionChanged);
  } else {
    m_transformGroup = scene_.createItemGroup(items);
    QRectF grpRect = m_transformGroup->boundingRect();
    m_transformGroup->setTransformOriginPoint(grpRect.center());
    m_transformOverlay = new TransformOverlay(m_transformGroup);
    connect(m_transformOverlay, &TransformOverlay::transformChanged, this, &MultiPageNoteView::onSelectionChanged);
    connect(m_transformOverlay, &TransformOverlay::interactionStarted, m_selectionMenu, &QWidget::hide);
    connect(m_transformOverlay, &TransformOverlay::interactionEnded, this, &MultiPageNoteView::onSelectionChanged);
  }
  m_transformOverlay->setZValue(99999);
  scene_.addItem(m_transformOverlay);

  onSelectionChanged();
}

void MultiPageNoteView::applyTransform() {
  if (m_transformOverlay) {
    scene_.removeItem(m_transformOverlay);
    delete m_transformOverlay;
    m_transformOverlay = nullptr;
  }
  if (m_transformGroup) {
    scene_.destroyItemGroup(m_transformGroup);
    m_transformGroup = nullptr;
  }
  if (onSaveRequested) onSaveRequested(note_);
}

void MultiPageNoteView::screenshotSelection() {
    copySelection(); 
}
