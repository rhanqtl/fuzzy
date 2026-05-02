include_directories(${Catch2_SOURCE_DIR}/src)

macro(fuzzy_add_test NAME)
    add_executable(${NAME})
    target_sources(${NAME} PRIVATE ${NAME}.cpp)
    target_link_libraries(${NAME} PRIVATE fuzzy Catch2::Catch2WithMain)
    add_test(NAME ${NAME} COMMAND ${NAME})
endmacro()

# fuzzy_add_test(arith)
add_subdirectory(planner)
