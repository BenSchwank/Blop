#include "multipagenoteview.h"
#include "TransformOverlay.h"
#include "TransformOverlay.h"
#include "UIStyles.h"
#include "tools/AbstractTool.h"
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
#include <QScrollBar>
#include <QClipboard>
#include <QColorDialog>
#include <QGuiApplication>
#ifdef BLOP_HAS_PDF
#include <QPdfDocument>
#endif
#include <algorithm>
#include <cmath>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFrame>
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
    auto addBtn = [&](QString text, auto signal) {
      QPushButton *btn = new QPushButton(text, this);
      connect(btn, &QPushButton::clicked, this, signal);
      layout->addWidget(btn);
    };
    addBtn("✂️", &NoteSelectionMenu::cutRequested);
    addBtn("📋", &NoteSelectionMenu::copyRequested);
    addBtn("🎨", &NoteSelectionMenu::colorRequested);
    addBtn("📐", &NoteSelectionMenu::cropRequested);
    addBtn("📸", &NoteSelectionMenu::screenshotRequested);
    QFrame *line = new QFrame;
    line->setFrameShape(QFrame::VLine);
    line->setStyleSheet("color: #555;");
    layout->addWidget(line);
    QPushButton *btnDel = new QPushButton("🗑️", this);
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

static constexpr int A4W = 793;
static constexpr int A4H = 1122;
static constexpr int PageSpacing = 60;

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

MultiPageNoteView::MultiPageNoteView(QWidget *parent) : QGraphicsView(parent) {
  m_undoStack = new QUndoStack(this);

  setScene(&scene_);
  scene_.setItemIndexMethod(QGraphicsScene::NoIndex);
  applyGraphicsViewCanvasBackground(this);

  // Gesten auf dem Viewport registrieren
  viewport()->grabGesture(Qt::PinchGesture);

  setOptimizationFlags(QGraphicsView::DontSavePainterState |
                       QGraphicsView::DontAdjustForAntialiasing);
  setRenderHints(QPainter::Antialiasing);
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

}

void MultiPageNoteView::setNote(Note *note) {
  note_ = note;
  if (m_undoStack)
    m_undoStack->clear();
  scene_.clear();
  pageItems_.clear();

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
    // Restore background image on the PageItem if present
    if (i < pageItems_.size() && !note_->pages[i].backgroundImage.isNull()) {
      pageItems_[i]->setBackgroundImage(note_->pages[i].backgroundImage);
    }
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
  // Vorhandene Seiten-Items entfernen
  for (auto *item : pageItems_) {
    // Sicherstellen, dass wir nicht versehentlich das Lineal oder andere Tools
    // löschen, falls diese in der scene_ liegen (RulerItem hat eigenen Type,
    // sollte sicher sein)
    scene_.removeItem(item);
    delete item;
  }
  pageItems_.clear();

  if (!note_)
    return;
  int n = std::max(1, (int)note_->pages.size());
  qreal y = 0;

  for (int i = 0; i < n; ++i) {
    auto pageItem = new PageItem(0, 0, A4W, A4H);
    // Seiten ganz unten (Z=0 oder negativ), damit Lineal drüber ist
    pageItem->setZValue(0);
    scene_.addItem(pageItem);
    pageItem->setPos(0, y);
    pageItems_.push_back(pageItem);
    y += A4H + PageSpacing;
  }
  scene_.setSceneRect(QRectF(-100, -100, A4W + 200, y + 200));
  ensureSceneRectCoversViewport();
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
  qreal y = idx * (A4H + PageSpacing);
  return QRectF(0, y, A4W, A4H);
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

void MultiPageNoteView::ensureSceneRectCoversViewport() {
  if (!viewport() || viewport()->width() <= 0 || viewport()->height() <= 0)
    return;
  const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
  if (vis.isEmpty())
    return;
  QRectF sr = scene_.sceneRect();
  scene_.setSceneRect(sr.united(vis.adjusted(-80, -80, 80, 80)));
}

void MultiPageNoteView::resizeEvent(QResizeEvent *e) {
  QGraphicsView::resizeEvent(e);
  ensureSceneRectCoversViewport();
}

void MultiPageNoteView::showEvent(QShowEvent *e) {
  QGraphicsView::showEvent(e);
  ensureSceneRectCoversViewport();
}

void MultiPageNoteView::wheelEvent(QWheelEvent *e) {
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
      RulerItem* ruler = nullptr;
      for (QGraphicsItem* item : scene_.items()) {
          if (item->type() == RulerItem::Type) {
              ruler = static_cast<RulerItem*>(item);
              break;
          }
      }
      if (ruler && ruler->isVisible()) {
          qreal delta = e->angleDelta().y();
          qreal step = (e->modifiers() & Qt::ShiftModifier) ? 1.0 : 15.0;
          if (delta > 0) ruler->setRotation(ruler->rotation() - step);
          else if (delta < 0) ruler->setRotation(ruler->rotation() + step);
          ruler->update();
          e->accept();
          return;
      }
  }

  if (e->modifiers() & Qt::ControlModifier) {
    qreal delta = e->angleDelta().y() / 120.0;
    qreal factor = (delta > 0) ? 1.1 : 0.9;
    scale(factor, factor);
    zoom_ *= factor;
    ensureSceneRectCoversViewport();
    e->accept();
  } else {
    QScrollBar *vb = verticalScrollBar();
    if (vb->value() >= vb->maximum() && e->angleDelta().y() < 0) {
      m_pullDistance += std::abs(e->angleDelta().y()) * 0.5;
      viewport()->update();

      if (m_pullDistance > 250.0f) {
        addNewPage();
        m_pullDistance = 0;
      }
      e->accept();
      return;
    }

    if (m_pullDistance > 0 && e->angleDelta().y() > 0) {
      m_pullDistance = 0;
      viewport()->update();
    }

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
  if (QGesture *pinch = event->gesture(Qt::PinchGesture)) {
    event->accept(pinch);
    pinchTriggered(static_cast<QPinchGesture *>(pinch));
  }
}

void MultiPageNoteView::pinchTriggered(QPinchGesture *gesture) {
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
      ensureSceneRectCoversViewport();
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

  if (isTouch) {
    if (m_penOnlyMode) {
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
      m_pullDistance += std::abs(delta.y()) * 0.5;
      viewport()->update();

      if (m_pullDistance > 250.0f) {
        addNewPage();
        m_pullDistance = 0;
      }
    } else {
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                      delta.x());
      verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
      if (m_pullDistance > 0) {
        m_pullDistance = 0;
        viewport()->update();
      }
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

void MultiPageNoteView::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pullDistance > 0) {
    m_pullDistance = 0;
    viewport()->update();
  }

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
      // 1. Hole den neu gezeichneten StrokeItem aus der Szene
      QList<QGraphicsItem*> itemsToRemove;
      for (QGraphicsItem* item : scene_.items()) {
        if (!item->parentItem() && item->type() == QGraphicsItem::UserType + 1) {
            auto* strokeItem = static_cast<StrokeItem*>(item);
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
                    s.isHighlighter = (strokeItem->strokeStyle() == StrokeItem::Highlighter);
                    
                    QPainterPath localPath;
                    for (int i=0; i < points.size(); ++i) {
                        QPointF localP = points[i].pos - pageTopLeft;
                        s.points.append(localP);
                        if (i == 0) localPath.moveTo(localP);
                        else localPath.lineTo(localP);
                    }
                    s.path = localPath;
                    pushStrokeUndoCommand(pIdx, std::move(s));
                }
            }
            itemsToRemove.append(item);
        }
      }

      for (auto* item : itemsToRemove) {
          scene_.removeItem(item);
          delete item;
      }

      if (tool->mode() == ToolMode::Lasso && !scene_.selectedItems().isEmpty()) {
          startTransformSession();
      }

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
  // Wenn Ruler oder ein anderes Tool aktiv ist, Tablet-Event ignorieren und
  // auf MouseEvent-Verarbeitung (oben) verlassen.
  if (ToolManager::instance().activeTool()) {
    e->ignore();
    return;
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
  QImage img(A4W, A4H, QImage::Format_ARGB32_Premultiplied);
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
  QPixmap pm = generateThumbnail(pageIndex, QSize(A4W, A4H));
  return pm.save(path, "PNG");
}

bool MultiPageNoteView::exportPageToPdf(int pageIndex, const QString &path) {
  QPdfWriter pdf(path);
  pdf.setPageSize(QPageSize(QPageSize::A4));
  QPainter p(&pdf);
  p.fillRect(QRectF(0, 0, A4W, A4H), Qt::white);
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

void MultiPageNoteView::drawForeground(QPainter *painter, const QRectF &rect) {
  QGraphicsView::drawForeground(painter, rect);

  if (m_pullDistance > 1.0f) {
    drawPullIndicator(painter);
  }

  AbstractTool *tool = ToolManager::instance().activeTool();
  if (tool) {
    painter->save();
    tool->drawOverlay(painter, rect);
    painter->restore();
  }
}

void MultiPageNoteView::drawPullIndicator(QPainter *painter) {
  painter->save();
  painter->resetTransform();
  painter->setClipping(false);

  int w = viewport()->width();
  int h = viewport()->height();
  float maxPull = 250.0f;
  float progress = qMin(m_pullDistance / maxPull, 1.0f);

  int size = 60;
  int yPos = h - size - 50 - (progress * 20);
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
    int spanAngle = -progress * 360 * 16;
    painter->drawArc(circleRect.adjusted(8, 8, -8, -8), 90 * 16, spanAngle);
  }

  if (progress > 0.15f) {
    painter->setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap));
    int center = size / 2;
    float growFactor = (progress - 0.15f) / 0.85f;
    int pSize = 12 * qMin(growFactor, 1.0f);
    QPoint c = circleRect.center();

    painter->drawLine(c.x(), c.y() - pSize, c.x(), c.y() + pSize);
    painter->drawLine(c.x() - pSize, c.y(), c.x() + pSize, c.y());
  }
  painter->restore();
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
    QImage img = doc.render(i, QSize(A4W, A4H));
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
    QColor c = QColorDialog::getColor(Qt::black, this, "Farbe ändern");
    if (c.isValid()) {
        for (auto *item : scene_.selectedItems()) {
            if (auto *pi = dynamic_cast<QGraphicsPathItem *>(item)) {
                QPen p = pi->pen();
                p.setColor(c);
                pi->setPen(p);
            }
        }
        scene_.update();
    }
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
