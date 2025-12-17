cmake_minimum_required(VERSION 3.16)

project(Blop VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# WICHTIG: Zurück zum Original (Nur Widgets und Network)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(Blop
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
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
    newnotedialog.h
    newnotedialog.cpp
    ToolMode.h
    ToolSettings.h
    UIStyles.h
    uiprofile.h
    uiprofilemanager.h uiprofilemanager.cpp
    profileeditordialog.h profileeditordialog.cpp
    PageItem.h
    Note.h
    # HIER ENTFERNT: BlopAsyncImageProvider und BlopImageResponse
    # Diese verursachen den Fehler und werden für Desktop nicht gebraucht.
)

# WICHTIG: Keine QML/Quick Abhängigkeiten mehr
target_link_libraries(Blop PRIVATE Qt6::Widgets Qt6::Network)
