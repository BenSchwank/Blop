cmake_minimum_required(VERSION 3.16)

# Android Setup
if(ANDROID)
    set(ANDROID_PLATFORM "android-33")
endif()

project(Blop VERSION 0.1 LANGUAGES CXX)

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
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Quick QuickControls2 Qml)

# --- WEB-MODUL WEICHE ---
if(ANDROID)
    # Android: Native WebView
    find_package(Qt6 REQUIRED COMPONENTS WebView)
else()
    # Desktop: WebEngine (Chromium) - REQUIRED
    # Dies stellt sicher, dass WebEngineWidgets gelinkt wird.
    find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets)
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
    noteeditor.cpp
    noteeditor.h
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
    UIStyles.h
    uiprofile.h

    # Image-Provider
    BlopAsyncImageProvider.h
    BlopAsyncImageProvider.cpp
    BlopImageResponse.h
    BlopImageResponse.cpp
)

# QML Modul Definition (Nur für Android relevant)
if(ANDROID)
    qt_add_qml_module(Blop
        URI Blop
        VERSION 1.0
        QML_FILES
            Main.qml
            ui/ToolSettingsOverlay.qml
    )
endif()

# Core Libraries verlinken
target_link_libraries(Blop PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
)

# --- PLATTFORM-SPEZIFISCHES LINKING ---
if(ANDROID)
    target_link_libraries(Blop PRIVATE Qt6::WebView)
    set_target_properties(Blop PROPERTIES
        QT_ANDROID_COMPILE_SDK_VERSION "33"
        QT_ANDROID_TARGET_SDK_VERSION "33"
        QT_ANDROID_MIN_SDK_VERSION "23"
    )
else()
    # Desktop: WebEngine linken und Define setzen
    target_link_libraries(Blop PRIVATE Qt6::WebEngineWidgets)
    target_compile_definitions(Blop PRIVATE BLOP_HAS_WEBENGINE)
endif()
