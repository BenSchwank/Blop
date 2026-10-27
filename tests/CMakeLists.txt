# Unit tests (QtTest). Desktop only; run with `ctest --test-dir build-check`.
find_package(Qt6 QUIET COMPONENTS Test Concurrent Gui)
if(NOT Qt6Test_FOUND)
    message(STATUS "Qt6::Test not found - skipping unit tests")
    return()
endif()

add_executable(blop_tests
    tst_core.cpp
    "${CMAKE_SOURCE_DIR}/src/core/notemanager.cpp"
    "${CMAKE_SOURCE_DIR}/src/util/atomicfile.cpp"
    "${CMAKE_SOURCE_DIR}/tools/math/MathExpressionParser.cpp"
)
set_target_properties(blop_tests PROPERTIES AUTOMOC ON WIN32_EXECUTABLE OFF)
target_include_directories(blop_tests PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src/core"
    "${CMAKE_SOURCE_DIR}/src/util"
    "${CMAKE_SOURCE_DIR}/tools/math"
)
target_link_libraries(blop_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Concurrent)
add_test(NAME blop_tests COMMAND blop_tests)