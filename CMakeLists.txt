cmake_minimum_required(VERSION 3.16)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Android Setup
if(ANDROID)
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
    settingsdialog.cpp
    settingsdialog.h
    SettingsDialog.ui
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
    set_target_properties(Blop PROPERTIES
        QT_ANDROID_COMPILE_SDK_VERSION "34"
        QT_ANDROID_TARGET_SDK_VERSION "34"
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

# Windows: Konsole bei Release-Builds ausblenden
if(WIN32)
    set_target_properties(Blop PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
# Forcing CMake refresh for AUTOMOC
