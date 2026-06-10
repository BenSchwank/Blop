cmake_minimum_required(VERSION 3.18)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Android Setup
if(ANDROID)
    # compileSdk 34: GHA android-35 android.jar can be broken (aapt2 load error); targetSdk 35 still satisfies Play.
    set(ANDROID_PLATFORM "android-34")
endif()

# Fetch version from Git
execute_process(
    COMMAND git describe --tags --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Fallback if git fails
if(NOT GIT_VERSION)
    set(GIT_VERSION "3.17.1")
endif()

# Remove 'v' prefix if it exists for the project() call
string(REGEX REPLACE "^v" "" CLEAN_VERSION "${GIT_VERSION}")
# If there's a git hash suffix (-1-gX), the VERSION might be invalid for CMake project().
# We'll just extract the numeric part x.y.z.w
string(REGEX MATCH "^([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+|[0-9]+\\.[0-9]+\\.[0-9]+|[0-9]+\\.[0-9]+|[0-9]+)" SAFE_VERSION "${CLEAN_VERSION}")
if(NOT SAFE_VERSION)
    set(SAFE_VERSION "3.17.1")
endif()

project(Blop VERSION ${SAFE_VERSION} LANGUAGES C CXX)

# Observability overrides (crash backend release matching & CI stamping)
set(BLOP_OVERRIDE_GIT_SHA "" CACHE STRING "Git SHA stamped into binaries (recommended in CI)")
if(BLOP_OVERRIDE_GIT_SHA STREQUAL "")
    execute_process(
        COMMAND git rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE BLOP_RESOLVED_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(BLOP_RESOLVED_GIT_SHA "${BLOP_OVERRIDE_GIT_SHA}")
endif()
if(NOT BLOP_RESOLVED_GIT_SHA)
    set(BLOP_RESOLVED_GIT_SHA "unknown")
endif()

set(BLOP_OBS_BUILD_NUMBER "0" CACHE STRING "Build id/run number surfaced in observability envelope")
set(BLOP_OBS_BUILD_FLAVOR "dev" CACHE STRING "Build flavor identifier (release, ci_release, …)")
option(BLOP_OBS_CONSENT_CRASH_UPLOAD "Bake crash-upload consent=true into binaries" OFF)
option(BLOP_OBS_CONSENT_ANALYTICS "Bake analytics consent=true into binaries" OFF)

option(BLOP_ENABLE_SENTRY "Link sentry-native for native crash reporting (FetchContent pinned tag)" OFF)

if(BLOP_ENABLE_SENTRY)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/blop_sentry.cmake")
else()
    set(BLOP_SENTRY_COMPILE_DSN_ESC "")
endif()

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- BASIS-PAKETE ---
# Hier wird nach Qt6Config.cmake gesucht.
# Durch den Fix im YAML wird dieser Pfad nun korrekt via CMAKE_PREFIX_PATH gefunden.
find_package(Qt6 REQUIRED COMPONENTS Widgets Network NetworkAuth Quick QuickWidgets QuickControls2 Qml Multimedia MultimediaWidgets)

# --- WEB-MODUL WEICHE ---
if(ANDROID)
    # Android: Native WebView
    find_package(Qt6 REQUIRED COMPONENTS WebView)
else()
    # Desktop: WebEngine (Chromium) - REQUIRED
    find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets)
    # PDF support for import (optional, but present in standard Qt6 desktop install)
    find_package(Qt6 OPTIONAL_COMPONENTS Pdf)
endif()

qt_add_executable(Blop
    main.cpp

    # Observability (crash backend, build metadata, runtime envelope)
    src/observability/blop_observability.h
    src/observability/blop_observability.cpp
    src/observability/blop_crash_backend.h
    src/observability/blop_crash_backend.cpp
    src/observability/blop_diag.h
    src/observability/blop_diag.cpp

    # UI (windows, dialogs, overlays, custom views, toolbars)
    src/ui/mainwindow.ui
    src/ui/SettingsDialog.ui
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/canvasview.cpp
    src/ui/canvasview.h
    src/ui/freegridview.h
    src/ui/freegridview.cpp
    src/ui/multipagenoteview.cpp
    src/ui/multipagenoteview.h
    src/ui/editoroverlays.cpp
    src/ui/editoroverlays.h
    src/ui/settingsdialog.cpp
    src/ui/settingsdialog.h
    src/ui/graphaxissettingsdialog.cpp
    src/ui/graphaxissettingsdialog.h
    src/ui/graphlegenddock.cpp
    src/ui/graphlegenddock.h
    src/ui/graphquickactionpopup.cpp
    src/ui/graphquickactionpopup.h
    src/ui/newnotedialog.h
    src/ui/newnotedialog.cpp
    src/ui/profileeditordialog.h
    src/ui/profileeditordialog.cpp
    src/ui/markdowneditor.cpp
    src/ui/markdowneditor.h
    src/ui/moderntoolbar.cpp
    src/ui/moderntoolbar.h
    src/ui/androidphonetoolbar.cpp
    src/ui/androidphonetoolbar.h
    src/ui/modetoolbar.cpp
    src/ui/modetoolbar.h
    src/ui/introscreen.h
    src/ui/introscreen.cpp
    src/ui/overlayscrollindicator.h
    src/ui/overlayscrollindicator.cpp
    src/ui/blopripple.h
    src/ui/blopripple.cpp
    src/ui/blopstyle.h
    src/ui/blopstyle.cpp
    src/ui/blop_inwindow_menu.h
    src/ui/blop_inwindow_menu.cpp
    src/ui/blop_theme.h
    src/ui/blop_theme.cpp
    src/ui/blop_modal.h
    src/ui/blop_modal.cpp

    # Core (note model, page management, profiles, helper headers)
    src/core/notemanager.cpp
    src/core/notemanager.h
    src/core/noteeditor.cpp
    src/core/noteeditor.h
    src/core/pagemanager.h
    src/core/pagemanager.cpp
    src/core/uiprofilemanager.h
    src/core/uiprofilemanager.cpp
    src/core/Note.h
    src/core/PageItem.h
    src/core/UIStyles.h
    src/core/UIStyles.cpp
    src/core/ToolMode.h
    src/core/ToolSettings.h
    src/core/TransformOverlay.h
    src/core/UIStyles.h
    src/core/SelectionMenuIcons.h
    src/core/uiprofile.h

    # Android-specific rendering pipeline
    src/android/androidicons.h
    src/android/androidicons.cpp
    src/android/androidtiledelegate.h
    src/android/androidtiledelegate.cpp

    # Auth (Google OAuth)
    src/auth/googleauthmanager.cpp
    src/auth/googleauthmanager.h

    # Util
    src/util/uiscale.h
    src/util/uiscale.cpp

    # QML async image provider
    src/imageprovider/BlopAsyncImageProvider.h
    src/imageprovider/BlopAsyncImageProvider.cpp
    src/imageprovider/BlopImageResponse.h
    src/imageprovider/BlopImageResponse.cpp

    # Tools (already organized under tools/)
    tools/AbstractTool.h
    tools/ToolManager.h
    tools/ToolManager.cpp
    tools/ToolUIBridge.h
    tools/ToolFactory.h
    tools/StrokeItem.h
    tools/AbstractStrokeTool.h
    tools/WritingTools.h
    tools/EraserTool.h
    tools/LassoTool.h
    tools/ShapeTool.h
    tools/GraphCanvasItem.h
    tools/GraphCanvasItem.cpp
    tools/GraphFormulaZone.h
    tools/GraphFormulaZone.cpp
    tools/math/MathTypes.h
    tools/math/MathExpressionParser.h
    tools/math/MathExpressionParser.cpp
    tools/math/MathEvaluator.h
    tools/math/MathEvaluator.cpp
    tools/math/NumericAnalysis.h
    tools/math/NumericAnalysis.cpp
    tools/math/LatexToBlopConverter.h
    tools/math/LatexToBlopConverter.cpp
    tools/math/MathInkRecognizer.h
    tools/math/MathInkRecognizer.cpp
    tools/ImageTool.h
    tools/RulerItem.h
    tools/RulerTool.h

    resources.qrc
)

# App-Icon für Windows hinzufügen
if(WIN32)
    target_sources(Blop PRIVATE app_icon.rc)
endif()

# QML Modul Definition (Nur für Android relevant)
if(ANDROID)
    qt_add_qml_module(Blop
        URI Blop
        VERSION 1.0
        QML_FILES
            Main.qml
            ui/ToolSettingsOverlay.qml
            AndroidWebView.qml
            AuthOverlay.qml
    )
endif()

# MOC outputs (e.g. canvasview.moc for Q_OBJECT classes in .cpp) live under
# Blop_autogen/include — ensure the compiler always sees that path (Android/IDE).
# Plus: every src/<bucket>/ folder is on the include path so all the existing
# `#include "foo.h"` lines (which assume root-relative discovery) keep
# working unchanged after the v119 folder restructure.
target_include_directories(Blop PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/Blop_autogen/include"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/android"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/auth"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/observability"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/imageprovider"
)

# Core Libraries verlinken
target_link_libraries(Blop PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::NetworkAuth
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

# Export BLOP_VERSION_STR for runtime usage in mainwindow.cpp
target_compile_definitions(Blop PRIVATE BLOP_VERSION_STR=\"${GIT_VERSION}\")

if(BLOP_ENABLE_SENTRY)
    target_compile_definitions(Blop PRIVATE BLOP_SENTRY_ENABLED=1)
    target_link_libraries(Blop PRIVATE sentry::sentry)
    if(BLOP_SENTRY_FORCE_TEST_CRASH)
        target_compile_definitions(Blop PRIVATE BLOP_SENTRY_FORCE_TEST_CRASH=1)
    endif()
    if(WIN32 AND TARGET crashpad_handler)
        add_custom_command(
            TARGET Blop
            POST_BUILD
            COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:crashpad_handler>"
                "$<TARGET_FILE_DIR:Blop>/"
            COMMENT "Bundling Crashpad handler next to Blop (Sentry)"
        )
    endif()
endif()

# --- PLATTFORM-SPEZIFISCHES LINKING ---
# Ensure ANDROID_VERSION_CODE has a fallback (local/Qt Creator: avoid INSTALL_FAILED_VERSION_DOWNGRADE
# when a store/CI build with higher versionCode is already installed). CI sets -DANDROID_VERSION_CODE explicitly.
if(NOT DEFINED ANDROID_VERSION_CODE OR "${ANDROID_VERSION_CODE}" STREQUAL "")
    string(TIMESTAMP _BLOP_ANDROID_VC "%s" UTC)
    if(NOT _BLOP_ANDROID_VC MATCHES "^[1-9][0-9]*$")
        find_program(_BLOP_PYTHON NAMES python3 python)
        if(_BLOP_PYTHON)
            execute_process(
                COMMAND "${_BLOP_PYTHON}" -c "import time; print(int(time.time()))"
                OUTPUT_VARIABLE _BLOP_ANDROID_VC
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
        endif()
    endif()
    if(NOT _BLOP_ANDROID_VC MATCHES "^[1-9][0-9]*$")
        set(_BLOP_ANDROID_VC "2000000")
    endif()
    if(_BLOP_ANDROID_VC GREATER 2100000000)
        set(_BLOP_ANDROID_VC "2099999999")
    endif()
    set(ANDROID_VERSION_CODE "${_BLOP_ANDROID_VC}")
    message(STATUS "ANDROID_VERSION_CODE (unset → dev fallback): ${ANDROID_VERSION_CODE}")
endif()

# --- Generated observability metadata (after ANDROID_VERSION_CODE + Qt are known) ---
if(ANDROID)
    set(BLOP_OBS_PLATFORM_SLUG "android")
elseif(WIN32)
    set(BLOP_OBS_PLATFORM_SLUG "windows")
elseif(APPLE)
    set(BLOP_OBS_PLATFORM_SLUG "darwin")
elseif(UNIX)
    set(BLOP_OBS_PLATFORM_SLUG "linux")
else()
    set(BLOP_OBS_PLATFORM_SLUG "unknown")
endif()

set(BLOP_OBS_PLATFORM_SLUG_ESC "${BLOP_OBS_PLATFORM_SLUG}")
set(BLOP_SENTRY_RELEASE_ESC "${GIT_VERSION}+${BLOP_RESOLVED_GIT_SHA}+build${BLOP_OBS_BUILD_NUMBER}")

set(BLOP_OBS_APP_VERSION_ESC "${GIT_VERSION}")
set(BLOP_OBS_PROJECT_VERSION_ESC "${PROJECT_VERSION}")
set(BLOP_OBS_GIT_SHA_ESC "${BLOP_RESOLVED_GIT_SHA}")
set(BLOP_OBS_BUILD_NUMBER_ESC "${BLOP_OBS_BUILD_NUMBER}")
set(BLOP_OBS_BUILD_FLAVOR_ESC "${BLOP_OBS_BUILD_FLAVOR}")
if(BLOP_OBS_CONSENT_CRASH_UPLOAD)
    set(BLOP_OBS_CONSENT_CRASH_INT 1)
else()
    set(BLOP_OBS_CONSENT_CRASH_INT 0)
endif()
if(BLOP_OBS_CONSENT_ANALYTICS)
    set(BLOP_OBS_CONSENT_ANALYTICS_INT 1)
else()
    set(BLOP_OBS_CONSENT_ANALYTICS_INT 0)
endif()
if(ANDROID)
    set(BLOP_OBS_ANDROID_VERSION_CODE_ESC "${ANDROID_VERSION_CODE}")
else()
    set(BLOP_OBS_ANDROID_VERSION_CODE_ESC "")
endif()
set(BLOP_OBS_QT_PKG_VERSION_ESC "${Qt6_VERSION}")

configure_file(
    "${CMAKE_SOURCE_DIR}/src/observability/blop_observability_build.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/blop_observability_build.h"
    @ONLY
)

if(ANDROID)
    target_link_libraries(Blop PRIVATE Qt6::WebView)

    # --- Android OpenSSL: Required for HTTPS (OAuth token exchange, backend calls) ---
    # Download from: git clone https://github.com/KDAB/android_openssl C:/Qt/android_openssl
    set(ANDROID_OPENSSL_ROOT "C:/Qt/android_openssl" CACHE PATH "Path to android_openssl prebuilt libs")
    if(EXISTS "${ANDROID_OPENSSL_ROOT}/ssl_3/${CMAKE_ANDROID_ARCH_ABI}/libssl_3.so")
        message(STATUS "Found android_openssl for ${CMAKE_ANDROID_ARCH_ABI}")
        set(OPENSSL_EXTRA_LIBS
            "${ANDROID_OPENSSL_ROOT}/ssl_3/${CMAKE_ANDROID_ARCH_ABI}/libssl_3.so"
            "${ANDROID_OPENSSL_ROOT}/ssl_3/${CMAKE_ANDROID_ARCH_ABI}/libcrypto_3.so"
        )
        set_target_properties(Blop PROPERTIES QT_ANDROID_EXTRA_LIBS "${OPENSSL_EXTRA_LIBS}")
    else()
        message(WARNING "android_openssl not found at ${ANDROID_OPENSSL_ROOT}! TLS will not work. Clone from https://github.com/KDAB/android_openssl")
    endif()

    # Ohne dieses Verzeichnis mischt androiddeployqt nur das Qt-Template ein → Paket bleibt org.qtproject.example.
    set_target_properties(Blop PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "com.benschwank.blop"
        QT_ANDROID_COMPILE_SDK_VERSION "34"
        QT_ANDROID_TARGET_SDK_VERSION "35"
        QT_ANDROID_MIN_SDK_VERSION "23"
        QT_ANDROID_VERSION_CODE "${ANDROID_VERSION_CODE}"
        QT_ANDROID_VERSION_NAME "${SAFE_VERSION}"
    )
else()
    # Desktop: WebEngine linken und Define setzen
    target_link_libraries(Blop PRIVATE Qt6::WebEngineWidgets)
    target_compile_definitions(Blop PRIVATE BLOP_HAS_WEBENGINE)
    # Optional PDF module
    if(Qt6Pdf_FOUND)
        target_link_libraries(Blop PRIVATE Qt6::Pdf)
        target_compile_definitions(Blop PRIVATE BLOP_HAS_PDF)
    endif()
endif()

# --- ONNX Runtime (Offline Math OCR) ---
# Pre-built binaries expected under externals/onnxruntime/
if(NOT ANDROID)
    set(ONNXRUNTIME_ROOT "${CMAKE_SOURCE_DIR}/externals/onnxruntime")
    if(EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h")
        message(STATUS "Found ONNX Runtime at ${ONNXRUNTIME_ROOT}")
        target_include_directories(Blop PRIVATE "${ONNXRUNTIME_ROOT}/include")
        target_link_directories(Blop PRIVATE "${ONNXRUNTIME_ROOT}/lib")
        target_link_libraries(Blop PRIVATE onnxruntime)
        target_compile_definitions(Blop PRIVATE BLOP_HAS_ONNX_OCR)

        # Copy onnxruntime.dll next to the executable (small, fast, needed at runtime)
        add_custom_command(TARGET Blop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
                "$<TARGET_FILE_DIR:Blop>/onnxruntime.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_shared.dll"
                "$<TARGET_FILE_DIR:Blop>/onnxruntime_providers_shared.dll"
            COMMENT "Copying onnxruntime DLLs to build dir"
        )
    else()
        message(STATUS "ONNX Runtime not found — offline math OCR disabled")
    endif()
endif()

# Windows: Konsole bei Release-Builds ausblenden
if(WIN32)
    set_target_properties(Blop PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# --- Additive automation (benchmarks, future headless tools): never linked into Blop ---
option(BLOP_BUILD_AUTOMATION "Register headless tooling targets (e.g. blop_benchmark_math); does not change Blop behavior" OFF)
if(BLOP_BUILD_AUTOMATION AND NOT ANDROID)
    add_subdirectory(benchmarks)
elseif(BLOP_BUILD_AUTOMATION AND ANDROID)
    message(STATUS "BLOP_BUILD_AUTOMATION=ON: skipping benchmarks subdirectory on Android toolchain (desktop CI only)")
endif()

# Forcing CMake refresh for AUTOMOC
