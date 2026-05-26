# Headless micro-benchmarks (additive). Never linked into the Blop GUI target.

if(NOT TARGET Qt6::Core)
    find_package(Qt6 REQUIRED COMPONENTS Core)
endif()

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
