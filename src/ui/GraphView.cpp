#include "GraphView.h"

#include "core/Calculator.h"
#include "core/Environment.h"

#include <QFontMetricsF>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <cmath>

using namespace Qt::StringLiterals;

namespace {

// Ticks are about this far apart, in pixels.
constexpr qreal XTickSpacing = 70;
constexpr qreal YTickSpacing = 50;

// The step nearest `rough` that is 1, 2 or 5 times a power of ten.
qreal niceStep(qreal rough)
{
    if (!(rough > 0) || !std::isfinite(rough))
        return 1;
    const qreal power = std::pow(10.0, std::floor(std::log10(rough)));
    const qreal normalized = rough / power;
    if (normalized < 1.5)
        return power;
    if (normalized < 3.5)
        return 2 * power;
    if (normalized < 7.5)
        return 5 * power;
    return 10 * power;
}

QString tickLabel(qreal value, qreal step)
{
    if (std::fabs(value) < step / 1000)
        return u"0"_s;
    QString label = QString::number(value, 'g', 6);
    label.replace(u"e+"_s, u"e"_s);
    return label;
}

} // namespace

GraphView::GraphView(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton);
    setAcceptHoverEvents(true);
    setAntialiasing(true);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    connect(this, &GraphView::styleChanged, this, [this] { update(); });
}

void GraphView::setCalculator(Calculator *calculator)
{
    if (m_calculator == calculator)
        return;
    if (m_calculator)
        disconnect(m_calculator, nullptr, this, nullptr);
    m_calculator = calculator;
    if (m_calculator) {
        connect(m_calculator, &Calculator::evaluated, this, &GraphView::refresh);
        connect(m_calculator, &Calculator::angleUnitChanged, this, &GraphView::refresh);
    }
    emit calculatorChanged();
    compile();
}

void GraphView::setCurves(const QVariantList &curves)
{
    if (m_curves == curves)
        return;
    m_curves = curves;
    emit curvesChanged();
    compile();
}

void GraphView::compile()
{
    m_compiled.clear();
    QStringList errors;
    for (const QVariant &variant : std::as_const(m_curves)) {
        const QVariantMap map = variant.toMap();
        Curve curve;
        curve.expression = map.value(u"expression"_s).toString().trimmed();
        curve.color = map.value(u"color"_s).value<QColor>();
        curve.visible = map.value(u"visible"_s, true).toBool();
        if (!curve.expression.isEmpty() && m_calculator) {
            try {
                curve.expr = parseExpression(curve.expression, m_calculator->environment().callPredicate());
            } catch (const CalcError &error) {
                curve.error = error.message;
            }
        }
        errors.append(curve.error);
        m_compiled.append(curve);
    }
    if (m_errors != errors) {
        m_errors = errors;
        emit errorsChanged();
    }
    update();
}

void GraphView::refresh()
{
    compile();
}

void GraphView::setRange(qreal xMin, qreal xMax, qreal yMin, qreal yMax)
{
    if (!(xMax > xMin) || !(yMax > yMin) || !std::isfinite(xMin + xMax + yMin + yMax))
        return;
    if (m_xMin == xMin && m_xMax == xMax && m_yMin == yMin && m_yMax == yMax)
        return;
    m_xMin = xMin;
    m_xMax = xMax;
    m_yMin = yMin;
    m_yMax = yMax;
    emit rangeChanged();
    update();
}

void GraphView::resetView()
{
    const qreal aspect = width() > 0 && height() > 0 ? height() / width() : 0.6;
    setRange(-10, 10, -10 * aspect, 10 * aspect);
}

void GraphView::zoomIn()
{
    zoomAt(0.8, QPointF(width() / 2, height() / 2));
}

void GraphView::zoomOut()
{
    zoomAt(1.25, QPointF(width() / 2, height() / 2));
}

void GraphView::zoomAt(qreal factor, QPointF center)
{
    const qreal x = fromPixelX(center.x());
    const qreal y = fromPixelY(center.y());
    const qreal xRange = (m_xMax - m_xMin) * factor;
    const qreal yRange = (m_yMax - m_yMin) * factor;
    if (xRange < 1e-9 || yRange < 1e-9 || xRange > 1e12 || yRange > 1e12)
        return;
    const qreal fx = width() > 0 ? center.x() / width() : 0.5;
    const qreal fy = height() > 0 ? 1 - center.y() / height() : 0.5;
    setRange(x - xRange * fx, x + xRange * (1 - fx), y - yRange * fy, y + yRange * (1 - fy));
}

qreal GraphView::toPixelX(qreal x) const
{
    return (x - m_xMin) / (m_xMax - m_xMin) * width();
}

qreal GraphView::toPixelY(qreal y) const
{
    return height() - (y - m_yMin) / (m_yMax - m_yMin) * height();
}

qreal GraphView::fromPixelX(qreal px) const
{
    return width() > 0 ? m_xMin + px / width() * (m_xMax - m_xMin) : m_xMin;
}

qreal GraphView::fromPixelY(qreal py) const
{
    return height() > 0 ? m_yMax - py / height() * (m_yMax - m_yMin) : m_yMax;
}

Number GraphView::sample(const Curve &curve, qreal x) const
{
    if (!curve.expr || !m_calculator)
        return std::nan("");
    try {
        return m_calculator->environment().evaluateWith(curve.expr, u"x"_s, x);
    } catch (const CalcError &) {
        return std::nan("");
    }
}

QVariantList GraphView::valuesAt(qreal x) const
{
    QVariantList values;
    for (const Curve &curve : m_compiled)
        values.append(curve.visible ? sample(curve, x) : std::nan(""));
    return values;
}

void GraphView::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), m_backgroundColor);
    if (width() < 2 || height() < 2)
        return;
    painter->setFont(m_font);
    paintGrid(painter);
    paintCurves(painter);
    if (m_hovering)
        paintHover(painter);
}

// From the scale alone, units per pixel, and not from how many ticks the
// width holds: resizing keeps the scale, so it shows more or fewer ticks
// at the same spacing, and only zooming changes the spacing.
qreal GraphView::xStep() const
{
    return width() > 0 ? niceStep((m_xMax - m_xMin) / width() * XTickSpacing) : 1;
}

qreal GraphView::yStep() const
{
    return height() > 0 ? niceStep((m_yMax - m_yMin) / height() * YTickSpacing) : 1;
}

void GraphView::paintGrid(QPainter *painter)
{
    const qreal xStep = this->xStep();
    const qreal yStep = this->yStep();
    const QFontMetricsF metrics(m_font);
    const bool xAxisVisible = m_yMin <= 0 && m_yMax >= 0;
    const bool yAxisVisible = m_xMin <= 0 && m_xMax >= 0;
    // Labels sit next to the axes, or along the edge when the axis is off-screen.
    const qreal axisY = xAxisVisible ? toPixelY(0) : (m_yMax < 0 ? 0 : height());
    const qreal axisX = yAxisVisible ? toPixelX(0) : (m_xMin > 0 ? 0 : width());

    if (m_showGrid) {
        painter->setPen(QPen(m_gridColor, 1));
        for (qreal x = std::ceil(m_xMin / xStep) * xStep; x <= m_xMax; x += xStep) {
            const qreal px = std::round(toPixelX(x)) + 0.5;
            painter->drawLine(QPointF(px, 0), QPointF(px, height()));
        }
        for (qreal y = std::ceil(m_yMin / yStep) * yStep; y <= m_yMax; y += yStep) {
            const qreal py = std::round(toPixelY(y)) + 0.5;
            painter->drawLine(QPointF(0, py), QPointF(width(), py));
        }
    }

    painter->setPen(QPen(m_axisColor, 1.5));
    if (xAxisVisible)
        painter->drawLine(QPointF(0, axisY), QPointF(width(), axisY));
    if (yAxisVisible)
        painter->drawLine(QPointF(axisX, 0), QPointF(axisX, height()));

    painter->setPen(m_textColor);
    const qreal labelY = xAxisVisible ? axisY + 4 : (m_yMax < 0 ? 4 : height() - metrics.height() - 4);
    for (qreal x = std::ceil(m_xMin / xStep) * xStep; x <= m_xMax; x += xStep) {
        if (yAxisVisible && std::fabs(x) < xStep / 1000)
            continue;
        const QString label = tickLabel(x, xStep);
        const qreal px = toPixelX(x);
        // A label half outside the view is left out.
        if (px < metrics.horizontalAdvance(label) / 2 || px > width() - metrics.horizontalAdvance(label) / 2)
            continue;
        painter->drawText(QRectF(px - 40, labelY, 80, metrics.height()), Qt::AlignHCenter | Qt::AlignTop, label);
    }
    for (qreal y = std::ceil(m_yMin / yStep) * yStep; y <= m_yMax; y += yStep) {
        if (xAxisVisible && std::fabs(y) < yStep / 1000)
            continue;
        const QString label = tickLabel(y, yStep);
        const qreal py = toPixelY(y);
        if (py < metrics.height() / 2 || py > height() - metrics.height() / 2)
            continue;
        const qreal w = metrics.horizontalAdvance(label) + 8;
        QRectF rect(axisX - w, py - metrics.height() / 2, w - 4, metrics.height());
        if (!yAxisVisible && m_xMin > 0)
            rect.moveLeft(4);
        else if (rect.left() < 0)
            rect.moveLeft(axisX + 4);
        painter->drawText(rect, Qt::AlignRight | Qt::AlignVCenter, label);
    }
    if (xAxisVisible && yAxisVisible)
        painter->drawText(QRectF(axisX - 44, axisY + 4, 40, metrics.height()), Qt::AlignRight | Qt::AlignTop, u"0"_s);
}

void GraphView::paintCurves(QPainter *painter)
{
    const int samples = qMax(2, int(width()) * 2);
    const qreal limit = height() * 10;
    for (const Curve &curve : std::as_const(m_compiled)) {
        if (!curve.visible || !curve.expr)
            continue;
        QPainterPath path;
        bool penDown = false;
        qreal lastPy = 0;
        for (int i = 0; i <= samples; ++i) {
            const qreal x = m_xMin + (m_xMax - m_xMin) * i / samples;
            const Number y = sample(curve, x);
            if (!std::isfinite(y)) {
                penDown = false;
                continue;
            }
            const qreal px = toPixelX(x);
            qreal py = toPixelY(y);
            // A jump from far above to far below the view is an asymptote,
            // not a line to draw across it.
            const bool jump = penDown && ((lastPy < -height() && py > 2 * height())
                                          || (lastPy > 2 * height() && py < -height()));
            py = qBound(-limit, py, limit);
            if (!penDown || jump) {
                path.moveTo(px, py);
                penDown = true;
            } else {
                path.lineTo(px, py);
            }
            lastPy = py;
        }
        painter->setPen(QPen(curve.color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPath(path);
    }
}

void GraphView::paintHover(QPainter *painter)
{
    const qreal x = m_hoverX;
    const qreal px = toPixelX(x);
    painter->setPen(QPen(m_axisColor, 1, Qt::DashLine));
    painter->drawLine(QPointF(px, 0), QPointF(px, height()));

    const QFontMetricsF metrics(m_font);
    QStringList lines{u"x = %1"_s.arg(QString::number(x, 'g', 6))};
    QList<QColor> colors{m_textColor};
    for (const Curve &curve : std::as_const(m_compiled)) {
        if (!curve.visible || !curve.expr)
            continue;
        const Number y = sample(curve, x);
        if (!std::isfinite(y))
            continue;
        const qreal py = toPixelY(y);
        if (py >= 0 && py <= height()) {
            painter->setPen(QPen(m_backgroundColor, 2));
            painter->setBrush(curve.color);
            painter->drawEllipse(QPointF(px, py), 4, 4);
        }
        lines.append(u"y = %1"_s.arg(QString::number(y, 'g', 6)));
        colors.append(curve.color);
    }
    qreal w = 0;
    for (const QString &line : lines)
        w = qMax(w, metrics.horizontalAdvance(line));
    const qreal lineHeight = metrics.height();
    QRectF box(0, 0, w + 16, lineHeight * lines.size() + 10);
    box.moveTopLeft(QPointF(m_hoverPos.x() + 14, m_hoverPos.y() + 14));
    if (box.right() > width())
        box.moveRight(m_hoverPos.x() - 14);
    if (box.bottom() > height())
        box.moveBottom(m_hoverPos.y() - 14);
    QColor fill = m_backgroundColor;
    fill.setAlpha(230);
    painter->setPen(QPen(m_gridColor, 1));
    painter->setBrush(fill);
    painter->drawRoundedRect(box, 4, 4);
    for (int i = 0; i < lines.size(); ++i) {
        painter->setPen(colors[i]);
        painter->drawText(QRectF(box.left() + 8, box.top() + 5 + i * lineHeight, w, lineHeight),
                          Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
    }
}

void GraphView::mousePressEvent(QMouseEvent *event)
{
    m_dragging = true;
    m_dragStart = event->position();
    m_dragXMin = m_xMin;
    m_dragYMin = m_yMin;
    event->accept();
}

void GraphView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging)
        return;
    const QPointF delta = event->position() - m_dragStart;
    const qreal dx = delta.x() / width() * (m_xMax - m_xMin);
    const qreal dy = delta.y() / height() * (m_yMax - m_yMin);
    const qreal xRange = m_xMax - m_xMin;
    const qreal yRange = m_yMax - m_yMin;
    setRange(m_dragXMin - dx, m_dragXMin - dx + xRange, m_dragYMin + dy, m_dragYMin + dy + yRange);
    m_hoverPos = event->position();
    m_hoverX = fromPixelX(m_hoverPos.x());
    emit hoverChanged();
}

void GraphView::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    event->accept();
}

void GraphView::mouseDoubleClickEvent(QMouseEvent *event)
{
    resetView();
    event->accept();
}

void GraphView::wheelEvent(QWheelEvent *event)
{
    const qreal steps = event->angleDelta().y() / 120.0;
    if (steps == 0)
        return;
    zoomAt(std::pow(0.85, steps), event->position());
    m_hoverX = fromPixelX(event->position().x());
    emit hoverChanged();
    event->accept();
}

void GraphView::hoverMoveEvent(QHoverEvent *event)
{
    m_hovering = true;
    m_hoverPos = event->position();
    m_hoverX = fromPixelX(m_hoverPos.x());
    emit hoverChanged();
    update();
}

void GraphView::hoverLeaveEvent(QHoverEvent *)
{
    m_hovering = false;
    emit hoverChanged();
    update();
}

void GraphView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    // Keep the scale, show more or less of the plane.
    if (oldGeometry.width() > 0 && oldGeometry.height() > 0 && newGeometry.size() != oldGeometry.size()) {
        const qreal xScale = (m_xMax - m_xMin) / oldGeometry.width();
        const qreal yScale = (m_yMax - m_yMin) / oldGeometry.height();
        const qreal xCenter = (m_xMin + m_xMax) / 2;
        const qreal yCenter = (m_yMin + m_yMax) / 2;
        setRange(xCenter - xScale * newGeometry.width() / 2, xCenter + xScale * newGeometry.width() / 2,
                 yCenter - yScale * newGeometry.height() / 2, yCenter + yScale * newGeometry.height() / 2);
    }
}
