/**
 * Lightweight CI/regression timings for critical math parsing + evaluation paths.
 * Not linked into the main app — opt-in via -DBLOP_BUILD_AUTOMATION=ON.
 */

#include "MathEvaluator.h"
#include "MathExpressionParser.h"

#include <QtGlobal>
#include <QString>

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace {

double envDouble(const char* key, double fallback) {
    const QByteArray v = qgetenv(key);
    if (v.isEmpty())
        return fallback;
    bool ok = false;
    const double d = QByteArray{v}.toDouble(&ok);
    return ok ? d : fallback;
}

int envInt(const char* key, int fallback) {
    const QByteArray v = qgetenv(key);
    if (v.isEmpty())
        return fallback;
    bool ok = false;
    const int i = QByteArray{v}.toInt(&ok);
    return ok ? i : fallback;
}

bool envBoolTrue(const char* key) {
    const QByteArray v = qgetenv(key);
    if (v.isEmpty())
        return false;
    QByteArray upper = v.toUpper();
    return upper == QByteArrayLiteral("1") || upper == QByteArrayLiteral("TRUE") ||
        upper == QByteArrayLiteral("YES") || upper == QByteArrayLiteral("ON");
}

} // namespace

int main() {
    // Representative expression exercised in graphs / math UI (no user data touched).
    const QString expr(
        QStringLiteral("sin(x)*x^2 + sqrt(abs(x))*log(x^2 + 1)"));

    const int parseRuns =
        envInt("BLOP_BENCH_PARSE_RUNS", envInt("BLOP_BENCH_RUNS", 2000));

    ParsedExpression exprCache;
    const auto tp0 = std::chrono::steady_clock::now();
    for (int i = 0; i < parseRuns; ++i) {
        // Re-parse cold each iteration to mimic repeated user edits (worst-ish case).
        exprCache = MathExpressionParser::parseFunctionExpression(expr);
        if (!exprCache.ok) {
            std::cerr << "parse_failed: "
                      << exprCache.error.toUtf8().constData() << '\n';
            return 2;
        }
    }
    const auto tp1 = std::chrono::steady_clock::now();

    const int evalRuns = envInt(
        "BLOP_BENCH_EVAL_RUNS",
        std::max(20000, parseRuns * static_cast<int>(50)));
    double acc = 0.0;
    for (int i = 0; i < evalRuns; ++i) {
        const double x = (static_cast<double>(i % 200) / 17.0) - 3.0;
        acc += MathEvaluator::evalAt(exprCache, x);
    }
    const auto tp2 = std::chrono::steady_clock::now();

    const double parseMs =
        std::chrono::duration<double, std::milli>(tp1 - tp0).count();
    const double evalMs =
        std::chrono::duration<double, std::milli>(tp2 - tp1).count();

    const bool md = envBoolTrue("GITHUB_ACTIONS");
    if (md) {
        std::cout << "## Micro-benchmark `blop_benchmark_math`\n\n";
        std::cout << "| Metric | Value |\n| --- | ---: |\n";
        std::cout << "| parse_ms | " << parseMs << " |\n";
        std::cout << "| parse_runs | " << parseRuns << " |\n";
        std::cout << "| eval_ms | " << evalMs << " |\n";
        std::cout << "| eval_runs | " << evalRuns << " |\n";
        std::cout << "| eval_sum | " << acc << " |\n\n";
    } else {
        std::cout << "blop_benchmark_math"
                  << " parse_ms=" << parseMs << " parse_runs=" << parseRuns
                  << " eval_ms=" << evalMs << " eval_runs=" << evalRuns
                  << " eval_sum=" << acc << '\n';
    }

    const double parseMax = envDouble("BLOP_BENCH_PARSE_MAX_MS", 0.0);
    const double evalMax = envDouble("BLOP_BENCH_EVAL_MAX_MS", 0.0);
    if (parseMax > 0.0 && parseMs > parseMax) {
        std::cerr << "threshold_exceeded: parse_ms " << parseMs << " > " << parseMax
                  << '\n';
        return 3;
    }
    if (evalMax > 0.0 && evalMs > evalMax) {
        std::cerr << "threshold_exceeded: eval_ms " << evalMs << " > " << evalMax
                  << '\n';
        return 4;
    }

    return 0;
}
