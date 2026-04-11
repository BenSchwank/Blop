#include "GraphCanvasItem.h"
#include "../uiscale.h"
#include "math/MathEvaluator.h"
#include "math/MathExpressionParser.h"
#include "math/NumericAnalysis.h"

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>
#include <QPainter>
#include <QFontMetricsF>
#include <cmath>

namespace {

constexpr qreal kCurveHitPx = 16.0;

double niceTickStep(double range, int targetDivisions) {
    if (range <= 0 || targetDivisions < 2)
        return 1.0;
    const double rough = range / static_cast<double>(targetDivisions - 1);
    const double p10 = std::pow(10.0, std::floor(std::log10(rough)));
    const double f = rough / p10;
    double nf = 10.0;
    if (f <= 1.0)
        nf = 1.0;
    else if (f <= 2.0)
        nf = 2.0;
    else if (f <= 5.0)
        nf = 5.0;
    return nf * p10;
}

QString formatAxisTick(double v) {
    const double av = std::abs(v);
    if (!std::isfinite(v))
        return QString();
    if (av >= 10000.0 || (av > 0.0 && av < 1e-4))
        return QString::number(v, 'g', 4);
    if (av < 0.01)
        return QString::number(v, 'f', 4);
    if (av < 1.0)
        return QString::number(v, 'g', 3);
    return QString::number(v, 'g', 4);
}

} // namespace

GraphCanvasItem::GraphCanvasItem(const QRectF& rect, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_rect(0, 0, qMax(80.0, rect.width()), qMax(60.0, rect.height())) {
    setPos(rect.topLeft());
    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    m_data.rect = m_rect;
    m_committedPlusCount = 0;
}

QVariant GraphCanvasItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    const QVariant v = QGraphicsObject::itemChange(change, value);
    if (change == ItemSelectedHasChanged)
        setZValue(value.toBool() ? 5.0 : 4.0);
    if (change == ItemPositionHasChanged && (flags() & ItemIsMovable))
        emit graphGeometryTweaked();
    return v;
}

QRectF GraphCanvasItem::boundingRect() const {
    QRectF b = m_rect.adjusted(-2, -2, 26, 14);
    if (!m_plusSuppressed)
        b = b.united(plusButtonHitLocalRect());
    return b;
}

QRectF GraphCanvasItem::plusButtonLocalRect() const {
    constexpr qreal kDy = 22.0;
    const qreal y = m_rect.top() + 8.0 + static_cast<qreal>(m_committedPlusCount) * kDy;
    return QRectF(m_rect.right() + 6.0, y, 16.0, 16.0);
}

QRectF GraphCanvasItem::plusButtonHitLocalRect() const {
    const qreal extra = qMax<qreal>(4.0, static_cast<qreal>(UiScale::dp(8)));
    return plusButtonLocalRect().adjusted(-6.0 - extra, -6.0 - extra, 6.0 + extra, 6.0 + extra);
}

QRectF GraphCanvasItem::plotAreaLocalRect() const {
    return m_rect.adjusted(10, 10, -10, -10);
}

bool GraphCanvasItem::hitXAxisAtLocal(const QPointF& localPos, double* outDataX) const {
    const QRectF pr = plotAreaLocalRect();
    if (!pr.contains(localPos))
        return false;
    if (!m_plusSuppressed && plusButtonHitLocalRect().contains(localPos))
        return false;
    const QRectF handle(m_rect.right() - 14, m_rect.bottom() - 14, 14, 14);
    if (handle.contains(localPos))
        return false;
    auto mapY = [&](double y) {
        return pr.bottom() - (y - m_data.yMin) / qMax(1e-6, (m_data.yMax - m_data.yMin)) * pr.height();
    };
    constexpr qreal kTol = 5.0;
    const qreal y0 = mapY(0.0);
    if (qAbs(localPos.y() - y0) > kTol)
        return false;
    if (outDataX) {
        *outDataX = m_data.xMin + (localPos.x() - pr.left()) / qMax(1e-6, pr.width()) *
            (m_data.xMax - m_data.xMin);
    }
    return true;
}

bool GraphCanvasItem::hitPlotBackgroundAtScene(const QPointF& scenePos) const {
    const QPointF lp = mapFromScene(scenePos);
    if (!plotAreaLocalRect().contains(lp))
        return false;
    if (!m_plusSuppressed && plusButtonHitLocalRect().contains(lp))
        return false;
    const QRectF handle(m_rect.right() - 14, m_rect.bottom() - 14, 14, 14);
    if (handle.contains(lp))
        return false;
    qreal dist = 0;
    const int idx = nearestFunctionAtScenePos(scenePos, &dist);
    if (idx >= 0 && dist < kCurveHitPx)
        return false;
    return true;
}

bool GraphCanvasItem::hitGraphChromeAtScene(const QPointF& scenePos) const {
    return boundingRect().contains(mapFromScene(scenePos));
}

void GraphCanvasItem::requestPlotBackgroundTap() {
    const QRectF pr = plotAreaLocalRect();
    emit plotBackgroundTapped(mapToScene(pr.center()));
}

void GraphCanvasItem::setCommittedFunctionCountForPlusLayout(int count) {
    count = qMax(0, count);
    if (m_committedPlusCount == count)
        return;
    prepareGeometryChange();
    m_committedPlusCount = count;
    update();
}

void GraphCanvasItem::setPlusButtonSuppressed(bool suppressed) {
    if (m_plusSuppressed == suppressed)
        return;
    prepareGeometryChange();
    m_plusSuppressed = suppressed;
    update();
}

QRectF GraphCanvasItem::plusButtonSceneRect() const {
    return mapRectToScene(plusButtonLocalRect().adjusted(-6, -6, 10, 10));
}

bool GraphCanvasItem::hitPlusButtonAtScene(const QPointF& scenePos) const {
    if (m_plusSuppressed)
        return false;
    return plusButtonHitLocalRect().contains(mapFromScene(scenePos));
}

void GraphCanvasItem::requestPlusTap() {
    emit plusTapped();
}

void GraphCanvasItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
    p->setRenderHint(QPainter::Antialiasing, true);
    if (isSelected()) {
        p->save();
        p->setPen(Qt::NoPen);
        const bool hasCurves = !m_data.functions.isEmpty();
        const int maxLayer = hasCurves ? 2 : 1;
        for (int layer = maxLayer; layer >= 1; --layer) {
            const qreal d = static_cast<qreal>(layer) * 1.25;
            const int alpha = hasCurves ? (24 - layer * 6) : 14;
            p->setBrush(QColor(76, 58, 168, alpha));
            p->drawRoundedRect(m_rect.translated(d, d), 8, 8);
        }
        p->setPen(QPen(QColor(107, 92, 230, 100), 2.0));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(m_rect.adjusted(-3, -3, 3, 3), 9, 9);
        p->restore();
    }
    p->setPen(QPen(QColor(98, 98, 104, 1), 1.2));
    p->setBrush(QColor(188, 190, 198, 120));
    p->drawRoundedRect(m_rect, 8, 8);

    const QRectF pr = m_rect.adjusted(10, 10, -10, -10);
    p->setBrush(QColor(248, 248, 249, 220));
    p->setPen(QPen(QColor(20, 20, 22), 1.1));
    p->drawRect(pr);

    auto mapX = [&](double x) { return pr.left() + (x - m_data.xMin) / qMax(1e-6, (m_data.xMax - m_data.xMin)) * pr.width(); };
    auto mapY = [&](double y) { return pr.bottom() - (y - m_data.yMin) / qMax(1e-6, (m_data.yMax - m_data.yMin)) * pr.height(); };

    const double xSpan = m_data.xMax - m_data.xMin;
    const double ySpan = m_data.yMax - m_data.yMin;
    const int xDivsAuto = qBound(3, int(pr.width() / 48.0), 12);
    const int yDivsAuto = qBound(3, int(pr.height() / 44.0), 10);
    double xStep = niceTickStep(xSpan, xDivsAuto);
    double yStep = niceTickStep(ySpan, yDivsAuto);
    if (m_data.xTickMode == 1 && m_data.xTickStep > 0.0)
        xStep = m_data.xTickStep;
    else if (m_data.xTickMode == 2) {
        const int c = qBound(2, m_data.xTickCount, 32);
        xStep = niceTickStep(xSpan, c);
    }
    if (m_data.yTickMode == 1 && m_data.yTickStep > 0.0)
        yStep = m_data.yTickStep;
    else if (m_data.yTickMode == 2) {
        const int c = qBound(2, m_data.yTickCount, 32);
        yStep = niceTickStep(ySpan, c);
    }

    p->setPen(QPen(QColor(72, 72, 77), 1));
    if (xStep > 0 && std::isfinite(xStep)) {
        const double xStart = std::ceil(m_data.xMin / xStep - 1e-12) * xStep;
        for (double xv = xStart; xv <= m_data.xMax + xStep * 1e-9; xv += xStep) {
            if (xv < m_data.xMin - 1e-12)
                continue;
            const qreal px = mapX(xv);
            if (px < pr.left() || px > pr.right())
                continue;
            if (std::abs(xv) > xStep * 1e-9)
                p->drawLine(QPointF(px, pr.top()), QPointF(px, pr.bottom()));
        }
    }
    if (yStep > 0 && std::isfinite(yStep)) {
        const double yStart = std::ceil(m_data.yMin / yStep - 1e-12) * yStep;
        for (double yv = yStart; yv <= m_data.yMax + yStep * 1e-9; yv += yStep) {
            if (yv < m_data.yMin - 1e-12)
                continue;
            const qreal py = mapY(yv);
            if (py < pr.top() || py > pr.bottom())
                continue;
            if (std::abs(yv) > yStep * 1e-9)
                p->drawLine(QPointF(pr.left(), py), QPointF(pr.right(), py));
        }
    }

    p->setPen(QPen(QColor(18, 18, 20), 1.1));
    p->drawRect(pr);

    const qreal x0 = mapX(0.0);
    const qreal y0 = mapY(0.0);
    p->setPen(QPen(QColor(8, 8, 8), 1.4));
    p->drawLine(QPointF(pr.left(), y0), QPointF(pr.right(), y0));
    p->drawLine(QPointF(x0, pr.top()), QPointF(x0, pr.bottom()));

    {
        QFont tickFont = p->font();
        tickFont.setPointSizeF(qMax(6.5, tickFont.pointSizeF() - 1.0));
        p->setFont(tickFont);
        QFontMetricsF fm(tickFont);
        constexpr qreal kTick = 5.0;
        p->setPen(QPen(QColor(12, 12, 12), 1));
        if (xStep > 0 && std::isfinite(xStep)) {
            const double xStart = std::ceil(m_data.xMin / xStep - 1e-12) * xStep;
            for (double xv = xStart; xv <= m_data.xMax + xStep * 1e-9; xv += xStep) {
                if (xv < m_data.xMin - 1e-12)
                    continue;
                const qreal px = mapX(xv);
                if (px < pr.left() || px > pr.right())
                    continue;
                p->drawLine(QPointF(px, pr.bottom()), QPointF(px, pr.bottom() + kTick));
                const QString txt = formatAxisTick(xv);
                const qreal tw = fm.horizontalAdvance(txt);
                qreal tx = px - tw * 0.5;
                tx = qBound(m_rect.left() + 2.0, tx, m_rect.right() - tw - 2.0);
                const qreal ty = pr.bottom() + kTick + 2.0;
                if (ty + fm.height() <= m_rect.bottom() - 1.0)
                    p->drawText(QPointF(tx, ty + fm.ascent()), txt);
            }
        }
        if (yStep > 0 && std::isfinite(yStep)) {
            const double yStart = std::ceil(m_data.yMin / yStep - 1e-12) * yStep;
            for (double yv = yStart; yv <= m_data.yMax + yStep * 1e-9; yv += yStep) {
                if (yv < m_data.yMin - 1e-12)
                    continue;
                const qreal py = mapY(yv);
                if (py < pr.top() || py > pr.bottom())
                    continue;
                p->drawLine(QPointF(pr.left() - kTick, py), QPointF(pr.left(), py));
                const QString txt = formatAxisTick(yv);
                const qreal tw = fm.horizontalAdvance(txt);
                qreal tx = pr.left() - kTick - tw - 3.0;
                tx = qBound(m_rect.left() + 2.0, tx, pr.left() - 2.0);
                const qreal th = fm.height();
                p->drawText(QRectF(tx, py - th * 0.5, tw, th), Qt::AlignRight | Qt::AlignVCenter, txt);
            }
        }
    }

    p->setPen(QPen(QColor(12, 12, 12), 1));
    p->drawText(QPointF(pr.right() - 12, qBound(pr.top() + 12.0, y0 - 5.0, pr.bottom() - 4.0)), QStringLiteral("x"));
    p->drawText(QPointF(qBound(pr.left() + 4.0, x0 + 4.0, pr.right() - 12.0), pr.top() + 12), QStringLiteral("y"));

    p->save();
    p->setClipRect(pr);

    QVector<double> selectedRoots;
    QVector<QPointF> selectedExtrema;
    for (int i = 0; i < m_data.functions.size(); ++i) {
        const auto& f = m_data.functions[i];
        if (!f.visible)
            continue;
        const ParsedExpression expr = MathExpressionParser::parseFunctionExpression(f.isDerivativeCurve ? f.sourceExpression : f.expression);
        if (!expr.ok)
            continue;
        const bool isActiveFn = (i == m_data.selectedFunction);
        QPainterPath path;
        bool started = false;
        constexpr int N = 260;
        for (int k = 0; k <= N; ++k) {
            const double x = m_data.xMin + (m_data.xMax - m_data.xMin) * (static_cast<double>(k) / N);
            const double y = f.isDerivativeCurve
                ? NumericAnalysis::derivativeCentral(expr, x)
                : MathEvaluator::evalAt(expr, x);
            if (!qIsFinite(y)) {
                started = false;
                continue;
            }
            const QPointF pt(mapX(x), mapY(y));
            if (!started) { path.moveTo(pt); started = true; }
            else path.lineTo(pt);
        }
        if (isActiveFn) {
            QColor glow = f.color;
            glow.setAlpha(100);
            p->setPen(QPen(glow, 7.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->drawPath(path);
        }
        p->setPen(QPen(f.color, isActiveFn ? 3.4 : 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(path);

        if (f.showDerivative) {
            p->setPen(QPen(f.color.lighter(145), 1.2, Qt::DashLine));
            QPainterPath dPath;
            bool dStarted = false;
            for (int k = 0; k <= N; ++k) {
                const double x = m_data.xMin + (m_data.xMax - m_data.xMin) * (static_cast<double>(k) / N);
                const double y = NumericAnalysis::derivativeCentral(expr, x);
                if (!qIsFinite(y)) {
                    dStarted = false;
                    continue;
                }
                const QPointF pt(mapX(x), mapY(y));
                if (!dStarted) { dPath.moveTo(pt); dStarted = true; }
                else dPath.lineTo(pt);
            }
            p->drawPath(dPath);
        }

        if (f.showTangent) {
            const double x0f = qBound(m_data.xMin, f.tangentX, m_data.xMax);
            const double y0f = MathEvaluator::evalAt(expr, x0f);
            const double mf = NumericAnalysis::derivativeCentral(expr, x0f);
            if (qIsFinite(y0f) && qIsFinite(mf)) {
                p->setPen(QPen(f.color.darker(110), 1.1, Qt::DashDotLine));
                QPainterPath tPath;
                bool tStarted = false;
                for (int k = 0; k <= N; ++k) {
                    const double x = m_data.xMin + (m_data.xMax - m_data.xMin) * (static_cast<double>(k) / N);
                    const double y = y0f + mf * (x - x0f);
                    if (!qIsFinite(y)) {
                        tStarted = false;
                        continue;
                    }
                    const QPointF pt(mapX(x), mapY(y));
                    if (!tStarted) { tPath.moveTo(pt); tStarted = true; }
                    else tPath.lineTo(pt);
                }
                p->drawPath(tPath);
                p->setPen(Qt::NoPen);
                p->setBrush(f.color.darker(120));
                p->drawEllipse(QPointF(mapX(x0f), mapY(y0f)), 2.7, 2.7);
            }
        }

        if (f.showRoots) {
            const QColor rootColor = f.rootMarkerColor;
            const QVector<double> roots = NumericAnalysis::findRootsBisection(expr, m_data.xMin, m_data.xMax, 300);
            p->setPen(Qt::NoPen);
            p->setBrush(rootColor);
            for (double rx : roots) {
                const QPointF c(mapX(rx), mapY(0.0));
                p->drawEllipse(c, 2.8, 2.8);
            }
            if (i == m_data.selectedFunction)
                selectedRoots = roots;
        }

        if (f.showExtrema) {
            p->setPen(Qt::NoPen);
            const QColor extColor = f.extremaMarkerColor;
            p->setBrush(extColor);
            const QVector<double> ex = NumericAnalysis::findExtrema(expr, m_data.xMin, m_data.xMax, 300);
            for (double exx : ex) {
                const double y = MathEvaluator::evalAt(expr, exx);
                if (!qIsFinite(y))
                    continue;
                p->drawEllipse(QPointF(mapX(exx), mapY(y)), 2.8, 2.8);
                if (i == m_data.selectedFunction)
                    selectedExtrema.push_back(QPointF(exx, y));
            }
        }
    }
    p->restore();

    if (m_data.selectedFunction >= 0 &&
        m_data.selectedFunction < m_data.functions.size()) {
        p->save();
        p->setClipRect(pr);
        p->setPen(QPen(QColor(36, 42, 56), 1));
        p->setBrush(QColor(246, 248, 252, 230));
        const QRectF legendRect(pr.right() - 108.0, pr.top() + 4.0, 104.0, 86.0);
        p->drawRoundedRect(legendRect, 6, 6);
        p->setPen(QPen(QColor(38, 48, 69), 1));
        const QColor rootColor = m_data.functions[m_data.selectedFunction].rootMarkerColor;
        const QColor extColor = m_data.functions[m_data.selectedFunction].extremaMarkerColor;
        p->setBrush(rootColor);
        p->drawEllipse(QPointF(legendRect.left() + 8, legendRect.top() + 16), 2.4, 2.4);
        p->setBrush(extColor);
        p->drawEllipse(QPointF(legendRect.left() + 8, legendRect.top() + 34), 2.4, 2.4);
        p->setPen(QPen(QColor(26, 32, 44), 1));
        p->drawText(QRectF(legendRect.left() + 14, legendRect.top() + 8, 82, 14),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    QStringLiteral("x0: %1").arg(selectedRoots.size()));
        p->drawText(QRectF(legendRect.left() + 14, legendRect.top() + 26, 82, 14),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    QStringLiteral("xe: %1").arg(selectedExtrema.size()));
        if (!selectedRoots.isEmpty()) {
            const QString rtxt =
                QStringLiteral("x0 = %1").arg(selectedRoots.first(), 0, 'g', 4);
            p->drawText(QRectF(legendRect.left() + 6, legendRect.top() + 44, 92, 14),
                        Qt::AlignLeft | Qt::AlignVCenter, rtxt);
        }
        if (!selectedExtrema.isEmpty()) {
            const QPointF ex = selectedExtrema.first();
            const QString etxt = QStringLiteral("xe = (%1, %2)")
                .arg(ex.x(), 0, 'g', 4)
                .arg(ex.y(), 0, 'g', 4);
            p->drawText(QRectF(legendRect.left() + 6, legendRect.top() + 62, 92, 14),
                        Qt::AlignLeft | Qt::AlignVCenter, etxt);
        }
        p->restore();
    }

    if (!m_plusSuppressed) {
        const QRectF plusRect = plusButtonLocalRect();
        p->setPen(QPen(QColor(53, 81, 129), 1.1));
        p->setBrush(QColor(220, 231, 249, 245));
        p->drawEllipse(plusRect);
        p->setPen(QPen(QColor(24, 45, 85), 1.6));
        const QPointF c = plusRect.center();
        p->drawLine(QPointF(c.x() - 4.0, c.y()), QPointF(c.x() + 4.0, c.y()));
        p->drawLine(QPointF(c.x(), c.y() - 4.0), QPointF(c.x(), c.y() + 4.0));
    }

    if (isSelected()) {
        p->setPen(QPen(QColor(75, 65, 210), 2.2, Qt::DashLine));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(m_rect.adjusted(-2, -2, 2, 2), 8, 8);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(75, 65, 210));
        p->drawRect(QRectF(m_rect.right() - 8, m_rect.bottom() - 8, 8, 8));
    }
}

GraphObject GraphCanvasItem::toData() const {
    GraphObject d = m_data;
    d.rect = QRectF(pos(), QSizeF(m_rect.width(), m_rect.height()));
    return d;
}

void GraphCanvasItem::fromData(const GraphObject& d) {
    prepareGeometryChange();
    m_data = d;
    setPos(d.rect.topLeft());
    m_rect = QRectF(0, 0, qMax(80.0, d.rect.width()), qMax(60.0, d.rect.height()));
    m_data.rect = m_rect;
    update();
}

void GraphCanvasItem::notifyGraphChanged() {
    emit graphChanged();
}

void GraphCanvasItem::setSelectedFunction(int idx) {
    if (m_data.functions.isEmpty()) {
        m_data.selectedFunction = -1;
    } else {
        m_data.selectedFunction = qBound(0, idx, m_data.functions.size() - 1);
    }
    update();
    emit graphChanged();
}

QPointF GraphCanvasItem::mapToLocalPlot(double x, double y) const {
    const QRectF pr = m_rect.adjusted(10, 10, -10, -10);
    const qreal px = pr.left() + (x - m_data.xMin) / qMax(1e-6, (m_data.xMax - m_data.xMin)) * pr.width();
    const qreal py = pr.bottom() - (y - m_data.yMin) / qMax(1e-6, (m_data.yMax - m_data.yMin)) * pr.height();
    return QPointF(px, py);
}

int GraphCanvasItem::nearestFunctionAtScenePos(const QPointF& scenePos, qreal* outDist) const {
    const QPointF lp = mapFromScene(scenePos);
    const QRectF pr = m_rect.adjusted(10, 10, -10, -10);
    static constexpr qreal kSampleDx[] = {0.0, -5.0, 5.0, -10.0, 10.0, -2.5, 2.5};
    const double xSpan = m_data.xMax - m_data.xMin;
    const qreal invW = 1.0 / qMax(1e-6, pr.width());

    qreal best = 1e9;
    int bestIdx = -1;
    for (int i = 0; i < m_data.functions.size(); ++i) {
        const auto& f = m_data.functions[i];
        if (!f.visible)
            continue;
        const ParsedExpression expr = MathExpressionParser::parseFunctionExpression(f.isDerivativeCurve ? f.sourceExpression : f.expression);
        if (!expr.ok)
            continue;
        qreal fnBest = 1e9;
        for (qreal dx : kSampleDx) {
            const qreal lx = lp.x() + dx;
            if (lx < pr.left() || lx > pr.right())
                continue;
            const double x = m_data.xMin + (lx - pr.left()) * invW * xSpan;
            const double y = f.isDerivativeCurve
                ? NumericAnalysis::derivativeCentral(expr, x)
                : MathEvaluator::evalAt(expr, x);
            if (!qIsFinite(y))
                continue;
            const QPointF onCurve = mapToLocalPlot(x, y);
            const qreal d = QLineF(lp, onCurve).length();
            if (d < fnBest)
                fnBest = d;
        }
        if (fnBest < best) {
            best = fnBest;
            bestIdx = i;
        }
    }
    if (outDist)
        *outDist = best;
    return bestIdx;
}

void GraphCanvasItem::processTapRelease(const QPointF& scenePos, int holdElapsedMs) {
    const QPointF localPos = mapFromScene(scenePos);
    qreal d = 1e9;
    const int idx = nearestFunctionAtScenePos(scenePos, &d);
    if (idx >= 0 && d < kCurveHitPx) {
        emit functionTapped(idx);
        if (holdElapsedMs >= 420)
            emit functionLongPressed(idx);
        m_data.rect = m_rect;
        emit graphChanged();
        return;
    }
    double ax = 0.0;
    if (isSelected() && hitXAxisAtLocal(localPos, &ax)) {
        emit xAxisTapped(ax, scenePos);
        m_data.rect = m_rect;
        emit graphChanged();
        return;
    }
    if (hitPlotBackgroundAtScene(scenePos)) {
        emit plotBackgroundTapped(scenePos);
        m_data.rect = m_rect;
        emit graphChanged();
        return;
    }
    m_data.rect = m_rect;
    emit graphChanged();
    if (scene()) {
        scene()->clearSelection();
        setSelected(true);
    }
}

void GraphCanvasItem::deliverTapFromScene(const QPointF& scenePos, int holdElapsedMs) {
    processTapRelease(scenePos, holdElapsedMs);
}

void GraphCanvasItem::mousePressEvent(QGraphicsSceneMouseEvent* e) {
    if (!m_plusSuppressed && plusButtonHitLocalRect().contains(e->pos())) {
        m_pressedPlus = true;
        e->accept();
        return;
    }
    const QRectF handle(m_rect.right() - 14, m_rect.bottom() - 14, 14, 14);
    if (isSelected() && handle.contains(e->pos())) {
        m_resizing = true;
        m_resizeStartScene = e->scenePos();
        m_resizeStartRect = m_rect;
        e->accept();
        return;
    }
    qreal d = 1e9;
    m_pressedFunction = nearestFunctionAtScenePos(e->scenePos(), &d);
    m_holdTimer.restart();
    QGraphicsObject::mousePressEvent(e);
}

void GraphCanvasItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e) {
    if (m_resizing) {
        const QPointF d = e->scenePos() - m_resizeStartScene;
        prepareGeometryChange();
        m_rect = QRectF(0, 0, qMax(80.0, m_resizeStartRect.width() + d.x()), qMax(60.0, m_resizeStartRect.height() + d.y()));
        m_data.rect = m_rect;
        update();
        emit graphChanged();
        e->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(e);
}

void GraphCanvasItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e) {
    if (m_pressedPlus) {
        m_pressedPlus = false;
        if (!m_plusSuppressed && plusButtonHitLocalRect().contains(e->pos()))
            emit plusTapped();
        e->accept();
        return;
    }
    if (m_resizing) {
        m_resizing = false;
        emit graphChanged();
        e->accept();
        return;
    }
    processTapRelease(e->scenePos(), m_holdTimer.isValid() ? int(m_holdTimer.elapsed()) : 0);
    QGraphicsObject::mouseReleaseEvent(e);
}
