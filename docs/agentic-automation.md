# Agentic automation (additive)

This project keeps **all automation opt-in** for local developers: default CMake builds only the `Blop` application. CI may enable extra targets for **signal** (benchmarks, future static analysis) without changing end-user features.

## Micro-benchmark (`blop_benchmark_math`)

- **CMake:** `-DBLOP_BUILD_AUTOMATION=ON` registers the `blop_benchmark_math` executable (Qt **Core** + existing `MathExpressionParser` / `MathEvaluator` sources). It is **not** linked into `Blop`.
- **Android toolchains:** the benchmark target is skipped (desktop/CI regression only).
- **Environment (optional strict mode):**
  - `BLOP_BENCH_PARSE_RUNS` / `BLOP_BENCH_RUNS` — parse iterations (default 2000).
  - `BLOP_BENCH_EVAL_RUNS` — evaluation iterations (default `max(20000, parse_runs*50)`).
  - `BLOP_BENCH_PARSE_MAX_MS` — if `> 0`, exit non-zero when parse phase exceeds this wall time.
  - `BLOP_BENCH_EVAL_MAX_MS` — same for eval phase.

On GitHub Actions, the benchmark prints a small Markdown table when `GITHUB_ACTIONS` is set (no user impact).

## Auto problem-solving (next layers)

Intended flow: **CI failure logs + optional Sentry incidents** → structured issue or agent → PR. No app feature is gated on telemetry; bench thresholds are **off** unless you set the `*_MAX_MS` variables.

## Control Center (Web-UI)

The **`control-center/`** Python app (FastAPI) runs on **`127.0.0.1:8765`**, proxies the GitHub REST API with a PAT, and can **dispatch** `workflow_dispatch` jobs (requires **`workflow_dispatch`** triggers on target workflows).

- **`automation_smoke.yml`** — quick connectivity test without Qt.  
- **`windows_build.yml` / `android_build.yml`** — full CI (now include `workflow_dispatch` for manual/API starts).

Setup: [`control-center/README.md`](../control-center/README.md). **Never** expose the PAT to browser clients — only the local server reads environment variables.

### Auto-fix ( später )

Hook the same backend to **`repository_dispatch`** or **`workflow_dispatch`** with inputs (`hint`, `sentry_issue_url`) calling a guarded agent job once PR bots are wired.
