cmake_minimum_required(VERSION 3.16)

project(Blop VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(Blop
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    canvasview.cpp
    canvasview.h
    freegridview.cpp
    freegridview.h
    settingsdialog.cpp
    settingsdialog.h
    settingsdialog.ui
    # --- DIESE HABEN GEFEHLT: ---
    notemanager.cpp
    notemanager.h
    noteeditor.cpp
    noteeditor.h
    multipagenoteview.cpp
    multipagenoteview.h
    moderntoolbar.cpp
    moderntoolbar.h
    modetoolbar.cpp
    modetoolbar.h
    Note.h
    ToolMode.h
    UIStyles.h
)

target_link_libraries(Blop PRIVATE Qt6::Widgets)
