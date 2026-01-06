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

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Quick QuickControls2 Qml)

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

    # NEU: PageManager
    pagemanager.h
    pagemanager.cpp

    # NEU: Tool Architecture Basis
    tools/AbstractTool.h
    tools/ToolManager.h
    tools/ToolUIBridge.h
    tools/ToolFactory.h

    # NEU: Die Werkzeuge selbst
    tools/AbstractStrokeTool.h
    tools/WritingTools.h
    tools/EraserTool.h
    tools/LassoTool.h
    tools/ShapeTool.h
    tools/ImageTool.h

    # Hilfs-Dateien
    Note.h
    PageItem.h
    ToolMode.h
    ToolSettings.h
    UIStyles.h
    uiprofile.h

    BlopAsyncImageProvider.h
    BlopAsyncImageProvider.cpp
    BlopImageResponse.h
    BlopImageResponse.cpp
)

if(ANDROID)
    qt_add_qml_module(Blop
        URI Blop
        VERSION 1.0
        QML_FILES
            Main.qml
            ui/ToolSettingsOverlay.qml
    )
endif()

target_link_libraries(Blop PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
)

if(ANDROID)
    set_target_properties(Blop PROPERTIES
        QT_ANDROID_COMPILE_SDK_VERSION "33"
        QT_ANDROID_TARGET_SDK_VERSION "33"
        QT_ANDROID_MIN_SDK_VERSION "23"
    )
endif()
