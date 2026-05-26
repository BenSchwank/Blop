# ADR: Client observability and native crash reporting (Sentry)

## Status

Accepted. Phase 0 (observability envelope + scaffolding) shipped in-tree **Phase 1 (milestone)** links **sentry-native** (pinned via CMake `FetchContent`) for real native crash capture on Windows and Android where the toolchain permits.

## Context

Blop ships as Qt 6 / C++17 on Windows (WebEngine + widgets) and Android (Gradle + JNI + native `.so`). We need:

- One **observability contract** stamped at compile time / readable at startup: platform slug, Qt package version vs runtime `qVersion()`, app version (`git describe` lineage), deterministic **Git SHA** for aligning CI artifacts with crash backends, Play **versionCode** on Android, build flavor/run id, and **consent placeholders** baked as integers (policy layer may still gate uploads at runtime).

- A pragmatic **native crash** path that works with CMake and does not fork the product into incompatible Android vs Windows stacks.

## Decision

1. **Observability envelope**  
   CMake generates `blop_observability_build.h` from `blop_observability_build.h.in`, then `blopObservabilityMeta()` + `blopLogObservabilityBootstrap()` emit a single stderr line and a `qInfo()` record early in `main.cpp` (immediately after `QApplication` construction).

2. **Crash backend: Sentry + sentry-native**  
   We standardize on **Sentry + `sentry-native`** for cross-platform native crashes and symbolication. It is wired when `-DBLOP_ENABLE_SENTRY=ON` (see CI).  
   - **Windows**: default **Crashpad** backend + **winhttp** transport; `crashpad_handler.exe` is copied next to `Blop.exe` at build time and again into the deployment folder in CI.  
   - **Android**: **inproc** backend (Crashpad is unsupported on Android in sentry-native). **HTTP transport** uses **libcurl** only if `find_package(CURL)` succeeds for the NDK toolchain; otherwise CMake falls back to **`SENTRY_TRANSPORT=none`** with a **warning**, and events will not upload until you complete the manual path below (this is common for Qt-Android-only CMake builds).

3. **Sentry release / dist**  
   The Sentry **release** string is **`BLOP_SENTRY_RELEASE_STR`** in `blop_observability_build.h`, equal to  
   `git describe --tags --always` + `+` + full git SHA + `+build` + `BLOP_OBS_BUILD_NUMBER` (same inputs as the observability envelope).  
   **Environment** is `BLOP_OBS_BUILD_FLAVOR`; **dist** is `BLOP_OBS_BUILD_NUMBER`.  
   GitHub Actions `sentry-cli debug-files upload` uses the same release formula so symbols match stack traces.

4. **DSN precedence (grep: `SENTRY_DSN`, `BLOP_SENTRY_DSN`)**  
   - If the environment variable **`SENTRY_DSN`** is set and non-empty, it is used.  
   - Otherwise, if CMake **`BLOP_SENTRY_DSN`** was set at configure time, that value is baked into **`BLOP_SENTRY_COMPILE_DSN`** and used.  
   - If neither is set, `sentry_init` is skipped and a log line explains that uploads are disabled (the library may still be linked in CI for symbol workflow).

5. **Optional local test crash**  
   Configure with `-DBLOP_SENTRY_FORCE_TEST_CRASH=ON` to `abort()` immediately after a successful `sentry_init` (guarded define **`BLOP_SENTRY_FORCE_TEST_CRASH`**).

6. **Fetch failure / opt-out**  
   If the `sentry-native` git fetch fails at configure time, disable Sentry with **`-DBLOP_ENABLE_SENTRY=OFF`** or fix network / use an offline CMake dependency cache. There is no silent stub: the goal is a clear CMake error from `FetchContent`, not a broken build pretending success.

## Sentry cloud setup

1. Create a **Sentry** project for the native SDK (platform “Native” or closest match).
2. **Required GitHub Actions secrets** for symbol upload (already guarded with `if: secrets`):  
   - `SENTRY_AUTH_TOKEN` (auth token with `project:releases` / org access as per Sentry docs)  
   - `SENTRY_ORG`  
   - `SENTRY_PROJECT`  
3. Optional: **`SENTRY_URL`** if you use self-hosted Sentry (same semantics as `sentry-cli`).
4. For **crash ingestion** from real devices/installers set **`SENTRY_DSN`** at runtime or **`BLOP_SENTRY_DSN`** at CMake configure (DSN is not required for `debug-files upload` in CI).

## CI symbol upload (strict mode)

Workflows use **`continue-on-error: true`** on the upload steps for a soft first rollout. To make uploads **fail the job** on error, remove `continue-on-error: true` from:

- `.github/workflows/windows_build.yml` → “Upload Windows debug files to Sentry”
- `.github/workflows/android_build.yml` → “Upload Android native debug artifacts to Sentry”

Upload arguments use **`--release "$(git describe --tags --always)+${GITHUB_SHA}+build${GITHUB_RUN_NUMBER}"`**, matching **`BLOP_SENTRY_RELEASE_STR`** in CMake for the same commit and `github.run_number` / `BLOP_OBS_BUILD_NUMBER`.

## Android: when `SENTRY_TRANSPORT=none`

If CMake warns that **libcurl** was not found for Android, native events will not be uploaded. **Manual follow-up (pick one):**

1. **Gradle + Prefab (recommended for production)**  
   Enable `buildFeatures { prefab true }`, add **`io.sentry:sentry-native-ndk`** (version aligned with your `BLOP_SENTRY_NATIVE_VERSION` / Sentry docs), and link the prefab targets from the Android CMake graph **or** rely on the Sentry Android SDK’s NDK package (see [sentry-native `ndk/README.md`](https://github.com/getsentry/sentry-native/blob/master/ndk/README.md)).

2. **Supply libcurl for the NDK toolchain**  
   CI runs **`scripts/ci/build_android_curl_mbedtls.sh`**, installs per-ABI static **curl + mbedTLS** under **`BLOP_ANDROID_CURL_INSTALL_ROOT/<abi>/`**, and CMake passes that cache variable so **`find_package(CURL)`** resolves before **`FetchContent_MakeAvailable(sentry_native)`**.  
   Locally or on forks: run the same script (Git Bash/WSL), then `-DBLOP_ANDROID_CURL_INSTALL_ROOT=/your/prefix`.

Until one of these is done, keep **`BLOP_ENABLE_SENTRY=ON`** only if local crash capture / symbol upload testing is still valuable; otherwise turn it off for Android-only CI to avoid confusion.

## Consequences

- Windows crash reports require **PDBs + Crashpad handler** beside the exe; CI uploads `Blop.exe` / `Blop.pdb` / `crashpad_handler.exe` (+ handler PDB when present).
- Android symbolication requires uploading the built **`libBlop.so`** artifacts (CI collects all `libBlop.so` under `build/`).
- Play signing keys and Sentry credentials stay out of the repo; uploads remain secret-guarded.

## Grep-friendly reference

- **`SENTRY_DSN`** — runtime env, highest precedence for DSN.  
- **`BLOP_SENTRY_DSN`** — CMake cache string, baked as **`BLOP_SENTRY_COMPILE_DSN`**.  
- **`BLOP_ENABLE_SENTRY`** — master switch; enables FetchContent + link.  
- **`BLOP_SENTRY_NATIVE_VERSION`** — pinned sentry-native tag (default `0.14.0`).  
- **`BLOP_SENTRY_ANDROID_TRANSPORT`** — `curl` (try `find_package(CURL)`) or `none` (skip curl lookup).  
- **`BLOP_ANDROID_CURL_INSTALL_ROOT`** — root with **`${ABI}/lib/libcurl.a`** from **`scripts/ci/build_android_curl_mbedtls.sh`**.  
- **`BLOP_SENTRY_FORCE_TEST_CRASH`** — optional `abort()` after init.  
- **`BLOP_SENTRY_RELEASE_STR`** — release string shared with **`sentry-cli --release`**.

Firebase Crashlytics remains a deferred alternative if we later want a Play-only Android stack without Sentry.

## Related (CI / agentic loop)

Headless **regression micro-benchmarks** and optional strict timing gates are documented in [`agentic-automation.md`](agentic-automation.md) (`-DBLOP_BUILD_AUTOMATION=ON`, Windows CI only).
