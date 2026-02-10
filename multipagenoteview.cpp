#include "multipagenoteview.h"
#include "UIStyles.h"
#include "tools/AbstractTool.h"
#include "tools/RulerTool.h" // NEU: Lineal-Werkzeug
#include "tools/ToolManager.h"
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPen>
#include <QPointingDevice>
#include <QScrollBar>
#include <algorithm>
#include <cmath>

static constexpr int A4W = 793;
static constexpr int A4H = 1122;
static constexpr int PageSpacing = 60;

MultiPageNoteView::MultiPageNoteView(QWidget *parent) : QGraphicsView(parent) {
  setScene(&scene_);
  setBackgroundBrush(UIStyles::SceneBackground);

  // Gesten auf dem Viewport registrieren
  viewport()->grabGesture(Qt::PinchGesture);

  setOptimizationFlags(QGraphicsView::DontSavePainterState |
                       QGraphicsView::DontAdjustForAntialiasing);
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

  setDragMode(QGraphicsView::NoDrag);
  setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
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
}

void MultiPageNoteView::setNote(Note *note) {
  note_ = note;
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
    for (const auto &s : note_->pages[i].strokes) {
      auto item = new QGraphicsPathItem(s.path);

      QPen pen;
      if (s.isEraser) {
        pen = QPen(UIStyles::PageBackground, s.width);
        item->setZValue(1.0);
      } else if (s.isHighlighter) {
        QColor c = s.color;
        c.setAlpha(80);
        pen = QPen(c, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        item->setZValue(0.5);
      } else {
        pen =
            QPen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        item->setZValue(1.0);
      }

      item->setPen(pen);
      scene_.addItem(item);
      item->setPos(pageRect(i).topLeft());
    }
  }
  scene_.blockSignals(wasBlocked);
}

void MultiPageNoteView::layoutPages() {
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

void MultiPageNoteView::resizeEvent(QResizeEvent *) {}

void MultiPageNoteView::wheelEvent(QWheelEvent *e) {
  if (e->modifiers() & Qt::ControlModifier) {
    qreal delta = e->angleDelta().y() / 120.0;
    qreal factor = (delta > 0) ? 1.1 : 0.9;
    scale(factor, factor);
    zoom_ *= factor;
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
  // ------------------------------------------

  if (e->button() == Qt::LeftButton) {
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
      if (onSaveRequested)
        onSaveRequested(note_);
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
    if (note_) {
      note_->pages[currentPage_].strokes.push_back(std::move(currentStroke_));
      if (onSaveRequested)
        onSaveRequested(note_);
    }
    currentPathItem_ = nullptr;
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
