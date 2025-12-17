cmake_minimum_required(VERSION 3.16)

project(Blop VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(Blop
    main.cpp

    # Hauptfenster (alles klein laut Bild)
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    # Canvas & Views (alles klein)
    canvasview.cpp
    canvasview.h
    freegridview.h
    freegridview.cpp
    multipagenoteview.cpp
    multipagenoteview.h

    # Dialoge & Einstellungen (alles klein)
    settingsdialog.cpp
    settingsdialog.h
    SettingsDialog.ui
    newnotedialog.h
    newnotedialog.cpp
    profileeditordialog.h
    profileeditordialog.cpp

    # Manager & Editor (alles klein)
    notemanager.cpp
    notemanager.h
    noteeditor.cpp
    noteeditor.h
    uiprofilemanager.h
    uiprofilemanager.cpp

    # Toolbar (alles klein)
    moderntoolbar.cpp
    moderntoolbar.h

    # Dateien mit Großbuchstaben (laut deinem Bild)
    Note.h
    PageItem.h
    ToolMode.h
    ToolSettings.h
    UIStyles.h

    # Profil Header (klein laut Bild)
    uiprofile.h
)


# --- NEU HINZUFÜGEN ---
if(ANDROID)
    # Wir zwingen Qt, Android SDK 33 zu nutzen (das ist stabil)
    set_property(TARGET Blop PROPERTY QT_ANDROID_COMPILE_SDK_VERSION 33)
    set_property(TARGET Blop PROPERTY QT_ANDROID_TARGET_SDK_VERSION 33)
endif()
# ----------------------

target_link_libraries(Blop PRIVATE Qt6::Widgets Qt6::Network)
