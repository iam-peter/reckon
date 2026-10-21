#pragma once

#include "core/Calculator.h"

#include <QColor>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QtQml/qqmlregistration.h>


// Colors the expression being typed: numbers, functions, variables and
// operators each in their color, unmatched parentheses and characters
// the calculator cannot read in the error color.
class ExpressionHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(Calculator *calculator READ calculator WRITE setCalculator NOTIFY calculatorChanged)
    Q_PROPERTY(QColor numberColor MEMBER m_numberColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor functionColor MEMBER m_functionColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor variableColor MEMBER m_variableColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor operatorColor MEMBER m_operatorColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor errorColor MEMBER m_errorColor NOTIFY colorsChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit ExpressionHighlighter(QObject *parent = nullptr);

    QQuickTextDocument *document() const { return m_document; }
    void setDocument(QQuickTextDocument *document);
    Calculator *calculator() const { return m_calculator; }
    void setCalculator(Calculator *calculator);
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

protected:
    void highlightBlock(const QString &text) override;

signals:
    void documentChanged();
    void calculatorChanged();
    void colorsChanged();
    void enabledChanged();

private:
    QQuickTextDocument *m_document = nullptr;
    Calculator *m_calculator = nullptr;
    QColor m_numberColor = QColor(0x1c, 0x6e, 0xd8);
    QColor m_functionColor = QColor(0x8a, 0x3f, 0xc0);
    QColor m_variableColor = QColor(0x1a, 0x7f, 0x5a);
    QColor m_operatorColor = QColor(0x70, 0x70, 0x70);
    QColor m_errorColor = QColor(0xd3, 0x2f, 0x2f);
    bool m_enabled = true;
};
