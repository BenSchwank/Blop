cmake_minimum_required(VERSION 3.16)

# --- WICHTIG: Zwingt Android SDK 33 (Stabil) ---
# Das muss VOR dem "project()"-Befehl stehen!
if(ANDROID)
    set(ANDROID_PLATFORM "android-33")
endif()

project(Blop VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(Blop
    main.cpp

    # Hauptfenster
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    # Canvas & Views
    canvasview.cpp
    canvasview.h
    freegridview.h
    freegridview.cpp
    multipagenoteview.cpp
    multipagenoteview.h

    # Dialoge & Einstellungen
    settingsdialog.cpp
    settingsdialog.h
    SettingsDialog.ui   # Großgeschrieben!
    newnotedialog.h
    newnotedialog.cpp
    profileeditordialog.h
    profileeditordialog.cpp

    # Manager & Editor
    notemanager.cpp
    notemanager.h
    noteeditor.cpp
    noteeditor.h
    uiprofilemanager.h
    uiprofilemanager.cpp

    # Toolbar
    moderntoolbar.cpp
    moderntoolbar.h

    # Hilfs-Header (Großgeschrieben laut deinen Infos)
    Note.h
    PageItem.h
    ToolMode.h
    ToolSettings.h
    UIStyles.h

    # Profil Header
    uiprofile.h
)

target_link_libraries(Blop PRIVATE Qt6::Widgets Qt6::Network)

# --- WICHTIG: Konfiguration für das Android-Package (APK) ---
if(ANDROID)
    set_target_properties(Blop PROPERTIES
        QT_ANDROID_COMPILE_SDK_VERSION "33"
        QT_ANDROID_TARGET_SDK_VERSION "33"
        QT_ANDROID_MIN_SDK_VERSION "23"
    )
endif()
