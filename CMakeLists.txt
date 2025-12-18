cmake_minimum_required(VERSION 3.16)

# Android Stabilisierung
if(ANDROID)
    set(ANDROID_PLATFORM "android-33")
endif()

project(Blop VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NEU: Quick und Qml hinzugefügt
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Quick QuickControls2 Qml)

qt_add_executable(Blop
    main.cpp

    # Hauptfenster & C++ Views (Behalten wir, damit die Logik da bleibt)
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    canvasview.cpp
    canvasview.h
    freegridview.h
    freegridview.cpp
    multipagenoteview.cpp
    multipagenoteview.h

    # Dialoge
    settingsdialog.cpp
    settingsdialog.h
    SettingsDialog.ui
    newnotedialog.h
    newnotedialog.cpp
    profileeditordialog.h
    profileeditordialog.cpp

    # Manager
    notemanager.cpp
    notemanager.h
    noteeditor.cpp
    noteeditor.h
    uiprofilemanager.h
    uiprofilemanager.cpp

    # Toolbar
    moderntoolbar.cpp
    moderntoolbar.h

    # Helper Headers
    Note.h
    PageItem.h
    ToolMode.h
    ToolSettings.h
    UIStyles.h
    uiprofile.h

    # WICHTIG: Image Provider für deine Thumbnails hinzufügen
    BlopAsyncImageProvider.h
    BlopAsyncImageProvider.cpp
    BlopImageResponse.h
    BlopImageResponse.cpp
)

# NEU: Das QML-Modul registrieren
qt_add_qml_module(Blop
    URI Blop
    VERSION 1.0
    QML_FILES
        Main.qml
)

# NEU: Libraries linken
target_link_libraries(Blop PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
)

# Android Settings
if(ANDROID)
    set_target_properties(Blop PROPERTIES
        QT_ANDROID_COMPILE_SDK_VERSION "33"
        QT_ANDROID_TARGET_SDK_VERSION "33"
        QT_ANDROID_MIN_SDK_VERSION "23"
    )
endif()
