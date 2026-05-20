include_directories(${Catch2_SOURCE_DIR}/src)

macro(fuzzy_add_test NAME)
    add_executable(${NAME})
    target_sources(${NAME} PRIVATE ${NAME}.cpp)
    target_link_libraries(${NAME} PRIVATE fuzzy Catch2::Catch2WithMain)
    add_test(NAME ${NAME} COMMAND ${NAME})
    if(FUZZY_COVERAGE AND NOT MSVC)
        target_link_options(${NAME} PRIVATE --coverage)
    endif()
endmacro()

add_subdirectory(planner)

fuzzy_add_test(soft)
fuzzy_add_test(sv_dsl)
fuzzy_add_test(array_agg)
fuzzy_add_test(randomize_with)
fuzzy_add_test(vector)
fuzzy_add_test(map_container)
fuzzy_add_test(inheritance)
fuzzy_add_test(rand_kind)
fuzzy_add_test(feature_solve_before)
fuzzy_add_test(feature_dist)
fuzzy_add_test(feature_expr_ops)
fuzzy_add_test(sv_combo_all)
fuzzy_add_test(model_api)
fuzzy_add_test(func_call)
fuzzy_add_test(feature_randc)
fuzzy_add_test(feature_enum)
fuzzy_add_test(feature_dict)
fuzzy_add_test(map_enum_key)
fuzzy_add_test(schedule_deps)

if(FUZZY_COVERAGE AND NOT MSVC)
    find_program(GCOVR_EXECUTABLE gcovr)
    if(GCOVR_EXECUTABLE)
        add_custom_target(
            fuzzy-lib-coverage-check
            COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure
            COMMAND
                ${GCOVR_EXECUTABLE} -r ${CMAKE_SOURCE_DIR}/src --object-directory ${CMAKE_BINARY_DIR}
                --fail-under-line 95 --print-summary
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run ctest then assert >=95% line coverage on src/ (gcovr; configure with -DFUZZY_COVERAGE=ON)"
            USES_TERMINAL)
    else()
        message(
            STATUS
                "gcovr not found: install (e.g. pip install gcovr) to enable target fuzzy-lib-coverage-check")
    endif()
endif()
