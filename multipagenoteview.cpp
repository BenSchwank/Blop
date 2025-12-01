#include "multipagenoteview.h"
#include "UIStyles.h"
#include <QGraphicsRectItem>
#include <QPainterPath>
#include <QPen>
#include <QScrollBar>
#include <cmath>


static constexpr int A4W = 793;
static constexpr int A4H = 1122;
static constexpr int PageSpacing = 40;

MultiPageNoteView::MultiPageNoteView(QWidget *parent) : QGraphicsView(parent) {
  setScene(&scene_);
  setBackgroundBrush(UIStyles::SceneBackground);
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
  setDragMode(QGraphicsView::ScrollHandDrag);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setAcceptDrops(false);
  viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
}

void MultiPageNoteView::setNote(Note *note) {
  note_ = note;
  scene_.clear();
  pageItems_.clear();
  layoutPages();
  // Render strokes
  if (!note_)
    return;
  for (int i = 0; i < note_->pages.size(); ++i) {
    for (const auto &s : note_->pages[i].strokes) {
      auto item = new QGraphicsPathItem(s.path);
      item->setZValue(1.0);
      QPen pen(s.isEraser ? QPen(QBrush(UIStyles::PageBackground), s.width)
                          : QPen(s.color, s.width, Qt::SolidLine, Qt::RoundCap,
                                 Qt::RoundJoin));
      item->setPen(pen);
      scene_.addItem(item);
      item->setPos(pageRect(i).topLeft());
    }
  }
}

void MultiPageNoteView::layoutPages() {
  if (!note_)
    return;
  int n = std::max(1, note_->pages.size());
  qreal y = 0;
  for (int i = 0; i < n; ++i) {
    auto rectItem = new QGraphicsRectItem(0, 0, A4W, A4H);
    rectItem->setBrush(UIStyles::PageBackground);
    rectItem->setPen(QPen(QColor(200, 200, 200), 1));
    rectItem->setZValue(0.0);
    scene_.addItem(rectItem);
    rectItem->setPos(0, y);
    pageItems_.push_back(rectItem);
    y += A4H + PageSpacing;
  }
  scene_.setSceneRect(QRectF(-20, -20, A4W + 40, y + 40));
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

void MultiPageNoteView::ensureOverscrollPage() {
  if (!note_)
    return;
  auto vbar = verticalScrollBar();
  int max = vbar->maximum();
  if (max - vbar->value() < 80) {
    note_->ensurePage(note_->pages.size());
    layoutPages();
    if (onSaveRequested)
      onSaveRequested(note_);
  }
}

void MultiPageNoteView::resizeEvent(QResizeEvent *) { ensureOverscrollPage(); }

void MultiPageNoteView::wheelEvent(QWheelEvent *e) {
  if (e->modifiers() & Qt::ControlModifier) {
    // Pinch-like zoom via Ctrl+Wheel
    qreal delta = e->angleDelta().y() / 120.0;
    qreal factor = (delta > 0) ? 1.1 : 0.9;
    qreal newZoom = std::clamp(zoom_ * factor, 0.25, 4.0);
    scale(newZoom / zoom_, newZoom / zoom_);
    zoom_ = newZoom;
    e->accept();
  } else {
    QGraphicsView::wheelEvent(e);
    ensureOverscrollPage();
  }
}

bool MultiPageNoteView::viewportEvent(QEvent *ev) {
  if (ev->type() == QEvent::TouchBegin || ev->type() == QEvent::TouchUpdate ||
      ev->type() == QEvent::TouchEnd) {
    // Simplified: touch pans by default; optional drawing could be enabled via
    // a setting
    return QGraphicsView::viewportEvent(ev);
  }
  return QGraphicsView::viewportEvent(ev);
}

void MultiPageNoteView::tabletEvent(QTabletEvent *e) {
  // Stylus draws regardless of finger setting
  if (!note_ || mode_ == ToolMode::Lasso) {
    e->ignore();
    return;
  }
  QPointF scenePos = mapToScene(e->pos());
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
    currentStroke_.width =
        (mode_ == ToolMode::Eraser ? 12.0
                                   : std::max<qreal>(2.0, e->pressure() * 4.0));
    currentStroke_.color = penColor_;
    currentStroke_.points.push_back(local);
    currentStroke_.path = QPainterPath(local);
    currentPathItem_ = new QGraphicsPathItem();
    QPen pen(currentStroke_.isEraser
                 ? QPen(QBrush(UIStyles::PageBackground), currentStroke_.width)
                 : QPen(currentStroke_.color, currentStroke_.width,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    currentPathItem_->setPen(pen);
    currentPathItem_->setZValue(1.0);
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
      note_->pages[currentPage_].strokes.push_back(currentStroke_);
      if (onSaveRequested)
        onSaveRequested(note_);
    }
    currentPathItem_ = nullptr;
    e->accept();
  } else {
    e->ignore();
  }
}

void MultiPageNoteView::mousePressEvent(QMouseEvent *e) {
  // Allow mouse drawing for testing
  if (e->button() == Qt::LeftButton) {
    QTabletEvent fake(QEvent::TabletPress, e->localPos(), e->globalPos(),
                      QPointF(), 1.0, 0, 0, 0, 0, Qt::NoModifier, 0);
    tabletEvent(&fake);
  } else {
    QGraphicsView::mousePressEvent(e);
  }
}

void MultiPageNoteView::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons() & Qt::LeftButton && drawing_) {
    QTabletEvent fake(QEvent::TabletMove, e->localPos(), e->globalPos(),
                      QPointF(), 1.0, 0, 0, 0, 0, e->modifiers(), 0);
    tabletEvent(&fake);
  } else {
    QGraphicsView::mouseMoveEvent(e);
  }
}

void MultiPageNoteView::mouseReleaseEvent(QMouseEvent *e) {
  if (drawing_) {
    QTabletEvent fake(QEvent::TabletRelease, e->localPos(), e->globalPos(),
                      QPointF(), 1.0, 0, 0, 0, 0, e->modifiers(), 0);
    tabletEvent(&fake);
  } else {
    QGraphicsView::mouseReleaseEvent(e);
  }
}

// In MultiPageNoteView
#include <QImage>
#include <QPainter>
#include <QPdfWriter>

bool exportPageToPng(int pageIndex, const QString &path) {
  QImage img(A4W, A4H, QImage::Format_ARGB32_Premultiplied);
  img.fill(UIStyles::PageBackground);
  QPainter p(&img);
  // draw strokes on that page
  if (note_ && pageIndex >= 0 && pageIndex < note_->pages.size()) {
    for (const auto &s : note_->pages[pageIndex].strokes) {
      QPen pen(s.isEraser ? QPen(QBrush(UIStyles::PageBackground), s.width)
                          : QPen(s.color, s.width, Qt::SolidLine, Qt::RoundCap,
                                 Qt::RoundJoin));
      p.setPen(pen);
      p.drawPath(s.path);
    }
  }
  return img.save(path);
}

bool exportPageToPdf(int pageIndex, const QString &path) {
  QPdfWriter pdf(path);
  pdf.setPageSize(QPageSize(QPageSize::A4));
  QPainter p(&pdf);
  p.fillRect(QRectF(0, 0, A4W, A4H), UIStyles::PageBackground);
  if (note_ && pageIndex >= 0 && pageIndex < note_->pages.size()) {
    for (const auto &s : note_->pages[pageIndex].strokes) {
      QPen pen(s.isEraser ? QPen(QBrush(UIStyles::PageBackground), s.width)
                          : QPen(s.color, s.width, Qt::SolidLine, Qt::RoundCap,
                                 Qt::RoundJoin));
      p.setPen(pen);
      p.drawPath(s.path);
    }
  }
  p.end();
  return true;
}
