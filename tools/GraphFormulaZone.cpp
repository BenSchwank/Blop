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
#include <QDebug>
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
    const qreal y = 8.0 + static_cast<qreal>(slotIndex) * kMinHeight;
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
// Helper to resize around strokes
void GraphFormulaZone::updateSize()
{
    qreal newW = kMinWidth;
    qreal newH = kMinHeight;
    qreal minY = 0.0;
    qreal maxY = 0.0;

    if (!m_strokes.isEmpty()) {
        QRectF bb;
        for (const auto &s : m_strokes)
            bb = bb.united(s.path.boundingRect());
            
        newW = qMax(newW, bb.right() + 45.0);
        minY = bb.top();
        maxY = bb.bottom();
    }

    if (minY < -10.0 || maxY > kMinHeight + 10.0) {
        qreal topPadding = qMax(0.0, -minY + 20.0);
        qreal bottomPadding = qMax(0.0, maxY - kMinHeight + 20.0);
        newH = kMinHeight + topPadding + bottomPadding;
        m_baselineY = 36.0 + topPadding;
    } else {
        m_baselineY = 36.0;
        newH = kMinHeight;
    }

    if (newW != m_currentWidth || newH != m_currentHeight) {
        prepareGeometryChange();
        m_currentWidth = newW;
        m_currentHeight = newH;
    }
}

QRectF GraphFormulaZone::boundingRect() const
{
    return QRectF(0, 0, m_currentWidth, m_currentHeight);
}

QRectF GraphFormulaZone::catchAreaSceneRect() const
{
    return mapRectToScene(boundingRect());
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
        p->drawLine(QPointF(4, m_baselineY), QPointF(m_currentWidth - 4, m_baselineY));
    }

    // ── Draw captured strokes (they live in local coords, move with graph) ──
    for (const auto &s : m_strokes) {
        QPen pen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawPath(s.path);
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
        p->drawText(QRectF(4, m_baselineY + 3, m_currentWidth - 24, 14),
                    Qt::AlignLeft | Qt::AlignTop,
                    m_recognizedExpr);
    }

    // ── Checkmark (Commit) button ───────────────────────────────────────────
    if (!m_recognizedExpr.isEmpty() || m_previewActive) {
        const qreal margin = 6.0;
        const qreal bxClear = m_currentWidth - kClearBtnSize - 4;
        const qreal bxCommit = bxClear - kCommitBtnSize - margin;
        const qreal byCommit = m_baselineY - 2; 
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
        const qreal byClear = m_baselineY + 2;
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

bool GraphFormulaZone::hitClearButton(const QPointF &scenePos) const
{
    if (!hasStrokes() && m_recognizedExpr.isEmpty())
        return false;
    const qreal bxClear = m_currentWidth - kClearBtnSize - 4;
    const qreal byClear = m_baselineY + 2;
    const QRectF btn(bxClear, byClear, kClearBtnSize, kClearBtnSize);
    return mapRectToScene(btn.adjusted(-5, -5, 5, 5)).contains(scenePos);
}

bool GraphFormulaZone::hitCommitButton(const QPointF &scenePos) const
{
    if (m_recognizedExpr.isEmpty() && !m_previewActive)
        return false;
    const qreal margin = 6.0;
    const qreal bxClear = m_currentWidth - kClearBtnSize - 4;
    const qreal bxCommit = bxClear - kCommitBtnSize - margin;
    const qreal byCommit = m_baselineY - 2; 
    const QRectF commitBtn(bxCommit, byCommit, kCommitBtnSize, kCommitBtnSize);
    return mapRectToScene(commitBtn.adjusted(-5, -5, 5, 5)).contains(scenePos);
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

    updateSize();

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

    updateSize();

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
        updateSize();

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
    updateSize();

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

    qDebug() << "[GraphFormulaZone] onRecognizeTimer: strokes=" << m_strokes.size();

    const QImage img = renderStrokesToImage();
    if (img.isNull()) {
        qDebug() << "[GraphFormulaZone] renderStrokesToImage returned null";
        return;
    }

    // ── Phase 1: Offline (ONNX) ──────────────────────────────────────
#ifdef BLOP_HAS_ONNX_OCR
    if (MathInkRecognizer::instance().isAvailable()) {
        const QString raw  = MathInkRecognizer::instance().recognize(img);
        const QString expr = LatexToBlopConverter::stripFunctionPrefix(raw);
        if (!expr.isEmpty()) {
            qDebug() << "[GraphFormulaZone] ONNX recognized:" << expr;
            m_recognizedExpr = expr;
            m_previewActive = true;
            update();
            emit expressionRecognized(expr);
            return;
        }
    }
#endif

    // ── Phase 2: Backend fallback ────────────────────────────────────
    qDebug() << "[GraphFormulaZone] Sending to backend for recognition...";
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
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    // No local server on mobile — go straight to local heuristic
    const QString defaultEndpoint;
#else
    const QString defaultEndpoint = QStringLiteral("http://127.0.0.1:8000/api/ai/math-ink/recognize");
#endif
    const QString endpoint = qEnvironmentVariable("BLOP_MATH_OCR_URL", defaultEndpoint);

    if (endpoint.isEmpty())
        return;

    qDebug() << "[GraphFormulaZone] Backend endpoint:" << endpoint;

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

    // Send strokes as coordinate arrays
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

    // Also send normalized strokes (0..1000 range) for better recognition
    QRectF bb;
    for (const auto &s : m_strokes)
        bb = bb.united(s.path.boundingRect());
    if (!bb.isEmpty()) {
        const qreal scaleX = 1000.0 / qMax(1.0, bb.width());
        const qreal scaleY = 1000.0 / qMax(1.0, bb.height());
        const qreal scale = qMin(scaleX, scaleY);
        QJsonArray normArr;
        for (const auto &s : m_strokes) {
            QJsonArray pts;
            for (int i = 0; i < s.path.elementCount(); ++i) {
                const auto e = s.path.elementAt(i);
                pts.append(QJsonArray{
                    (e.x - bb.left()) * scale,
                    (e.y - bb.top()) * scale
                });
            }
            normArr.append(pts);
        }
        payload.insert(QStringLiteral("strokes_normalized"), normArr);
    }

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
            qDebug() << "[GraphFormulaZone] Backend error:" << rep->errorString();
            rep->deleteLater();
            QString fb = recognizeLocalCandidates();
            if (!fb.isEmpty()) {
                qDebug() << "[GraphFormulaZone] Using local fallback:" << fb;
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
        QString expr = LatexToBlopConverter::stripFunctionPrefix(
            obj.value(QStringLiteral("expression")).toString().trimmed());
        qDebug() << "[GraphFormulaZone] Backend recognized:" << expr;
        if (expr.isEmpty()) {
            expr = recognizeLocalCandidates();
            qDebug() << "[GraphFormulaZone] Backend returned empty, local fallback:" << expr;
        }
        if (expr.isEmpty())
            return;

        m_recognizedExpr = expr;
        m_previewActive = true;
        update();
        emit expressionRecognized(expr);
    });
}

// ── Helpers for stroke-level structural analysis ────────────────────────

namespace {

/// Average curvature of a single stroke (degrees per sample point).
qreal strokeCurvature(const QPainterPath &p)
{
    qreal sum = 0.0;
    int   cnt = 0;
    const int n = p.elementCount();
    for (int i = 1; i < n - 1; ++i) {
        const QPointF a(p.elementAt(i-1).x, p.elementAt(i-1).y);
        const QPointF b(p.elementAt(i  ).x, p.elementAt(i  ).y);
        const QPointF c(p.elementAt(i+1).x, p.elementAt(i+1).y);
        const QLineF ab(a, b), bc(b, c);
        if (ab.length() < 0.5 || bc.length() < 0.5) continue;
        qreal ang = std::abs(ab.angleTo(bc));
        if (ang > 180.0) ang = 360.0 - ang;
        sum += ang;
        ++cnt;
    }
    return cnt > 0 ? sum / cnt : 0.0;
}

/// True when a stroke looks "roughly horizontal" — low height, low curvature.
bool isHorizontalStroke(const QPainterPath &p, qreal maxAR = 0.35)
{
    const QRectF r = p.boundingRect();
    if (r.width() < 3.0) return false;
    return (r.height() / r.width()) < maxAR && strokeCurvature(p) < 4.0;
}

} // anonymous namespace

// ──────────────────────────────────────────────────────────────────────────

QString GraphFormulaZone::recognizeLocalCandidates() const
{
    if (m_strokes.isEmpty())
        return {};

    // ── Full bounding box ───────────────────────────────────────────
    QRectF bb;
    for (const auto &s : m_strokes)
        bb = bb.united(s.path.boundingRect());
    if (bb.isEmpty())
        return {};

    const qreal fullH = bb.height();
    const qreal fullW = bb.width();

    // ══════════════════════════════════════════════════════════════════
    //  STEP 1 — Detect the "=" sign
    //  Two short, roughly horizontal strokes that are vertically
    //  stacked and horizontally overlapping.
    // ══════════════════════════════════════════════════════════════════
    qreal equalsRightEdge = -1e9;

    for (int i = 0; i < m_strokes.size(); ++i) {
        const QRectF r1 = m_strokes[i].path.boundingRect();
        if (!isHorizontalStroke(m_strokes[i].path)) continue;
        if (r1.width() > fullW * 0.35) continue; // "=" bars aren't very wide

        for (int j = i + 1; j < m_strokes.size(); ++j) {
            const QRectF r2 = m_strokes[j].path.boundingRect();
            if (!isHorizontalStroke(m_strokes[j].path)) continue;
            if (r2.width() > fullW * 0.35) continue;

            // Horizontally overlapping?
            const qreal overlap = qMin(r1.right(), r2.right())
                                - qMax(r1.left(),  r2.left());
            if (overlap < qMin(r1.width(), r2.width()) * 0.4) continue;

            // Vertically close but not the same line
            const qreal vGap = std::abs(r1.center().y() - r2.center().y());
            if (vGap < 3.0 || vGap > fullH * 0.45) continue;

            // Widths reasonably similar
            const qreal wRatio = qMin(r1.width(), r2.width())
                               / qMax(r1.width(), r2.width());
            if (wRatio < 0.4) continue;

            // This pair looks like "=".
            equalsRightEdge = qMax(r1.right(), r2.right());
            break;
        }
        if (equalsRightEdge > -1e8) break;
    }

    // ══════════════════════════════════════════════════════════════════
    //  STEP 2 — Collect only the EXPRESSION strokes (after "=")
    //  Everything before / including "=" is the prefix and is ignored.
    // ══════════════════════════════════════════════════════════════════
    QVector<const InkStroke*> expr;
    QRectF eBB;

    if (equalsRightEdge > -1e8) {
        const qreal threshold = equalsRightEdge + 4.0;   // small gap
        for (const auto &s : m_strokes) {
            if (s.path.boundingRect().center().x() > threshold) {
                expr.append(&s);
                eBB = eBB.united(s.path.boundingRect());
            }
        }
    }

    // Fallback: if no "=" found, use all strokes as the expression
    if (expr.isEmpty()) {
        for (const auto &s : m_strokes) {
            expr.append(&s);
        }
        eBB = bb;
    }

    if (expr.isEmpty())
        return QStringLiteral("x");

    const qreal eH = eBB.height();
    const qreal eW = eBB.width();

    // ══════════════════════════════════════════════════════════════════
    //  STEP 3 — Structural analysis of the EXPRESSION strokes
    // ══════════════════════════════════════════════════════════════════

    // ── 3a. Superscript detection ────────────────────────────────────
    //   A small stroke whose centre is in the top 40 % of the expression
    //   bounding box AND whose height is < 50 % of expression height.
    int superscriptCount = 0;
    int baseCount        = 0;
    const qreal supThreshold = eBB.top() + eH * 0.40;

    for (const auto *s : expr) {
        const QRectF sb = s->path.boundingRect();
        const bool isSmall = sb.height() < eH * 0.50
                          && sb.width()  < eW * 0.40;
        if (isSmall && sb.center().y() < supThreshold) {
            ++superscriptCount;
        } else {
            ++baseCount;
        }
    }

    // ── 3b. Plus-sign detection ──────────────────────────────────────
    //   Two short strokes that cross near their centres, one mostly
    //   horizontal, one mostly vertical.
    bool hasPlus = false;
    for (int i = 0; i < expr.size() && !hasPlus; ++i) {
        const QRectF ri = expr[i]->path.boundingRect();
        if (ri.width() < 3.0 && ri.height() < 3.0) continue;
        for (int j = i + 1; j < expr.size(); ++j) {
            const QRectF rj = expr[j]->path.boundingRect();
            if (!ri.intersects(rj)) continue;

            // One wider than tall, the other taller than wide?
            const bool iH = ri.width() > ri.height();
            const bool jH = rj.width() > rj.height();
            if (iH == jH) continue;   // both same orientation → not a +

            // Both should be short relative to the expression
            if (ri.width() > eW * 0.35 || rj.width() > eW * 0.35) continue;
            if (ri.height() > eH * 0.55 || rj.height() > eH * 0.55) continue;

            hasPlus = true;
            break;
        }
    }

    // ── 3c. Minus-sign detection ─────────────────────────────────────
    //   A single short horizontal stroke in the middle-height band
    //   that is NOT part of an "=" pair (already filtered).
    bool hasMinus = false;
    for (const auto *s : expr) {
        if (!isHorizontalStroke(s->path, 0.30)) continue;
        const QRectF sb = s->path.boundingRect();
        if (sb.width() > eW * 0.35) continue; // too wide → probably a fraction line
        const qreal cy = sb.center().y();
        if (cy > eBB.top() + eH * 0.25 && cy < eBB.top() + eH * 0.75) {
            hasMinus = true;
            break;
        }
    }

    // ── 3d. Fraction line detection ──────────────────────────────────
    //   A long horizontal stroke spanning most of the expression width.
    bool hasFraction = false;
    for (const auto *s : expr) {
        if (!isHorizontalStroke(s->path, 0.20)) continue;
        if (s->path.boundingRect().width() > eW * 0.50) {
            hasFraction = true;
            break;
        }
    }

    // ── 3e. Average curvature of expression strokes ──────────────────
    qreal totalCurv = 0.0;
    int   curvN     = 0;
    for (const auto *s : expr) {
        const qreal c = strokeCurvature(s->path);
        if (c > 0.0) { totalCurv += c; ++curvN; }
    }
    const qreal avgCurv = curvN > 0 ? totalCurv / curvN : 0.0;

    // ══════════════════════════════════════════════════════════════════
    //  STEP 4 — Build the expression from detected features
    // ══════════════════════════════════════════════════════════════════

    // Fraction → 1/x
    if (hasFraction && !superscriptCount) {
        return QStringLiteral("1/x");
    }

    // Superscript → x^2 variant
    if (superscriptCount >= 1) {
        if (hasPlus)  return QStringLiteral("x^2+1");
        if (hasMinus) return QStringLiteral("x^2-1");
        return QStringLiteral("x^2");
    }

    // High curvature + wide → sin/cos
    if (avgCurv > 8.0 && eW / qMax(1.0, eH) > 1.5)
        return QStringLiteral("sin(x)");

    // Moderate curvature → x^2
    if (avgCurv > 5.5)
        return QStringLiteral("x^2");

    // Plus → x+1
    if (hasPlus) return QStringLiteral("x+1");

    // Minus → x-1
    if (hasMinus) return QStringLiteral("x-1");

    // Very few strokes, low curvature → simple linear
    if (baseCount <= 2 && avgCurv < 3.0)
        return QStringLiteral("x");

    return QStringLiteral("2*x");
}
