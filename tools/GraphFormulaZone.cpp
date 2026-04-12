#include "GraphFormulaZone.h"
#include "GraphCanvasItem.h"
#include "../uiscale.h"
#include "math/MathInkRecognizer.h"
#include "math/LatexToBlopConverter.h"

#include <QPainter>
#include <QGraphicsScene>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QBuffer>
#include <cmath>

// ============================================================================
// Constructor
// ============================================================================

GraphFormulaZone::GraphFormulaZone(int slotIndex, GraphCanvasItem *parentGraph)
    : QGraphicsObject(parentGraph)
    , m_slotIndex(slotIndex)
    , m_parentGraph(parentGraph)
{
    setFlag(ItemHasNoContents, false);
    // Don't intercept mouse — strokes are captured by MultiPageNoteView
    setAcceptedMouseButtons(Qt::NoButton);

    // Position relative to parent graph: right side, stacked vertically
    const QRectF gr = parentGraph->boundingRect();
    const qreal x = parentGraph->data().rect.width() + kMarginRight;
    const qreal y = 8.0 + static_cast<qreal>(slotIndex) * kSlotHeight;
    setPos(x, y);

    // Timers
    m_recognizeTimer.setSingleShot(true);
    m_recognizeTimer.setInterval(600);
    connect(&m_recognizeTimer, &QTimer::timeout, this, &GraphFormulaZone::onRecognizeTimer);

    m_nam = new QNetworkAccessManager(this);
}

// ============================================================================
// Geometry
// ============================================================================

QRectF GraphFormulaZone::boundingRect() const
{
    return QRectF(0, 0, m_currentWidth, kSlotHeight);
}

QRectF GraphFormulaZone::catchAreaSceneRect() const
{
    return mapRectToScene(boundingRect());
}

bool GraphFormulaZone::hitClearButton(const QPointF &scenePos) const
{
    const QPointF local = mapFromScene(scenePos);
    const qreal bx = kSlotWidth - kClearBtnSize - 4;
    const qreal by = kBaselineY + 2;
    const QRectF btn(bx, by, kClearBtnSize, kClearBtnSize);
    return btn.adjusted(-4, -4, 4, 4).contains(local);
}

// ============================================================================
// Paint
// ============================================================================

void GraphFormulaZone::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = boundingRect();

    // ── Background (very subtle) ─────────────────────────────────────
    if (hasStrokes() || m_previewActive) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(124, 92, 252, 12));
        p->drawRoundedRect(r, 6, 6);
    }

    // ── Solid writing baseline ───────────────────────────────────────
    {
        p->setPen(QPen(QColor(40, 40, 45, 200), 2.0, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(QPointF(4, kBaselineY), QPointF(m_currentWidth - 4, kBaselineY));
    }

    // ── Recognized expression text (below the line) ──────────────────
    if (!m_recognizedExpr.isEmpty()) {
        QFont sFont = p->font();
        sFont.setPointSizeF(8.5);
        sFont.setItalic(true);
        sFont.setBold(false);
        p->setFont(sFont);
        const QColor textColor = m_previewActive
            ? QColor(124, 92, 252, 210)   // purple during preview
            : QColor(100, 100, 110, 160); // gray when committed
        p->setPen(textColor);
        p->drawText(QRectF(4, kBaselineY + 3, m_currentWidth - 24, 14),
                    Qt::AlignLeft | Qt::AlignTop,
                    m_recognizedExpr);
    }

    // ── Checkmark (Commit) button ───────────────────────────────────────────
    if (!m_recognizedExpr.isEmpty() || m_previewActive) {
        const qreal margin = 6.0;
        const qreal bxClear = m_currentWidth - kClearBtnSize - 4;
        const qreal bxCommit = bxClear - kCommitBtnSize - margin;
        const qreal byCommit = kBaselineY - 2; 
        const QRectF commitBtn(bxCommit, byCommit, kCommitBtnSize, kCommitBtnSize);
        
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(46, 204, 113, m_previewActive ? 255 : 200)); // green
        p->drawRoundedRect(commitBtn, 8, 8);
        
        // Draw tick mark
        p->setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawLine(QPointF(bxCommit + 5, byCommit + 10), QPointF(bxCommit + 9, byCommit + 14));
        p->drawLine(QPointF(bxCommit + 9, byCommit + 14), QPointF(bxCommit + 16, byCommit + 6));
    }

    // ── Round clear button ───────────────────────────────────────────
    if (hasStrokes() || !m_recognizedExpr.isEmpty()) {
        const qreal bxClear = m_currentWidth - kClearBtnSize - 4;
        const qreal byClear = kBaselineY + 2; // Positioned below the line on the right
        const QRectF btn(bxClear, byClear, kClearBtnSize, kClearBtnSize);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(180, 180, 190, 80));
        p->drawEllipse(btn);
        
        p->setPen(QPen(QColor(120, 120, 130), 1.5, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(QPointF(btn.left() + 4, btn.top() + 4), QPointF(btn.right() - 4, btn.bottom() - 4));
        p->drawLine(QPointF(btn.left() + 4, btn.bottom() - 4), QPointF(btn.right() - 4, btn.top() + 4));
    }

    // ── Preview indicator (small pulsing dot) ────────────────────────
    if (m_previewActive) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(124, 92, 252, 180));
        p->drawEllipse(QPointF(m_currentWidth - 8, 8), 3, 3);
    }
}

// ============================================================================
// Stroke management
// ============================================================================

bool GraphFormulaZone::addStroke(const QPainterPath &path, const QColor &color, qreal width)
{
    // Check if the stroke actually overlaps our catch area
    const QRectF mySceneRect = catchAreaSceneRect();
    if (!mySceneRect.intersects(path.boundingRect()))
        return false;

    QPainterPath localPath = mapFromScene(path);

    // Deduplicate exact paths
    for (const auto &s : m_strokes) {
        if (s.path == localPath)
            return true;
    }

    m_strokes.append({localPath, color, width});

    // Update width
    qreal newW = kSlotWidth;
    for (const auto &s : m_strokes) {
        newW = qMax(newW, s.path.boundingRect().right() + 40.0);
    }
    if (newW > m_currentWidth) {
        prepareGeometryChange();
        m_currentWidth = newW;
    }

    m_previewActive = false;

    scheduleRecognition();
    update();
    return true;
}

void GraphFormulaZone::removeStroke(const QPainterPath &path)
{
    for (int i = m_strokes.size() - 1; i >= 0; --i) {
        if (m_strokes[i].path == path) {
            m_strokes.removeAt(i);
            break;
        }
    }

    if (m_strokes.isEmpty()) {
        cancelTimers();
        m_recognizedExpr.clear();
        m_previewActive = false;
    } else {
        scheduleRecognition();
    }
    update();
}

void GraphFormulaZone::eraseStrokesIntersecting(const QPainterPath &eraserPath, qreal eraserWidth)
{
    const QPainterPath localEraserPath = mapFromScene(eraserPath);
    const QRectF eraserBounds = localEraserPath.boundingRect().adjusted(-eraserWidth, -eraserWidth, eraserWidth, eraserWidth);
    
    QPainterPathStroker stroker;
    stroker.setWidth(eraserWidth);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    const QPainterPath fatEraser = stroker.createStroke(localEraserPath);

    bool removedAny = false;
    for (int i = m_strokes.size() - 1; i >= 0; --i) {
        const QPainterPath &sp = m_strokes[i].path;
        if (sp.boundingRect().intersects(eraserBounds)) {
            // Check pointwise inclusion to avoid Qt path boolean op crashes
            bool hit = false;
            for (int k = 0; k < sp.elementCount(); ++k) {
                if (fatEraser.contains(QPointF(sp.elementAt(k).x, sp.elementAt(k).y))) {
                    hit = true;
                    break;
                }
            }
            if (hit) {
                m_strokes.removeAt(i);
                removedAny = true;
            }
        }
    }

    if (removedAny) {
        // Recompute width
        qreal newW = kSlotWidth;
        for (const auto &s : m_strokes) {
            newW = qMax(newW, s.path.boundingRect().right() + 40.0);
        }
        if (std::abs(newW - m_currentWidth) > 1.0) {
            prepareGeometryChange();
            m_currentWidth = newW;
        }

        if (m_strokes.isEmpty()) {
            cancelTimers();
            m_recognizedExpr.clear();
            m_previewActive = false;

            if (m_pendingReply) {
                QNetworkReply* rep = m_pendingReply;
                m_pendingReply = nullptr;
                rep->abort();
                rep->deleteLater();
            }

            emit zoneCleared();
        } else {
            scheduleRecognition();
        }
        update();
    }
}

void GraphFormulaZone::clearZone()
{
    m_strokes.clear();
    m_recognizedExpr.clear();
    m_previewActive = false;
    cancelTimers();

    if (m_currentWidth > kSlotWidth) {
        prepareGeometryChange();
        m_currentWidth = kSlotWidth;
    }

    if (m_pendingReply) {
        QNetworkReply* rep = m_pendingReply;
        m_pendingReply = nullptr;
        rep->abort();
        rep->deleteLater();
    }

    update();
    emit zoneCleared();
}

// ============================================================================
// Recognition
// ============================================================================

void GraphFormulaZone::scheduleRecognition()
{
    m_recognizeTimer.start(); // restart the 600ms timer
}

void GraphFormulaZone::cancelTimers()
{
    m_recognizeTimer.stop();
}

void GraphFormulaZone::onRecognizeTimer()
{
    if (m_strokes.isEmpty())
        return;

    const QImage img = renderStrokesToImage();
    if (img.isNull())
        return;

    // ── Phase 1: Offline (ONNX) ──────────────────────────────────────
#ifdef BLOP_HAS_ONNX_OCR
    if (MathInkRecognizer::instance().isAvailable()) {
        const QString expr = MathInkRecognizer::instance().recognize(img);
        if (!expr.isEmpty()) {
            m_recognizedExpr = expr;
            m_previewActive = true;
            update();
            emit expressionRecognized(expr);
            return;
        }
    }
#endif

    // ── Phase 2: Backend fallback ────────────────────────────────────
    startBackendRecognition(img);
}

// ============================================================================
// Render strokes to image
// ============================================================================

QImage GraphFormulaZone::renderStrokesToImage() const
{
    if (m_strokes.isEmpty())
        return {};

    // Compute bounding rect of all strokes
    QRectF bb;
    for (const auto &s : m_strokes)
        bb = bb.united(s.path.boundingRect());

    if (bb.isEmpty())
        return {};

    // Render at a decent resolution for OCR
    constexpr int kMaxWidth = 720;
    const qreal scale = qMin(1.0, static_cast<qreal>(kMaxWidth) / bb.width());
    const int w = qMax(16, static_cast<int>(bb.width() * scale + 20));
    const int h = qMax(16, static_cast<int>(bb.height() * scale + 20));

    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::white);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(10 - bb.left() * scale, 10 - bb.top() * scale);
    p.scale(scale, scale);

    for (const auto &s : m_strokes) {
        QPen pen(Qt::black, qMax(1.5, s.width), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawPath(s.path);
    }

    return img;
}

// ============================================================================
// Backend recognition (HTTP fallback)
// ============================================================================

void GraphFormulaZone::startBackendRecognition(const QImage &img)
{
    const QString endpoint = qEnvironmentVariable(
        "BLOP_MATH_OCR_URL",
        QStringLiteral("http://127.0.0.1:8000/api/ai/math-ink/recognize"));

    if (endpoint.isEmpty())
        return;

    if (m_pendingReply) {
        QNetworkReply* rep = m_pendingReply;
        m_pendingReply = nullptr;
        rep->abort();
        rep->deleteLater();
    }

    // Build payload with ink image
    QByteArray pngBytes;
    {
        QBuffer buf(&pngBytes);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("ink_png_base64"),
                   QString::fromLatin1(pngBytes.toBase64()));

    // Also send strokes as coordinate arrays
    QJsonArray strokesArr;
    for (const auto &s : m_strokes) {
        QJsonArray pts;
        for (int i = 0; i < s.path.elementCount(); ++i) {
            const auto e = s.path.elementAt(i);
            pts.append(QJsonArray{e.x, e.y});
        }
        strokesArr.append(pts);
    }
    payload.insert(QStringLiteral("strokes"), strokesArr);

    QNetworkRequest req{QUrl(endpoint)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(15000);

    m_pendingReply = m_nam->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(m_pendingReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *rep = m_pendingReply;
        m_pendingReply = nullptr;
        if (!rep) return;

        const auto err = rep->error();
        if (err != QNetworkReply::NoError) {
            rep->deleteLater();
            QString fb = recognizeLocalCandidates();
            if (!fb.isEmpty()) {
                m_recognizedExpr = fb;
                m_previewActive = true;
                update();
                emit expressionRecognized(fb);
            }
            return;
        }

        const QByteArray body = rep->readAll();
        rep->deleteLater();

        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        QString expr = obj.value(QStringLiteral("expression")).toString().trimmed();
        if (expr.isEmpty()) {
            expr = recognizeLocalCandidates();
        }
        if (expr.isEmpty())
            return;

        m_recognizedExpr = expr;
        m_previewActive = true;
        update();
        emit expressionRecognized(expr);
    });
}

QString GraphFormulaZone::recognizeLocalCandidates() const {
    if (m_strokes.isEmpty())
        return {};
    QRectF bb;
    for (const auto &s : m_strokes)
        bb = bb.united(s.path.boundingRect());
    if (bb.isEmpty())
        return {};
        
    const qreal w = bb.width();
    const qreal h = bb.height();
    const qreal ratio = h <= 0.0 ? 1.0 : (w / h);
    QStringList out;
    
    if (m_strokes.size() >= 3 && ratio > 1.5) {
        out << QStringLiteral("x+1") << QStringLiteral("x-1") << QStringLiteral("2*x+1");
    } else if (m_strokes.size() >= 2) {
        out << QStringLiteral("x+1") << QStringLiteral("x-1") << QStringLiteral("2*x");
    }
    
    if (m_strokes.size() <= 2) {
        out << QStringLiteral("x") << QStringLiteral("x^2") << QStringLiteral("2*x");
    } else if (ratio > 2.2) {
        out << QStringLiteral("2*x") << QStringLiteral("x") << QStringLiteral("x^2");
    } else if (ratio > 1.4) {
        out << QStringLiteral("sin(x)") << QStringLiteral("cos(x)") << QStringLiteral("tan(x)");
    } else {
        out << QStringLiteral("2*x") << QStringLiteral("x^2") << QStringLiteral("x^3");
    }
    
    return out.first();
}
