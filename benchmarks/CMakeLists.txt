# Headless automation targets (additive). Never linked into the Blop GUI target.

if(NOT TARGET Qt6::Core)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
endif()
if(NOT TARGET Qt6::Widgets)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
endif()

# --- Math micro-benchmark -------------------------------------------------
add_executable(blop_benchmark_math
    benchmark_math.cpp
    "${CMAKE_SOURCE_DIR}/tools/math/MathExpressionParser.cpp"
    "${CMAKE_SOURCE_DIR}/tools/math/MathEvaluator.cpp"
)

target_include_directories(blop_benchmark_math PRIVATE
    "${CMAKE_SOURCE_DIR}/tools/math"
)

target_link_libraries(blop_benchmark_math PRIVATE Qt6::Core)

set_target_properties(blop_benchmark_math PROPERTIES
    WIN32_EXECUTABLE OFF
)

message(STATUS "BLOP_BUILD_AUTOMATION: target blop_benchmark_math registered")

# --- A4 JSON persistence round-trip (Phase 0.1) ---------------------------
add_executable(blop_test_persistence_roundtrip
    "${CMAKE_SOURCE_DIR}/tests/persistence_roundtrip.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/notemanager.cpp"
    "${CMAKE_SOURCE_DIR}/tools/math/MathExpressionParser.cpp"
    "${CMAKE_SOURCE_DIR}/tools/math/MathEvaluator.cpp"
)

target_include_directories(blop_test_persistence_roundtrip PRIVATE
    "${CMAKE_SOURCE_DIR}/src/core"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/tools/math"
)

target_link_libraries(blop_test_persistence_roundtrip PRIVATE
    Qt6::Core
    Qt6::Gui
)

set_target_properties(blop_test_persistence_roundtrip PROPERTIES
    WIN32_EXECUTABLE OFF
    AUTOMOC ON
    AUTOUIC OFF
    AUTORCC OFF
)

enable_testing()
add_test(NAME persistence_roundtrip COMMAND blop_test_persistence_roundtrip)

message(STATUS "BLOP_BUILD_AUTOMATION: target blop_test_persistence_roundtrip registered")

# --- Infinite binary V5 persistence round-trip (Phase 0.1) ----------------
add_executable(blop_test_infinite_persistence_roundtrip
    "${CMAKE_SOURCE_DIR}/tests/infinite_persistence_roundtrip.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/infinitecanvasstore.cpp"
)

target_include_directories(blop_test_infinite_persistence_roundtrip PRIVATE
    "${CMAKE_SOURCE_DIR}/src/core"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/tools"
)

target_link_libraries(blop_test_infinite_persistence_roundtrip PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(blop_test_infinite_persistence_roundtrip PROPERTIES
    WIN32_EXECUTABLE OFF
    AUTOMOC ON
    AUTOUIC OFF
    AUTORCC OFF
)

add_test(NAME infinite_persistence_roundtrip
         COMMAND blop_test_infinite_persistence_roundtrip)

message(STATUS "BLOP_BUILD_AUTOMATION: target blop_test_infinite_persistence_roundtrip registered")
