#include "ExpressionHighlighter.h"

#include "core/Calculator.h"
#include "core/Lexer.h"

#include <QTextDocument>

ExpressionHighlighter::ExpressionHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
    connect(this, &ExpressionHighlighter::colorsChanged, this, &QSyntaxHighlighter::rehighlight);
}

void ExpressionHighlighter::setDocument(QQuickTextDocument *document)
{
    if (m_document == document)
        return;
    m_document = document;
    QSyntaxHighlighter::setDocument(document ? document->textDocument() : nullptr);
    emit documentChanged();
}

void ExpressionHighlighter::setCalculator(Calculator *calculator)
{
    if (m_calculator == calculator)
        return;
    if (m_calculator)
        disconnect(m_calculator, nullptr, this, nullptr);
    m_calculator = calculator;
    if (m_calculator)
        connect(m_calculator, &Calculator::evaluated, this, &QSyntaxHighlighter::rehighlight);
    emit calculatorChanged();
    rehighlight();
}

void ExpressionHighlighter::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
    rehighlight();
}

void ExpressionHighlighter::highlightBlock(const QString &text)
{
    if (!m_enabled)
        return;
    const QList<Token> tokens = tokenize(text, true);
    QList<int> openParens;
    for (const Token &token : tokens) {
        QTextCharFormat format;
        switch (token.type) {
        case Token::Number:
            format.setForeground(m_numberColor);
            break;
        case Token::Identifier:
            if (m_calculator && m_calculator->isFunction(token.text))
                format.setForeground(m_functionColor);
            else if (m_calculator && m_calculator->isVariable(token.text))
                format.setForeground(m_variableColor);
            else
                continue;
            break;
        case Token::Operator:
        case Token::Assign:
        case Token::Comma:
        case Token::Semicolon:
            format.setForeground(m_operatorColor);
            break;
        case Token::LeftParen:
            openParens.append(token.position);
            continue;
        case Token::RightParen:
            if (openParens.isEmpty()) {
                format.setForeground(m_errorColor);
                format.setFontWeight(QFont::Bold);
                break;
            }
            openParens.removeLast();
            continue;
        case Token::Unknown:
            format.setForeground(m_errorColor);
            format.setFontUnderline(true);
            break;
        case Token::End:
            continue;
        }
        setFormat(token.position, token.length, format);
    }
    QTextCharFormat unmatched;
    unmatched.setForeground(m_errorColor);
    unmatched.setFontWeight(QFont::Bold);
    for (int position : openParens)
        setFormat(position, 1, unmatched);
}
