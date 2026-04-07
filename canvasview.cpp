#include "canvasview.h"
#include "SelectionMenuIcons.h"
#include "TransformOverlay.h"
#include "UIStyles.h"
#include "mainwindow.h"
#include "tools/AbstractTool.h"
#include "tools/RulerTool.h" // Wichtig: RulerTool Header
#include "tools/StrokeItem.h"
#include "tools/ToolManager.h"
#include "uiscale.h"
#include <QUndoCommand>

// ---------------------------------------------------------------------------
// Undo commands for canvas items
// ---------------------------------------------------------------------------
class AddItemCommand : public QUndoCommand {
public:
    AddItemCommand(QGraphicsScene *scene, QGraphicsItem *item)
        : m_scene(scene), m_item(item) {}

    ~AddItemCommand() override {
        // If the item is not on the scene, we own it and must delete it
        if (m_item && !m_scene->items().contains(m_item))
            delete m_item;
    }

    void undo() override {
        if (m_item) m_scene->removeItem(m_item);
    }

    // redo() is also called on first push — but item is already on scene then.
    // We use m_firstRedo to skip that initial add.
    void redo() override {
        if (m_firstRedo) { m_firstRedo = false; return; }
        if (m_item) m_scene->addItem(m_item);
    }

private:
    QGraphicsScene *m_scene;
    QGraphicsItem *m_item;
    bool m_firstRedo{true};
};

class RemoveItemCommand : public QUndoCommand {
public:
    RemoveItemCommand(QGraphicsScene *scene, QGraphicsItem *item)
        : m_scene(scene), m_item(item) {}

    ~RemoveItemCommand() override {
        if (m_item && !m_scene->items().contains(m_item))
            delete m_item;
    }

    void undo() override {
        if (m_item) m_scene->addItem(m_item);
    }

    void redo() override {
        if (m_firstRedo) { m_firstRedo = false; return; }
        if (m_item) m_scene->removeItem(m_item);
    }

private:
    QGraphicsScene *m_scene;
    QGraphicsItem *m_item;
    bool m_firstRedo{true};
};

#include <QBitmap>
#include <QPalette>
#include <QShowEvent>
#include <QClipboard>
#include "editoroverlays.h"
#include <QCursor>
#include <QDataStream>
#include <QFile>
#include <QFileDialog>
#include <QGestureEvent>
#include <QGuiApplication>
#include <QGraphicsSceneMouseEvent>
#include <QInputDevice>
#include <QTabletEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QVector2D>
#include <QPdfWriter>
#include <QUndoCommand>
#include <QtMath>
#include <QtGlobal>
#include <cmath>
#include <utility>

static float pageScale() {
#ifdef Q_OS_ANDROID
  return UiScale::dp(100) / 100.0f;
#else
  return 1.0f;
#endif
}
static float pageWidthPx() { return 794.0f * 1.5f * pageScale(); }
static float pageHeightPx() { return 1123.0f * 1.5f * pageScale(); }
static float pageGapPx() { return UiScale::dp(60); }
static float totalPageHeightPx() { return pageHeightPx() + pageGapPx(); }

#ifdef Q_OS_ANDROID
/// Solid canvas color via palette (no viewport stylesheet overlay). Android only.
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

// =============================================================================
// HELPER: BAKING TRANSFORM & CURSOR
// =============================================================================

// =============================================================================
// MENUS (Selection, Crop, Resizer, Transform)
// =============================================================================

class SelectionMenu : public QWidget {
  Q_OBJECT
public:
  explicit SelectionMenu(QWidget *parent = nullptr) : QWidget(parent) {
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
               &SelectionMenu::cutRequested);
    addIconBtn(SelectionMenuIcons::copyIcon(), QStringLiteral("Kopieren"),
               &SelectionMenu::copyRequested);
    addIconBtn(SelectionMenuIcons::colorIcon(), QStringLiteral("Farbe"),
               &SelectionMenu::colorRequested);
    addIconBtn(SelectionMenuIcons::cropIcon(), QStringLiteral("Zuschneiden"),
               &SelectionMenu::cropRequested);
    addIconBtn(SelectionMenuIcons::screenshotIcon(), QStringLiteral("Screenshot"),
               &SelectionMenu::screenshotRequested);
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
            &SelectionMenu::deleteRequested);
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

class CropMenu : public QWidget {
  Q_OBJECT
public:
  explicit CropMenu(QWidget *parent = nullptr) : QWidget(parent) {
    setStyleSheet(
        "QWidget { background-color: #1E1E1E; border-radius: 8px; border: 1px "
        "solid #6c5ce7; }"
        "QPushButton { background: #333; border: none; color: #DDD; "
        "border-radius: 4px; padding: 6px 12px; font-size: 12px; }"
        "QPushButton:checked { background-color: #6c5ce7; color: white; }"
        "QPushButton:hover { background-color: #444; }"
        "QPushButton#ApplyBtn { background-color: #2E7D32; color: white; }"
        "QPushButton#CancelBtn { background-color: #C62828; color: white; }");
    setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    btnRect = new QPushButton("Rechteck", this);
    btnRect->setCheckable(true);
    btnFree = new QPushButton("Freihand", this);
    btnFree->setCheckable(true);
    QButtonGroup *grp = new QButtonGroup(this);
    grp->addButton(btnRect);
    grp->addButton(btnFree);
    connect(btnRect, &QPushButton::clicked, this, &CropMenu::rectRequested);
    connect(btnFree, &QPushButton::clicked, this, &CropMenu::freehandRequested);
    layout->addWidget(btnRect);
    layout->addWidget(btnFree);
    QFrame *line = new QFrame;
    line->setFrameShape(QFrame::VLine);
    line->setStyleSheet("color: #555;");
    layout->addWidget(line);
    QPushButton *btnApply = new QPushButton("✔", this);
    btnApply->setObjectName("ApplyBtn");
    QPushButton *btnCancel = new QPushButton("✕", this);
    btnCancel->setObjectName("CancelBtn");
    connect(btnApply, &QPushButton::clicked, this, &CropMenu::applyRequested);
    connect(btnCancel, &QPushButton::clicked, this, &CropMenu::cancelRequested);
    layout->addWidget(btnApply);
    layout->addWidget(btnCancel);
    adjustSize();
    hide();
  }
  void setMode(bool rect) {
    if (rect)
      btnRect->setChecked(true);
    else
      btnFree->setChecked(true);
  }
signals:
  void rectRequested();
  void freehandRequested();
  void applyRequested();
  void cancelRequested();

private:
  QPushButton *btnRect, *btnFree;
};

class CropResizer : public QGraphicsObject {
public:
  enum Handle {
    None,
    TopLeft,
    Top,
    TopRight,
    Left,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
    Center
  };
  CropResizer(QRectF rect) : m_rect(rect) {
    setFlags(ItemIsSelectable | ItemIsMovable);
    setAcceptHoverEvents(true);
  }
  QRectF boundingRect() const override {
    return m_rect.adjusted(-20, -20, 20, 20);
  }
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *,
             QWidget *) override {
    painter->setPen(QPen(QColor(Qt::white), 2, Qt::DashLine));
    painter->setBrush(QColor(255, 255, 255, 30));
    painter->drawRect(m_rect);
    painter->setPen(QPen(QColor(Qt::black), 1));
    painter->setBrush(Qt::white);
    qreal r = 6.0;
    QList<QPointF> handles = {m_rect.topLeft(),
                              m_rect.topRight(),
                              m_rect.bottomLeft(),
                              m_rect.bottomRight(),
                              QPointF(m_rect.center().x(), m_rect.top()),
                              QPointF(m_rect.center().x(), m_rect.bottom()),
                              QPointF(m_rect.left(), m_rect.center().y()),
                              QPointF(m_rect.right(), m_rect.center().y())};
    for (auto p : handles)
      painter->drawEllipse(p, r, r);
  }
  QRectF currentRect() const { return mapRectToScene(m_rect); }

protected:
  void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override {
    Handle h = handleAt(event->pos());
    if (h == TopLeft || h == BottomRight)
      setCursor(Qt::SizeFDiagCursor);
    else if (h == TopRight || h == BottomLeft)
      setCursor(Qt::SizeBDiagCursor);
    else if (h == Top || h == Bottom)
      setCursor(Qt::SizeVerCursor);
    else if (h == Left || h == Right)
      setCursor(Qt::SizeHorCursor);
    else
      setCursor(Qt::SizeAllCursor);
    QGraphicsObject::hoverMoveEvent(event);
  }
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
    m_dragHandle = handleAt(event->pos());
    m_startPos = event->pos();
    m_startRect = m_rect;
    if (m_dragHandle == None)
      m_dragHandle = Center;
  }
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
    if (m_dragHandle == Center) {
      moveBy(event->pos().x() - m_startPos.x(),
             event->pos().y() - m_startPos.y());
      return;
    }
    QPointF pos = event->pos();
    qreal l = m_startRect.left(), r = m_startRect.right(),
          t = m_startRect.top(), b = m_startRect.bottom();
    if (m_dragHandle == TopLeft || m_dragHandle == Left ||
        m_dragHandle == BottomLeft)
      l = pos.x();
    if (m_dragHandle == TopRight || m_dragHandle == Right ||
        m_dragHandle == BottomRight)
      r = pos.x();
    if (m_dragHandle == TopLeft || m_dragHandle == Top ||
        m_dragHandle == TopRight)
      t = pos.y();
    if (m_dragHandle == BottomLeft || m_dragHandle == Bottom ||
        m_dragHandle == BottomRight)
      b = pos.y();
    m_rect = QRectF(QPointF(l, t), QPointF(r, b)).normalized();
    prepareGeometryChange();
    update();
  }

private:
  Handle handleAt(QPointF pos) {
    double r = 20.0;
    if (QVector2D(pos - m_rect.topLeft()).length() < r)
      return TopLeft;
    if (QVector2D(pos - m_rect.topRight()).length() < r)
      return TopRight;
    if (QVector2D(pos - m_rect.bottomLeft()).length() < r)
      return BottomLeft;
    if (QVector2D(pos - m_rect.bottomRight()).length() < r)
      return BottomRight;
    if (QVector2D(pos - QPointF(m_rect.center().x(), m_rect.top())).length() <
        r)
      return Top;
    if (QVector2D(pos - QPointF(m_rect.center().x(), m_rect.bottom()))
            .length() < r)
      return Bottom;
    if (QVector2D(pos - QPointF(m_rect.left(), m_rect.center().y())).length() <
        r)
      return Left;
    if (QVector2D(pos - QPointF(m_rect.right(), m_rect.center().y())).length() <
        r)
      return Right;
    return None;
  }
  QRectF m_rect;
  QRectF m_startRect;
  QPointF m_startPos;
  Handle m_dragHandle;
};


// =============================================================================
// CANVAS VIEW IMPLEMENTATION
// =============================================================================

CanvasView::CanvasView(QWidget *parent)
    : QGraphicsView(parent), m_toolManager(nullptr), m_currentItem(nullptr),
      m_lassoItem(nullptr), m_isDrawing(false), m_isInfinite(true),
      m_penOnlyMode(false), m_pageColor(UIStyles::SceneBackground) // Default: dark page
      ,
      m_pageStyle(PageStyle::Squared), m_gridSize(40),
      m_currentTool(ToolType::Pen), m_penColor(Qt::black), m_penWidth(3),
      m_isPanning(false), m_pullDistance(0.0f), m_cropResizer(nullptr),
      m_transformOverlay(nullptr), m_transformGroup(nullptr) {
  m_scene = new QGraphicsScene(this);
  setScene(m_scene);
  m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
  m_a4Rect = QRectF(0, 0, pageWidthPx(), pageHeightPx());
  m_undoStack = new QUndoStack(this);

  setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
#ifdef Q_OS_ANDROID
  setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing |
                       QGraphicsView::DontSavePainterState);
#else
  setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);
#endif
  setRenderHint(QPainter::Antialiasing);
  setRenderHint(QPainter::SmoothPixmapTransform, false);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setResizeAnchor(QGraphicsView::AnchorUnderMouse);

#ifdef Q_OS_ANDROID
  applyGraphicsViewCanvasBackground(this);
#endif

  grabGesture(Qt::PinchGesture);
  viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

  setPageFormat(true);
  setMouseTracking(true);
  setDragMode(QGraphicsView::NoDrag);

  updateBackgroundTile();

  connect(this, &CanvasView::contentModified, this,
          &CanvasView::updateSceneRect);

  // --- MENUS ---
  m_selectionMenu = new SelectionMenu(this);
  connect(m_selectionMenu, &SelectionMenu::deleteRequested, this,
          &CanvasView::deleteSelection);
  connect(m_selectionMenu, &SelectionMenu::duplicateRequested, this,
          &CanvasView::duplicateSelection);
  connect(m_selectionMenu, &SelectionMenu::copyRequested, this,
          &CanvasView::copySelection);
  connect(m_selectionMenu, &SelectionMenu::cutRequested, this,
          &CanvasView::cutSelection);
  connect(m_selectionMenu, &SelectionMenu::colorRequested, this,
          &CanvasView::changeSelectionColor);
  connect(m_selectionMenu, &SelectionMenu::screenshotRequested, this,
          &CanvasView::screenshotSelection);
  connect(m_selectionMenu, &SelectionMenu::cropRequested, this,
          &CanvasView::startCropSession);


  m_cropMenu = new CropMenu(this);
  connect(m_cropMenu, &CropMenu::rectRequested, this,
          &CanvasView::setCropModeRect);
  connect(m_cropMenu, &CropMenu::freehandRequested, this,
          &CanvasView::setCropModeFreehand);
  connect(m_cropMenu, &CropMenu::applyRequested, this, &CanvasView::applyCrop);
  connect(m_cropMenu, &CropMenu::cancelRequested, this,
          &CanvasView::cancelCrop);
}

CanvasView::~CanvasView() {}

void CanvasView::setToolManager(ToolManager *manager) {
  m_toolManager = manager;
  if (m_toolManager) {
    connect(m_toolManager, &ToolManager::toolChanged, this,
            [this](AbstractTool *tool) {
              // FIX: Hier prüfen wir, ob das Lineal aktiviert wurde, und zeigen
              // es an.
              if (tool && tool->mode() == ToolMode::Ruler) {
                RulerTool::ensureRulerExists(m_scene, m_toolManager->config());
              }
              viewport()->update();
            });
  }
}

void CanvasView::setPageFormat(bool isInfinite) {
  m_isInfinite = isInfinite;
  if (m_isInfinite) {
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    updateSceneRect();
  } else {
    m_scene->setSceneRect(m_a4Rect);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  }
  m_scene->update();
  viewport()->update();
}

void CanvasView::updateSceneRect() {
  if (!m_isInfinite)
    return;
  QRectF contentRect = m_scene->itemsBoundingRect();
  qreal margin = 2000.0;
  if (contentRect.isNull()) {
    contentRect = QRectF(0, 0, width(), height());
  }
  contentRect.adjust(-margin, -margin, margin, margin);
  QRectF viewRect = mapToScene(viewport()->rect()).boundingRect();
  contentRect = contentRect.united(viewRect);
  m_scene->setSceneRect(contentRect);
}

void CanvasView::setPageColor(const QColor &color) {
  // Einfacher Dark-Mode Toggle für bestehende Striche
  bool isNowDark = (color.value() < 100);
  QList<QGraphicsItem *> items = m_scene->items();
  for (auto *item : std::as_const(items)) {
    if (item->type() == QGraphicsPathItem::Type) {
      QGraphicsPathItem *pathItem = static_cast<QGraphicsPathItem *>(item);
      if (pathItem->zValue() == 0.1)
        continue;
      QColor c = pathItem->pen().color();
      if (isNowDark && (c == Qt::black || c == QColor(0, 0, 0))) {
        QPen p = pathItem->pen();
        p.setColor(Qt::white);
        pathItem->setPen(p);
      } else if (!isNowDark && (c == Qt::white || c == QColor(255, 255, 255))) {
        QPen p = pathItem->pen();
        p.setColor(Qt::black);
        pathItem->setPen(p);
      }
    }
  }
  m_pageColor = color;
  updateBackgroundTile();
  viewport()->update();
  emit contentModified();
}

void CanvasView::setPageStyle(PageStyle style) {
  if (m_pageStyle != style) {
    m_pageStyle = style;
    updateBackgroundTile();
    viewport()->update();
  }
}

void CanvasView::setGridSize(int size) {
  if (m_gridSize != size && size > 5) {
    m_gridSize = size;
    updateBackgroundTile();
    viewport()->update();
  }
}

void CanvasView::updateBackgroundTile() {
  int baseSize = m_gridSize;
  if (baseSize < 1)
    baseSize = 40;
  int multiplier = 512 / baseSize;
  if (multiplier < 1)
    multiplier = 1;
  int texSize = baseSize * multiplier;
  m_bgTile = QPixmap(texSize, texSize);
  m_bgTile.fill(Qt::transparent);
  if (m_pageStyle == PageStyle::Blank)
    return;
  QPainter p(&m_bgTile);
  p.setRenderHint(QPainter::Antialiasing, false);
  
  // Sanftere Linienfarben gegen das Ausfransen/Brennen der weißen Linien
  QColor lineColor = (m_pageColor.value() < 128) ? QColor(255, 255, 255, 12)
                                                 : QColor(0, 0, 0, 15);
  p.setPen(QPen(lineColor, 1));
  if (m_pageStyle == PageStyle::Squared) {
    for (int i = 1; i <= multiplier; ++i) {
      int pos = i * baseSize - 1;
      p.drawLine(0, pos, texSize, pos);
      p.drawLine(pos, 0, pos, texSize);
    }
  } else if (m_pageStyle == PageStyle::Lined) {
    for (int i = 1; i <= multiplier; ++i) {
      int pos = i * baseSize - 1;
      p.drawLine(0, pos, texSize, pos);
    }
  } else if (m_pageStyle == PageStyle::Dotted) {
    p.setPen(Qt::NoPen);
    p.setBrush(lineColor);
    for (int x = 1; x <= multiplier; ++x) {
      for (int y = 1; y <= multiplier; ++y) {
        p.drawRect(x * baseSize - 2, y * baseSize - 2, 2, 2);
      }
    }
  }
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect) {
#ifndef Q_OS_ANDROID
  // Desktop: single light/neutral fill (legacy behavior; avoids full blackscreen).
  painter->fillRect(rect, m_pageColor);
#else
  // Android: infinite uses page color; paged uses dark outside the sheet.
  if (m_isInfinite)
    painter->fillRect(rect, m_pageColor);
  else
    painter->fillRect(rect, UIStyles::SceneBackground);
#endif

  if (m_isInfinite && !m_bgTile.isNull() && m_pageStyle != PageStyle::Blank) {
    double zoomLevel = transform().m11();
    if (m_gridSize * zoomLevel < 8.0)
      return;
    painter->save();
    qreal w = m_bgTile.width();
    qreal h = m_bgTile.height();
    qreal ox = std::fmod(rect.left(), w);
    if (ox < 0)
      ox += w;
    qreal oy = std::fmod(rect.top(), h);
    if (oy < 0)
      oy += h;
    painter->drawTiledPixmap(rect, m_bgTile, QPointF(ox, oy));
    painter->restore();
  } else if (!m_isInfinite) {
    painter->save();
    int startPage =
        std::max(0, static_cast<int>(rect.top() / totalPageHeightPx()));
    int endPage = static_cast<int>(rect.bottom() / totalPageHeightPx());
    int maxPage = static_cast<int>(m_a4Rect.height() / totalPageHeightPx());
    if (endPage > maxPage)
      endPage = maxPage;
    for (int i = startPage; i <= endPage; ++i) {
      qreal y = i * totalPageHeightPx();
      QRectF pageRect(0, y, pageWidthPx(), pageHeightPx());
      painter->fillRect(pageRect, m_pageColor);
      painter->setBrushOrigin(pageRect.topLeft());
      painter->drawTiledPixmap(pageRect, m_bgTile);
    }
    painter->restore();
  }
}

void CanvasView::drawForeground(QPainter *painter, const QRectF &rect) {
  QGraphicsView::drawForeground(painter, rect);
  if (m_pullDistance > 1.0f)
    drawPullIndicator(painter);

  // TOOL OVERLAY ZEICHNEN
  // Hier nutzen wir jetzt den m_toolManager
  if (m_toolManager && m_toolManager->activeTool()) {
    painter->save();
    m_toolManager->activeTool()->drawOverlay(painter, rect);
    painter->restore();
  }

  if (!m_isInfinite) {
    painter->save();
    painter->setPen(QPen(QColor(0, 0, 0, 60), 1, Qt::SolidLine));
    painter->setBrush(Qt::NoBrush);
    int startPage =
        std::max(0, static_cast<int>(rect.top() / totalPageHeightPx()));
    int endPage = static_cast<int>(rect.bottom() / totalPageHeightPx());
    int maxPage = static_cast<int>(m_a4Rect.height() / totalPageHeightPx());
    if (endPage > maxPage)
      endPage = maxPage;
    for (int i = startPage; i <= endPage; ++i) {
      QRectF pageRect(0, i * totalPageHeightPx(), pageWidthPx(), pageHeightPx());
      painter->drawRect(pageRect);
    }
    painter->restore();
  }
}

void CanvasView::drawPullIndicator(QPainter *painter) {
  painter->save();
  painter->resetTransform();
  painter->setClipping(false);
  int w = viewport()->width();
  int h = viewport()->height();
  float maxPull = 250.0f;
  float progress = qMin(m_pullDistance / maxPull, 1.0f);
  int size = 60;
  int yPos = h - size - 150 - (progress * 20);
  int xPos = (w - size) / 2;
  QRect circleRect(xPos, yPos, size, size);
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setBrush(QColor(40, 40, 40, 200));
  painter->setPen(Qt::NoPen);
  painter->drawEllipse(circleRect);
  if (progress > 0.05f) {
    QPen arcPen(QColor(0x6c5ce7));
    arcPen.setWidth(4);
    arcPen.setCapStyle(Qt::RoundCap);
    painter->setPen(arcPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawArc(circleRect.adjusted(8, 8, -8, -8), 90 * 16,
                     -progress * 360 * 16);
  }
  if (progress > 0.15f) {
    painter->setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap));
    int center = size / 2;
    float growFactor = (progress - 0.15f) / 0.85f;
    int pSize = 12 * growFactor;
    QPoint c = circleRect.center();
    painter->drawLine(c.x(), c.y() - pSize, c.x(), c.y() + pSize);
    painter->drawLine(c.x() - pSize, c.y(), c.x() + pSize, c.y());
  }
  painter->restore();
}

void CanvasView::addNewPage() {
  m_a4Rect.setHeight(m_a4Rect.height() + pageHeightPx() + pageGapPx());
  setSceneRect(m_a4Rect);
  viewport()->update();
}

bool CanvasView::saveToFile() {
  if (m_filePath.isEmpty())
    return false;
  QFile file(m_filePath);
  if (!file.open(QIODevice::WriteOnly))
    return false;
  QDataStream out(&file);
  
  out << (quint32)0xB10B0003; // Protocol Format V3 (Stroke Pressure Support)
  out << m_isInfinite;
  
  QList<QGraphicsItem *> items = m_scene->items(Qt::AscendingOrder);
  int count = 0;
  for (auto *item : std::as_const(items)) {
    if (item->type() == QGraphicsItem::UserType + 1 && item != m_lassoItem &&
        item != m_cropResizer && item != m_transformOverlay) {
        count++;
    }
  }
  
  out << count;
  
  for (auto *item : std::as_const(items)) {
    if (item->type() == QGraphicsItem::UserType + 1 && item != m_lassoItem &&
        item != m_cropResizer && item != m_transformOverlay) {
            
      StrokeItem *strokeItem = static_cast<StrokeItem *>(item);
      out << strokeItem->pos() << strokeItem->pen().color()
          << (int)strokeItem->pen().width() << strokeItem->path();
          
      // V3 Serialization: Pressure Points Buffer
      const QVector<StrokePoint>& pts = strokeItem->points();
      out << (qint32)pts.size();
      for(const auto& p : pts) {
          out << p.pos << p.pressure;
      }
    }
  }
  return true;
}

bool CanvasView::loadFromFile() {
  if (m_filePath.isEmpty())
    return false;
  QFile file(m_filePath);
  if (!file.open(QIODevice::ReadOnly))
    return false;
  QDataStream in(&file);
  quint32 magic;
  in >> magic;
  
  if (magic == 0xB10B0001) {
    m_isInfinite = true;
  } else if (magic == 0xB10B0002 || magic == 0xB10B0003) {
    in >> m_isInfinite;
  } else {
    return false; // Unknown Corrupt Format
  }
  
  setPageFormat(m_isInfinite);
  m_scene->clear();
  m_undoStack->clear();
  
  int count;
  in >> count;
  bool wasBlocked = m_scene->blockSignals(true);
  for (int i = 0; i < count; ++i) {
    QPointF pos;
    QColor color;
    int width;
    QPainterPath path;
    in >> pos >> color >> width >> path;
    QPen pen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    
    QVector<StrokePoint> pts;
    if (magic >= 0xB10B0003) {
        qint32 ptCount;
        in >> ptCount;
        for(int j = 0; j < ptCount; ++j) {
            QPointF ppos; qreal ppress;
            in >> ppos >> ppress;
            pts.append({ppos, ppress});
        }
    }
    
    StrokeItem *item = new StrokeItem(path, pen, pts, StrokeItem::Normal);
    m_scene->addItem(item);
    item->setPos(pos);
    if (color.alpha() < 255)
      item->setZValue(0.1);
    else
      item->setZValue(1.0);
  }
  m_scene->blockSignals(wasBlocked);
  if (m_isInfinite)
    updateSceneRect();
  return true;
}

void CanvasView::toggleRuler(bool active) {
    if (active) {
        if (m_toolManager) {
            const QPointF spawn = mapToScene(viewport()->rect().center());
            RulerTool::ensureRulerExists(scene(), m_toolManager->config(), spawn);
        }
    } else {
        for (QGraphicsItem *item : m_scene->items()) {
            if (item->type() == RulerItem::Type) {
                item->setVisible(false);
                break;
            }
        }
    }
}

void CanvasView::setTool(ToolType tool) {
  m_currentTool = tool;

  // FIX: Wenn das Ruler-Tool über die UI ausgewählt wurde, sicherstellen, dass
  // es existiert
  if (tool == ToolType::Ruler) {
    if (m_toolManager) {
      const QPointF spawn = mapToScene(viewport()->rect().center());
      RulerTool::ensureRulerExists(m_scene, m_toolManager->config(), spawn);
    }
    // Keine Cursor-Änderung hier, RulerTool steuert das nicht direkt
  } else if (tool == ToolType::Select) {
    setCursor(Qt::ArrowCursor);
    setDragMode(QGraphicsView::RubberBandDrag);
  } else if (tool == ToolType::Text) {
    setCursor(Qt::IBeamCursor);
    setDragMode(QGraphicsView::NoDrag);
  } else {
    clearSelection();
    setDragMode(QGraphicsView::NoDrag);
    if (tool == ToolType::Pen)
      setCursor(Qt::CrossCursor);
    else if (tool == ToolType::Highlighter)
      setCursor(Qt::CrossCursor);
    else if (tool == ToolType::Eraser)
      setCursor(Qt::ForbiddenCursor);
    else if (tool == ToolType::Lasso)
      setCursor(Qt::CrossCursor);
  }
}

void CanvasView::setPenColor(const QColor &color) { m_penColor = color; }
void CanvasView::setPenWidth(int width) { m_penWidth = width; }

void CanvasView::undo() {
  m_undoStack->undo();
  emit contentModified();
}
void CanvasView::redo() {
  m_undoStack->redo();
  emit contentModified();
}
bool CanvasView::canUndo() const { return m_undoStack->canUndo(); }
bool CanvasView::canRedo() const { return m_undoStack->canRedo(); }

void CanvasView::deleteSelection() {
  const QList<QGraphicsItem *> selected = m_scene->selectedItems();
  if (selected.isEmpty())
    return;
  for (auto *item : std::as_const(selected))
    m_scene->removeItem(item);
  m_selectionMenu->hide();
  emit contentModified();
}

void CanvasView::duplicateSelection() {
  const QList<QGraphicsItem *> selected = m_scene->selectedItems();
  if (selected.isEmpty())
    return;
  clearSelection();
  for (auto *item : std::as_const(selected)) {
    if (item->type() == QGraphicsPathItem::Type) {
      QGraphicsPathItem *pathItem = static_cast<QGraphicsPathItem *>(item);
      QGraphicsPathItem *newItem =
          m_scene->addPath(pathItem->path(), pathItem->pen());
      newItem->setPos(item->pos() + QPointF(20, 20));
      newItem->setFlags(QGraphicsItem::ItemIsSelectable |
                        QGraphicsItem::ItemIsMovable);
      newItem->setZValue(pathItem->zValue());
      newItem->setSelected(true);
      // Hier sollte eigentlich ein Undo-Command hin
    }
  }
  updateSelectionMenuPosition();
  emit contentModified();
}

void CanvasView::copySelection() {
  const QList<QGraphicsItem *> selected = m_scene->selectedItems();
  if (selected.isEmpty())
    return;
  QRectF selRect;
  for (auto *i : selected)
    selRect = selRect.united(i->sceneBoundingRect());
  if (!selRect.isEmpty()) {
    QImage img(selRect.size().toSize(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    m_scene->render(&p, QRectF(0, 0, img.width(), img.height()), selRect);
    QGuiApplication::clipboard()->setImage(img);
  }
  m_selectionMenu->hide();
}

void CanvasView::cutSelection() {
  copySelection();
  deleteSelection();
}

void CanvasView::changeSelectionColor() {
  QColor c = Qt::black;
  for (auto *item : m_scene->selectedItems()) {
    if (auto *pi = dynamic_cast<QGraphicsPathItem *>(item)) {
      c = pi->pen().color();
      break;
    }
  }
  if (!showColorPickerOverlay(this, &c, QStringLiteral("Farbe ändern"))) {
    m_selectionMenu->hide();
    return;
  }
  for (auto *item : m_scene->selectedItems()) {
    if (auto *pi = dynamic_cast<QGraphicsPathItem *>(item)) {
      QPen p = pi->pen();
      p.setColor(c);
      pi->setPen(p);
    }
  }
  m_scene->update();
  m_selectionMenu->hide();
}

void CanvasView::screenshotSelection() {
  const QList<QGraphicsItem *> selected = m_scene->selectedItems();
  if (selected.isEmpty())
    return;
  QRectF selRect;
  for (auto *i : selected)
    selRect = selRect.united(i->sceneBoundingRect());
  if (!selRect.isEmpty()) {
    QImage img(selRect.size().toSize(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    m_scene->render(&p, QRectF(0, 0, img.width(), img.height()), selRect);
    QString path =
        QFileDialog::getSaveFileName(this, "Save", "sel.png", "*.png");
    if (!path.isEmpty())
      img.save(path);
  }
  m_selectionMenu->hide();
}

// === CROP SYSTEM ===

void CanvasView::startCropSession() {
  m_selectionMenu->hide();
  m_interactionMode = InteractionMode::Crop;
  updateCropMenuPosition();
  m_cropMenu->show();
  setCropModeRect();
}

void CanvasView::setCropModeRect() {
  m_cropSubMode = 1;
  m_cropMenu->setMode(true);
  QRectF totalRect;
  for (auto *i : m_scene->selectedItems())
    totalRect = totalRect.united(i->sceneBoundingRect());
  if (m_cropResizer) {
    m_scene->removeItem(m_cropResizer);
    delete m_cropResizer;
  }
  m_cropResizer =
      new CropResizer(totalRect.isEmpty() ? QRectF(0, 0, 200, 200) : totalRect);
  m_cropResizer->setZValue(99999);
  m_scene->addItem(m_cropResizer);
  m_lassoItem = nullptr;
}

void CanvasView::setCropModeFreehand() {
  m_cropSubMode = 2;
  m_cropMenu->setMode(false);
  if (m_cropResizer) {
    m_scene->removeItem(m_cropResizer);
    delete m_cropResizer;
    m_cropResizer = nullptr;
  }
}

void CanvasView::applyCrop() {
  QPainterPath clipPath;
  if (m_cropSubMode == 1 && m_cropResizer) {
    clipPath.addRect(m_cropResizer->currentRect());
    m_scene->removeItem(m_cropResizer);
    delete m_cropResizer;
    m_cropResizer = nullptr;
  } else if (m_cropSubMode == 2 && m_lassoItem) {
    clipPath = m_lassoItem->path();
    m_scene->removeItem(m_lassoItem);
    delete m_lassoItem;
    m_lassoItem = nullptr;
  }
  if (clipPath.isEmpty()) {
    m_cropMenu->hide();
    m_interactionMode = InteractionMode::None;
    return;
  }
  for (auto *item : m_scene->selectedItems()) {
    if (auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item)) {
      QPainterPath localClip = item->mapFromScene(clipPath);
      localClip.setFillRule(Qt::WindingFill);
      pathItem->setPath(pathItem->path().intersected(localClip));
    }
  }
  m_cropMenu->hide();
  m_interactionMode = InteractionMode::None;
  emit contentModified();
}

void CanvasView::cancelCrop() {
  if (m_cropResizer) {
    m_scene->removeItem(m_cropResizer);
    delete m_cropResizer;
    m_cropResizer = nullptr;
  }
  if (m_lassoItem) {
    m_scene->removeItem(m_lassoItem);
    delete m_lassoItem;
    m_lassoItem = nullptr;
  }
  m_cropMenu->hide();
  m_interactionMode = InteractionMode::None;
}

// === TRANSFORM SYSTEM ===

void CanvasView::startTransformSession() {
  QList<QGraphicsItem *> items = m_scene->selectedItems();
  if (items.isEmpty())
    return;

  // Clean up previous session if still active
  if (m_transformOverlay) {
    m_scene->removeItem(m_transformOverlay);
    delete m_transformOverlay;
    m_transformOverlay = nullptr;
  }
  if (m_transformGroup) {
    m_scene->destroyItemGroup(m_transformGroup);
    m_transformGroup = nullptr;
  }
  m_interactionMode = InteractionMode::Transform;
  if (items.count() == 1) {
    QGraphicsItem *singleItem = items.first();
    m_transformGroup = nullptr;
    QPointF oldCenter = singleItem->sceneBoundingRect().center();
    singleItem->setTransformOriginPoint(singleItem->boundingRect().center());
    QPointF newCenter = singleItem->sceneBoundingRect().center();
    singleItem->moveBy(oldCenter.x() - newCenter.x(),
                       oldCenter.y() - newCenter.y());
    bakeTransform(singleItem);
    m_transformOverlay = new TransformOverlay(singleItem);
    connect(m_transformOverlay, &TransformOverlay::transformChanged, this, &CanvasView::updateSelectionMenuPosition);
    connect(m_transformOverlay, &TransformOverlay::interactionStarted, m_selectionMenu, &QWidget::hide);
    connect(m_transformOverlay, &TransformOverlay::interactionEnded, this, &CanvasView::updateSelectionMenuPosition);
  } else {
    m_transformGroup = m_scene->createItemGroup(items);
    QRectF grpRect = m_transformGroup->boundingRect();
    m_transformGroup->setTransformOriginPoint(grpRect.center());
    m_transformOverlay = new TransformOverlay(m_transformGroup);
    connect(m_transformOverlay, &TransformOverlay::transformChanged, this, &CanvasView::updateSelectionMenuPosition);
    connect(m_transformOverlay, &TransformOverlay::interactionStarted, m_selectionMenu, &QWidget::hide);
    connect(m_transformOverlay, &TransformOverlay::interactionEnded, this, &CanvasView::updateSelectionMenuPosition);
  }
  m_transformOverlay->setZValue(99999);
  m_scene->addItem(m_transformOverlay);
}

void CanvasView::applyTransform() {
  if (m_transformOverlay) {
    m_scene->removeItem(m_transformOverlay);
    delete m_transformOverlay;
    m_transformOverlay = nullptr;
  }
  if (m_transformGroup) {
    m_scene->destroyItemGroup(m_transformGroup);
    m_transformGroup = nullptr;
  }
  m_interactionMode = InteractionMode::None;
  emit contentModified();
}

void CanvasView::updateSelectionMenuPosition() {
  if (m_scene->selectedItems().isEmpty()) {
    m_selectionMenu->hide();
    return;
  }
  QRectF totalRect;
  for (auto *item : m_scene->selectedItems())
    totalRect = totalRect.united(item->sceneBoundingRect());
  QPoint viewPos = mapFromScene(totalRect.topLeft());
  int x = viewPos.x() +
          (mapFromScene(totalRect.bottomRight()).x() - viewPos.x()) / 2 -
          m_selectionMenu->width() / 2;
  int y = viewPos.y() - m_selectionMenu->height() - 55;
  x = qMax(0, qMin(x, width() - m_selectionMenu->width()));
  y = qMax(0, y);
  m_selectionMenu->move(x, y);
  m_selectionMenu->show();
  m_selectionMenu->raise();
}

void CanvasView::updateCropMenuPosition() {
  QRectF totalRect;
  for (auto *item : m_scene->selectedItems())
    totalRect = totalRect.united(item->sceneBoundingRect());
  if (totalRect.isEmpty())
    totalRect = mapToScene(viewport()->rect()).boundingRect();
  QPoint viewPos = mapFromScene(totalRect.topLeft());
  int x = viewPos.x() +
          (mapFromScene(totalRect.bottomRight()).x() - viewPos.x()) / 2 -
          m_cropMenu->width() / 2;
  int y = viewPos.y() - m_cropMenu->height() - 10;
  x = qMax(0, qMin(x, width() - m_cropMenu->width()));
  y = qMax(0, y);
  m_cropMenu->move(x, y);
}

void CanvasView::clearSelection() {
  if (m_interactionMode != InteractionMode::None)
    return;
  m_scene->clearSelection();
  if (m_lassoItem) {
    m_scene->removeItem(m_lassoItem);
    delete m_lassoItem;
    m_lassoItem = nullptr;
  }
  m_selectionMenu->hide();
}

void CanvasView::wheelEvent(QWheelEvent *event) {
  if (m_toolManager && m_toolManager->activeToolMode() == ToolMode::Ruler) {
      RulerItem* ruler = nullptr;
      for (QGraphicsItem* item : m_scene->items()) {
          if (item->type() == RulerItem::Type) {
              ruler = static_cast<RulerItem*>(item);
              break;
          }
      }
      if (ruler && ruler->isVisible()) {
          qreal delta = event->angleDelta().y();
          qreal step = (event->modifiers() & Qt::ShiftModifier) ? 1.0 : 15.0;
          if (delta > 0) ruler->setRotation(ruler->rotation() - step);
          else if (delta < 0) ruler->setRotation(ruler->rotation() + step);
          ruler->update();
          event->accept();
          return;
      }
  }

  if (event->modifiers() & Qt::ControlModifier) {
    double angle = event->angleDelta().y();
    double factor = std::pow(1.0015, angle);
    double currentScale = transform().m11();
    if ((currentScale < 0.1 && factor < 1) || (currentScale > 10 && factor > 1))
      return;
    scale(factor, factor);
    viewport()->update();
    event->accept();
  } else {
    if (!m_isInfinite) {
      QScrollBar *vb = verticalScrollBar();
      if (vb->value() >= vb->maximum() && event->angleDelta().y() < 0) {
        m_pullDistance += std::abs(event->angleDelta().y()) * 0.5;
        viewport()->update();
        if (m_pullDistance > 250.0f) {
          addNewPage();
          m_pullDistance = 0;
        }
        event->accept();
        return;
      } else {
        if (m_pullDistance > 0) {
          m_pullDistance = 0;
          viewport()->update();
        }
      }
    }
    QGraphicsView::wheelEvent(event);
  }
}

void CanvasView::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
#ifdef Q_OS_ANDROID
  if (m_isInfinite) {
    updateSceneRect();
  } else if (viewport() && viewport()->width() > 0 && viewport()->height() > 0) {
    const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
    if (!vis.isEmpty()) {
      QRectF sr = m_scene->sceneRect();
      m_scene->setSceneRect(sr.united(vis.adjusted(-100, -100, 100, 100)));
    }
  }
#else
  updateSceneRect();
#endif
}

void CanvasView::showEvent(QShowEvent *event) {
  QGraphicsView::showEvent(event);
#ifdef Q_OS_ANDROID
  if (m_isInfinite) {
    updateSceneRect();
  } else if (viewport() && viewport()->width() > 0 && viewport()->height() > 0) {
    const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
    if (!vis.isEmpty()) {
      QRectF sr = m_scene->sceneRect();
      m_scene->setSceneRect(sr.united(vis.adjusted(-100, -100, 100, 100)));
    }
  }
#endif
}

// === WICHTIG: EVENT DELEGATION AN TOOL MANAGER ===

void CanvasView::tabletEvent(QTabletEvent *event) {
    if (m_toolManager && m_toolManager->activeTool() && m_interactionMode == InteractionMode::None) {
        // Automatically switch to drawing mode (disabling touch-panning temporarily if they hover with pen)
        QPointF scenePos = mapToScene(event->position().toPoint());
        
        if (m_toolManager->activeTool()->handleTabletEvent(event, scenePos)) {
            if (event->type() == QEvent::TabletPress) {
                m_isDrawing = true;
            } else if (event->type() == QEvent::TabletRelease) {
                m_isDrawing = false;
                if (auto* item = m_toolManager->activeTool()->lastCompletedItem()) {
                    m_undoStack->push(new AddItemCommand(m_scene, item));
                    m_toolManager->activeTool()->clearLastCompletedItem();
                }
                emit contentModified();
                if (m_toolManager->activeTool()->mode() == ToolMode::Lasso) {
                    finishLasso();
                }
            }
            event->accept();
            return;
        }
    }
    QGraphicsView::tabletEvent(event);
}


void CanvasView::mousePressEvent(QMouseEvent *event) {
  const QInputDevice *dev = event->device();
  
  // Ignore fallback mouse events generated by the OS for Stylus EXCEPT when using tools like Lasso that rely on mouse events,
  // or when we are interacting with Transform overlays.
  if (dev && dev->type() == QInputDevice::DeviceType::Stylus) {
      if (m_interactionMode == InteractionMode::None && 
          m_toolManager && m_toolManager->activeTool() &&
          m_toolManager->activeTool()->mode() != ToolMode::Lasso) {
          event->accept();
          return;
      }
  }

  bool isTouch = (dev && dev->type() == QInputDevice::DeviceType::TouchScreen);
  if (!isTouch && event->source() == Qt::MouseEventSynthesizedBySystem)
    isTouch = true;

  // 1. Prüfen ob wir im Transform-Modus sind (höchste Prio)
  bool hitHandle = false;
  if (m_interactionMode == InteractionMode::Transform && m_transformOverlay) {
    TransformOverlay::Handle h =
        m_transformOverlay->handleAt(mapToScene(event->pos()));
    if (h != TransformOverlay::None)
      hitHandle = true;
  }

  // Panning: only apply when touch events are used AND pen-only mode is enabled
  // Regular mouse (left button) should ALWAYS reach tool delegation, not pan.
  bool isRegularMouse = (dev == nullptr || dev->type() == QInputDevice::DeviceType::Mouse);
  if (m_penOnlyMode && isTouch && !hitHandle && !isRegularMouse) {
    m_isPanning = true;
    m_lastPanPos = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }
  if (event->button() == Qt::MiddleButton) {
    m_isPanning = true;
    m_lastPanPos = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }

  // 2. Crop Modus
  if (m_interactionMode == InteractionMode::Crop) {
    if (m_cropSubMode == 2) { // Freehand
      m_isDrawing = true;
      m_currentPath = QPainterPath();
      m_currentPath.moveTo(mapToScene(event->pos()));
      if (!m_lassoItem) {
        m_lassoItem = new QGraphicsPathItem();
        m_lassoItem->setPen(QPen(Qt::red, 2, Qt::DashLine));
        m_lassoItem->setZValue(99999);
        m_scene->addItem(m_lassoItem);
      }
    }
    QGraphicsView::mousePressEvent(event);
    return;
  }

  if (m_interactionMode == InteractionMode::Transform) {
    if (m_transformOverlay && !m_transformOverlay->isHandleAt(mapToScene(event->pos())))
      applyTransform();
    else {
      QGraphicsView::mousePressEvent(event);
      return;
    }
  }

  // 3. TOOL MANAGER DELEGATION
  // Wir nutzen m_toolManager, NICHT instance(), um sicherzugehen
  if (m_toolManager && m_toolManager->activeTool() &&
      event->button() == Qt::LeftButton) {
    QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMousePress);
    QPointF scenePos = mapToScene(event->pos());
    scEvent.setScenePos(scenePos);
    scEvent.setButton(event->button());
    scEvent.setButtons(event->buttons());
    scEvent.setModifiers(event->modifiers());

    // Snapshot current items so we can detect newly added items on release
    m_preStrokeItems = QSet<QGraphicsItem *>(m_scene->items().begin(), m_scene->items().end());

    if (m_toolManager->activeTool()->handleMousePress(&scEvent, m_scene)) {
      m_isDrawing = true;
      event->accept();
      return;
    }
  }

  // 4. Standard Selektion Logic (wenn kein Tool aktiv war)
  if (event->button() == Qt::LeftButton) {
    QPointF scenePos = mapToScene(event->pos());
    if (m_selectionMenu->isVisible() && !itemAt(event->pos()))
      m_selectionMenu->hide();
    bool onPage = false;
    if (!m_isInfinite) {
      qreal currentY = 0;
      while (currentY < m_a4Rect.height()) {
        QRectF pageRect(0, currentY, pageWidthPx(), pageHeightPx());
        if (pageRect.contains(scenePos)) {
          onPage = true;
          break;
        }
        currentY += pageHeightPx() + pageGapPx();
      }
      if (!onPage)
        return;
    }
  }
  QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event) {
  const QInputDevice *dev = event->device();
  if (dev && dev->type() == QInputDevice::DeviceType::Stylus) {
      event->accept();
      return;
  }

  if (m_isPanning) {
    QPoint delta = event->pos() - m_lastPanPos;
    m_lastPanPos = event->pos();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    event->accept();
    return;
  }

  if (m_interactionMode == InteractionMode::Crop && m_cropSubMode == 2 &&
      m_isDrawing && m_lassoItem) {
    m_currentPath.lineTo(mapToScene(event->pos()));
    m_lassoItem->setPath(m_currentPath);
    return;
  }

  // TOOL MANAGER DELEGATION
  if (m_toolManager && m_toolManager->activeTool() &&
      m_interactionMode == InteractionMode::None) {
    // Auch ohne Drawing senden wir Move Events (für Hover/Cursor oder
    // Lineal-Verschieben)
    QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMouseMove);
    QPointF scenePos = mapToScene(event->pos());
    scEvent.setScenePos(scenePos);
    scEvent.setButtons(event->buttons());
    scEvent.setModifiers(event->modifiers());

    if (m_toolManager->activeTool()->handleMouseMove(&scEvent, m_scene)) {
      event->accept();
      return;
    }
  }
  QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event) {
  const QInputDevice *dev = event->device();
  if (dev && dev->type() == QInputDevice::DeviceType::Stylus) {
      event->accept();
      return;
  }

  bool isTouch = (dev && dev->type() == QInputDevice::DeviceType::TouchScreen);
  if (!isTouch && event->source() == Qt::MouseEventSynthesizedBySystem)
    isTouch = true;

  if (m_penOnlyMode && isTouch) {
    m_isPanning = false;
    if (m_pullDistance > 0) {
      m_pullDistance = 0;
      viewport()->update();
    }
    setTool(m_currentTool);
    event->accept();
    return;
  }
  if (event->button() == Qt::MiddleButton) {
    m_isPanning = false;
    if (m_pullDistance > 0) {
      m_pullDistance = 0;
      viewport()->update();
    }
    setTool(m_currentTool);
    event->accept();
    return;
  }

  if (m_interactionMode == InteractionMode::Crop && m_cropSubMode == 2 &&
      m_isDrawing) {
    m_isDrawing = false;
    m_currentPath.closeSubpath();
    if (m_lassoItem)
      m_lassoItem->setPath(m_currentPath);
    return;
  }

  // TOOL MANAGER DELEGATION
  if (m_toolManager && m_toolManager->activeTool() &&
      m_interactionMode == InteractionMode::None) {
    QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMouseRelease);
    QPointF scenePos = mapToScene(event->pos());
    scEvent.setScenePos(scenePos);
    scEvent.setButton(event->button());

    if (m_toolManager->activeTool()->handleMouseRelease(&scEvent, m_scene)) {
      m_isDrawing = false;
      if (auto* item = m_toolManager->activeTool()->lastCompletedItem()) {
          m_undoStack->push(new AddItemCommand(m_scene, item));
          m_toolManager->activeTool()->clearLastCompletedItem();
      }
      emit contentModified();

      // Push undo commands for any items added since the stroke started
      if (!m_preStrokeItems.isEmpty()) {
          for (QGraphicsItem *item : m_scene->items()) {
              if (!m_preStrokeItems.contains(item)) {
                  m_undoStack->push(new AddItemCommand(m_scene, item));
              }
          }
          m_preStrokeItems.clear();
      }

      if (m_toolManager->activeTool()->mode() == ToolMode::Lasso) {
        finishLasso();
      }
      event->accept();
      return;
    }
  }
  QGraphicsView::mouseReleaseEvent(event);
}

bool CanvasView::event(QEvent *event) {
  if (event->type() == QEvent::Gesture) {
    gestureEvent(static_cast<QGestureEvent *>(event));
    return true;
  }
  return QGraphicsView::event(event);
}

void CanvasView::gestureEvent(QGestureEvent *event) {
  if (QGesture *pinch = event->gesture(Qt::PinchGesture))
    pinchTriggered(static_cast<QPinchGesture *>(pinch));
}

void CanvasView::pinchTriggered(QPinchGesture *gesture) {
  QPointF scenePos = mapToScene(gesture->centerPoint().toPoint());
  QGraphicsItem *hoveredItem = scene()->itemAt(scenePos, transform());
  while (hoveredItem && hoveredItem->type() != QGraphicsItem::UserType + 100) {
    hoveredItem = hoveredItem->parentItem();
  }

  if (hoveredItem && hoveredItem->type() == QGraphicsItem::UserType + 100) {
    qreal rotationDelta = gesture->rotationAngle() - gesture->lastRotationAngle();
    hoveredItem->setRotation(hoveredItem->rotation() + rotationDelta);

    QPointF diff = mapToScene(gesture->centerPoint().toPoint()) -
                   mapToScene(gesture->lastCenterPoint().toPoint());
    hoveredItem->moveBy(diff.x(), diff.y());
    return;
  }

  if (m_interactionMode == InteractionMode::Transform && m_transformOverlay) {
    if (gesture->state() == Qt::GestureStarted) {
      bakeTransform(m_transformOverlay->target());
      m_transformOverlay->sync();
    }
    QGraphicsItem *item = m_transformOverlay->target();
    qreal scaleFactor = gesture->scaleFactor();
    qreal rotationAngle =
        gesture->rotationAngle() - gesture->lastRotationAngle();
    QPointF diff = mapToScene(gesture->centerPoint().toPoint()) -
                   mapToScene(gesture->lastCenterPoint().toPoint());
    item->moveBy(diff.x(), diff.y());
    QPointF scenePivot = mapToScene(gesture->centerPoint().toPoint());
    QPointF localPivot = item->transform().inverted().map(scenePivot);
    QTransform delta;
    delta.translate(localPivot.x(), localPivot.y());
    delta.rotate(-rotationAngle);
    delta.scale(scaleFactor, scaleFactor);
    delta.translate(-localPivot.x(), -localPivot.y());
    item->setTransform(delta * item->transform());
    m_transformOverlay->sync();
    return;
  }
  if (m_isDrawing) {
    m_isDrawing = false;
  }
  if (m_isPanning)
    m_isPanning = false;
  QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
  if (changeFlags & QPinchGesture::ScaleFactorChanged) {
    qreal factor = gesture->scaleFactor();
    double currentScale = transform().m11();
    if (!((currentScale < 0.1 && factor < 1) ||
          (currentScale > 10 && factor > 1)))
      scale(factor, factor);
  }
  if (gesture->state() == Qt::GestureUpdated) {
    QPoint delta =
        gesture->centerPoint().toPoint() - gesture->lastCenterPoint().toPoint();
    if (!delta.isNull()) {
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                      delta.x());
      verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    }
  }
}

void CanvasView::finishLasso() {
  if (!m_scene->selectedItems().isEmpty()) {
    startTransformSession();
    updateSelectionMenuPosition();
  } else {
    m_selectionMenu->hide();
  }
}

void CanvasView::applyEraser(const QPointF &pos) {
  QRectF eraserRect(pos.x() - 10, pos.y() - 10, 20, 20);
  const QList<QGraphicsItem *> items = m_scene->items(eraserRect);
  bool erased = false;
  for (auto *item : std::as_const(items)) {
    if (item->type() == QGraphicsPathItem::Type) {
      m_scene->removeItem(item);
      m_undoStack->push(new RemoveItemCommand(m_scene, item));
      erased = true;
    }
  }
  if (erased)
    emit contentModified();
}

bool CanvasView::exportToPDF(const QString &path) {
    if (path.isEmpty()) return false;
    
    QPdfWriter pdfWriter(path);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setResolution(300);
    pdfWriter.setCreator("Blop Notes");
    
    QPainter painter(&pdfWriter);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRectF bounds;
    if (m_isInfinite) {
        bounds = m_scene->itemsBoundingRect();
        if (bounds.isEmpty()) bounds = QRectF(0, 0, 800, 1130);
        else bounds.adjust(-50, -50, 50, 50);
    } else {
        bounds = m_scene->sceneRect();
    }
    
    // Calculate aspect ratio matching
    QRect targetRect = pdfWriter.pageLayout().paintRectPixels(pdfWriter.resolution());
    double scale = qMin(targetRect.width() / bounds.width(), targetRect.height() / bounds.height());
    
    painter.translate(targetRect.center());
    painter.scale(scale, scale);
    painter.translate(-bounds.center());
    
    // Draw back    ground grid
    drawBackground(&painter, bounds);
    // Draw scene items
    m_scene->render(&painter, bounds, bounds);
    
    return true;
}

bool CanvasView::exportToImage(const QString &path) {
    if (path.isEmpty()) return false;
    
    QRectF bounds;
    if (m_isInfinite) {
        bounds = m_scene->itemsBoundingRect();
        if (bounds.isEmpty()) bounds = QRectF(0, 0, 1920, 1080);
        else bounds.adjust(-50, -50, 50, 50);
    } else {
        bounds = m_scene->sceneRect();
    }
    
    QImage image(bounds.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(m_pageColor);
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Translate painter to match image zero coordinate logic
    painter.translate(-bounds.topLeft());
    
    // Render the grid background
    drawBackground(&painter, bounds);
    
    // Render strokes
    m_scene->render(&painter, bounds, bounds);
    
    return image.save(path);
}

#include "canvasview.moc"
