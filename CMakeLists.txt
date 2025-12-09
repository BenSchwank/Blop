cmake_minimum_required(VERSION 3.16)

project(Blop VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Wir brauchen KEIN OpenGL mehr, nur Widgets (da wir jetzt Bitmap-Caching nutzen)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(Blop
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    # Deine optimierte CanvasView (enthält jetzt die High-Performance Logik)
    canvasview.cpp
    canvasview.h

    settingsdialog.cpp
    settingsdialog.h
    settingsdialog.ui
    freegridview.h
    freegridview.cpp
    notemanager.cpp
    notemanager.h
    noteeditor.cpp
    noteeditor.h
    multipagenoteview.cpp
    multipagenoteview.h
    moderntoolbar.cpp
    moderntoolbar.h
    ToolMode.h
    ToolSettings.h
    UIStyles.h
    windows_icon.qrc
    windows_icon.rc
    UiProfile.h
    uiprofilemanager.h uiprofilemanager.cpp
    InputPredictor.h
    InputPredictor.cpp

    # ENTFERNT: highperformancecanvas.h/.cpp
)

target_link_libraries(Blop PRIVATE Qt6::Widgets)
