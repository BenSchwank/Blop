# Opt-in QA console launcher. Not linked into Blop.exe.
# Ships next to Blop.exe in Windows deployment / release ZIP.

if(NOT WIN32 OR ANDROID)
    return()
endif()

add_executable(BlopTrace
    main.cpp
)

set_target_properties(BlopTrace PROPERTIES
    WIN32_EXECUTABLE OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

target_link_libraries(BlopTrace PRIVATE
    shell32
    ole32
)

# Sit next to Blop in the same build dir for local testing.
add_custom_command(TARGET BlopTrace POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:BlopTrace>
        $<TARGET_FILE_DIR:Blop>/BlopTrace.exe
    COMMENT "Copy BlopTrace.exe next to Blop.exe"
)

message(STATUS "BlopTrace: Windows session-trace launcher registered")
