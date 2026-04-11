#include "MathExpressionParser.h"

#include <QtMath>
#include <QRegularExpression>
#include <cmath>
#include <memory>

namespace {

enum class NodeType {
    Constant,
    Variable,
    UnaryMinus,
    Add,
    Sub,
    Mul,
    Div,
    Pow,
    FuncSin,
    FuncCos,
    FuncTan,
    FuncExp,
    FuncLog,
    FuncSqrt,
    FuncAbs
};

struct Node {
    NodeType type{NodeType::Constant};
    double value{0.0};
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
};

struct AstParseResult {
    bool ok{false};
    QString error;
    QString normalizedInput;
    std::unique_ptr<Node> root;
};

class Parser {
public:
    explicit Parser(QString src) : m_src(std::move(src)) {}

    AstParseResult parseRoot() {
        AstParseResult ar;
        QString normalized = normalizeInput(m_src);
        if (normalized.startsWith("y", Qt::CaseInsensitive) || normalized.startsWith("f(", Qt::CaseInsensitive)) {
            const int eq = normalized.indexOf('=');
            if (eq >= 0)
                normalized = normalized.mid(eq + 1).trimmed();
        }
        m_src = normalized;
        m_pos = 0;
        m_error.clear();
        ar.root = parseExpression();
        skipWs();
        if (!ar.root || m_pos != m_src.size()) {
            ar.error = m_error.isEmpty() ? QStringLiteral("Ungueltige Eingabe") : m_error;
            ar.normalizedInput = normalized;
            return ar;
        }
        ar.ok = true;
        ar.normalizedInput = normalized;
        return ar;
    }

    ParsedExpression parse() {
        ParsedExpression out;
        AstParseResult ar = parseRoot();
        out.normalizedInput = ar.normalizedInput;
        if (!ar.ok || !ar.root) {
            out.error = ar.error.isEmpty() ? QStringLiteral("Ungueltige Eingabe") : ar.error;
            return out;
        }
        out.ok = true;
        // std::function verlangt ein copy-constructible Target.
        // unique_ptr-Capture macht das Lambda move-only; daher shared_ptr.
        auto sharedRoot = std::shared_ptr<Node>(ar.root.release());
        out.fn = [sharedRoot](double x) -> double {
            std::function<double(const Node*, double)> eval = [&](const Node* n, double xv) -> double {
                if (!n)
                    return qQNaN();
                switch (n->type) {
                case NodeType::Constant: return n->value;
                case NodeType::Variable: return xv;
                case NodeType::UnaryMinus: return -eval(n->left.get(), xv);
                case NodeType::Add: return eval(n->left.get(), xv) + eval(n->right.get(), xv);
                case NodeType::Sub: return eval(n->left.get(), xv) - eval(n->right.get(), xv);
                case NodeType::Mul: return eval(n->left.get(), xv) * eval(n->right.get(), xv);
                case NodeType::Div: {
                    const double d = eval(n->right.get(), xv);
                    if (qFuzzyIsNull(d))
                        return qQNaN();
                    return eval(n->left.get(), xv) / d;
                }
                case NodeType::Pow: return std::pow(eval(n->left.get(), xv), eval(n->right.get(), xv));
                case NodeType::FuncSin: return std::sin(eval(n->left.get(), xv));
                case NodeType::FuncCos: return std::cos(eval(n->left.get(), xv));
                case NodeType::FuncTan: return std::tan(eval(n->left.get(), xv));
                case NodeType::FuncExp: return std::exp(eval(n->left.get(), xv));
                case NodeType::FuncLog: {
                    const double v = eval(n->left.get(), xv);
                    if (v <= 0.0)
                        return qQNaN();
                    return std::log(v);
                }
                case NodeType::FuncSqrt: {
                    const double v = eval(n->left.get(), xv);
                    if (v < 0.0)
                        return qQNaN();
                    return std::sqrt(v);
                }
                case NodeType::FuncAbs: return std::fabs(eval(n->left.get(), xv));
                }
                return qQNaN();
            };
            return eval(sharedRoot.get(), x);
        };
        return out;
    }

private:
    static QString normalizeInput(const QString& raw) {
        QString s = raw.trimmed();
        s.replace(QChar(0x2212), '-'); // unicode minus
        s.replace(QChar(0x2013), '-');
        s.replace(QChar(0x2014), '-');
        s.replace(QChar(0x00D7), '*'); // multiplication signs
        s.replace(QChar(0x22C5), '*');
        s.replace(QChar(0x00F7), '/');

        s.replace(QRegularExpression("\\bsln\\s*\\(", QRegularExpression::CaseInsensitiveOption), "sin(");
        s.replace(QRegularExpression("\\bSIN\\b"), "sin");
        s.replace(QRegularExpression("\\bSin\\b"), "sin");
        s.replace(QRegularExpression("\\bCos\\b"), "cos");
        s.replace(QRegularExpression("\\bTan\\b"), "tan");
        s.replace(QRegularExpression("\\btg\\s*\\(", QRegularExpression::CaseInsensitiveOption), "tan(");
        s.replace(QRegularExpression("\\bln\\s*\\(", QRegularExpression::CaseInsensitiveOption), "log(");
        s.replace(QRegularExpression("\\bsen\\s*\\(", QRegularExpression::CaseInsensitiveOption), "sin(");
        s.replace(QRegularExpression("\\bctg\\s*\\(", QRegularExpression::CaseInsensitiveOption), "tan(");
        s.replace(QRegularExpression("\\bSqrt\\b"), "sqrt");
        s.replace(QRegularExpression("\\broot\\s*\\(", QRegularExpression::CaseInsensitiveOption), "sqrt(");
        s.replace(QRegularExpression("\\bpi\\b", QRegularExpression::CaseInsensitiveOption), "pi");
        s.replace('{', '(');
        s.replace('}', ')');
        s.replace('[', '(');
        s.replace(']', ')');

        // decimal comma between digits -> dot (repeat for multi-digit mantissas)
        QString prev;
        do {
            prev = s;
            s.replace(QRegularExpression("(\\d),(\\d)"), "\\1.\\2");
        } while (s != prev);

        insertImplicitMultiplication(s);
        return s.trimmed();
    }

    static void insertImplicitMultiplication(QString &s) {
        for (int iter = 0; iter < 10; ++iter) {
            const QString before = s;
            // 2x, 3X -> 2*x
            s.replace(QRegularExpression("(\\d)([xX])"), "\\1*\\2");
            // 2( -> 2*(
            s.replace(QRegularExpression("(\\d)\\("), "\\1*(");
            // )3 -> )*3
            s.replace(QRegularExpression("\\)(\\d)"), ")*\\1");
            // )( -> )*(
            s.replace(QStringLiteral(")("), QStringLiteral(")*("));
            // )x -> )*x
            s.replace(QRegularExpression("\\)([xX])"), ")*\\1");
            // x2 -> x*2 (schueler notation)
            s.replace(QRegularExpression("([xX])(\\d)"), "\\1*\\2");
            // 3sin( -> 3*sin(
            s.replace(QRegularExpression(R"((\d)(sin|cos|tan|sqrt|log|exp|abs)\s*\()",
                                       QRegularExpression::CaseInsensitiveOption),
                      "\\1*\\2(");
            if (s == before)
                break;
        }
    }

    std::unique_ptr<Node> parseExpression() {
        auto lhs = parseTerm();
        if (!lhs)
            return {};
        while (true) {
            skipWs();
            if (match('+')) {
                auto rhs = parseTerm();
                if (!rhs)
                    return {};
                auto n = std::make_unique<Node>();
                n->type = NodeType::Add;
                n->left = std::move(lhs);
                n->right = std::move(rhs);
                lhs = std::move(n);
            } else if (match('-')) {
                auto rhs = parseTerm();
                if (!rhs)
                    return {};
                auto n = std::make_unique<Node>();
                n->type = NodeType::Sub;
                n->left = std::move(lhs);
                n->right = std::move(rhs);
                lhs = std::move(n);
            } else {
                break;
            }
        }
        return lhs;
    }

    std::unique_ptr<Node> parseTerm() {
        auto lhs = parsePower();
        if (!lhs)
            return {};
        while (true) {
            skipWs();
            if (match('*')) {
                auto rhs = parsePower();
                if (!rhs)
                    return {};
                auto n = std::make_unique<Node>();
                n->type = NodeType::Mul;
                n->left = std::move(lhs);
                n->right = std::move(rhs);
                lhs = std::move(n);
            } else if (match('/')) {
                auto rhs = parsePower();
                if (!rhs)
                    return {};
                auto n = std::make_unique<Node>();
                n->type = NodeType::Div;
                n->left = std::move(lhs);
                n->right = std::move(rhs);
                lhs = std::move(n);
            } else {
                break;
            }
        }
        return lhs;
    }

    std::unique_ptr<Node> parsePower() {
        auto base = parseUnary();
        if (!base)
            return {};
        skipWs();
        if (match('^')) {
            auto exp = parsePower();
            if (!exp)
                return {};
            auto n = std::make_unique<Node>();
            n->type = NodeType::Pow;
            n->left = std::move(base);
            n->right = std::move(exp);
            return n;
        }
        return base;
    }

    std::unique_ptr<Node> parseUnary() {
        skipWs();
        if (match('-')) {
            auto n = std::make_unique<Node>();
            n->type = NodeType::UnaryMinus;
            n->left = parseUnary();
            if (!n->left)
                return {};
            return n;
        }
        return parsePrimary();
    }

    std::unique_ptr<Node> parsePrimary() {
        skipWs();
        if (match('(')) {
            auto n = parseExpression();
            skipWs();
            if (!match(')')) {
                m_error = QStringLiteral("')' erwartet");
                return {};
            }
            return n;
        }
        if (peekIsLetter()) {
            const QString id = parseIdentifier().toLower();
            if (id == "x") {
                auto n = std::make_unique<Node>();
                n->type = NodeType::Variable;
                return n;
            }
            if (id == "pi") {
                auto n = std::make_unique<Node>();
                n->type = NodeType::Constant;
                n->value = M_PI;
                return n;
            }
            skipWs();
            if (!match('(')) {
                m_error = QStringLiteral("Funktion '%1' braucht Klammern").arg(id);
                return {};
            }
            auto arg = parseExpression();
            skipWs();
            if (!match(')')) {
                m_error = QStringLiteral("')' nach Funktion erwartet");
                return {};
            }
            auto n = std::make_unique<Node>();
            n->left = std::move(arg);
            if (id == "sin") n->type = NodeType::FuncSin;
            else if (id == "cos") n->type = NodeType::FuncCos;
            else if (id == "tan") n->type = NodeType::FuncTan;
            else if (id == "exp") n->type = NodeType::FuncExp;
            else if (id == "log") n->type = NodeType::FuncLog;
            else if (id == "sqrt") n->type = NodeType::FuncSqrt;
            else if (id == "abs") n->type = NodeType::FuncAbs;
            else {
                m_error = QStringLiteral("Unbekannte Funktion: %1").arg(id);
                return {};
            }
            return n;
        }
        return parseNumber();
    }

    std::unique_ptr<Node> parseNumber() {
        skipWs();
        const int start = m_pos;
        bool dot = false;
        while (m_pos < m_src.size()) {
            const QChar c = m_src[m_pos];
            if (c.isDigit()) {
                ++m_pos;
            } else if (c == '.' && !dot) {
                dot = true;
                ++m_pos;
            } else {
                break;
            }
        }
        if (start == m_pos) {
            m_error = QStringLiteral("Zahl/Variable erwartet");
            return {};
        }
        bool ok = false;
        const double v = m_src.mid(start, m_pos - start).toDouble(&ok);
        if (!ok) {
            m_error = QStringLiteral("Ungueltige Zahl");
            return {};
        }
        auto n = std::make_unique<Node>();
        n->type = NodeType::Constant;
        n->value = v;
        return n;
    }

    void skipWs() {
        while (m_pos < m_src.size() && m_src[m_pos].isSpace())
            ++m_pos;
    }
    bool match(QChar c) {
        skipWs();
        if (m_pos < m_src.size() && m_src[m_pos] == c) {
            ++m_pos;
            return true;
        }
        return false;
    }
    bool peekIsLetter() const {
        return m_pos < m_src.size() && m_src[m_pos].isLetter();
    }
    QString parseIdentifier() {
        const int start = m_pos;
        while (m_pos < m_src.size() && (m_src[m_pos].isLetter() || m_src[m_pos].isDigit()))
            ++m_pos;
        return m_src.mid(start, m_pos - start);
    }

    QString m_src;
    int m_pos{0};
    QString m_error;
};

// ─── Symbolic derivative (display only) ─────────────────────────────────────

std::unique_ptr<Node> cloneTree(const Node* n) {
    if (!n)
        return {};
    auto c = std::make_unique<Node>();
    c->type = n->type;
    c->value = n->value;
    if (n->left)
        c->left = cloneTree(n->left.get());
    if (n->right)
        c->right = cloneTree(n->right.get());
    return c;
}

std::unique_ptr<Node> makeConstant(double v) {
    auto n = std::make_unique<Node>();
    n->type = NodeType::Constant;
    n->value = v;
    return n;
}

std::unique_ptr<Node> makeVariable() {
    auto n = std::make_unique<Node>();
    n->type = NodeType::Variable;
    return n;
}

std::unique_ptr<Node> wrapFunc(NodeType t, std::unique_ptr<Node> arg) {
    auto n = std::make_unique<Node>();
    n->type = t;
    n->left = std::move(arg);
    return n;
}

std::unique_ptr<Node> makeUnaryMinus(std::unique_ptr<Node> a) {
    if (!a)
        return {};
    auto n = std::make_unique<Node>();
    n->type = NodeType::UnaryMinus;
    n->left = std::move(a);
    return n;
}

std::unique_ptr<Node> makeAdd(std::unique_ptr<Node> a, std::unique_ptr<Node> b) {
    auto n = std::make_unique<Node>();
    n->type = NodeType::Add;
    n->left = std::move(a);
    n->right = std::move(b);
    return n;
}

std::unique_ptr<Node> makeSub(std::unique_ptr<Node> a, std::unique_ptr<Node> b) {
    auto n = std::make_unique<Node>();
    n->type = NodeType::Sub;
    n->left = std::move(a);
    n->right = std::move(b);
    return n;
}

std::unique_ptr<Node> makeMul(std::unique_ptr<Node> a, std::unique_ptr<Node> b) {
    auto n = std::make_unique<Node>();
    n->type = NodeType::Mul;
    n->left = std::move(a);
    n->right = std::move(b);
    return n;
}

std::unique_ptr<Node> makeDiv(std::unique_ptr<Node> a, std::unique_ptr<Node> b) {
    auto n = std::make_unique<Node>();
    n->type = NodeType::Div;
    n->left = std::move(a);
    n->right = std::move(b);
    return n;
}

std::unique_ptr<Node> makePow(std::unique_ptr<Node> a, std::unique_ptr<Node> b) {
    auto n = std::make_unique<Node>();
    n->type = NodeType::Pow;
    n->left = std::move(a);
    n->right = std::move(b);
    return n;
}

bool isConstZero(const Node* n) {
    return n && n->type == NodeType::Constant && qFuzzyIsNull(n->value);
}

bool isConstOne(const Node* n) {
    return n && n->type == NodeType::Constant && qFuzzyCompare(n->value, 1.0);
}

std::unique_ptr<Node> differentiate(const Node* n) {
    if (!n)
        return {};
    switch (n->type) {
    case NodeType::Constant:
        return makeConstant(0);
    case NodeType::Variable:
        return makeConstant(1);
    case NodeType::UnaryMinus: {
        auto du = differentiate(n->left.get());
        if (!du)
            return {};
        return makeUnaryMinus(std::move(du));
    }
    case NodeType::Add: {
        auto dl = differentiate(n->left.get());
        auto dr = differentiate(n->right.get());
        if (!dl || !dr)
            return {};
        return makeAdd(std::move(dl), std::move(dr));
    }
    case NodeType::Sub: {
        auto dl = differentiate(n->left.get());
        auto dr = differentiate(n->right.get());
        if (!dl || !dr)
            return {};
        return makeSub(std::move(dl), std::move(dr));
    }
    case NodeType::Mul: {
        auto du = differentiate(n->left.get());
        auto dv = differentiate(n->right.get());
        if (!du || !dv)
            return {};
        return makeAdd(makeMul(std::move(du), cloneTree(n->right.get())),
                       makeMul(cloneTree(n->left.get()), std::move(dv)));
    }
    case NodeType::Div: {
        auto du = differentiate(n->left.get());
        auto dv = differentiate(n->right.get());
        if (!du || !dv)
            return {};
        auto u = cloneTree(n->left.get());
        auto v = cloneTree(n->right.get());
        auto num = makeSub(makeMul(std::move(du), cloneTree(v.get())),
                           makeMul(std::move(u), std::move(dv)));
        auto den = makePow(std::move(v), makeConstant(2));
        return makeDiv(std::move(num), std::move(den));
    }
    case NodeType::Pow: {
        if (!n->right || n->right->type != NodeType::Constant)
            return {};
        const double c = n->right->value;
        auto du = differentiate(n->left.get());
        if (!du)
            return {};
        if (qFuzzyIsNull(c))
            return makeConstant(0);
        if (qFuzzyCompare(c, 1.0))
            return std::move(du);
        auto coeff = makeConstant(c);
        auto powPart = makePow(cloneTree(n->left.get()), makeConstant(c - 1.0));
        return makeMul(std::move(coeff), makeMul(std::move(powPart), std::move(du)));
    }
    case NodeType::FuncSin: {
        auto du = differentiate(n->left.get());
        if (!du)
            return {};
        return makeMul(wrapFunc(NodeType::FuncCos, cloneTree(n->left.get())), std::move(du));
    }
    case NodeType::FuncCos: {
        auto du = differentiate(n->left.get());
        if (!du)
            return {};
        return makeMul(makeUnaryMinus(wrapFunc(NodeType::FuncSin, cloneTree(n->left.get()))), std::move(du));
    }
    case NodeType::FuncTan: {
        auto du = differentiate(n->left.get());
        if (!du)
            return {};
        auto cosU = wrapFunc(NodeType::FuncCos, cloneTree(n->left.get()));
        auto den = makePow(std::move(cosU), makeConstant(2));
        auto sec2 = makeDiv(makeConstant(1), std::move(den));
        return makeMul(std::move(sec2), std::move(du));
    }
    case NodeType::FuncExp: {
        auto du = differentiate(n->left.get());
        if (!du)
            return {};
        return makeMul(wrapFunc(NodeType::FuncExp, cloneTree(n->left.get())), std::move(du));
    }
    case NodeType::FuncLog: {
        auto du = differentiate(n->left.get());
        if (!du)
            return {};
        return makeMul(makeDiv(makeConstant(1), cloneTree(n->left.get())), std::move(du));
    }
    case NodeType::FuncSqrt: {
        auto du = differentiate(n->left.get());
        if (!du)
            return {};
        auto two = makeConstant(2);
        auto root = wrapFunc(NodeType::FuncSqrt, cloneTree(n->left.get()));
        auto den = makeMul(std::move(two), std::move(root));
        return makeMul(makeDiv(makeConstant(1), std::move(den)), std::move(du));
    }
    case NodeType::FuncAbs:
        return {};
    }
    return {};
}

std::unique_ptr<Node> simplify(std::unique_ptr<Node> n) {
    if (!n)
        return {};
    switch (n->type) {
    case NodeType::Constant:
    case NodeType::Variable:
        return n;
    case NodeType::UnaryMinus: {
        n->left = simplify(std::move(n->left));
        if (!n->left)
            return {};
        if (n->left->type == NodeType::Constant) {
            n->left->value = -n->left->value;
            return std::move(n->left);
        }
        if (n->left->type == NodeType::UnaryMinus && n->left->left)
            return std::move(n->left->left);
        return n;
    }
    case NodeType::Add: {
        n->left = simplify(std::move(n->left));
        n->right = simplify(std::move(n->right));
        if (isConstZero(n->left.get()))
            return std::move(n->right);
        if (isConstZero(n->right.get()))
            return std::move(n->left);
        if (n->left->type == NodeType::Constant && n->right->type == NodeType::Constant) {
            n->left->value += n->right->value;
            return std::move(n->left);
        }
        return n;
    }
    case NodeType::Sub: {
        n->left = simplify(std::move(n->left));
        n->right = simplify(std::move(n->right));
        if (isConstZero(n->right.get()))
            return std::move(n->left);
        if (n->left->type == NodeType::Constant && n->right->type == NodeType::Constant) {
            n->left->value -= n->right->value;
            return std::move(n->left);
        }
        return n;
    }
    case NodeType::Mul: {
        n->left = simplify(std::move(n->left));
        n->right = simplify(std::move(n->right));
        if (isConstZero(n->left.get()) || isConstZero(n->right.get()))
            return makeConstant(0);
        if (isConstOne(n->left.get()))
            return std::move(n->right);
        if (isConstOne(n->right.get()))
            return std::move(n->left);
        if (n->left->type == NodeType::Constant && n->right->type == NodeType::Constant) {
            n->left->value *= n->right->value;
            return std::move(n->left);
        }
        return n;
    }
    case NodeType::Div: {
        n->left = simplify(std::move(n->left));
        n->right = simplify(std::move(n->right));
        if (isConstOne(n->right.get()))
            return std::move(n->left);
        if (isConstZero(n->left.get()))
            return makeConstant(0);
        if (n->left->type == NodeType::Constant && n->right->type == NodeType::Constant &&
            !qFuzzyIsNull(n->right->value)) {
            n->left->value /= n->right->value;
            return std::move(n->left);
        }
        return n;
    }
    case NodeType::Pow: {
        n->left = simplify(std::move(n->left));
        n->right = simplify(std::move(n->right));
        if (isConstZero(n->right.get()) || isConstOne(n->left.get()))
            return makeConstant(1);
        if (isConstOne(n->right.get()))
            return std::move(n->left);
        if (n->left->type == NodeType::Constant && n->right->type == NodeType::Constant) {
            n->left->value = std::pow(n->left->value, n->right->value);
            return std::move(n->left);
        }
        return n;
    }
    case NodeType::FuncSin:
    case NodeType::FuncCos:
    case NodeType::FuncTan:
    case NodeType::FuncExp:
    case NodeType::FuncLog:
    case NodeType::FuncSqrt:
    case NodeType::FuncAbs:
        n->left = simplify(std::move(n->left));
        return n;
    }
    return n;
}

static QString funcName(NodeType t) {
    switch (t) {
    case NodeType::FuncSin:
        return QStringLiteral("sin");
    case NodeType::FuncCos:
        return QStringLiteral("cos");
    case NodeType::FuncTan:
        return QStringLiteral("tan");
    case NodeType::FuncExp:
        return QStringLiteral("exp");
    case NodeType::FuncLog:
        return QStringLiteral("log");
    case NodeType::FuncSqrt:
        return QStringLiteral("sqrt");
    case NodeType::FuncAbs:
        return QStringLiteral("abs");
    default:
        return {};
    }
}

bool needParenForAddSub(const Node* c) {
    return c && (c->type == NodeType::Add || c->type == NodeType::Sub);
}

bool needParenForMulDiv(const Node* c) {
    return c && (c->type == NodeType::Add || c->type == NodeType::Sub || c->type == NodeType::Mul ||
                 c->type == NodeType::Div);
}

QString nodeToString(const Node* n) {
    if (!n)
        return {};
    switch (n->type) {
    case NodeType::Constant: {
        if (qFuzzyCompare(n->value, M_PI))
            return QStringLiteral("pi");
        const double r = std::round(n->value);
        if (qFuzzyCompare(n->value, r) && std::fabs(r) <= 9e15)
            return QString::number(static_cast<qint64>(r));
        return QString::number(n->value, 'g', 12);
    }
    case NodeType::Variable:
        return QStringLiteral("x");
    case NodeType::UnaryMinus: {
        QString inner = nodeToString(n->left.get());
        if (needParenForAddSub(n->left.get()))
            inner = QLatin1Char('(') + inner + QLatin1Char(')');
        return QStringLiteral("-") + inner;
    }
    case NodeType::Add:
        return nodeToString(n->left.get()) + QStringLiteral("+") + nodeToString(n->right.get());
    case NodeType::Sub: {
        QString L = nodeToString(n->left.get());
        QString R = nodeToString(n->right.get());
        if (needParenForAddSub(n->right.get()))
            R = QLatin1Char('(') + R + QLatin1Char(')');
        return L + QStringLiteral("-") + R;
    }
    case NodeType::Mul: {
        QString L = nodeToString(n->left.get());
        QString R = nodeToString(n->right.get());
        if (needParenForAddSub(n->left.get()))
            L = QLatin1Char('(') + L + QLatin1Char(')');
        if (needParenForAddSub(n->right.get()))
            R = QLatin1Char('(') + R + QLatin1Char(')');
        return L + QStringLiteral("*") + R;
    }
    case NodeType::Div: {
        QString L = nodeToString(n->left.get());
        QString R = nodeToString(n->right.get());
        if (needParenForMulDiv(n->left.get()))
            L = QLatin1Char('(') + L + QLatin1Char(')');
        if (needParenForMulDiv(n->right.get()))
            R = QLatin1Char('(') + R + QLatin1Char(')');
        return L + QStringLiteral("/") + R;
    }
    case NodeType::Pow: {
        QString L = nodeToString(n->left.get());
        QString R = nodeToString(n->right.get());
        if (needParenForMulDiv(n->left.get()))
            L = QLatin1Char('(') + L + QLatin1Char(')');
        if (n->right->type != NodeType::Constant && n->right->type != NodeType::Variable &&
            n->right->type != NodeType::UnaryMinus)
            R = QLatin1Char('(') + R + QLatin1Char(')');
        return L + QStringLiteral("^") + R;
    }
    case NodeType::FuncSin:
    case NodeType::FuncCos:
    case NodeType::FuncTan:
    case NodeType::FuncExp:
    case NodeType::FuncLog:
    case NodeType::FuncSqrt:
    case NodeType::FuncAbs:
        return funcName(n->type) + QLatin1Char('(') + nodeToString(n->left.get()) + QLatin1Char(')');
    }
    return {};
}

} // namespace

ParsedExpression MathExpressionParser::parseFunctionExpression(const QString& input) {
    Parser p(input);
    return p.parse();
}

QString MathExpressionParser::symbolicDerivativeString(const QString& input) {
    Parser p(input);
    AstParseResult ar = p.parseRoot();
    if (!ar.ok || !ar.root)
        return {};
    auto d = differentiate(ar.root.get());
    if (!d)
        return {};
    d = simplify(std::move(d));
    d = simplify(std::move(d));
    return nodeToString(d.get());
}
