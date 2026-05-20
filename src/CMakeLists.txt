add_library(fuzzy STATIC)
target_sources(fuzzy
    PRIVATE
        ast/Expr.cpp
        fuzzy.cpp
        schedule/ScheduleEngine.cpp
        schedule/ScheduleGraphBuilder.cpp
)
target_include_directories(fuzzy
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${z3_SOURCE_DIR}/src/api
        ${z3_SOURCE_DIR}/src/api/c++
        ${z3_BINARY_DIR}/src/api
)
target_link_libraries(fuzzy
    PUBLIC
        libz3
)

if(FUZZY_COVERAGE AND NOT MSVC)
    target_compile_options(fuzzy PRIVATE --coverage -g -O0)
    target_link_options(fuzzy PUBLIC --coverage)
endif()

# fuzzy_gen requires Clang, skip for now
# add_subdirectory(fuzzy_gen)
