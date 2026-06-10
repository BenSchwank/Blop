#include "multipagenoteview.h"
#include "SelectionMenuIcons.h"
#include "TransformOverlay.h"
#include "croptools.h"
#include "blop_modal.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "editoroverlays.h"
#include "UIStyles.h"
#include "overlayscrollindicator.h"
#include "uiscale.h"
#include "tools/AbstractTool.h"
#include "tools/AbstractStrokeTool.h"
#include "tools/RulerTool.h" // NEU: Lineal-Werkzeug
#include "tools/ToolManager.h"
#include "tools/StrokeItem.h"
#include "tools/GraphCanvasItem.h"
#include "graphaxissettingsdialog.h"
#include "graphlegenddock.h"
#include "graphquickactionpopup.h"
#include "tools/math/MathExpressionParser.h"
#include "tools/math/NumericAnalysis.h"
#include "tools/math/MathInkRecognizer.h"
#include "tools/GraphFormulaZone.h"
#include <QFuture>
#include <QFutureWatcher>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPen>
#include <QPixmapCache>
#include <QtConcurrent/QtConcurrentRun>
#include <QPointingDevice>
#include <QPolygonF>
#include <QScrollBar>
#include <QClipboard>
#include <QCursor>
#include <QDateTime>
#include <QGuiApplication>
#include <QLineF>
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
#include <QCheckBox>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QEventPoint>
#include <QTouchEvent>
#include <QUrl>
#include <QBuffer>
#include <QPalette>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QToolTip>
#include <QUndoCommand>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QGraphicsItem>
#include <functional>

class MultiPageNoteView;

namespace {

QRect graphItemRectInViewCoords(QGraphicsView *v, GraphCanvasItem *graph) {
  if (!v || !graph)
    return {};
  return v->mapFromScene(graph->sceneBoundingRect()).boundingRect();
}

QPoint primaryPanelAnchor(QGraphicsView *v, GraphCanvasItem *graph, int anchorKind, int mh, int margin) {
  if (!v || !graph)
    return {};
  if (anchorKind == 0) {
    const QRectF ar = graph->plusButtonSceneRect();
    const QPoint br = v->mapFromScene(ar.bottomRight());
    QPoint topLeft(br.x() + 6, br.y() + 2);
    if (topLeft.y() + mh > v->height() - margin)
      topLeft.setY(v->mapFromScene(ar.topLeft()).y() - mh - 6);
    return topLeft;
  }
  const QRectF r = graph->sceneBoundingRect();
  const QPoint tr = v->mapFromScene(r.topRight());
  return QPoint(tr.x() + 10, tr.y());
}

QPoint pickSmartPanelPos(QGraphicsView *view, GraphCanvasItem *graph, int anchorKind, int mw, int mh,
                         int margin) {
  if (!view || !graph)
    return QPoint(margin, margin);
  const QRect avail(0, 0, view->width(), view->height());
  const QRect gr = graphItemRectInViewCoords(view, graph);
  QVector<QPoint> cands;
  cands.append(primaryPanelAnchor(view, graph, anchorKind, mh, margin));
  cands.append(QPoint(gr.right() + margin, gr.top()));
  cands.append(QPoint(gr.left() - mw - margin, gr.top()));
  cands.append(QPoint(gr.left(), gr.bottom() + margin));
  cands.append(QPoint(gr.left(), gr.top() - mh - margin));
  auto clamp = [&](QPoint tl) {
    const int x = qBound(margin, tl.x(), qMax(margin, view->width() - mw - margin));
    const int y = qBound(margin, tl.y(), qMax(margin, view->height() - mh - margin));
    return QPoint(x, y);
  };
  for (QPoint tl : cands) {
    const QPoint c = clamp(tl);
    if (avail.contains(QRect(c, QSize(mw, mh))))
      return c;
  }
  return clamp(cands.first());
}

QPoint legendTopLeftDocked(QGraphicsView *view, GraphCanvasItem *graph, int lw, int lh, int margin) {
  if (!view || !graph)
    return QPoint(margin, margin);
  QRect gr = view->mapFromScene(graph->sceneBoundingRect()).boundingRect();
  const QRect vp(0, 0, view->width(), view->height());
  const int g = 10;
  if (!gr.isValid() || gr.width() < 2 || gr.height() < 2) {
    const QPoint c = view->mapFromScene(graph->sceneBoundingRect().center());
    gr = QRect(c.x(), c.y(), 1, 1);
  }
  const QPoint right(gr.right() + g, gr.top());
  const QPoint left(gr.left() - g - lw, gr.top());
  auto clamp = [&](QPoint tl) {
    return QPoint(qBound(margin, tl.x(), qMax(margin, view->width() - lw - margin)),
                  qBound(margin, tl.y(), qMax(margin, view->height() - lh - margin)));
  };
  const QRect rr(right, QSize(lw, lh));
  const QRect rl(left, QSize(lw, lh));
  if (vp.intersects(rr))
    return clamp(right);
  if (vp.intersects(rl))
    return clamp(left);
  return clamp(right);
}

QPoint quickPopupTopLeftForView(QGraphicsView *view, const QPointF &scenePos, int pw, int ph, int margin) {
  if (!view)
    return QPoint(margin, margin);
  const QPoint c = view->mapFromScene(scenePos);
  QPoint tl = c + QPoint(10, -ph - 10);
  if (tl.y() < margin)
    tl.setY(c.y() + 14);
  tl.setX(qBound(margin, tl.x(), qMax(margin, view->width() - pw - margin)));
  tl.setY(qBound(margin, tl.y(), qMax(margin, view->height() - ph - margin)));
  return tl;
}

GraphCanvasItem *graphCanvasHittingPlus(QGraphicsScene *scene, const QPointF &scenePos) {
  if (!scene)
    return nullptr;
  const QList<QGraphicsItem *> stack = scene->items(scenePos);
  for (QGraphicsItem *raw : stack) {
    for (QGraphicsItem *w = raw; w; w = w->parentItem()) {
      if (w->type() == GraphCanvasItem::Type) {
        auto *gi = static_cast<GraphCanvasItem *>(w);
        if (gi->hitPlusButtonAtScene(scenePos))
          return gi;
        break;
      }
    }
  }
  return nullptr;
}

GraphCanvasItem *graphCanvasHittingChrome(QGraphicsScene *scene, const QPointF &scenePos) {
  if (!scene)
    return nullptr;
  const QList<QGraphicsItem *> stack = scene->items(scenePos);
  for (QGraphicsItem *raw : stack) {
    for (QGraphicsItem *w = raw; w; w = w->parentItem()) {
      if (w->type() == GraphCanvasItem::Type) {
        auto *gi = static_cast<GraphCanvasItem *>(w);
        if (gi->hitGraphChromeAtScene(scenePos))
          return gi;
        break;
      }
    }
  }
  return nullptr;
}
} // namespace

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

/// Undo/redo a single stroke added to a GraphFormulaZone.
class FormulaZoneStrokeCommand : public QUndoCommand {
public:
    FormulaZoneStrokeCommand(QPointer<GraphFormulaZone> zone,
                             const QPainterPath &path,
                             const QColor &color, qreal width)
        : QUndoCommand(), m_zone(zone), m_path(path),
          m_color(color), m_width(width) {}

    void undo() override {
        if (m_zone)
            m_zone->removeStroke(m_path);
    }

    void redo() override {
        // Skip the very first redo: the stroke was already added before we
        // pushed this command.  Only re-add on subsequent redo calls.
        if (m_firstRedo) { m_firstRedo = false; return; }
        if (m_zone)
            m_zone->addStroke(m_path, m_color, m_width);
    }

private:
    QPointer<GraphFormulaZone> m_zone;
    QPainterPath m_path;
    QColor       m_color;
    qreal        m_width;
    bool         m_firstRedo{true};
};

class NoteSelectionMenu : public QWidget {
  Q_OBJECT
public:
  explicit NoteSelectionMenu(QWidget *parent = nullptr) : QWidget(parent) {
    setStyleSheet(BlopTheme::themed(
        "QWidget { background-color: #252526; border-radius: 8px; border: 1px "
        "solid #444; }"
        "QPushButton { background: transparent; border: none; color: #F4F2FF; "
        "font-weight: bold; padding: 5px 8px; font-size: 14px; }"
        "QPushButton:hover { background-color: #3E3E42; border-radius: 4px; }"));
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
    addIconBtn(SelectionMenuIcons::duplicateIcon(),
               QStringLiteral("Duplizieren"),
               &NoteSelectionMenu::duplicateRequested);
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

class GraphInkInputWidget : public QWidget {
public:
  explicit GraphInkInputWidget(QWidget* p = nullptr) : QWidget(p) {
    setMinimumHeight(78);
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    setMouseTracking(true);
  }
  QVector<QPainterPath> strokes() const { return m_strokes; }
  void clearInk() {
    m_strokes.clear();
    m_current = QPainterPath();
    m_drawing = false;
    m_touchActive = false;
    update();
  }
  /// High-contrast raster of ink for vision-based recognition (white background, black strokes).
  /// Output is grayscale with aspect ratio matching BTTR/CROHME conventions (wider than tall).
  QImage inkImageForRecognition() const {
    QRectF bb;
    for (const auto &s : m_strokes) {
      for (const QPolygonF &poly : s.toSubpathPolygons()) {
        if (!poly.isEmpty())
          bb |= poly.boundingRect();
      }
    }
    if (!bb.isValid() || bb.isEmpty())
      return {};
    constexpr qreal kMargin = 10.0;
    bb.adjust(-kMargin, -kMargin, kMargin, kMargin);
    const qreal w = qMax(bb.width(), 1.0);
    const qreal h = qMax(bb.height(), 1.0);
    // Target aspect ratio closer to BTTR input (512x128 = 4:1).
    // Keep width at 512 and compute height preserving aspect ratio,
    // clamped so very tall expressions still fit.
    constexpr int kOutW = 512;
    const int kOutH = qBound(64, int(kOutW * h / w + 0.5), 256);
    QImage img(kOutW, kOutH, QImage::Format_Grayscale8);
    img.fill(255);  // white
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal sx = (kOutW - 20) / w;
    const qreal sy = (kOutH - 20) / h;
    const qreal sc = qMin(sx, sy);
    p.translate(10.0 - bb.left() * sc, 10.0 - bb.top() * sc);
    p.scale(sc, sc);
    // Pen width ~2-3px at final resolution, matching CROHME stroke widths
    const qreal penW = qMax(1.5, 2.5 / sc);
    p.setPen(QPen(Qt::black, penW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto &s : m_strokes)
      p.drawPath(s);
    return img;
  }
protected:
  bool event(QEvent *e) override {
    switch (e->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd: {
      auto *te = static_cast<QTouchEvent *>(e);
      const QList<QEventPoint> tpoints = te->points();
      if (tpoints.isEmpty()) {
        if (e->type() == QEvent::TouchEnd && m_drawing && m_touchActive) {
          m_drawing = false;
          m_touchActive = false;
          if (!m_current.isEmpty())
            m_strokes.push_back(m_current);
          m_current = QPainterPath();
          update();
          if (onInkChanged)
            onInkChanged();
          te->accept();
          return true;
        }
        break;
      }
      const QEventPoint &tp = tpoints.constFirst();
      const QPoint pos = mapFromGlobal(tp.globalPosition().toPoint());
      switch (e->type()) {
      case QEvent::TouchBegin:
        m_touchActive = true;
        m_drawing = true;
        m_current = QPainterPath();
        m_current.moveTo(pos);
        m_lastPos = pos;
        updatePointRegion(pos);
        te->accept();
        return true;
      case QEvent::TouchUpdate:
        if (m_drawing && m_touchActive) {
          m_current.lineTo(pos);
          updateSegmentRegion(m_lastPos, pos);
          m_lastPos = pos;
        }
        te->accept();
        return true;
      case QEvent::TouchEnd:
        if (m_drawing && m_touchActive) {
          m_drawing = false;
          m_touchActive = false;
          if (!m_current.isEmpty())
            m_strokes.push_back(m_current);
          m_current = QPainterPath();
          update();
          if (onInkChanged)
            onInkChanged();
        }
        te->accept();
        return true;
      default:
        break;
      }
    }
    default:
      break;
    }
    return QWidget::event(e);
  }
  void mousePressEvent(QMouseEvent* e) override {
    if (m_touchActive)
      return;
    m_drawing = true;
    m_current = QPainterPath();
    m_current.moveTo(e->pos());
    m_lastPos = e->pos();
    updatePointRegion(e->pos());
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    if (!m_drawing || m_touchActive)
      return;
    m_current.lineTo(e->pos());
    updateSegmentRegion(m_lastPos, e->pos());
    m_lastPos = e->pos();
  }
  void mouseReleaseEvent(QMouseEvent*) override {
    if (!m_drawing || m_touchActive)
      return;
    m_drawing = false;
    if (!m_current.isEmpty()) m_strokes.push_back(m_current);
    m_current = QPainterPath();
    update();
    if (onInkChanged) onInkChanged();
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.fillRect(rect(), UIStyles::PageBackground);
    p.setPen(QPen(QColor(255, 255, 255, 35), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(230, 228, 255, 200), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const auto& s : m_strokes) p.drawPath(s);
    p.drawPath(m_current);
  }
public:
  std::function<void()> onInkChanged;
private:
  // v3.17.4: per-segment dirty-rect repaints. Stylus sampling on Android
  // hits 120-240 Hz; the previous full-widget update() per touch-move
  // forced a full repaint of the entire ink surface every sample. With
  // dirty-rect updates Qt only invalidates the small rectangle between
  // the previous and the new touch point (padded by stroke width).
  static constexpr int kPenPad = 4;
  void updatePointRegion(const QPoint &p) {
    update(QRect(p, p).adjusted(-kPenPad, -kPenPad, kPenPad, kPenPad));
  }
  void updateSegmentRegion(const QPoint &a, const QPoint &b) {
    QRect r(a, b);
    r = r.normalized().adjusted(-kPenPad, -kPenPad, kPenPad, kPenPad);
    update(r);
  }

  QVector<QPainterPath> m_strokes;
  QPainterPath m_current;
  QPoint m_lastPos;
  bool m_drawing{false};
  bool m_touchActive{false};
};

class GraphFormulaEntryBar : public QWidget {
  Q_OBJECT
public:
  explicit GraphFormulaEntryBar(QWidget *parent = nullptr) : QWidget(parent) {
    setObjectName(QStringLiteral("GraphFormulaEntryBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    const QString side = UIStyles::Sidebar.name();
    const QString page = UIStyles::PageBackground.name();
    const QString accent = UIStyles::Accent.name();
    const QString text = UIStyles::Text.name();
    const QString sub = UIStyles::TextSecondary.name();
    setStyleSheet(QStringLiteral(
        "QWidget#GraphFormulaEntryBar { background-color: %1; border-radius: 14px; "
        "border: 1px solid rgba(124,92,252,0.38); }"
        "QWidget#GraphFormulaEntryBar QLineEdit { background-color: %2; border: 1px solid rgba(255,255,255,0.12); "
        "border-radius: 10px; padding: 6px 10px; min-height: 30px; color: %3; }"
        "QWidget#GraphFormulaEntryBar QToolButton { background-color: rgba(255,255,255,0.15); "
        "border: 1px solid rgba(255,255,255,0.24); border-radius: 8px; min-width: 30px; min-height: 30px; color: %3; font-weight: 700; }"
        "QWidget#GraphFormulaEntryBar QToolButton:hover { background-color: rgba(124,92,252,0.32); border-color: rgba(124,92,252,0.55); }"
        "QWidget#GraphFormulaEntryBar QLabel { color: %4; }"
        "QWidget#GraphFormulaEntryBar QToolButton#GraphFormulaOk { background-color: %5; color: #f6f4ff; border: none; border-radius: 8px; }"
        "QWidget#GraphFormulaEntryBar QToolButton#GraphFormulaOk:hover { background-color: #8b6eff; }")
                      .arg(side, page, text, sub, accent));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(14, 14, 14, 14);
    v->setSpacing(10);
    m_ink = new GraphInkInputWidget(this);
    m_ink->setMinimumHeight(52);
    m_ink->setMaximumHeight(76);
    m_ink->setMinimumWidth(160);
    v->addWidget(m_ink);
    auto *row = new QHBoxLayout();
    row->setSpacing(6);
    m_expr = new QLineEdit(this);
    m_expr->setPlaceholderText(QStringLiteral("z.B. sin(x), x^2"));
    m_btnOk = new QToolButton(this);
    m_btnOk->setObjectName(QStringLiteral("GraphFormulaOk"));
    m_btnOk->setText(QStringLiteral("✓"));
    m_btnOk->setToolTip(QStringLiteral("Uebernehmen"));
    m_btnCancel = new QToolButton(this);
    m_btnCancel->setText(QStringLiteral("×"));
    m_btnCancel->setToolTip(QStringLiteral("Abbrechen"));
    m_btnInkClear = new QToolButton(this);
    m_btnInkClear->setText(QStringLiteral("⌧"));
    m_btnInkClear->setToolTip(QStringLiteral("Zeichenflaeche leeren"));
    row->addWidget(m_expr, 0);
    row->addWidget(m_btnInkClear);
    row->addWidget(m_btnOk);
    row->addWidget(m_btnCancel);
    v->addLayout(row);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setMaximumWidth(400);
    m_status->setStyleSheet(
        QStringLiteral("QLabel { font-size: 11px; color: %1; }").arg(sub));
    v->addWidget(m_status);

    m_autoTimer = new QTimer(this);
    m_autoTimer->setInterval(400);
    m_autoTimer->setSingleShot(true);
    m_nam = new QNetworkAccessManager(this);
    m_ink->onInkChanged = [this]() {
      const bool plusFirstStroke = m_afterPlusOpen && !m_ink->strokes().isEmpty();
      if (plusFirstStroke)
        m_afterPlusOpen = false;
      else
        m_autoTimer->start();
      if (plusFirstStroke) {
        QTimer::singleShot(400, this, [this]() {
          if (!m_ink->strokes().isEmpty())
            startHandwritingRecognition(false);
        });
      }
    };
    connect(m_autoTimer, &QTimer::timeout, this, [this]() {
      if (!m_ink->strokes().isEmpty())
        startHandwritingRecognition(false);
    });
    connect(m_btnInkClear, &QToolButton::clicked, this, [this]() {
      m_autoTimer->stop();
      abortPendingRecognize();
      m_ink->clearInk();
      setStatus(QString());
    });
    connect(m_btnOk, &QToolButton::clicked, this, [this]() { emit commitRequested(); });
    connect(m_btnCancel, &QToolButton::clicked, this, [this]() { emit cancelRequested(); });
    connect(m_expr, &QLineEdit::textChanged, this, [this](const QString &t) {
      updateExprWidth();
      emit liveTextChanged(t.trimmed());
    });
    hide();
  }

  QString expressionText() const { return m_expr->text(); }

  /// Call after opening from "+" so the first ink stroke schedules recognition soon.
  void notifyPlusOpened() { m_afterPlusOpen = true; }

  void prepareOpen() {
    abortPendingRecognize();
    m_autoTimer->stop();
    m_afterPlusOpen = false;
    m_expr->clear();
    m_ink->clearInk();
    setStatus(QString());
    updateExprWidth();
    m_expr->setFocus();
  }

  void setStatus(const QString &text, bool isError = false) {
    if (!m_status)
      return;
    m_status->setStyleSheet(isError
      ? QStringLiteral("QLabel { font-size: 11px; color: #ff8a8a; }")
      : QStringLiteral("QLabel { font-size: 11px; color: %1; }")
            .arg(UIStyles::TextSecondary.name()));
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
  }

signals:
  void liveTextChanged(const QString &expr);
  void commitRequested();
  void cancelRequested();

private:
  void startHandwritingRecognition(bool offerCandidateMenuOnEmpty);
  void finishRecognizeWithFallback(const QString &backendExpr,
                                   bool offerCandidateMenuOnEmpty);
  void abortPendingRecognize();
  QJsonObject buildRecognizePayload() const;

  void updateExprWidth() {
    const QFontMetrics fm(m_expr->font());
    const QString sample = m_expr->text().isEmpty() ? m_expr->placeholderText() : m_expr->text();
    const int textW = fm.horizontalAdvance(sample);
    const int inner = qBound(72, textW + 32, 320);
    m_expr->setFixedWidth(inner);
    adjustSize();
  }

  // Verbesserte lokale Heuristik – nutzt Strichanzahl, Seitenverhältnis,
  // Streckenlänge und Kurvigkeit um häufige Funktionen zu erkennen.
  QStringList recognizeLocalCandidates() const {
    const auto st = m_ink->strokes();
    if (st.isEmpty())
      return {};

    // --- Geometrie berechnen ---
    QRectF bb;
    qreal totalLen = 0.0;
    qreal totalCurv = 0.0;  // Summe der Winkeländerungen (Kurvigkeit)
    int totalPts = 0;
    for (const auto &s : st) {
      bb = bb.united(s.boundingRect());
      const auto polys = s.toSubpathPolygons();
      for (const auto &poly : polys) {
        for (int i = 1; i < poly.size(); ++i)
          totalLen += QLineF(poly[i-1], poly[i]).length();
        for (int i = 1; i + 1 < poly.size(); ++i) {
          QLineF l1(poly[i-1], poly[i]);
          QLineF l2(poly[i], poly[i+1]);
          totalCurv += qAbs(l1.angleTo(l2));
        }
        totalPts += poly.size();
      }
    }
    if (bb.isEmpty()) return {};

    const int    nStrokes = st.size();
    const qreal  w        = bb.width();
    const qreal  h        = bb.height();
    const qreal  ratio    = h > 1.0 ? (w / h) : w;  // Breite/Höhe
    const qreal  sqratio  = h > 1.0 ? (bb.height() / bb.width()) : 1.0; // Höhe/Breite
    const qreal  avgCurv  = totalPts > 2 ? (totalCurv / totalPts) : 0.0;
    const bool   curved   = avgCurv > 15.0;  // Striche sind eher kurvig
    const bool   hasLoop  = avgCurv > 40.0;  // sehr kurvig = Kreis/Oval

    // --- Heuristik ---
    QStringList candidates;

    // Einzelner sehr kurzer Strich → x oder c
    if (nStrokes == 1 && totalLen < 80) {
      candidates << QStringLiteral("x") << QStringLiteral("x^2") << QStringLiteral("2");
      return candidates;
    }

    // Sehr hoch (sqratio > 2) → oft Bruch, Integral, Summe
    if (sqratio > 2.0 && nStrokes >= 2) {
      candidates << QStringLiteral("1/x")
                 << QStringLiteral("(x+1)/(x-1)")
                 << QStringLiteral("x^2/2");
    }

    // Breite und kurvig → trig. Funktionen
    if (ratio > 2.5 && curved) {
      candidates << QStringLiteral("sin(x)")
                 << QStringLiteral("cos(x)")
                 << QStringLiteral("sin(2*x)")
                 << QStringLiteral("cos(2*x)")
                 << QStringLiteral("tan(x)");
    }

    // Runde/kreisartige Form → Parabel / Kreis
    if (hasLoop || (ratio > 0.6 && ratio < 1.8 && curved)) {
      candidates << QStringLiteral("x^2")
                 << QStringLiteral("-x^2")
                 << QStringLiteral("x^2-1")
                 << QStringLiteral("x^2+1");
    }

    // Lange gerade Striche → Geraden, Polynome
    if (ratio > 1.5 && !curved && nStrokes <= 3) {
      candidates << QStringLiteral("2*x")
                 << QStringLiteral("x")
                 << QStringLiteral("-x")
                 << QStringLiteral("3*x-2");
    }

    // Viele Striche → komplexere Ausdrücke
    if (nStrokes >= 4) {
      candidates << QStringLiteral("x^3-x")
                 << QStringLiteral("sin(x)+x")
                 << QStringLiteral("x^2-2*x+1")
                 << QStringLiteral("sqrt(x)")
                 << QStringLiteral("log(x)");
    }

    // Allgemeine Fallbacks
    if (candidates.isEmpty() || candidates.size() < 3) {
      candidates << QStringLiteral("x^2")
                 << QStringLiteral("sin(x)")
                 << QStringLiteral("x")
                 << QStringLiteral("2*x")
                 << QStringLiteral("sqrt(x)");
    }

    // Deduplizieren, max 6 Vorschläge
    QStringList result;
    for (const QString &c : candidates) {
      if (!result.contains(c)) result.append(c);
      if (result.size() >= 6) break;
    }
    return result;
  }

  GraphInkInputWidget *m_ink{nullptr};
  QLineEdit *m_expr{nullptr};
  QToolButton *m_btnInkClear{nullptr};

  QToolButton *m_btnOk{nullptr};
  QToolButton *m_btnCancel{nullptr};
  QLabel *m_status{nullptr};
  QTimer *m_autoTimer{nullptr};
  QNetworkAccessManager *m_nam{nullptr};
  QNetworkReply *m_pendingReply{nullptr};
  bool m_afterPlusOpen{false};
};

void GraphFormulaEntryBar::abortPendingRecognize() {
  if (m_pendingReply) {
    disconnect(m_pendingReply, nullptr, this, nullptr);
    m_pendingReply->abort();
    m_pendingReply->deleteLater();
    m_pendingReply = nullptr;
  }

}

QJsonObject GraphFormulaEntryBar::buildRecognizePayload() const {
  QJsonArray strokesJson;
  QJsonArray strokesNormJson;
  constexpr int kMaxPerStroke = 128;
  QRectF inkBb;
  for (const auto &s : m_ink->strokes()) {
    for (const QPolygonF &poly : s.toSubpathPolygons()) {
      if (!poly.isEmpty())
        inkBb |= poly.boundingRect();
    }
  }
  const double side = qMax(inkBb.width(), inkBb.height());
  const bool haveNorm = side > 1e-5;
  for (const auto &s : m_ink->strokes()) {
    QJsonArray pts;
    QJsonArray ptsNorm;
    const auto polys = s.toSubpathPolygons();
    for (const auto &poly : polys) {
      const int n = poly.size();
      if (n <= 0)
        continue;
      const int take = qMin(kMaxPerStroke, qMax(1, n));
      for (int i = 0; i < take; ++i) {
        const int idx = (take <= 1) ? 0 : (i * (n - 1)) / (take - 1);
        const QPointF pt = poly.at(idx);
        QJsonArray pair;
        pair.append(pt.x());
        pair.append(pt.y());
        pts.append(pair);
        if (haveNorm) {
          QJsonArray np;
          np.append(int(qBound(0.0, (pt.x() - inkBb.left()) / side * 1000.0, 1000.0)));
          np.append(int(qBound(0.0, (pt.y() - inkBb.top()) / side * 1000.0, 1000.0)));
          ptsNorm.append(np);
        }
      }
    }
    strokesJson.append(pts);
    if (haveNorm)
      strokesNormJson.append(ptsNorm);
  }
  QJsonObject payload;
  payload.insert(QStringLiteral("strokes"), strokesJson);
  if (haveNorm)
    payload.insert(QStringLiteral("strokes_normalized"), strokesNormJson);
  payload.insert(QStringLiteral("hint"),
                 QStringLiteral("Einzelner mathematischer Ausdruck fuer y=f(x): Operatoren + - * / ^ oder **, "
                                "Klammern, sin(x) cos(x) tan(x) sqrt(x) log(x) exp(x) abs(x), Konstante pi."));
  payload.insert(QStringLiteral("current_expression"), m_expr->text().trimmed());
  const QImage snap = m_ink->inkImageForRecognition();
  if (!snap.isNull()) {
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    if (snap.save(&buf, "PNG"))
      payload.insert(QStringLiteral("ink_png_base64"), QString::fromLatin1(png.toBase64()));
  }
  return payload;
}

void GraphFormulaEntryBar::finishRecognizeWithFallback(const QString &backendExpr,
                                                       bool offerCandidateMenuOnEmpty) {
  setStatus(QString());
  if (!backendExpr.isEmpty()) {
    m_expr->setText(backendExpr);
    setStatus(QStringLiteral("Erkannt — mit Haken uebernehmen"), false);
    return;
  }
  const QStringList locals = recognizeLocalCandidates();
  if (locals.isEmpty()) {
    setStatus(QStringLiteral("Nichts erkannt"), true);
    return;
  }
  if (offerCandidateMenuOnEmpty) {
    QMenu menu(this);
    for (const QString &c : locals)
      menu.addAction(c);
    QAction *chosen = menu.exec(m_btnInkClear->mapToGlobal(QPoint(0, m_btnInkClear->height())));
    if (chosen)
      m_expr->setText(chosen->text());
    if (!m_expr->text().trimmed().isEmpty())
      setStatus(QStringLiteral("Gewaehlt — mit Haken uebernehmen"), false);
  } else {
    // Auto-Erkennung hat nichts gefunden — ersten lokalen Vorschlag anbieten
    if (!locals.isEmpty()) {
      m_expr->setText(locals.first());
      setStatus(QStringLiteral("Vorschlag — mit Haken uebernehmen oder manuell aendern"), false);
    } else {
      setStatus(QStringLiteral("Nicht erkannt — bitte Ausdruck manuell eingeben."), true);
    }
  }
}

void GraphFormulaEntryBar::startHandwritingRecognition(bool offerCandidateMenuOnEmpty) {
  if (!m_ink || m_ink->strokes().isEmpty()) {
    setStatus(QStringLiteral("Nichts gezeichnet"), true);
    return;
  }

  // ── Phase 1: Offline-Erkennung (ONNX, sofort, nur Desktop) ────────
#ifdef BLOP_HAS_ONNX_OCR
  if (MathInkRecognizer::instance().isAvailable()) {
    setStatus(QStringLiteral("Offline-Erkennung..."), false);
    const QImage snap = m_ink->inkImageForRecognition();
    if (!snap.isNull()) {
      const QString localExpr = MathInkRecognizer::instance().recognize(snap);
      if (!localExpr.isEmpty()) {
        finishRecognizeWithFallback(localExpr, offerCandidateMenuOnEmpty);
        return;
      }
    }
    // Offline lieferte nichts — weiter zum Backend
  }
#endif

  // ── Phase 2: Backend-Erkennung ──────────────────────────────────────
  // Nur anfragen wenn die URL nicht localhost ist (auf Android nicht
  // erreichbar) oder explizit per Umgebungsvariable gesetzt wurde.
  const QString defaultEndpoint =
#ifdef Q_OS_ANDROID
      QString();  // kein localhost-Request auf Android
#else
      QStringLiteral("http://127.0.0.1:8000/api/ai/math-ink/recognize");
#endif
  const QString endpoint = qEnvironmentVariable("BLOP_MATH_OCR_URL", defaultEndpoint);

  if (endpoint.isEmpty()) {
    // Kein Server konfiguriert → direkt lokale Vorschläge
    finishRecognizeWithFallback(QString(), offerCandidateMenuOnEmpty);
    return;
  }

  abortPendingRecognize();
  setStatus(QStringLiteral("Erkennung laeuft..."), false);
  QNetworkRequest req{QUrl(endpoint)};
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  req.setTransferTimeout(8000);  // 8s statt 25s — schnellerer Fallback
  m_pendingReply =
      m_nam->post(req, QJsonDocument(buildRecognizePayload()).toJson(QJsonDocument::Compact));
  connect(m_pendingReply, &QNetworkReply::finished, this, [this, offerCandidateMenuOnEmpty]() {
    QNetworkReply *rep = m_pendingReply;
    m_pendingReply = nullptr;
    if (!rep) {
      finishRecognizeWithFallback(QString(), offerCandidateMenuOnEmpty);
      return;
    }
    const QByteArray body = rep->readAll();
    const QNetworkReply::NetworkError err = rep->error();
    rep->deleteLater();
    if (err != QNetworkReply::NoError) {
      // Server nicht erreichbar → sofort lokale Vorschläge
      finishRecognizeWithFallback(QString(), offerCandidateMenuOnEmpty);
      return;
    }
    const QJsonObject obj = QJsonDocument::fromJson(body).object();
    finishRecognizeWithFallback(obj.value(QStringLiteral("expression")).toString().trimmed(),
                                offerCandidateMenuOnEmpty);
  });
}

class GraphTangentXPopup : public QWidget {
public:
  GraphTangentXPopup(MultiPageNoteView *view,
                     std::function<void(GraphCanvasItem *, double, bool)> onCommit)
      : QWidget(view), m_view(view), m_onCommit(std::move(onCommit)) {
    setObjectName(QStringLiteral("GraphTangentXPopup"));
    setAttribute(Qt::WA_StyledBackground, true);
#ifndef Q_OS_ANDROID
    // v3.17.4: drop the shadow on Android. The popup hovers above the canvas
    // and would otherwise force a per-frame offscreen rasterisation while the
    // user drags the tangent. The border alone is enough visual separation.
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(22);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 130));
    setGraphicsEffect(shadow);
#endif
    const QString bg = UIStyles::Sidebar.name();
    const QString accent = UIStyles::Accent.name();
    const QString text = UIStyles::Text.name();
    const QString sub = UIStyles::TextSecondary.name();
    setStyleSheet(QStringLiteral(
        "QWidget#GraphTangentXPopup { background-color: %1; border-radius: 12px; "
        "border: 1px solid rgba(124,92,252,0.38); }"
        "QWidget#GraphTangentXPopup QLabel { color: %3; }"
        "QWidget#GraphTangentXPopup QDoubleSpinBox { background-color: %1; color: %3; "
        "border: 1px solid rgba(255,255,255,0.14); border-radius: 8px; padding: 4px 8px; min-height: 28px; }"
        "QWidget#GraphTangentXPopup QPushButton { background-color: %2; color: #f0eefc; border: none; border-radius: 8px; "
        "padding: 7px 16px; font-weight: 600; }"
        "QWidget#GraphTangentXPopup QPushButton#PopupCancel { background-color: rgba(255,255,255,0.1); color: %3; }"
        "QWidget#GraphTangentXPopup QCheckBox { color: %4; spacing: 6px; }")
                      .arg(bg, accent, text, sub));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("Stelle auf der x-Achse"), this);
    QFont tf = title->font();
    tf.setBold(true);
    if (tf.pointSizeF() > 0) {
        tf.setPointSizeF(tf.pointSizeF() + 1.0);
    } else if (tf.pixelSize() > 0) {
        tf.setPixelSize(tf.pixelSize() + 2);
    } else {
        tf.setPointSizeF(10.0);
    }
    title->setFont(tf);
    lay->addWidget(title);
    m_x = new QDoubleSpinBox(this);
    m_x->setDecimals(4);
    m_x->setRange(-1e6, 1e6);
    connect(m_x, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this]() { refreshExtra(); });
    lay->addWidget(m_x);
    m_showTangent = new QCheckBox(QStringLiteral("Tangente anzeigen"), this);
    m_showTangent->setChecked(true);
    lay->addWidget(m_showTangent);
    m_extra = new QLabel(this);
    m_extra->setWordWrap(true);
    m_extra->setStyleSheet(QStringLiteral("QLabel { font-size: 11px; color: %1; }").arg(sub));
    m_extra->hide();
    lay->addWidget(m_extra);
    auto *row = new QHBoxLayout();
    row->addStretch(1);
    auto *btnCancel = new QPushButton(QStringLiteral("Abbrechen"), this);
    btnCancel->setObjectName(QStringLiteral("PopupCancel"));
    auto *btnOk = new QPushButton(QStringLiteral("Uebernehmen"), this);
    row->addWidget(btnCancel);
    row->addWidget(btnOk);
    lay->addLayout(row);
    connect(btnCancel, &QPushButton::clicked, this, &GraphTangentXPopup::hide);
    connect(btnOk, &QPushButton::clicked, this, [this]() {
      if (m_graph && m_onCommit)
        m_onCommit(m_graph, m_x->value(), m_showTangent->isChecked());
      hide();
    });
    setFixedWidth(280);
    hide();
  }

  void present(GraphCanvasItem *gi, const QPointF &scenePos, double dataX) {
    m_graph = gi;
    if (!m_view || !gi)
      return;
    const GraphObject d = gi->data();
    m_x->blockSignals(true);
    m_x->setRange(d.xMin, d.xMax);
    m_x->setValue(qBound(d.xMin, dataX, d.xMax));
    m_x->blockSignals(false);
    refreshExtra();
    adjustSize();
    auto *gv = static_cast<QGraphicsView *>(m_view);
    QPoint vp = gv->mapFromScene(scenePos);
    QPoint p = vp + QPoint(16, 16);
    const int w = qMax(width(), 260);
    const int h = height();
    const QRect vr = gv->rect();
    if (p.x() + w > vr.right())
      p.setX(vr.right() - w - 10);
    if (p.y() + h > vr.bottom())
      p.setY(qMax(8, vp.y() - h - 10));
    if (p.x() < 8)
      p.setX(8);
    if (p.y() < 8)
      p.setY(8);
    move(p);
    show();
    raise();
  }

private:
  void refreshExtra() {
    if (!m_extra || !m_graph)
      return;
    const GraphObject d = m_graph->data();
    if (d.selectedFunction < 0 || d.selectedFunction >= d.functions.size()) {
      m_extra->hide();
      return;
    }
    const GraphFunction &f = d.functions[d.selectedFunction];
    if (!f.isDerivativeCurve) {
      m_extra->hide();
      return;
    }
    const ParsedExpression p = MathExpressionParser::parseFunctionExpression(f.sourceExpression);
    if (!p.ok) {
      m_extra->hide();
      return;
    }
    const double xv = m_x->value();
    const double slope = NumericAnalysis::derivativeCentral(p, xv);
    if (!qIsFinite(slope)) {
      m_extra->hide();
      return;
    }
    m_extra->setText(
        QStringLiteral("Ableitung f'(x) an dieser Stelle: %1").arg(slope, 0, 'g', 5));
    m_extra->show();
  }

  MultiPageNoteView *m_view;
  std::function<void(GraphCanvasItem *, double, bool)> m_onCommit;
  GraphCanvasItem *m_graph{nullptr};
  QDoubleSpinBox *m_x{nullptr};
  QCheckBox *m_showTangent{nullptr};
  QLabel *m_extra{nullptr};
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
  // No cache: with the OpenGL viewport in place the GPU clears each frame
  // anyway, and CacheBackground would just waste memory on a pixmap that
  // is never reused.
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

  // Gesten auf dem Viewport registrieren. NOTE: a QOpenGLWidget viewport was
  // tried in v3.15.4.112 but broke pinch-zoom on Android because touch
  // events get re-routed through the SurfaceView and the gesture recogniser
  // never sees them. Reverted to the default raster viewport; canvas
  // performance comes from the per-PageItem ItemCoordinateCache instead.
  viewport()->grabGesture(Qt::PinchGesture);

  setOptimizationFlags(QGraphicsView::DontSavePainterState |
                       QGraphicsView::DontAdjustForAntialiasing);
#ifdef Q_OS_ANDROID
  setRenderHints(QPainter::Antialiasing);
#else
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
#endif
#ifdef Q_OS_ANDROID
  setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
#else
  setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
#endif

  setDragMode(QGraphicsView::NoDrag);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setAcceptDrops(false);

  viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

  // v3.16.0: keine nativen Scrollbars, dafuer ein duenner Auto-Hide-Indikator.
  OverlayScrollIndicator::install(this);

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
  connect(m_selectionMenu, &NoteSelectionMenu::duplicateRequested, this, &MultiPageNoteView::duplicateSelection);
  // v3.18.0: Crop war als Button + Signal vorhanden, aber nie verbunden.
  connect(m_selectionMenu, &NoteSelectionMenu::cropRequested, this, &MultiPageNoteView::startCropSession);

  // Rechteck-Crop-Session (Freihand-Modus gibt es hier nicht, daher nur ✔/✕).
  m_cropMenu = new CropMenu(this, /*showModeButtons=*/false);
  connect(m_cropMenu, &CropMenu::applyRequested, this, &MultiPageNoteView::applyCrop);
  connect(m_cropMenu, &CropMenu::cancelRequested, this, &MultiPageNoteView::cancelCrop);
  m_graphLegendDock = new GraphLegendDock(this);
  m_graphLegendDock->hide();
  m_graphQuickPopup = new GraphQuickActionPopup(this);
  m_graphQuickPopup->hide();

  connect(m_graphLegendDock, &GraphLegendDock::selectionRequested, this, [this](int idx) {
    if (!m_selectedGraphItem)
      return;
    m_graphQuickPopupWanted = false;
    hideGraphQuickPopup();
    m_selectedGraphItem->setSelectedFunction(idx);
    syncGraphPlusLayout(m_selectedGraphItem);
    if (m_graphPanelExplicitOpen && m_graphLegendDock) {
      bindGraphChrome(m_selectedGraphItem);
      syncGraphLegendLayout();
    }
  });
  m_graphEntryBar = new GraphFormulaEntryBar(this);
  m_graphEntryBar->hide();
  // Note: GraphFormulaEntryBar is kept as a fallback but zones are primary input
  m_tangentXPopup = new GraphTangentXPopup(this, [this](GraphCanvasItem *gi, double x, bool st) {
    if (!gi)
      return;
    auto d = gi->data();
    if (d.functions.isEmpty())
      return;
    const int idx = qBound(0, d.selectedFunction, d.functions.size() - 1);
    d.functions[idx].tangentX = qBound(d.xMin, x, d.xMax);
    d.functions[idx].showTangent = st;
    gi->fromData(d);
    if (m_selectedGraphItem == gi)
      bindGraphChrome(gi);
    syncGraphPlusLayout(gi);
    syncGraphItemsToNote();
  });
  connect(m_graphLegendDock, &GraphLegendDock::entryBarRequested, this, [this]() {
    if (!m_selectedGraphItem)
      return;
    openGraphEntryBarForGraph(m_selectedGraphItem);
  });
  connect(m_graphEntryBar, &GraphFormulaEntryBar::liveTextChanged, this, [this](const QString& expr) {
    if (!m_selectedGraphItem)
      return;
    auto d = m_selectedGraphItem->data();
    if (expr.isEmpty()) {
      if (m_livePreviewIndex >= 0 && m_livePreviewIndex < d.functions.size()) {
        d.functions.removeAt(m_livePreviewIndex);
        m_livePreviewIndex = -1;
        if (d.functions.isEmpty())
          d.selectedFunction = -1;
        else
          d.selectedFunction = qBound(0, d.selectedFunction, d.functions.size() - 1);
        m_selectedGraphItem->fromData(d);
        m_graphEntryBar->setStatus(QString());
        syncGraphPlusLayout(m_selectedGraphItem);
        if (m_graphPanelExplicitOpen)
          bindGraphChrome(m_selectedGraphItem);
        repositionGraphEntryBar();
      }
      return;
    }
    const ParsedExpression parsed = MathExpressionParser::parseFunctionExpression(expr);
    if (!parsed.ok) {
      m_graphEntryBar->setStatus(QStringLiteral("Eingabe ungueltig"), true);
      return;
    }
    m_graphEntryBar->setStatus(QStringLiteral("Live: %1").arg(parsed.normalizedInput), false);
    if (m_livePreviewIndex >= 0 && m_livePreviewIndex < d.functions.size()) {
      d.functions[m_livePreviewIndex].expression = parsed.normalizedInput;
    } else {
      GraphFunction previewFn;
      previewFn.expression = parsed.normalizedInput;
      previewFn.color = QColor(227, 132, 46);
      previewFn.visible = true;
      d.functions.push_back(previewFn);
      m_livePreviewIndex = d.functions.size() - 1;
    }
    d.selectedFunction = m_livePreviewIndex;
    m_selectedGraphItem->fromData(d);
    syncGraphPlusLayout(m_selectedGraphItem);
    if (m_graphPanelExplicitOpen)
      bindGraphChrome(m_selectedGraphItem);
    repositionGraphEntryBar();
  });
  connect(m_graphEntryBar, &GraphFormulaEntryBar::commitRequested, this, [this]() {
    if (!m_selectedGraphItem || !m_graphEntryBar)
      return;
    const QString expr = m_graphEntryBar->expressionText().trimmed();
    if (expr.isEmpty()) {
      m_graphEntryBar->setStatus(QStringLiteral("Bitte einen Ausdruck eingeben"), true);
      return;
    }
    const ParsedExpression parsed = MathExpressionParser::parseFunctionExpression(expr);
    if (!parsed.ok) {
      m_graphEntryBar->setStatus(QStringLiteral("Ungueltig: %1").arg(parsed.error), true);
      return;
    }
    auto d = m_selectedGraphItem->data();
    const QColor fixedColor = QColor(94, 92, 230);
    if (m_livePreviewIndex >= 0 && m_livePreviewIndex < d.functions.size()) {
      d.functions[m_livePreviewIndex].expression = parsed.normalizedInput;
      d.functions[m_livePreviewIndex].color = fixedColor;
      d.selectedFunction = m_livePreviewIndex;
      m_livePreviewIndex = -1;
    } else {
      GraphFunction fn;
      fn.expression = parsed.normalizedInput;
      fn.visible = true;
      d.functions.push_back(fn);
      d.selectedFunction = d.functions.size() - 1;
    }
    m_selectedGraphItem->fromData(d);
    closeGraphEntryBar();
    syncGraphPlusLayout(m_selectedGraphItem);
    if (m_graphPanelExplicitOpen) {
      bindGraphChrome(m_selectedGraphItem);
      syncGraphLegendLayout();
    }
    syncGraphItemsToNote();
  });
  connect(m_graphEntryBar, &GraphFormulaEntryBar::cancelRequested, this, [this]() {
    abandonGraphEntrySession();
    if (m_graphPanelExplicitOpen && m_selectedGraphItem) {
      bindGraphChrome(m_selectedGraphItem);
      syncGraphLegendLayout();
    }
    syncGraphItemsToNote();
  });
  connect(m_graphLegendDock, &GraphLegendDock::removeRequested, this, [this](int requestedIdx) {
    if (!m_selectedGraphItem) return;
    auto d = m_selectedGraphItem->data();
    if (d.functions.isEmpty()) return;
    const int idx = qBound(0, requestedIdx >= 0 ? requestedIdx : d.selectedFunction, d.functions.size() - 1);
    d.functions.removeAt(idx);
    if (m_livePreviewIndex == idx)
      m_livePreviewIndex = -1;
    else if (m_livePreviewIndex > idx)
      --m_livePreviewIndex;
    d.selectedFunction = d.functions.isEmpty() ? -1 : qBound(0, idx - 1, d.functions.size() - 1);
    m_selectedGraphItem->fromData(d);
    bindGraphChrome(m_selectedGraphItem);
    syncGraphPlusLayout(m_selectedGraphItem);
    syncGraphLegendLayout();
    syncGraphItemsToNote();
  });
  connect(m_graphQuickPopup, &GraphQuickActionPopup::toggleRequested, this, [this](const QString& what) {
    if (!m_selectedGraphItem) return;
    auto d = m_selectedGraphItem->data();
    if (d.functions.isEmpty()) return;
    const int idx = qBound(0, d.selectedFunction, d.functions.size() - 1);
    if (idx < 0 || idx >= d.functions.size()) return;
    if (what == "derivative_create") {
      const GraphFunction &base = d.functions[idx];
      const QString src = (base.isDerivativeCurve && !base.sourceExpression.isEmpty())
          ? base.sourceExpression
          : base.expression;
      const QString innerLabel = base.isDerivativeCurve ? base.expression : src;
      const QString firstSym = MathExpressionParser::symbolicDerivativeString(src);
      QString plotSource = src;
      QString displayExpr = firstSym;
      if (base.isDerivativeCurve) {
        plotSource = firstSym.isEmpty() ? src : firstSym;
        displayExpr =
            firstSym.isEmpty() ? QString() : MathExpressionParser::symbolicDerivativeString(firstSym);
      }
      GraphFunction derFn;
      derFn.sourceExpression = plotSource;
      derFn.expression =
          displayExpr.isEmpty() ? QStringLiteral("d/dx(%1)").arg(innerLabel) : displayExpr;
      derFn.isDerivativeCurve = true;
      derFn.color = base.color.darker(120);
      d.functions[idx].showTangent = true;
      d.functions[idx].tangentX = qBound(d.xMin, d.functions[idx].tangentX, d.xMax);
      d.functions.push_back(derFn);
      d.selectedFunction = d.functions.size() - 1;
    }
    else if (what == "roots") d.functions[idx].showRoots = !d.functions[idx].showRoots;
    else if (what == "extrema") d.functions[idx].showExtrema = !d.functions[idx].showExtrema;
    m_selectedGraphItem->fromData(d);
    bindGraphChrome(m_selectedGraphItem);
    syncGraphPlusLayout(m_selectedGraphItem);
    syncGraphLegendLayout();
    syncGraphItemsToNote();
  });
  connect(m_graphQuickPopup, &GraphQuickActionPopup::tangentXRequested, this, [this](double x0) {
    if (!m_selectedGraphItem) return;
    auto d = m_selectedGraphItem->data();
    if (d.functions.isEmpty()) return;
    const int idx = qBound(0, d.selectedFunction, d.functions.size() - 1);
    if (idx < 0 || idx >= d.functions.size()) return;
    d.functions[idx].tangentX = qBound(d.xMin, x0, d.xMax);
    d.functions[idx].showTangent = true;
    m_selectedGraphItem->fromData(d);
    syncGraphPlusLayout(m_selectedGraphItem);
    bindGraphChrome(m_selectedGraphItem);
    syncGraphItemsToNote();
  });
  connect(m_graphQuickPopup, &GraphQuickActionPopup::tangentAtFirstRootRequested, this, [this]() {
    if (!m_selectedGraphItem)
      return;
    auto d = m_selectedGraphItem->data();
    if (d.functions.isEmpty())
      return;
    const int idx = qBound(0, d.selectedFunction, d.functions.size() - 1);
    if (idx < 0 || idx >= d.functions.size())
      return;
    const auto &f = d.functions[idx];
    const QString expr = f.isDerivativeCurve ? f.sourceExpression : f.expression;
    const ParsedExpression parsed = MathExpressionParser::parseFunctionExpression(expr);
    double x0 = 0.0;
    if (parsed.ok) {
      const QVector<double> roots =
          NumericAnalysis::findRootsBisection(parsed, d.xMin, d.xMax, 300);
      if (!roots.isEmpty())
        x0 = roots.first();
    }
    d.functions[idx].tangentX = qBound(d.xMin, x0, d.xMax);
    d.functions[idx].showTangent = true;
    m_selectedGraphItem->fromData(d);
    bindGraphChrome(m_selectedGraphItem);
    syncGraphPlusLayout(m_selectedGraphItem);
    syncGraphItemsToNote();
  });
  connect(m_graphQuickPopup, &GraphQuickActionPopup::tangentManualRequested, this, [this]() {
    if (!m_selectedGraphItem || !m_tangentXPopup)
      return;
    const QRectF sr = m_selectedGraphItem->sceneBoundingRect();
    const QPointF center = sr.center();
    auto d = m_selectedGraphItem->data();
    double x0 = 0.0;
    if (d.selectedFunction >= 0 && d.selectedFunction < d.functions.size())
      x0 = d.functions[d.selectedFunction].tangentX;
    m_tangentXPopup->present(m_selectedGraphItem, center, x0);
  });
  const auto removeWholeGraph = [this]() {
    GraphCanvasItem *gi = m_selectedGraphItem;
    if (!gi)
      return;
    if (m_graphEntryBarOpen && m_graphEntryTargetGraph == gi)
      abandonGraphEntrySession();
    else
      closeGraphEntryBar();
    if (m_graphPlusBypassItem == gi)
      m_graphPlusBypassItem = nullptr;
    if (m_graphPlotBypassItem == gi)
      m_graphPlotBypassItem = nullptr;
    m_graphPanelExplicitOpen = false;
    m_graphPanelTargetGraph = nullptr;
    m_selectedGraphItem = nullptr;
    m_livePreviewIndex = -1;
    hideGraphLegendQuick();
    bindGraphChrome(nullptr);
    {
      QSignalBlocker blocker(&scene_);
      scene_.clearSelection();
      delete gi;
    }
    syncGraphItemsToNote();
  };
  connect(m_graphLegendDock, &GraphLegendDock::removeGraphWidgetRequested, this, removeWholeGraph);
  connect(m_graphQuickPopup, &GraphQuickActionPopup::removeGraphRequested, this, removeWholeGraph);
  connect(m_graphQuickPopup, &GraphQuickActionPopup::removeSelectedFunctionRequested, this, [this]() {
    if (!m_selectedGraphItem)
      return;
    auto d = m_selectedGraphItem->data();
    if (d.functions.isEmpty())
      return;
    const int idx = qBound(0, d.selectedFunction, d.functions.size() - 1);
    d.functions.removeAt(idx);
    if (m_livePreviewIndex == idx)
      m_livePreviewIndex = -1;
    else if (m_livePreviewIndex > idx)
      --m_livePreviewIndex;
    d.selectedFunction = d.functions.isEmpty() ? -1 : qBound(0, idx - 1, d.functions.size() - 1);
    m_selectedGraphItem->fromData(d);
    bindGraphChrome(m_selectedGraphItem);
    syncGraphPlusLayout(m_selectedGraphItem);
    syncGraphLegendLayout();
    syncGraphItemsToNote();
  });
  connect(m_graphQuickPopup, &GraphQuickActionPopup::axisSettingsRequested, this, [this]() {
    if (!m_selectedGraphItem)
      return;
    // v3.17.2: on Android, wrap the dialog in a BlopModal so it never
    // becomes a top-level QWindow (the v3.16.x EGL deadlock path). On
    // Desktop we keep the historic QDialog::exec() since the platform
    // has no such issue and exec() is the simpler control-flow.
#ifdef Q_OS_ANDROID
    auto *dlg = new GraphAxisSettingsDialog(m_selectedGraphItem, window());
    BlopModal::execBlocking(window(), dlg);
    dlg->deleteLater();
#else
    GraphAxisSettingsDialog dlg(m_selectedGraphItem, window());
    dlg.exec();
#endif
    bindGraphChrome(m_selectedGraphItem);
    syncGraphLegendLayout();
    syncGraphItemsToNote();
  });

  // v3.17.5: coalesce scroll-driven layout work into one frame-aligned
  // dispatch instead of running 3 layout functions per pixel of scroll.
  // On Android the per-pixel storm was the single largest source of jank
  // during stylus scrolling -- valueChanged fires for every pixel and
  // each handler triggered geometry changes on chrome widgets.
  m_scrollLayoutCoalescer = new QTimer(this);
  m_scrollLayoutCoalescer->setSingleShot(true);
  m_scrollLayoutCoalescer->setInterval(16);
  connect(m_scrollLayoutCoalescer, &QTimer::timeout, this, [this]() {
    syncPagesBarVisibility();
    syncGraphLegendLayout();
    repositionGraphEntryBar();
#ifdef Q_OS_ANDROID
    // v3.17.6: piggy-back lazy page hydration on the existing scroll
    // coalescer so we don't fire a second timer cascade per scroll burst.
    hydrateVisibleRange();
#endif
  });
  auto kickCoalescer = [this]() {
    if (m_scrollLayoutCoalescer && !m_scrollLayoutCoalescer->isActive())
      m_scrollLayoutCoalescer->start();
  };
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this, kickCoalescer);
  connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, kickCoalescer);
  // Overlay über der Skeleton-Fläche — muss Maus-Events fangen (kein
  // WA_TransparentForMouseEvents, sonst gehen Klicks zur QGraphicsView durch).
  m_bottomSheet = new QWidget(this);

  m_pagesBarCard = new QFrame(m_bottomSheet);
  m_pagesBarCard->setObjectName(QStringLiteral("PagesBarStrip"));
  m_pagesBarCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  // v3.16.1: unify the pages-bar surface with the rest of the overlays.
  m_pagesBarCard->setStyleSheet(
      BlopStyle::surfaceStyle(QStringLiteral("PagesBarStrip")) +
      QStringLiteral(
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

void MultiPageNoteView::hydratePageContent(int i) {
  if (!note_ || i < 0 || i >= note_->pages.size())
    return;
  if (m_hydratedPages.contains(i))
    return;
  if (i >= pageItems_.size())
    return;
  m_hydratedPages.insert(i);

  bool wasBlocked = scene_.blockSignals(true);
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
  for (const auto& g : note_->pages[i].graphs) {
    auto* gi = new GraphCanvasItem(g.rect);
    gi->fromData(g);
    gi->setParentItem(pageItems_[i]);
    gi->setFlag(QGraphicsItem::ItemIsSelectable, true);
    gi->setFlag(QGraphicsItem::ItemIsMovable, true);
    gi->setZValue(4.0);
    bindGraphItemSignals(gi);
    syncGraphPlusLayout(gi);
  }
  scene_.blockSignals(wasBlocked);
}

void MultiPageNoteView::hydrateVisibleRange() {
  if (!note_)
    return;
  const QRectF vp = mapToScene(viewport()->rect()).boundingRect();
  const qreal pageH = a4hPx() + pageSpacingPx();
  if (pageH <= 0.0)
    return;
  // v3.17.6: hydrate visible pages + N padding on either side so that
  // scrolling does not visibly stall while waiting for content. N=3 is
  // a tradeoff between memory pressure on very long notes and avoiding
  // visible "blank page" flashes during fast flicks.
  constexpr int kPad = 3;
  int first = qMax(0, int(vp.top() / pageH) - kPad);
  int last = qMin(note_->pages.size() - 1, int(vp.bottom() / pageH) + kPad);
  for (int i = first; i <= last; ++i)
    hydratePageContent(i);
}

void MultiPageNoteView::setNote(Note *note) {
  note_ = note;
  cancelCrop(); // v3.18.0: offene Crop-Session beenden, bevor scene_.clear()
                // den Resizer löschen würde (dangling pointer).
  if (m_undoStack)
    m_undoStack->clear();
  scene_.clear();
  pageItems_.clear();
  m_hydratedPages.clear();
  m_pagesBarAnchorStrip = nullptr;
  resetGraphChromeAfterSceneClear();

  layoutPages();

  // Wenn wir beim Laden im Lineal-Modus sind, muss das Lineal wiederhergestellt
  // werden (da scene_.clear() es gelöscht hat)
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
    RulerTool::ensureRulerExists(&scene_, ToolManager::instance().config());
  }

  if (!note_)
    return;

#ifdef Q_OS_ANDROID
  // v3.17.6: lazy page hydration. Only build the StrokeItem/GraphCanvasItem
  // graph for pages currently visible in the viewport (+/- N pages of
  // padding). Off-screen pages get hydrated on-demand from the scroll
  // coalescer in MultiPageNoteView's ctor. For big notes (>50 pages) this
  // cuts setNote() from hundreds of ms to a few dozen.
  hydrateVisibleRange();
#else
  for (int i = 0; i < note_->pages.size(); ++i)
    hydratePageContent(i);
#endif
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
        if (RulerItem* ruler = findActiveRuler(&scene_))
            ruler->setVisible(false);
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

#ifdef Q_OS_ANDROID
// Compute and apply a transform so a single A4 page fits horizontally inside
// the current viewport. Uses the horizontal scrollbar (cheap, no layout pass)
// instead of centerOn(mapToScene(...)) which can trigger reentrant resize
// events while the scene is being rebuilt by setNote().
//
// Defensive: only runs when the note + pages are actually present, the
// viewport has a sensible width, and we're not already auto-fitting.
void MultiPageNoteView::autoFitPageToViewportWidth() {
  if (m_userTouchedZoom)
    return;
  if (m_isAutoFitting)
    return;
  if (!note_)
    return;
  if (pageItems_.isEmpty())
    return;
  if (!viewport())
    return;
  const int viewW = viewport()->width();
  if (viewW <= UiScale::dp(80))
    return;
  const int pageW = a4wPx();
  if (pageW <= 0)
    return;

  m_isAutoFitting = true;

  const qreal targetPageW =
      qreal(viewW) - qreal(qMax(0, UiScale::safeHorizontalPaddingPx(this)));
  qreal scaleFactor = targetPageW / qreal(pageW);
  scaleFactor = qBound<qreal>(0.30, scaleFactor, 1.0);

  QTransform t;
  t.scale(scaleFactor, scaleFactor);
  setTransform(t);
  zoom_ = scaleFactor;

  // Position via horizontal scrollbar so the first page is centred. Avoid
  // centerOn(mapToScene(...)) which forces a layout pass and can recurse.
  const qreal pageCenterScene = qreal(pageW) / 2.0;
  const int targetScrollX =
      qRound(pageCenterScene * scaleFactor - qreal(viewW) / 2.0);
  if (QScrollBar *hb = horizontalScrollBar()) {
    hb->setValue(qBound(hb->minimum(), targetScrollX, hb->maximum()));
  }

  ensureSceneRectCoversViewport();
  m_isAutoFitting = false;
}

void MultiPageNoteView::requestAutoFit() {
  // Re-arm the one-shot fit so a new note opens at "page fits viewport".
  m_pendingInitialFit = true;
  // Note: do NOT reset m_userTouchedZoom here - if the user explicitly zoomed,
  // they probably want the same zoom to persist across notes within a session.
  if (isVisible()) {
    QTimer::singleShot(0, this, [this]() {
      m_pendingInitialFit = false;
      autoFitPageToViewportWidth();
    });
  }
}
#endif

void MultiPageNoteView::resizeEvent(QResizeEvent *e) {
  QGraphicsView::resizeEvent(e);
  syncPagesBarVisibility();
  syncGraphLegendLayout();
  repositionGraphEntryBar();
#ifdef Q_OS_ANDROID
  ensureSceneRectCoversViewport();
  // We intentionally do NOT auto-fit on resize: that can recurse with
  // scrollbar toggles caused by setTransform and has been the source of a
  // crash when opening a note. requestAutoFit() handles the deliberate cases.
#endif
}

void MultiPageNoteView::showEvent(QShowEvent *e) {
  QGraphicsView::showEvent(e);
  syncPagesBarVisibility();
#ifdef Q_OS_ANDROID
  ensureSceneRectCoversViewport();
  if (m_pendingInitialFit && !m_userTouchedZoom) {
    m_pendingInitialFit = false;
    // Defer one event-loop cycle so the viewport has its final size after the
    // first show / layout pass.
    QTimer::singleShot(0, this, [this]() { autoFitPageToViewportWidth(); });
  }
#endif
}

void MultiPageNoteView::wheelEvent(QWheelEvent *e) {
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
    const QPointF scenePos = mapToScene(e->position().toPoint());
    RulerItem *ruler = findActiveRuler(&scene_);
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
    m_userTouchedZoom = true;
    ensureSceneRectCoversViewport();
#endif
    syncPagesBarVisibility();
    syncGraphLegendLayout();
    repositionGraphEntryBar();
    e->accept();
  } else {
    QScrollBar *vb = verticalScrollBar();
    if (vb->value() >= vb->maximum() && e->angleDelta().y() < 0) {
      m_pullDistance += std::abs(e->angleDelta().y()) * 0.5f;
      // v3.16.1: visual pull-progress on the Plus-Card (SkeletonPageItem).
      const double prog =
          qMin(1.0, double(m_pullDistance) / double(kBottomRevealPull()));
      if (m_pagesBarAnchorStrip)
        m_pagesBarAnchorStrip->setPullProgress(prog);
      if (m_pullDistance >= kBottomRevealPull()) {
        // v3.16.1: pull-up beyond threshold commits a NEW page directly
        // instead of just scrolling the bottom sheet into view. The PagesBar
        // QWidget remains accessible at note-end for templates/import/etc.
        m_pullDistance = 0.f;
        if (m_pagesBarAnchorStrip)
          m_pagesBarAnchorStrip->setPullProgress(0.0);
        addNewPage();
        if (note_)
          scrollToPage(note_->pages.size() - 1);
      }
      e->accept();
      return;
    }

    if (m_pullDistance > 0.f && e->angleDelta().y() > 0) {
      m_pullDistance = 0.f;
      if (m_pagesBarAnchorStrip)
        m_pagesBarAnchorStrip->setPullProgress(0.0);
    }

    QGraphicsView::wheelEvent(e);
  }
}

bool MultiPageNoteView::viewportEvent(QEvent *ev) {
#ifdef Q_OS_ANDROID
  // Direct two-finger pinch handling - Qt's QPinchGesture recogniser
  // routinely misses the second finger on Android because the drawing
  // tool already consumed the first touch as a synthesised mouse-press,
  // so we read the touchpoints ourselves and compute the scale.
  switch (ev->type()) {
  case QEvent::TouchBegin:
  case QEvent::TouchUpdate:
  case QEvent::TouchEnd: {
    auto *te = static_cast<QTouchEvent *>(ev);
    int activeCount = 0;
    for (const auto &p : te->points()) {
      if (p.state() != QEventPoint::Released)
        ++activeCount;
    }
    if (activeCount >= 2)
      return handleAndroidPinch(te);
    if (m_androidPinchActive)
      m_androidPinchActive = false;
    break;
  }
  default:
    break;
  }
#endif
  if (ev->type() == QEvent::Gesture) {
    gestureEvent(static_cast<QGestureEvent *>(ev));
    return true;
  }
  return QGraphicsView::viewportEvent(ev);
}

#ifdef Q_OS_ANDROID
bool MultiPageNoteView::handleAndroidPinch(QTouchEvent *te) {
  const auto pts = te->points();
  if (pts.size() < 2)
    return false;
  const QPointF p1 = pts[0].position();
  const QPointF p2 = pts[1].position();
  const qreal dist = QLineF(p1, p2).length();
  if (!m_androidPinchActive) {
    if (dist < 4.0)
      return true;
    m_androidPinchActive = true;
    m_androidPinchInitialDistance = dist;
    m_androidPinchInitialScale = transform().m11();
    m_androidPinchAnchorScene =
        mapToScene(((p1 + p2) / 2.0).toPoint());
    m_userTouchedZoom = true;
    return true;
  }
  if (m_androidPinchInitialDistance < 1.0)
    return true;
  const qreal ratio = dist / m_androidPinchInitialDistance;
  const qreal target =
      qBound(qreal(0.05), m_androidPinchInitialScale * ratio, qreal(12.0));
  QTransform t;
  t.scale(target, target);
  setTransform(t);
  centerOn(m_androidPinchAnchorScene);
  return true;
}
#endif

void MultiPageNoteView::gestureEvent(QGestureEvent *event) {
  bool handledByRuler = false;
  if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
    if (QGesture *pinch = event->gesture(Qt::PinchGesture)) {
      auto *pg = static_cast<QPinchGesture *>(pinch);
      const QPointF cScene = mapToScene(pg->centerPoint().toPoint());
      RulerItem *ruler = findActiveRuler(&scene_);
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
    RulerItem *ruler = findActiveRuler(scene());
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
      m_userTouchedZoom = true;
      ensureSceneRectCoversViewport();
#endif
      syncPagesBarVisibility();
      syncGraphLegendLayout();
      repositionGraphEntryBar();
    }
  }

  if (changeFlags & QPinchGesture::CenterPointChanged) {
    QPointF delta = gesture->centerPoint() - gesture->lastCenterPoint();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    syncGraphLegendLayout();
    repositionGraphEntryBar();
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

  // v3.18.0: während einer Crop-Session gehen alle Eingaben an den
  // CropResizer (QGraphicsView-Routing) statt an die Zeichen-Tools.
  if (m_cropResizer && e->button() == Qt::LeftButton) {
    QGraphicsView::mousePressEvent(e);
    return;
  }

  // Graph "+" / chrome reach the item before touch-pan and tools.
  if (e->button() == Qt::LeftButton) {
    const QPointF scenePos = mapToScene(e->pos());

    // Formula zone buttons may lie outside the page rect — check before everything.
    if (m_activeFormulaZone) {
      if (m_activeFormulaZone->hitClearButton(scenePos)) {
        m_activeFormulaZone->clearZone();
        e->accept();
        return;
      }
      if (m_activeFormulaZone->hitCommitButton(scenePos)) {
        m_activeFormulaZone->closeInlineEditor();
        emit m_activeFormulaZone->commitRequested(m_activeFormulaZone->recognizedExpression());
        e->accept();
        return;
      }
      if (m_activeFormulaZone->hitExpressionLabel(scenePos)) {
        m_activeFormulaZone->openInlineEditor();
        e->accept();
        return;
      }
    }

    GraphCanvasItem *plusHit = graphCanvasHittingPlus(&scene_, scenePos);
    GraphCanvasItem *chromeHit = graphCanvasHittingChrome(&scene_, scenePos);
    if (pageAt(scenePos) >= 0 || plusHit || chromeHit) {
      if (plusHit) {
        m_graphPlusBypassItem = plusHit;
        QGraphicsView::mousePressEvent(e);
        return;
      }
      if (chromeHit) {
        m_graphPlotBypassItem = chromeHit;
        QGraphicsView::mousePressEvent(e);
        return;
      }
    }

    // v3.16.1: tap on the Plus-Card (SkeletonPageItem) creates a new page
    // directly. The QGraphicsView itemAt() check finds the strip even when
    // it's outside the page rect.
    if (m_pagesBarAnchorStrip && m_pagesBarAnchorStrip->isVisible() &&
        m_pagesBarAnchorStrip->sceneBoundingRect().contains(scenePos)) {
      addNewPage();
      if (note_)
        scrollToPage(note_->pages.size() - 1);
      e->accept();
      return;
    }
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
      RulerItem *ruler = findActiveRuler(&scene_);
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

  if ((m_graphPlusBypassItem || m_graphPlotBypassItem || m_cropResizer) &&
      (e->buttons() & Qt::LeftButton)) {
    QGraphicsView::mouseMoveEvent(e);
    return;
  }

  if (m_isPanning) {
    QPoint delta = e->pos() - m_lastPanPos;
    m_lastPanPos = e->pos();

    QScrollBar *vb = verticalScrollBar();
    if (vb->value() >= vb->maximum() && delta.y() < 0) {
      m_pullDistance += std::abs(delta.y()) * 0.5f;
      // v3.16.1: drive the Plus-Card visual feedback during touch pull-up.
      const double prog =
          qMin(1.0, double(m_pullDistance) / double(kBottomRevealPull()));
      if (m_pagesBarAnchorStrip)
        m_pagesBarAnchorStrip->setPullProgress(prog);
      if (m_pullDistance >= kBottomRevealPull()) {
        // v3.16.1: threshold commits a new page directly instead of just
        // surfacing the existing PagesBar sheet.
        m_pullDistance = 0.f;
        if (m_pagesBarAnchorStrip)
          m_pagesBarAnchorStrip->setPullProgress(0.0);
        addNewPage();
        if (note_)
          scrollToPage(note_->pages.size() - 1);
      }
    } else {
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                      delta.x());
      verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
      if (m_pullDistance > 0.f) {
        m_pullDistance = 0.f;
        if (m_pagesBarAnchorStrip)
          m_pagesBarAnchorStrip->setPullProgress(0.0);
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

void MultiPageNoteView::commitPendingStrokeItemsToNote(AbstractTool *tool) {
  if (!tool || !note_)
    return;
  QList<QGraphicsItem *> itemsToRemove;
  for (QGraphicsItem *item : scene_.items()) {
    if (!item->parentItem() && item->type() == QGraphicsItem::UserType + 1) {
      auto *strokeItem = static_cast<StrokeItem *>(item);
      auto points = strokeItem->points();
      if (!points.isEmpty()) {
        // ── Formula-zone eraser: must happen BEFORE the pageAt() check ──────
        // The formula zone is positioned outside the page boundary, so when
        // the eraser starts there pageAt() returns -1 and the inner zone check
        // is never reached.  We handle it here unconditionally.
        if (m_activeFormulaZone && strokeItem->strokeStyle() == StrokeItem::Eraser) {
          QPainterPath zoneEraserPath;
          for (int i = 0; i < points.size(); ++i) {
            if (i == 0) zoneEraserPath.moveTo(points[i].pos);
            else        zoneEraserPath.lineTo(points[i].pos);
          }
          if (m_activeFormulaZone->catchAreaSceneRect()
                  .intersects(zoneEraserPath.boundingRect())) {
            m_activeFormulaZone->eraseStrokesIntersecting(
                zoneEraserPath, strokeItem->pen().widthF());
          }
        }

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
          QPainterPath scenePath;
          for (int i = 0; i < points.size(); ++i) {
            QPointF sceneP = points[i].pos;
            QPointF localP = sceneP - pageTopLeft;
            s.points.append(localP);
            if (i == 0) {
              localPath.moveTo(localP);
              scenePath.moveTo(sceneP);
            } else {
              localPath.lineTo(localP);
              scenePath.lineTo(sceneP);
            }
          }
          s.path = localPath;

          bool capturedByZone = false;
          if (m_activeFormulaZone && !s.isEraser) {
            // Eraser is already handled above; only capture ink/highlighter here.
            if (!s.isHighlighter) {
              const QRectF zoneScene = m_activeFormulaZone->catchAreaSceneRect();
              if (zoneScene.intersects(scenePath.boundingRect())) {
                if (m_activeFormulaZone->addStroke(scenePath, s.color, s.width)) {
                  capturedByZone = true;
                  // Push to undo stack so Ctrl+Z removes the formula zone stroke
                  if (m_undoStack) {
                    m_undoStack->push(new FormulaZoneStrokeCommand(
                        m_activeFormulaZone, scenePath, s.color, s.width));
                  }
                }
              }
            }
          }

          if (!capturedByZone) {
            pushStrokeUndoCommand(pIdx, std::move(s));
          }
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

  if (m_cropResizer && e->button() == Qt::LeftButton) {
    QGraphicsView::mouseReleaseEvent(e);
    return;
  }
  if (m_graphPlusBypassItem && e->button() == Qt::LeftButton) {
    QGraphicsView::mouseReleaseEvent(e);
    m_graphPlusBypassItem = nullptr;
    return;
  }
  if (m_graphPlotBypassItem && e->button() == Qt::LeftButton) {
    QGraphicsView::mouseReleaseEvent(e);
    m_graphPlotBypassItem = nullptr;
    return;
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
      GraphCanvasItem *newGraph = qgraphicsitem_cast<GraphCanvasItem *>(tool->lastCompletedItem());
      commitPendingStrokeItemsToNote(tool);
      syncGraphItemsToNote();
      if (newGraph) {
        // v3.17.2: see comment in GraphQuickActionPopup connect site --
        // BlopModal::execBlocking avoids top-level QWindow on Android.
#ifdef Q_OS_ANDROID
        auto *dlg = new GraphAxisSettingsDialog(newGraph, window());
        BlopModal::execBlocking(window(), dlg);
        dlg->deleteLater();
#else
        GraphAxisSettingsDialog dlg(newGraph, window());
        dlg.exec();
#endif
        syncGraphItemsToNote();
      }
      tool->clearLastCompletedItem();
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
  // v3.18.0: in einer Crop-Session den Stift NICHT als Zeichen-Tool
  // behandeln. e->ignore() lässt Qt synthetische Maus-Events erzeugen,
  // die über mousePressEvent() an den CropResizer geroutet werden.
  if (m_cropResizer) {
    e->ignore();
    return;
  }

  const QPointF scenePos = mapToScene(e->position().toPoint());

  if (note_ && mode_ != ToolMode::Lasso) {
    if (e->type() == QEvent::TabletRelease && m_graphTabletPendingItem) {
      GraphCanvasItem *const gi = m_graphTabletPendingItem.data();
      const QPointF pressScene = m_graphTabletPressScene;
      const qint64 pressMs = m_graphTabletPressMs;
      m_graphTabletPendingItem = nullptr;
      if (gi && QLineF(scenePos, pressScene).length() < 22.0) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 elapsedMs = nowMs - pressMs;
        const qint64 clampedMs = qBound(qint64{0}, elapsedMs, qint64{600000});
        const int holdMs = static_cast<int>(clampedMs);
        gi->deliverTapFromScene(pressScene, holdMs);
      }
      e->accept();
      return;
    }
    if (e->type() == QEvent::TabletMove && m_graphTabletPendingItem) {
      if (QLineF(scenePos, m_graphTabletPressScene).length() > 24.0)
        m_graphTabletPendingItem = nullptr;
      e->accept();
      return;
    }
    if (e->type() == QEvent::TabletPress) {
      // Formula zone buttons may lie outside the page rect — check BEFORE pageAt().
      if (m_activeFormulaZone) {
        if (m_activeFormulaZone->hitClearButton(scenePos)) {
          m_activeFormulaZone->clearZone();
          e->accept();
          return;
        }
        if (m_activeFormulaZone->hitCommitButton(scenePos)) {
          m_activeFormulaZone->closeInlineEditor();
          emit m_activeFormulaZone->commitRequested(m_activeFormulaZone->recognizedExpression());
          e->accept();
          return;
        }
        if (m_activeFormulaZone->hitExpressionLabel(scenePos)) {
          m_activeFormulaZone->openInlineEditor();
          e->accept();
          return;
        }
      }

      if (pageAt(scenePos) >= 0) {
        if (GraphCanvasItem *hitGraph = graphCanvasHittingPlus(&scene_, scenePos)) {
          hitGraph->requestPlusTap();
          e->accept();
          return;
        }
        if (GraphCanvasItem *hitGraph = graphCanvasHittingChrome(&scene_, scenePos)) {
          m_graphTabletPendingItem = hitGraph;
          m_graphTabletPressScene = scenePos;
          m_graphTabletPressMs = QDateTime::currentMSecsSinceEpoch();
          e->accept();
          return;
        }
      }
    }
  }

  AbstractTool *tool = ToolManager::instance().activeTool();

  // ── Live eraser → formula zone ────────────────────────────────────
  // Formula zone strokes are NOT QGraphicsScene items — the EraserTool's
  // eraseAt() never sees them.  We intercept every eraser move/press here
  // and forward it directly to the formula zone.
  if (tool && m_activeFormulaZone
      && tool->mode() == ToolMode::Eraser
      && (e->type() == QEvent::TabletPress || e->type() == QEvent::TabletMove))
  {
      const QRectF zoneScene = m_activeFormulaZone->catchAreaSceneRect();
      const qreal r = 12.0; // eraser radius
      if (zoneScene.adjusted(-r, -r, r, r).contains(scenePos)) {
          QPainterPath ep;
          ep.addEllipse(scenePos, r, r);
          m_activeFormulaZone->eraseStrokesIntersecting(ep, r * 2.0);
      }
  }

  if (tool && note_ && mode_ != ToolMode::Lasso) {
    tool->setStrokeSceneForTablet(&scene_);
    if (tool->handleTabletEvent(e, scenePos)) {
      e->accept();
      if (e->type() == QEvent::TabletRelease) {
        GraphCanvasItem *newGraph = qgraphicsitem_cast<GraphCanvasItem *>(tool->lastCompletedItem());
        commitPendingStrokeItemsToNote(tool);
        syncGraphItemsToNote();
        if (newGraph) {
#ifdef Q_OS_ANDROID
          auto *dlg = new GraphAxisSettingsDialog(newGraph, window());
          BlopModal::execBlocking(window(), dlg);
          dlg->deleteLater();
#else
          GraphAxisSettingsDialog dlg(newGraph, window());
          dlg.exec();
#endif
          syncGraphItemsToNote();
        }
        tool->clearLastCompletedItem();
      }
      return;
    }
  }

  if (!note_ || mode_ == ToolMode::Lasso) {
    e->ignore();
    return;
  }

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

namespace {
// v3.17.6: pure stroke-render helper, safe to invoke from any thread.
// Operates only on QImage / QPainter and value-type Stroke fields, so it
// can be dispatched to QtConcurrent::run without touching scene state.
QImage renderThumbnailImage(int pageW, int pageH, const QSize &size,
                            const QVector<Stroke> &strokes) {
  QImage img(pageW, pageH, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::white);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);

  for (const auto &s : strokes) {
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
  return img.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
} // namespace

QPixmap MultiPageNoteView::generateThumbnail(int pageIndex, const QSize &size) {
  if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size()) {
    QPixmap empty(size);
    empty.fill(Qt::white);
    return empty;
  }
  // v3.17.6: try cache first (populated by both sync + async paths).
  const QString key = thumbnailCacheKey(pageIndex, size);
  QPixmap cached;
  if (QPixmapCache::find(key, &cached))
    return cached;
  QImage scaled = renderThumbnailImage(a4wPx(), a4hPx(), size,
                                       note_->pages[pageIndex].strokes);
  QPixmap pm = QPixmap::fromImage(scaled);
  QPixmapCache::insert(key, pm);
  return pm;
}

QString MultiPageNoteView::thumbnailCacheKey(int pageIndex,
                                             const QSize &size) const {
  // Cache-bust on note identity + page revision so a stroke edit invalidates
  // its thumbnail entry. Note* pointer alone is fine for the cache lifetime
  // since QPixmapCache is process-wide and entries get evicted on memory
  // pressure anyway.
  return QStringLiteral("blop_thumb_%1_%2_%3x%4")
      .arg(reinterpret_cast<quintptr>(note_))
      .arg(pageIndex)
      .arg(size.width())
      .arg(size.height());
}

void MultiPageNoteView::generateThumbnailAsync(
    int pageIndex, const QSize &size,
    std::function<void(QPixmap)> callback) {
  if (!callback)
    return;
  if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size()) {
    QPixmap empty(size);
    empty.fill(Qt::white);
    callback(empty);
    return;
  }
  // Hit-cache fast path keeps the synchronous behaviour for repeat asks.
  const QString key = thumbnailCacheKey(pageIndex, size);
  QPixmap cached;
  if (QPixmapCache::find(key, &cached)) {
    callback(cached);
    return;
  }
  const int pageW = a4wPx();
  const int pageH = a4hPx();
  // Deep-copy strokes into the worker. QPainterPath / QColor are
  // implicitly-shared value types; the worker mutates none of them.
  QVector<Stroke> strokes = note_->pages[pageIndex].strokes;
  QPointer<MultiPageNoteView> guard(this);
  auto *watcher = new QFutureWatcher<QImage>(this);
  connect(watcher, &QFutureWatcher<QImage>::finished, this,
          [watcher, guard, callback, key, size]() {
            QImage img = watcher->result();
            watcher->deleteLater();
            if (!guard) {
              callback(QPixmap());
              return;
            }
            QPixmap pm = QPixmap::fromImage(img);
            QPixmapCache::insert(key, pm);
            callback(pm);
          });
  watcher->setFuture(QtConcurrent::run(
      [pageW, pageH, size, strokes]() {
        return renderThumbnailImage(pageW, pageH, size, strokes);
      }));
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
void MultiPageNoteView::scrollToPage(int pageIndex, bool animate) {
  if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size())
    return;
  QRectF r = pageRect(pageIndex);
  const int target = qRound(r.top());
  QScrollBar *vb = verticalScrollBar();
  if (!animate || !vb) {
    if (vb)
      vb->setValue(target);
    return;
  }
  // v3.16.1: animated jump between pages. 280ms OutCubic matches the rest
  // of the v3.16.1 unified animation language (BlopStyle slide-in, MorphTray
  // geometry anim, etc.). The animation is parented to vb so it's cleaned
  // up automatically if the view goes away mid-anim.
  auto *anim = new QPropertyAnimation(vb, "value", vb);
  anim->setDuration(280);
  anim->setEasingCurve(QEasingCurve::OutCubic);
  anim->setStartValue(vb->value());
  anim->setEndValue(target);
  anim->start(QAbstractAnimation::DeleteWhenStopped);
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
    // v119 perf: clip overlay drawing to the exposed rect so a
    // full-viewport overlay (e.g. cursor / ruler crosshair) cannot
    // dirty the entire viewport on every move.
    painter->setClipRect(rect, Qt::IntersectClip);
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
  // v3.18.0: während einer Crop-Session kein Selektionsmenü über dem
  // Resizer aufpoppen lassen (der CropResizer selbst ist selektierbar).
  if (m_cropResizer) {
    if (m_selectionMenu)
      m_selectionMenu->hide();
    return;
  }
  QList<QGraphicsItem*> items = scene_.selectedItems();
  GraphCanvasItem* graphItem = nullptr;
  for (QGraphicsItem* item : items) {
    if (item->type() == GraphCanvasItem::Type) {
      graphItem = static_cast<GraphCanvasItem*>(item);
      break;
    }
  }
  if (graphItem) {
    if (m_selectionMenu) m_selectionMenu->hide();
    if (m_graphEntryBarOpen && m_graphEntryTargetGraph && m_graphEntryTargetGraph != graphItem)
      abandonGraphEntrySession();
    m_selectedGraphItem = graphItem;
    syncGraphPlusLayout(graphItem);
    m_graphPanelTargetGraph = graphItem;
    m_graphPanelExplicitOpen = true;
    bindGraphChrome(m_selectedGraphItem);
    presentGraphLegendAnimated();
    return;
  }
  abandonGraphEntrySession();
  m_selectedGraphItem = nullptr;
  m_graphPanelExplicitOpen = false;
  m_graphPanelTargetGraph = nullptr;
  hideGraphLegendQuick();

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
    syncGraphItemsToNote();
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

// v3.18.0: war vorher ein leerer Stub. Selektierte Stroke-Items werden als
// neue Strokes ins Notiz-Modell übernommen (per StrokeAddUndoCommand, also
// undo-fähig), Graph-Items werden über toData()/fromData() geklont und über
// syncGraphItemsToNote() persistiert. Die Kopien sitzen 20px versetzt.
void MultiPageNoteView::duplicateSelection() {
  if (!note_)
    return;
  const QList<QGraphicsItem *> selected = scene_.selectedItems();
  if (selected.isEmpty())
    return;

  const QPointF offset(20, 20);
  bool duplicatedGraph = false;
  bool hasStrokeTargets = false;
  for (QGraphicsItem *item : selected) {
    if (item->type() != GraphCanvasItem::Type &&
        !(item->parentItem() &&
          item->parentItem()->type() == GraphCanvasItem::Type) &&
        dynamic_cast<QGraphicsPathItem *>(item)) {
      hasStrokeTargets = true;
      break;
    }
  }
  const bool useMacro = m_undoStack && hasStrokeTargets;
  if (useMacro)
    m_undoStack->beginMacro(QStringLiteral("Auswahl duplizieren"));

  for (QGraphicsItem *item : selected) {
    if (item->type() == GraphCanvasItem::Type) {
      auto *src = static_cast<GraphCanvasItem *>(item);
      GraphObject d = src->toData();
      auto *gi = new GraphCanvasItem(d.rect);
      gi->fromData(d);
      if (src->parentItem())
        gi->setParentItem(src->parentItem());
      else
        scene_.addItem(gi);
      gi->setPos(src->pos() + offset);
      gi->setFlag(QGraphicsItem::ItemIsSelectable, true);
      gi->setFlag(QGraphicsItem::ItemIsMovable, true);
      gi->setZValue(src->zValue());
      duplicatedGraph = true;
      continue;
    }
    // Items, die zu einem Graphen gehören (Formelzonen etc.), überspringen.
    if (item->parentItem() &&
        item->parentItem()->type() == GraphCanvasItem::Type)
      continue;
    auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
    if (!pathItem)
      continue;

    QPainterPath scenePath = pathItem->mapToScene(pathItem->path());
    scenePath.translate(offset);
    int pIdx = pageAt(scenePath.boundingRect().center());
    if (pIdx < 0)
      pIdx = pageAt(pathItem->sceneBoundingRect().center());
    if (pIdx < 0)
      continue;
    note_->ensurePage(pIdx);
    const QPointF pageTopLeft = pageRect(pIdx).topLeft();

    Stroke s;
    s.pageIndex = pIdx;
    const QPen pen = pathItem->pen();
    s.width = pen.widthF();
    QColor color = pen.color();
    if (auto *si = dynamic_cast<StrokeItem *>(item)) {
      s.isEraser = (si->strokeStyle() == StrokeItem::Eraser);
      s.isHighlighter = (si->strokeStyle() == StrokeItem::Highlighter);
    } else if (color.alpha() < 255) {
      s.isHighlighter = true;
    }
    if (s.isHighlighter)
      color.setAlpha(255);
    s.color = color;
    s.path = scenePath.translated(-pageTopLeft);
    for (int i = 0; i < s.path.elementCount(); ++i)
      s.points.append(QPointF(s.path.elementAt(i).x, s.path.elementAt(i).y));

    pushStrokeUndoCommand(pIdx, std::move(s));
  }

  if (useMacro)
    m_undoStack->endMacro();
  if (duplicatedGraph)
    syncGraphItemsToNote();
  onSelectionChanged();
}

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
  syncGraphItemsToNote();
  if (onSaveRequested) onSaveRequested(note_);
}

// === CROP SYSTEM (v3.18.0, analog CanvasView) ===============================

void MultiPageNoteView::startCropSession() {
  if (m_cropResizer)
    cancelCrop();

  // Crop-Ziele einsammeln, bevor eine evtl. aktive Transform-Session
  // aufgelöst wird (die Szene-Selektion kann dabei verloren gehen).
  m_cropTargets.clear();
  QRectF totalRect;
  for (QGraphicsItem *item : scene_.selectedItems()) {
    if (item->type() == GraphCanvasItem::Type)
      continue;
    if (item->parentItem() &&
        item->parentItem()->type() == GraphCanvasItem::Type)
      continue;
    if (!dynamic_cast<QGraphicsPathItem *>(item))
      continue;
    m_cropTargets.append(item);
    totalRect = totalRect.united(item->sceneBoundingRect());
  }
  if (m_cropTargets.isEmpty())
    return;

  if (m_transformOverlay)
    applyTransform();
  if (m_selectionMenu)
    m_selectionMenu->hide();

  m_cropResizer = new CropResizer(
      totalRect.isEmpty() ? QRectF(0, 0, 200, 200) : totalRect);
  m_cropResizer->setZValue(99999);
  scene_.addItem(m_cropResizer);

  positionCropMenu();
  m_cropMenu->show();
  m_cropMenu->raise();
}

void MultiPageNoteView::positionCropMenu() {
  if (!m_cropMenu || !m_cropResizer)
    return;
  const QRectF r = m_cropResizer->currentRect();
  const QPoint viewPos = mapFromScene(r.topLeft());
  int x = viewPos.x() + int(mapFromScene(r.topRight()).x() - viewPos.x()) / 2 -
          m_cropMenu->width() / 2;
  int y = viewPos.y() - m_cropMenu->height() - 10;
  x = qBound(0, x, qMax(0, width() - m_cropMenu->width()));
  y = qBound(0, y, qMax(0, height() - m_cropMenu->height()));
  m_cropMenu->move(x, y);
}

void MultiPageNoteView::applyCrop() {
  QPainterPath clipPath;
  if (m_cropResizer) {
    clipPath.addRect(m_cropResizer->currentRect());
    scene_.removeItem(m_cropResizer);
    delete m_cropResizer;
    m_cropResizer = nullptr;
  }
  if (m_cropMenu)
    m_cropMenu->hide();
  if (clipPath.isEmpty()) {
    m_cropTargets.clear();
    return;
  }
  for (QGraphicsItem *item : std::as_const(m_cropTargets)) {
    if (auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item)) {
      QPainterPath localClip = pathItem->mapFromScene(clipPath);
      localClip.setFillRule(Qt::WindingFill);
      pathItem->setPath(pathItem->path().intersected(localClip));
    }
  }
  m_cropTargets.clear();
  scene_.update();
  if (m_selectionMenu)
    m_selectionMenu->hide();
  if (onSaveRequested)
    onSaveRequested(note_);
}

void MultiPageNoteView::cancelCrop() {
  if (m_cropResizer) {
    scene_.removeItem(m_cropResizer);
    delete m_cropResizer;
    m_cropResizer = nullptr;
  }
  if (m_cropMenu)
    m_cropMenu->hide();
  m_cropTargets.clear();
}

namespace {
// Leichter In-Window-Toast (Pattern aus mainwindow.cpp): kein QDialog::exec,
// damit der Android-Single-Window-Surface nicht blockiert wird.
void showViewToast(QWidget *anchor, const QString &text,
                   int durationMs = 2400) {
  QWidget *win = anchor ? anchor->window() : nullptr;
  if (!win)
    return;
  auto *toast = new QLabel(text, win);
  toast->setAttribute(Qt::WA_DeleteOnClose);
  toast->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  toast->setAlignment(Qt::AlignCenter);
  toast->setWordWrap(true);
  toast->setStyleSheet(QStringLiteral(
      "QLabel { background: rgba(20,20,30,0.92); color: #E8E4FF;"
      " border: 1px solid rgba(124,92,252,0.45); border-radius: 10px;"
      " padding: 10px 18px; font-size: 14px; }"));
  const int maxW = qMin(win->width() - UiScale::dp(40), UiScale::dp(360));
  toast->setMaximumWidth(maxW);
  toast->adjustSize();
  toast->move((win->width() - toast->width()) / 2,
              win->height() - toast->height() - UiScale::dp(60));
  toast->show();
  toast->raise();
  QTimer::singleShot(durationMs, toast, &QWidget::close);
}
} // namespace

// v3.18.0: vorher landete der Screenshot nur im Clipboard. Jetzt zusätzlich
// "Als PNG speichern": Desktop über QFileDialog, Android direkt ins
// App-Pictures-Verzeichnis mit Toast-Bestätigung.
void MultiPageNoteView::screenshotSelection() {
  const QList<QGraphicsItem *> selected = scene_.selectedItems();
  if (selected.isEmpty())
    return;
  QRectF selRect;
  for (auto *i : selected)
    selRect = selRect.united(i->sceneBoundingRect());
  if (selRect.isEmpty())
    return;
  QImage img(selRect.size().toSize(), QImage::Format_ARGB32);
  img.fill(Qt::transparent);
  {
    QPainter p(&img);
    scene_.render(&p, QRectF(0, 0, img.width(), img.height()), selRect);
  }
  QGuiApplication::clipboard()->setImage(img);

#ifdef Q_OS_ANDROID
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  QDir().mkpath(dir);
  const QString fileName =
      QStringLiteral("Blop_%1.png")
          .arg(QDateTime::currentDateTime().toString(
              QStringLiteral("yyyyMMdd_HHmmss")));
  const QString path = dir + QLatin1Char('/') + fileName;
  if (img.save(path))
    showViewToast(this,
                  QStringLiteral("Screenshot gespeichert: %1").arg(fileName));
  else
    showViewToast(this,
                  QStringLiteral("Screenshot konnte nicht gespeichert werden"));
#else
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Screenshot speichern"),
      QStringLiteral("auswahl.png"), QStringLiteral("PNG-Bild (*.png)"));
  if (!path.isEmpty() && !img.save(path))
    showViewToast(this,
                  QStringLiteral("Screenshot konnte nicht gespeichert werden"));
#endif
  if (m_selectionMenu)
    m_selectionMenu->hide();
}

void MultiPageNoteView::syncGraphPlusLayout(GraphCanvasItem *gi) {
  if (!gi)
    return;
  auto d = gi->data();
  int committed = d.functions.size();
  if (m_livePreviewIndex >= 0 && m_livePreviewIndex < committed && gi == m_selectedGraphItem)
    committed -= 1;
  gi->setCommittedFunctionCountForPlusLayout(qMax(0, committed));
  const bool entryOpen = m_graphEntryBar && m_graphEntryBar->isVisible() && m_graphEntryBarOpen &&
                         m_graphEntryTargetGraph == gi;
  gi->setPlusButtonSuppressed(entryOpen);
}

void MultiPageNoteView::closeGraphEntryBar() {
  m_graphEntryBarOpen = false;
  m_graphEntryTargetGraph = nullptr;
  if (m_graphEntryBar) {
    hideGraphEntryBarQuick();
    m_graphEntryBar->setStatus(QString());
  }
  if (m_selectedGraphItem)
    syncGraphPlusLayout(m_selectedGraphItem);
}

void MultiPageNoteView::abandonGraphEntrySession() {
  GraphCanvasItem *gi = m_graphEntryTargetGraph;
  if (m_graphEntryBarOpen && gi) {
    auto d = gi->data();
    if (m_livePreviewIndex >= 0 && m_livePreviewIndex < d.functions.size()) {
      d.functions.removeAt(m_livePreviewIndex);
      m_livePreviewIndex = -1;
      if (d.functions.isEmpty())
        d.selectedFunction = -1;
      else
        d.selectedFunction = qBound(0, d.selectedFunction, d.functions.size() - 1);
      gi->fromData(d);
    }
    syncGraphPlusLayout(gi);
  }
  closeGraphEntryBar();
}

void MultiPageNoteView::openGraphEntryBarForGraph(GraphCanvasItem *gi, bool fromPlus) {
  if (!gi || !m_graphEntryBar)
    return;
  if (m_graphEntryBarOpen && m_graphEntryTargetGraph == gi) {
    m_graphEntryBar->raise();
    repositionGraphEntryBar();
    if (fromPlus)
      m_graphEntryBar->notifyPlusOpened();
    return;
  }
  if (m_graphEntryBarOpen && m_graphEntryTargetGraph != gi)
    abandonGraphEntrySession();
  m_selectedGraphItem = gi;
  m_graphEntryBarOpen = true;
  m_graphEntryTargetGraph = gi;
  m_graphEntryBar->prepareOpen();
  syncGraphPlusLayout(gi);
  presentGraphEntryBarAnimated();
  if (fromPlus)
    m_graphEntryBar->notifyPlusOpened();
}

void MultiPageNoteView::repositionGraphEntryBar() {
  if (!m_graphEntryBarOpen || !m_selectedGraphItem || !m_graphEntryBar)
    return;
  m_graphEntryBar->adjustSize();
  const int mw = m_graphEntryBar->width();
  const int mh = m_graphEntryBar->height();
  const int margin = 8;
  QPoint topLeft;
  if (m_graphLegendDock && m_graphLegendDock->isVisible() && m_graphPanelExplicitOpen &&
      m_selectedGraphItem == m_graphPanelTargetGraph) {
    const QRect lr = m_graphLegendDock->geometry();
    topLeft = QPoint(lr.center().x() - mw / 2, lr.bottom() + margin);
    topLeft.setX(qBound(margin, topLeft.x(), qMax(margin, width() - mw - margin)));
    topLeft.setY(qBound(margin, topLeft.y(), qMax(margin, height() - mh - margin)));
  } else {
    topLeft = pickSmartPanelPos(this, m_selectedGraphItem, 0, mw, mh, margin);
  }
  m_graphEntryBar->move(topLeft);
}

void MultiPageNoteView::syncGraphLegendLayout() {
  const int margin = 10;
  if (m_graphPanelExplicitOpen && m_selectedGraphItem && m_graphLegendDock && m_graphLegendDock->isVisible()) {
    m_graphLegendDock->adjustSize();
    const int lw = m_graphLegendDock->width();
    const int lh = m_graphLegendDock->height();
    const QPoint tl = legendTopLeftDocked(this, m_selectedGraphItem, lw, lh, margin);
    m_graphLegendDock->move(tl);
  }
  if (m_graphQuickPopupWanted && m_graphQuickPopup && m_selectedGraphItem) {
    m_graphQuickPopup->adjustSize();
    const int pw = m_graphQuickPopup->width();
    const int ph = m_graphQuickPopup->height();
    const QPoint pt = quickPopupTopLeftForView(this, m_graphQuickAnchorScene, pw, ph, margin);
    m_graphQuickPopup->move(pt);
    if (!m_graphQuickPopup->isVisible()) {
      m_graphQuickPopup->show();
    }
    m_graphQuickPopup->raise();
  }
  repositionGraphEntryBar();
}

void MultiPageNoteView::bindGraphChrome(GraphCanvasItem *gi) {
  if (gi) {
    const auto &gd = gi->data();
    if (!gd.functions.isEmpty() && gd.selectedFunction < 0)
      gi->setSelectedFunction(0);
  }
  if (m_graphLegendDock)
    m_graphLegendDock->bind(gi);
  if (m_graphQuickPopup)
    m_graphQuickPopup->bind(gi);
}

void MultiPageNoteView::resetGraphChromeAfterSceneClear() {
  m_graphPlusBypassItem = nullptr;
  m_graphPlotBypassItem = nullptr;
  m_graphTabletPendingItem = nullptr;
  m_selectedGraphItem = nullptr;
  m_graphPanelTargetGraph = nullptr;
  m_graphPanelExplicitOpen = false;
  m_graphQuickPopupWanted = false;
  m_livePreviewIndex = -1;
  m_graphEntryBarOpen = false;
  m_graphEntryTargetGraph = nullptr;
  hideGraphLegendQuick();
  bindGraphChrome(nullptr);
  if (m_graphEntryBar) {
    hideGraphEntryBarQuick();
    m_graphEntryBar->setStatus(QString());
  }
}

void MultiPageNoteView::updateGraphChromeIfVisible() {
  if (!m_graphPanelExplicitOpen || !m_graphLegendDock || !m_graphLegendDock->isVisible())
    return;
  if (!m_selectedGraphItem || m_selectedGraphItem->scene() != &scene_)
    return;
  bindGraphChrome(m_selectedGraphItem);
  syncGraphLegendLayout();
}

// v3.17.4: opacity-effect lifecycle is now bounded to the brief fade
// animation (140 ms). After the animation ends the effect is removed via
// setGraphicsEffect(nullptr), so the widget paints natively in steady-state.
// Previously the QGraphicsOpacityEffect lived for the lifetime of the dock
// and forced an offscreen pixmap rasterisation per frame -- on the Android
// software path that was responsible for ~6-8 dropped frames every time the
// graph chrome was visible.

void MultiPageNoteView::ensureGraphLegendFadeSetup() {
  if (!m_graphLegendDock || m_graphLegendFadeAnim)
    return;
  m_graphLegendFadeAnim = new QPropertyAnimation(this);
  m_graphLegendFadeAnim->setDuration(140);
  m_graphLegendFadeAnim->setEasingCurve(QEasingCurve::OutCubic);
  m_graphLegendFadeAnim->setPropertyName("opacity");
  connect(m_graphLegendFadeAnim, &QPropertyAnimation::finished, this, [this]() {
    if (!m_graphLegendDock)
      return;
    if (m_graphLegendOpacityFx && m_graphLegendOpacityFx->opacity() <= 0.02) {
      m_graphLegendDock->hide();
    }
    // Drop the effect so steady-state paints take the fast path.
    m_graphLegendDock->setGraphicsEffect(nullptr);
    m_graphLegendOpacityFx = nullptr;
  });
}

void MultiPageNoteView::ensureGraphEntryBarFadeSetup() {
  if (!m_graphEntryBar || m_graphEntryBarFadeAnim)
    return;
  m_graphEntryBarFadeAnim = new QPropertyAnimation(this);
  m_graphEntryBarFadeAnim->setDuration(140);
  m_graphEntryBarFadeAnim->setEasingCurve(QEasingCurve::OutCubic);
  m_graphEntryBarFadeAnim->setPropertyName("opacity");
  connect(m_graphEntryBarFadeAnim, &QPropertyAnimation::finished, this, [this]() {
    if (!m_graphEntryBar)
      return;
    if (m_graphEntryBarOpacityFx && m_graphEntryBarOpacityFx->opacity() <= 0.02) {
      m_graphEntryBar->hide();
    }
    m_graphEntryBar->setGraphicsEffect(nullptr);
    m_graphEntryBarOpacityFx = nullptr;
  });
}

void MultiPageNoteView::hideGraphQuickPopup() {
  m_graphQuickPopupWanted = false;
  if (m_graphQuickPopup)
    m_graphQuickPopup->hide();
}

void MultiPageNoteView::hideGraphLegendQuick() {
  if (m_graphLegendFadeAnim)
    m_graphLegendFadeAnim->stop();
  if (m_graphLegendDock) {
    m_graphLegendDock->setGraphicsEffect(nullptr);
    m_graphLegendOpacityFx = nullptr;
    m_graphLegendDock->hide();
  }
  hideGraphQuickPopup();
}

void MultiPageNoteView::hideGraphEntryBarQuick() {
  if (m_graphEntryBarFadeAnim)
    m_graphEntryBarFadeAnim->stop();
  if (m_graphEntryBar) {
    m_graphEntryBar->setGraphicsEffect(nullptr);
    m_graphEntryBarOpacityFx = nullptr;
    m_graphEntryBar->hide();
  }
}

void MultiPageNoteView::presentGraphLegendAnimated() {
  if (!m_graphLegendDock)
    return;
  ensureGraphLegendFadeSetup();
  const bool alreadyVisible = m_graphLegendDock->isVisible();
  if (alreadyVisible && !m_graphLegendOpacityFx) {
    // Steady-state fully opaque, just sync layout.
    syncGraphLegendLayout();
    m_graphLegendDock->raise();
    if (m_graphQuickPopupWanted && m_graphQuickPopup)
      m_graphQuickPopup->raise();
    return;
  }
  m_graphLegendFadeAnim->stop();
  // Install a transient opacity effect that only lives for the fade.
  m_graphLegendOpacityFx = new QGraphicsOpacityEffect(m_graphLegendDock);
  m_graphLegendOpacityFx->setOpacity(alreadyVisible ? 1.0 : 0.0);
  m_graphLegendDock->setGraphicsEffect(m_graphLegendOpacityFx);
  m_graphLegendFadeAnim->setTargetObject(m_graphLegendOpacityFx);
  if (!alreadyVisible) {
    m_graphLegendDock->show();
  }
  m_graphLegendDock->raise();
  syncGraphLegendLayout();
  if (m_graphQuickPopupWanted && m_graphQuickPopup) {
    m_graphQuickPopup->show();
    syncGraphLegendLayout();
    m_graphQuickPopup->raise();
  }
  m_graphLegendFadeAnim->setStartValue(m_graphLegendOpacityFx->opacity());
  m_graphLegendFadeAnim->setEndValue(1.0);
  m_graphLegendFadeAnim->start();
  QTimer::singleShot(0, this, [this]() {
    if (m_graphLegendDock && m_graphLegendDock->isVisible() && m_graphPanelExplicitOpen && m_selectedGraphItem)
      syncGraphLegendLayout();
  });
}

void MultiPageNoteView::presentGraphEntryBarAnimated() {
  if (!m_graphEntryBar)
    return;
  ensureGraphEntryBarFadeSetup();
  repositionGraphEntryBar();
  const bool alreadyVisible = m_graphEntryBar->isVisible();
  if (alreadyVisible && !m_graphEntryBarOpacityFx) {
    m_graphEntryBar->raise();
    return;
  }
  m_graphEntryBarFadeAnim->stop();
  m_graphEntryBarOpacityFx = new QGraphicsOpacityEffect(m_graphEntryBar);
  m_graphEntryBarOpacityFx->setOpacity(alreadyVisible ? 1.0 : 0.0);
  m_graphEntryBar->setGraphicsEffect(m_graphEntryBarOpacityFx);
  m_graphEntryBarFadeAnim->setTargetObject(m_graphEntryBarOpacityFx);
  if (!alreadyVisible)
    m_graphEntryBar->show();
  m_graphEntryBar->raise();
  m_graphEntryBarFadeAnim->setStartValue(m_graphEntryBarOpacityFx->opacity());
  m_graphEntryBarFadeAnim->setEndValue(1.0);
  m_graphEntryBarFadeAnim->start();
}

void MultiPageNoteView::refreshGraphPanelForSelection() {
  if (!m_selectedGraphItem || !m_graphLegendDock) {
    hideGraphLegendQuick();
    m_graphPanelExplicitOpen = false;
    m_graphPanelTargetGraph = nullptr;
    return;
  }
  bindGraphChrome(m_selectedGraphItem);
  if (!m_graphPanelExplicitOpen) {
    hideGraphLegendQuick();
    return;
  }
  if (m_graphQuickPopupWanted && m_graphQuickPopup)
    m_graphQuickPopup->setAnchorScenePos(m_graphQuickAnchorScene);
  presentGraphLegendAnimated();
}

void MultiPageNoteView::bindGraphItemSignals(GraphCanvasItem *gi) {
  if (!gi)
    return;
  // data(9001) markiert "Signale bereits verdrahtet" — verhindert doppelte
  // connects, wenn ein Item sowohl hydratePageContent() als auch
  // syncGraphItemsToNote() durchläuft.
  if (static_cast<QGraphicsItem *>(gi)->data(9001).toBool())
    return;

  connect(gi, &GraphCanvasItem::graphChanged, this,
          [this]() { syncGraphItemsToNote(); });
  connect(gi, &GraphCanvasItem::functionTapped, this, [this, gi](int idx) {
    abandonGraphEntrySession();
    QSignalBlocker blocker(&scene_);
    scene_.clearSelection();
    gi->setSelected(true);
    m_selectedGraphItem = gi;
    m_graphPanelTargetGraph = gi;
    m_graphPanelExplicitOpen = true;
    m_graphQuickPopupWanted = false;
    hideGraphQuickPopup();
    gi->setSelectedFunction(idx);
    refreshGraphPanelForSelection();
  });
  connect(gi, &GraphCanvasItem::functionLongPressed, this,
          [this, gi](int idx) {
    abandonGraphEntrySession();
    QSignalBlocker blocker(&scene_);
    scene_.clearSelection();
    gi->setSelected(true);
    m_selectedGraphItem = gi;
    m_graphPanelTargetGraph = gi;
    m_graphPanelExplicitOpen = true;
    m_graphQuickPopupWanted = false;
    hideGraphQuickPopup();
    gi->setSelectedFunction(idx);
    refreshGraphPanelForSelection();
  });
  connect(gi, &GraphCanvasItem::plusTapped, this, [this, gi]() {
    QSignalBlocker blocker(&scene_);
    scene_.clearSelection();
    gi->setSelected(true);
    m_selectedGraphItem = gi;
    m_graphQuickPopupWanted = false;
    hideGraphQuickPopup();
    m_graphPanelTargetGraph = gi;
    m_graphPanelExplicitOpen = false; // Legend-Dock nicht automatisch öffnen
    openGraphFormulaZone(gi);
  });
  connect(gi, &GraphCanvasItem::plotBackgroundTapped, this,
          [this, gi](QPointF scenePos) {
    abandonGraphEntrySession();
    QSignalBlocker blocker(&scene_);
    scene_.clearSelection();
    gi->setSelected(true);
    m_selectedGraphItem = gi;
    m_graphPanelTargetGraph = gi;
    m_graphPanelExplicitOpen = true;
    m_graphQuickAnchorScene = scenePos;
    m_graphQuickPopupWanted = !gi->data().functions.isEmpty();
    if (!m_graphQuickPopupWanted)
      hideGraphQuickPopup();
    refreshGraphPanelForSelection();
  });
  connect(gi, &GraphCanvasItem::xAxisTapped, this,
          [this, gi](double dataX, QPointF scenePos) {
    if (m_tangentXPopup)
      m_tangentXPopup->present(gi, scenePos, dataX);
  });
  connect(gi, &GraphCanvasItem::graphGeometryTweaked, this, [this, gi]() {
    if ((m_graphPanelExplicitOpen && m_selectedGraphItem == gi) ||
        (m_graphEntryBarOpen && m_graphEntryTargetGraph == gi)) {
      syncGraphLegendLayout();
    }
  });

  gi->setData(9001, true);
}

void MultiPageNoteView::syncGraphItemsToNote() {
  if (!note_) return;
  if (m_syncingGraphs) return;
  m_syncingGraphs = true;
  for (auto& p : note_->pages) p.graphs.clear();

  const auto all = scene_.items(Qt::AscendingOrder);
  for (QGraphicsItem* item : all) {
    if (item->type() != GraphCanvasItem::Type) continue;
    auto* gi = static_cast<GraphCanvasItem*>(item);
    bindGraphItemSignals(gi);
    syncGraphPlusLayout(gi);
    QPointF sceneCenter = gi->sceneBoundingRect().center();
    int pIdx = pageAt(sceneCenter);
    if (pIdx < 0 || pIdx >= pageItems_.size()) continue;
    if (gi->parentItem() != pageItems_[pIdx]) {
      const QPointF sceneTopLeft = gi->sceneBoundingRect().topLeft();
      gi->setParentItem(pageItems_[pIdx]);
      gi->setPos(pageItems_[pIdx]->mapFromScene(sceneTopLeft));
    }
    GraphObject d = gi->toData();
    d.rect.moveTopLeft(gi->pos());
    note_->pages[pIdx].graphs.push_back(std::move(d));
  }
  updateGraphChromeIfVisible();
  if (onSaveRequested) onSaveRequested(note_);
  m_syncingGraphs = false;
}

// ============================================================================
// Inline Formula Zone (replaces the old popup GraphFormulaEntryBar)
// ============================================================================

void MultiPageNoteView::openGraphFormulaZone(GraphCanvasItem *gi) {
  if (!gi)
    return;

  // If there's already an active zone for this graph, just return
  if (m_activeFormulaZone && m_activeFormulaZone->parentItem() == gi)
    return;

  // Clean up any existing zone
  if (m_activeFormulaZone) {
    m_activeFormulaZone->deleteLater();
    m_activeFormulaZone = nullptr;
  }

  // Determine slot index = number of committed functions (zone goes after them)
  const int slotIdx = gi->data().functions.size();

  // Create the zone as a child of the graph (follows graph movement)
  auto *zone = new GraphFormulaZone(slotIdx, gi);
  m_activeFormulaZone = zone;

  // Update the "+" position so it moves below the zone
  gi->setCommittedFunctionCountForPlusLayout(slotIdx + 1);

  // ── Connect zone signals ───────────────────────────────────────────

  // Preview: add a dashed preview curve to the graph
  connect(zone, &GraphFormulaZone::expressionRecognized, this,
          [this, gi](const QString &expr) {
    if (!gi) return;
    const ParsedExpression parsed = MathExpressionParser::parseFunctionExpression(expr);
    if (!parsed.ok) return;

    auto fns = gi->data().functions;

    // Remove old preview if exists
    if (m_livePreviewIndex >= 0 && m_livePreviewIndex < fns.size()) {
      fns[m_livePreviewIndex].expression = parsed.normalizedInput;
    } else {
      GraphFunction previewFn;
      previewFn.expression = parsed.normalizedInput;
      previewFn.color = QColor(124, 92, 252, 120); // semi-transparent purple
      previewFn.visible = true;
      fns.push_back(previewFn);
      m_livePreviewIndex = fns.size() - 1;
    }
    gi->updateFunctions(fns, m_livePreviewIndex);
    syncGraphPlusLayout(gi);
  });

  // Manual commit: make the curve permanent
  connect(zone, &GraphFormulaZone::commitRequested, this,
          [this, gi](const QString &expr) {
    if (!gi) return;
    const ParsedExpression parsed = MathExpressionParser::parseFunctionExpression(expr);
    if (!parsed.ok) return;

    auto fns = gi->data().functions;
    const QColor finalColor = QColor(94, 92, 230);

    if (m_livePreviewIndex >= 0 && m_livePreviewIndex < fns.size()) {
      fns[m_livePreviewIndex].expression = parsed.normalizedInput;
      fns[m_livePreviewIndex].color = finalColor;
      int sel = m_livePreviewIndex;
      m_livePreviewIndex = -1;
      gi->updateFunctions(fns, sel);
    } else {
      GraphFunction fn;
      fn.expression = parsed.normalizedInput;
      fn.color = finalColor;
      fn.visible = true;
      fns.push_back(fn);
      gi->updateFunctions(fns, fns.size() - 1);
    }

    // Clean up the zone
    if (m_activeFormulaZone) {
      m_activeFormulaZone->deleteLater();
      m_activeFormulaZone = nullptr;
    }

    syncGraphPlusLayout(gi);
    if (m_graphPanelExplicitOpen) {
      bindGraphChrome(gi);
      syncGraphLegendLayout();
    }
    syncGraphItemsToNote();
  });

  // Zone cleared: remove preview curve
  connect(zone, &GraphFormulaZone::zoneCleared, this, [this, gi]() {
    if (!gi) return;
    if (m_livePreviewIndex >= 0) {
      auto fns = gi->data().functions;
      if (m_livePreviewIndex < fns.size()) {
        fns.removeAt(m_livePreviewIndex);
        int sel = -1;
        if (!fns.isEmpty())
          sel = qBound(0, gi->data().selectedFunction, fns.size() - 1);
        gi->updateFunctions(fns, sel);
      }
      m_livePreviewIndex = -1;
    }
    syncGraphPlusLayout(gi);
  });
}

// Removed captureStrokesInFormulaZones as logic is now inside commitPendingStrokeItemsToNote
