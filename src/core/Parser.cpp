#include "Environment.h"
#include "Expression.h"
#include "Lexer.h"

#include <cmath>

using namespace Qt::StringLiterals;

namespace {

struct NumberExpr : Expr
{
    Number value;
    Number evaluate(Environment &) const override { return value; }
};

struct VariableExpr : Expr
{
    QString name;
    Number evaluate(Environment &env) const override { return env.variable(name, position); }
};

struct CallExpr : Expr
{
    QString name;
    QList<ExprPtr> arguments;
    Number evaluate(Environment &env) const override
    {
        QList<Number> values;
        values.reserve(arguments.size());
        for (const ExprPtr &argument : arguments)
            values.append(argument->evaluate(env));
        return env.call(name, values, position);
    }
};

struct AssignExpr : Expr
{
    QString name;
    ExprPtr value;
    Number evaluate(Environment &env) const override
    {
        const Number result = value->evaluate(env);
        env.assign(name, result, position);
        return result;
    }
};

enum class UnaryOp { Negate, Plus, BitNot, Factorial };

struct UnaryExpr : Expr
{
    UnaryOp op;
    ExprPtr operand;
    Number evaluate(Environment &env) const override
    {
        const Number x = operand->evaluate(env);
        switch (op) {
        case UnaryOp::Negate:
            return -x;
        case UnaryOp::Plus:
            return x;
        case UnaryOp::BitNot:
            return Number(~Environment::toInteger(x, position));
        case UnaryOp::Factorial:
            if (x < 0 && std::floor(x) == x)
                throw CalcError{u"Factorial of a negative integer"_s, position};
            return std::tgamma(x + 1);
        }
        return x;
    }
};

enum class BinaryOp {
    Add, Subtract, Multiply, Divide, Power, Modulo, IntDivide,
    And, Or, Xor, ShiftLeft, ShiftRight,
    Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual,
};

struct BinaryExpr : Expr
{
    BinaryOp op;
    ExprPtr left;
    ExprPtr right;
    Number evaluate(Environment &env) const override
    {
        const Number a = left->evaluate(env);
        const Number b = right->evaluate(env);
        switch (op) {
        case BinaryOp::Add:
            return a + b;
        case BinaryOp::Subtract:
            return a - b;
        case BinaryOp::Multiply:
            return a * b;
        case BinaryOp::Divide:
            if (b == 0)
                throw CalcError{u"Division by zero"_s, position};
            return a / b;
        case BinaryOp::Power:
            if (a == 0 && b < 0)
                throw CalcError{u"Division by zero"_s, position};
            return std::pow(a, b);
        case BinaryOp::Modulo:
            if (b == 0)
                throw CalcError{u"Division by zero"_s, position};
            return std::fmod(a, b);
        case BinaryOp::IntDivide:
            if (b == 0)
                throw CalcError{u"Division by zero"_s, position};
            return std::trunc(a / b);
        case BinaryOp::And:
            return Number(Environment::toInteger(a, position) & Environment::toInteger(b, position));
        case BinaryOp::Or:
            return Number(Environment::toInteger(a, position) | Environment::toInteger(b, position));
        case BinaryOp::Xor:
            return Number(Environment::toInteger(a, position) ^ Environment::toInteger(b, position));
        case BinaryOp::ShiftLeft: {
            const qint64 n = Environment::toInteger(b, position);
            if (n < 0 || n > 62)
                throw CalcError{u"Shift count out of range"_s, position};
            return Number(Environment::toInteger(a, position) << n);
        }
        case BinaryOp::ShiftRight: {
            const qint64 n = Environment::toInteger(b, position);
            if (n < 0 || n > 62)
                throw CalcError{u"Shift count out of range"_s, position};
            return Number(Environment::toInteger(a, position) >> n);
        }
        case BinaryOp::Equal:
            return a == b ? 1 : 0;
        case BinaryOp::NotEqual:
            return a != b ? 1 : 0;
        case BinaryOp::Less:
            return a < b ? 1 : 0;
        case BinaryOp::Greater:
            return a > b ? 1 : 0;
        case BinaryOp::LessEqual:
            return a <= b ? 1 : 0;
        case BinaryOp::GreaterEqual:
            return a >= b ? 1 : 0;
        }
        return 0;
    }
};

// Recursive descent over the tokens, lowest precedence first:
// assignment, or, xor, and, equality, comparison, shifts, + -, * / mod
// div and implicit multiplication, unary - + ~ not, ^ (right-associative,
// binding tighter than unary minus on its left but allowing one on its
// right), then postfix ! and the primaries.
class Parser
{
public:
    Parser(const QString &text, const FunctionPredicate &isFunction)
        : m_text(text), m_tokens(tokenize(text)), m_isFunction(isFunction)
    {
    }

    Statement statement()
    {
        Statement result;
        if (looksLikeDefinition()) {
            result.functionName = take().text;
            take(); // (
            while (true) {
                const Token &parameter = take();
                if (parameter.type != Token::Identifier)
                    throw CalcError{u"Parameter name expected"_s, parameter.position};
                if (result.parameters.contains(parameter.text))
                    throw CalcError{u"Parameter %1 given twice"_s.arg(parameter.text), parameter.position};
                result.parameters.append(parameter.text);
                if (peek().type == Token::Comma) {
                    take();
                    continue;
                }
                break;
            }
            expect(Token::RightParen, u")"_s);
            expect(Token::Assign, u"="_s);
            const int bodyStart = peek().position;
            result.expr = expression();
            result.bodySource = m_text.mid(bodyStart).trimmed();
        } else {
            result.expr = expression();
        }
        if (peek().type != Token::End)
            throw CalcError{u"Unexpected %1"_s.arg(describe(peek())), peek().position};
        return result;
    }

    ExprPtr expressionOnly()
    {
        ExprPtr result = expression();
        if (peek().type != Token::End)
            throw CalcError{u"Unexpected %1"_s.arg(describe(peek())), peek().position};
        return result;
    }

private:
    const Token &peek(int ahead = 0) const
    {
        return m_tokens[qMin(m_index + ahead, m_tokens.size() - 1)];
    }
    const Token &take() { return m_tokens[qMin(m_index++, m_tokens.size() - 1)]; }

    void expect(Token::Type type, const QString &what)
    {
        if (peek().type != type)
            throw CalcError{u"Expected %1"_s.arg(what), peek().position};
        take();
    }

    static QString describe(const Token &token)
    {
        if (token.type == Token::End)
            return u"end of expression"_s;
        return u"'%1'"_s.arg(token.text);
    }

    bool isOperator(const QString &text) const
    {
        return peek().type == Token::Operator && peek().text == text;
    }

    // name ( name, name ) =
    bool looksLikeDefinition() const
    {
        if (peek().type != Token::Identifier || peek(1).type != Token::LeftParen)
            return false;
        int i = 2;
        while (true) {
            if (peek(i).type != Token::Identifier)
                return false;
            ++i;
            if (peek(i).type == Token::Comma) {
                ++i;
                continue;
            }
            break;
        }
        return peek(i).type == Token::RightParen && peek(i + 1).type == Token::Assign;
    }

    ExprPtr expression() { return assignment(); }

    ExprPtr assignment()
    {
        if (peek().type == Token::Identifier && peek(1).type == Token::Assign) {
            auto node = std::make_shared<AssignExpr>();
            node->position = peek().position;
            node->name = take().text;
            take();
            node->value = assignment();
            return node;
        }
        return orExpr();
    }

    template <typename Next>
    ExprPtr binaryChain(Next next, std::initializer_list<std::pair<const char *, BinaryOp>> ops)
    {
        ExprPtr left = (this->*next)();
        while (true) {
            bool matched = false;
            for (const auto &[text, op] : ops) {
                if (isOperator(QLatin1StringView(text))) {
                    auto node = std::make_shared<BinaryExpr>();
                    node->position = take().position;
                    node->op = op;
                    node->left = left;
                    node->right = (this->*next)();
                    left = node;
                    matched = true;
                    break;
                }
            }
            if (!matched)
                return left;
        }
    }

    ExprPtr orExpr() { return binaryChain(&Parser::xorExpr, {{"or", BinaryOp::Or}, {"|", BinaryOp::Or}}); }
    ExprPtr xorExpr() { return binaryChain(&Parser::andExpr, {{"xor", BinaryOp::Xor}}); }
    ExprPtr andExpr() { return binaryChain(&Parser::equality, {{"and", BinaryOp::And}, {"&", BinaryOp::And}}); }
    ExprPtr equality()
    {
        return binaryChain(&Parser::comparison, {{"==", BinaryOp::Equal}, {"!=", BinaryOp::NotEqual}});
    }
    ExprPtr comparison()
    {
        return binaryChain(&Parser::shift, {{"<=", BinaryOp::LessEqual}, {">=", BinaryOp::GreaterEqual},
                                            {"<", BinaryOp::Less}, {">", BinaryOp::Greater}});
    }
    ExprPtr shift()
    {
        return binaryChain(&Parser::additive, {{"<<", BinaryOp::ShiftLeft}, {"shl", BinaryOp::ShiftLeft},
                                               {">>", BinaryOp::ShiftRight}, {"shr", BinaryOp::ShiftRight}});
    }
    ExprPtr additive()
    {
        return binaryChain(&Parser::multiplicative, {{"+", BinaryOp::Add}, {"-", BinaryOp::Subtract}});
    }

    ExprPtr multiplicative()
    {
        ExprPtr left = unary();
        while (true) {
            BinaryOp op;
            if (isOperator(u"*"_s))
                op = BinaryOp::Multiply;
            else if (isOperator(u"/"_s))
                op = BinaryOp::Divide;
            else if (isOperator(u"%"_s) || isOperator(u"mod"_s))
                op = BinaryOp::Modulo;
            else if (isOperator(u"div"_s))
                op = BinaryOp::IntDivide;
            else if (peek().type == Token::Identifier || peek().type == Token::LeftParen) {
                // 2x, 2(3), (1)(2): multiplication without a sign.
                auto node = std::make_shared<BinaryExpr>();
                node->position = peek().position;
                node->op = BinaryOp::Multiply;
                node->left = left;
                node->right = unary();
                left = node;
                continue;
            } else {
                return left;
            }
            auto node = std::make_shared<BinaryExpr>();
            node->position = take().position;
            node->op = op;
            node->left = left;
            node->right = unary();
            left = node;
        }
    }

    ExprPtr unary()
    {
        UnaryOp op;
        if (isOperator(u"-"_s))
            op = UnaryOp::Negate;
        else if (isOperator(u"+"_s))
            op = UnaryOp::Plus;
        else if (isOperator(u"~"_s) || isOperator(u"not"_s))
            op = UnaryOp::BitNot;
        else
            return power();
        auto node = std::make_shared<UnaryExpr>();
        node->position = take().position;
        node->op = op;
        node->operand = unary();
        return node;
    }

    ExprPtr power()
    {
        ExprPtr base = postfix();
        if (!isOperator(u"^"_s))
            return base;
        auto node = std::make_shared<BinaryExpr>();
        node->position = take().position;
        node->op = BinaryOp::Power;
        node->left = base;
        node->right = unary();
        return node;
    }

    ExprPtr postfix()
    {
        ExprPtr operand = primary();
        while (isOperator(u"!"_s)) {
            auto node = std::make_shared<UnaryExpr>();
            node->position = take().position;
            node->op = UnaryOp::Factorial;
            node->operand = operand;
            operand = node;
        }
        return operand;
    }

    ExprPtr primary()
    {
        const Token &token = peek();
        switch (token.type) {
        case Token::Number: {
            auto node = std::make_shared<NumberExpr>();
            node->position = token.position;
            node->value = take().value;
            return node;
        }
        case Token::Identifier: {
            take();
            const bool call = peek().type == Token::LeftParen && (!m_isFunction || m_isFunction(token.text));
            if (!call) {
                auto node = std::make_shared<VariableExpr>();
                node->position = token.position;
                node->name = token.text;
                return node;
            }
            auto node = std::make_shared<CallExpr>();
            node->position = token.position;
            node->name = token.text;
            take(); // (
            if (peek().type != Token::RightParen) {
                while (true) {
                    node->arguments.append(expression());
                    if (peek().type == Token::Comma) {
                        take();
                        continue;
                    }
                    break;
                }
            }
            expect(Token::RightParen, u")"_s);
            return node;
        }
        case Token::LeftParen: {
            take();
            ExprPtr inner = expression();
            expect(Token::RightParen, u")"_s);
            return inner;
        }
        case Token::End:
            throw CalcError{u"Expression is incomplete"_s, token.position};
        default:
            throw CalcError{u"Unexpected %1"_s.arg(describe(token)), token.position};
        }
    }

    QString m_text;
    QList<Token> m_tokens;
    FunctionPredicate m_isFunction;
    int m_index = 0;
};

} // namespace

Statement parseStatement(const QString &text, const FunctionPredicate &isFunction)
{
    return Parser(text, isFunction).statement();
}

ExprPtr parseExpression(const QString &text, const FunctionPredicate &isFunction)
{
    return Parser(text, isFunction).expressionOnly();
}
