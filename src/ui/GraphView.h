#pragma once

#include "core/Calculator.h"
#include "core/Expression.h"

#include <QColor>
#include <QFont>
#include <QQuickPaintedItem>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>


// Plots y = f(x) for a list of expressions in x against the calculator's
// variables and functions: a grid with labelled axes, the way Grapher
// drew them. Drag pans, the wheel zooms at the pointer, a double click
// goes back to the standard view, and while the pointer is over it the
// values of every curve at that x are shown.
class GraphView : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Calculator *calculator READ calculator WRITE setCalculator NOTIFY calculatorChanged)
    // Maps with expression, color and visible.
    Q_PROPERTY(QVariantList curves READ curves WRITE setCurves NOTIFY curvesChanged)
    Q_PROPERTY(qreal xMin READ xMin NOTIFY rangeChanged)
    Q_PROPERTY(qreal xMax READ xMax NOTIFY rangeChanged)
    Q_PROPERTY(qreal yMin READ yMin NOTIFY rangeChanged)
    Q_PROPERTY(qreal yMax READ yMax NOTIFY rangeChanged)
    // The distance between ticks, in units: 1, 2 or 5 times a power of ten.
    Q_PROPERTY(qreal xStep READ xStep NOTIFY rangeChanged)
    Q_PROPERTY(qreal yStep READ yStep NOTIFY rangeChanged)
    Q_PROPERTY(bool showGrid MEMBER m_showGrid NOTIFY styleChanged)
    Q_PROPERTY(QColor backgroundColor MEMBER m_backgroundColor NOTIFY styleChanged)
    Q_PROPERTY(QColor gridColor MEMBER m_gridColor NOTIFY styleChanged)
    Q_PROPERTY(QColor axisColor MEMBER m_axisColor NOTIFY styleChanged)
    Q_PROPERTY(QColor textColor MEMBER m_textColor NOTIFY styleChanged)
    Q_PROPERTY(QFont font MEMBER m_font NOTIFY styleChanged)
    // One entry per curve: what is wrong with its expression, or empty.
    Q_PROPERTY(QStringList errors READ errors NOTIFY errorsChanged)
    Q_PROPERTY(bool hovering READ hovering NOTIFY hoverChanged)
    Q_PROPERTY(qreal hoverX READ hoverX NOTIFY hoverChanged)

public:
    explicit GraphView(QQuickItem *parent = nullptr);

    Calculator *calculator() const { return m_calculator; }
    void setCalculator(Calculator *calculator);
    QVariantList curves() const { return m_curves; }
    void setCurves(const QVariantList &curves);
    qreal xMin() const { return m_xMin; }
    qreal xMax() const { return m_xMax; }
    qreal yMin() const { return m_yMin; }
    qreal yMax() const { return m_yMax; }
    qreal xStep() const;
    qreal yStep() const;
    QStringList errors() const { return m_errors; }
    bool hovering() const { return m_hovering; }
    qreal hoverX() const { return m_hoverX; }

    Q_INVOKABLE void setRange(qreal xMin, qreal xMax, qreal yMin, qreal yMax);
    // -10..10 across, the same scale up and down.
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    // The curves sampled anew, after the variables they use changed.
    Q_INVOKABLE void refresh();
    // The value of every curve at `x`, NaN where there is none.
    Q_INVOKABLE QVariantList valuesAt(qreal x) const;

    void paint(QPainter *painter) override;

signals:
    void calculatorChanged();
    void curvesChanged();
    void rangeChanged();
    void styleChanged();
    void errorsChanged();
    void hoverChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    struct Curve
    {
        QString expression;
        QColor color;
        bool visible = true;
        ExprPtr expr;
        QString error;
    };

    void compile();
    void zoomAt(qreal factor, QPointF center);
    qreal toPixelX(qreal x) const;
    qreal toPixelY(qreal y) const;
    qreal fromPixelX(qreal px) const;
    qreal fromPixelY(qreal py) const;
    Number sample(const Curve &curve, qreal x) const;
    void paintGrid(QPainter *painter);
    void paintCurves(QPainter *painter);
    void paintHover(QPainter *painter);

    Calculator *m_calculator = nullptr;
    QVariantList m_curves;
    QList<Curve> m_compiled;
    QStringList m_errors;
    qreal m_xMin = -10;
    qreal m_xMax = 10;
    qreal m_yMin = -6;
    qreal m_yMax = 6;
    bool m_showGrid = true;
    QColor m_backgroundColor = Qt::white;
    QColor m_gridColor = QColor(0xe2, 0xe2, 0xe2);
    QColor m_axisColor = QColor(0x40, 0x40, 0x40);
    QColor m_textColor = QColor(0x40, 0x40, 0x40);
    QFont m_font;
    bool m_hovering = false;
    qreal m_hoverX = 0;
    QPointF m_hoverPos;
    bool m_dragging = false;
    QPointF m_dragStart;
    qreal m_dragXMin = 0;
    qreal m_dragYMin = 0;
};
