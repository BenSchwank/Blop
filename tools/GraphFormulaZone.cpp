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
        const QString expr = MathInkRecognizer::instance().recognize(img);
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
        QString expr = obj.value(QStringLiteral("expression")).toString().trimmed();
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

QString GraphFormulaZone::recognizeLocalCandidates() const
{
    if (m_strokes.isEmpty())
        return {};

    // ── Bounding box ────────────────────────────────────────────────
    QRectF bb;
    for (const auto &s : m_strokes)
        bb = bb.united(s.path.boundingRect());
    if (bb.isEmpty())
        return {};

    const qreal w     = bb.width();
    const qreal h     = bb.height();
    const qreal ratio = (h <= 1.0) ? 10.0 : (w / h); // width / height

    // ── Arc length and average curvature ────────────────────────────
    qreal totalCurv = 0.0;
    int   curvCount = 0;

    for (const auto &s : m_strokes) {
        const int n = s.path.elementCount();
        for (int i = 1; i < n - 1; ++i) {
            const QPointF a(s.path.elementAt(i-1).x, s.path.elementAt(i-1).y);
            const QPointF b(s.path.elementAt(i  ).x, s.path.elementAt(i  ).y);
            const QPointF c(s.path.elementAt(i+1).x, s.path.elementAt(i+1).y);
            const QLineF  ab(a, b), bc(b, c);
            if (ab.length() < 0.5 || bc.length() < 0.5) continue;
            qreal ang = std::abs(ab.angleTo(bc));
            if (ang > 180.0) ang = 360.0 - ang;
            totalCurv += ang;
            ++curvCount;
        }
    }
    const qreal avgCurv = (curvCount > 0) ? (totalCurv / curvCount) : 0.0;

    // ── Superscript detection ────────────────────────────────────────
    // When the user writes "f(x) = x^2", the exponent "2" sits in the
    // upper third of the bounding box while the base strokes are in the
    // lower two-thirds.  Detect this pattern to reliably recognise x^n
    // regardless of how many helper characters (f, (, ), =) were written.
    const qreal topThird  = bb.top()  + h / 3.0;
    const qreal botThird  = bb.top()  + h * 2.0 / 3.0;
    int superscriptStrokes = 0; // small strokes in the top third
    int baselineStrokes    = 0; // strokes whose centre is in lower two-thirds

    for (const auto &s : m_strokes) {
        const QRectF sb   = s.path.boundingRect();
        const qreal  cy   = sb.center().y();
        const qreal  sh   = sb.height();
        // A superscript stroke is small AND sits high up
        if (cy < topThird && sh < h * 0.45)
            ++superscriptStrokes;
        if (cy > botThird * 0.6) // centre in lower portion
            ++baselineStrokes;
    }
    const bool likelySuperscript = (superscriptStrokes >= 1)
                                && (baselineStrokes    >= 2)
                                && (h > 25.0);

    const int nStr = m_strokes.size();

    // ── Rule-based classification ────────────────────────────────────

    // Superscript pattern detected (e.g. "x^2", "f(x)=x^2", "x^3") → always x^n
    if (likelySuperscript) {
        // If there is a clear intercept hint (many base strokes) → x^2+1
        if (baselineStrokes >= 5)
            return QStringLiteral("x^2+1");
        return QStringLiteral("x^2");
    }

    // Single stroke
    if (nStr == 1) {
        if (avgCurv > 9.0 && ratio > 2.0)
            return QStringLiteral("sin(x)");   // wide, wavy
        if (avgCurv > 5.0)
            return QStringLiteral("x^2");      // curved → parabola
        if (avgCurv < 2.5 && ratio > 2.5)
            return QStringLiteral("x");        // straight & wide → identity
        return QStringLiteral("2*x");          // diagonal line
    }

    // Two strokes
    if (nStr == 2) {
        if (avgCurv > 7.0)
            return QStringLiteral("x^2+1");
        if (ratio > 1.5)
            return QStringLiteral("x+1");
        return QStringLiteral("x^2");
    }

    // Three strokes
    if (nStr == 3) {
        if (avgCurv > 6.0)
            return QStringLiteral("x^2+1");
        if (ratio > 2.0)
            return QStringLiteral("2*x+1");
        return QStringLiteral("x^2+1");
    }

    // Four+ strokes without detected superscript
    if (nStr >= 4) {
        if (avgCurv > 5.0)
            return QStringLiteral("x^2+1");
        return QStringLiteral("x+1");
    }

    return QStringLiteral("x");
}
