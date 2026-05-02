add_executable(fuzzy-gen)
target_sources(fuzzy-gen PRIVATE
    main.cpp
)
target_include_directories(fuzzy-gen PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)
target_link_directories(fuzzy-gen PRIVATE
    ${LLVM_LIBRARY_DIRS}
    ${CLANG_LIBRARY_DIRS}
)
target_link_libraries(fuzzy-gen PRIVATE
    clang-cpp
    LLVM
)
install(TARGETS fuzzy-gen
DESTINATION bin
)
