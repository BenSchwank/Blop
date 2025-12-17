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
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    # Dateien wurden auf CamelCase geändert, um Linux-Fehler zu beheben:
    CanvasView.cpp CanvasView.h
    SettingsDialog.cpp SettingsDialog.h SettingsDialog.ui
    FreeGridView.h FreeGridView.cpp
    NoteManager.cpp NoteManager.h
    NoteEditor.cpp NoteEditor.h
    MultiPageNoteView.cpp MultiPageNoteView.h
    ModernToolbar.cpp ModernToolbar.h
    NewNoteDialog.h NewNoteDialog.cpp
    ToolMode.h ToolSettings.h UIStyles.h
    UiProfile.h UiProfileManager.h UiProfileManager.cpp
    ProfileEditorDialog.h ProfileEditorDialog.cpp
    PageItem.h Note.h
)

target_link_libraries(Blop PRIVATE Qt6::Widgets Qt6::Network)
