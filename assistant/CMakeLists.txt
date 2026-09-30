cmake_minimum_required(VERSION 3.18)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(BlopAssistent
    src/main.cpp
    src/core/CommandEngine.cpp
    src/core/ActionRunner.cpp
    src/core/NoteWriter.cpp
    src/platform/SpeechInput.cpp
    src/ui/NotchWindow.cpp
    src/ui/PhoneShell.cpp
)

target_include_directories(BlopAssistent PRIVATE src)

target_link_libraries(BlopAssistent PRIVATE Qt6::Widgets)

set_target_properties(BlopAssistent PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    EXCLUDE_FROM_ALL TRUE
)

if(WIN32)
    set_target_properties(BlopAssistent PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(BlopAssistent PRIVATE user32)
    add_custom_command(TARGET BlopAssistent POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:BlopAssistent>
            ${CMAKE_BINARY_DIR}/$<TARGET_FILE_NAME:BlopAssistent>
        COMMENT "Copy BlopAssistent.exe next to the build root"
    )
endif()

if(ANDROID)
    target_link_libraries(BlopAssistent PRIVATE Qt6::CorePrivate)
    set_target_properties(BlopAssistent PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "com.benschwank.blopassistent"
        QT_ANDROID_APP_NAME "Blop Assistent"
        QT_ANDROID_COMPILE_SDK_VERSION "34"
        QT_ANDROID_TARGET_SDK_VERSION "35"
        QT_ANDROID_MIN_SDK_VERSION "23"
    )
endif()

message(STATUS "Blop Assistent: separate target BlopAssistent (not linked into Blop)")
