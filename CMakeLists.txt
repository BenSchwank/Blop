cmake_minimum_required(VERSION 3.16)
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
    set(GIT_VERSION "3.13.5.6")
endif()

# Remove 'v' prefix if it exists for the project() call
string(REGEX REPLACE "^v" "" CLEAN_VERSION "${GIT_VERSION}")
# If there's a git hash suffix (-1-gX), the VERSION might be invalid for CMake project().
# We'll just extract the numeric part x.y.z.w
string(REGEX MATCH "^([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+|[0-9]+\\.[0-9]+\\.[0-9]+|[0-9]+\\.[0-9]+|[0-9]+)" SAFE_VERSION "${CLEAN_VERSION}")
if(NOT SAFE_VERSION)
    set(SAFE_VERSION "3.13.5.6")
endif()

project(Blop VERSION ${SAFE_VERSION} LANGUAGES CXX)

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
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    canvasview.cpp
    canvasview.h
    freegridview.h
    freegridview.cpp
    multipagenoteview.cpp
    multipagenoteview.h
    editoroverlays.cpp
    editoroverlays.h
    settingsdialog.cpp
    settingsdialog.h
    SettingsDialog.ui
    graphaxissettingsdialog.cpp
    graphaxissettingsdialog.h
    graphlegenddock.cpp
    graphlegenddock.h
    graphquickactionpopup.cpp
    graphquickactionpopup.h
    newnotedialog.h
    newnotedialog.cpp
    profileeditordialog.h
    profileeditordialog.cpp
    notemanager.cpp
    notemanager.h
    googleauthmanager.cpp
    googleauthmanager.h
    noteeditor.cpp
    noteeditor.h
    markdowneditor.cpp
    markdowneditor.h
    uiprofilemanager.h
    uiprofilemanager.cpp
    uiscale.h
    uiscale.cpp
    moderntoolbar.cpp
    moderntoolbar.h
    modetoolbar.cpp
    modetoolbar.h

    # PageManager
    pagemanager.h
    pagemanager.cpp

    # Tools
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

    # Helper
    Note.h
    PageItem.h
    ToolMode.h
    ToolSettings.h
    TransformOverlay.h
    UIStyles.h
    SelectionMenuIcons.h
    uiprofile.h

    # Intro Animation
    introscreen.h
    introscreen.cpp

    # Image-Provider
    BlopAsyncImageProvider.h
    BlopAsyncImageProvider.cpp
    BlopImageResponse.h
    BlopImageResponse.cpp
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
target_include_directories(Blop PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/Blop_autogen/include"
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

# --- PLATTFORM-SPEZIFISCHES LINKING ---
# Ensure ANDROID_VERSION_CODE has a fallback
if(NOT DEFINED ANDROID_VERSION_CODE OR "${ANDROID_VERSION_CODE}" STREQUAL "")
    set(ANDROID_VERSION_CODE "1")
endif()

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
# Forcing CMake refresh for AUTOMOC
