add_library(fuzzy STATIC)
target_sources(fuzzy
    PRIVATE
        ast/Expr.cpp
        fuzzy.cpp
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

# fuzzy_gen requires Clang, skip for now
# add_subdirectory(fuzzy_gen)
