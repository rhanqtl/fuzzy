include(FetchContent)

FetchContent_Declare(z3
    GIT_REPOSITORY https://github.com/Z3Prover/z3
    GIT_TAG z3-4.13.4
    GIT_SHALLOW TRUE
)
set(Z3_BUILD_LIBZ3_SHARED OFF CACHE BOOL "" FORCE)
set(Z3_BUILD_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(Z3_BUILD_TEST_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(Z3_BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
set(Z3_BUILD_DOTNET_BINDINGS OFF CACHE BOOL "" FORCE)
set(Z3_BUILD_JAVA_BINDINGS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(z3)

if(FUZZY_INCLUDE_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.0
    )
    FetchContent_MakeAvailable(Catch2)
endif()
