cmake_minimum_required(VERSION 3.20)

project(fuzzy)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "")
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(STATUS "CMAKE_INSTALL_PREFIX not set, defaulting to ${CMAKE_CURRENT_SOURCE_DIR}/install")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "" FORCE)
endif()

option(FUZZY_INCLUDE_TESTS "whether or not to build tests" OFF)
option(FUZZY_INCLUDE_EXAMPLES "whether or not to build examples" ON)

# include_guard(GLOBAL)
# if(NOT TARGET LLVMSupport)
#     find_package(LLVM REQUIRED CONFIG)
# endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(FindDeps)
include(FindLLVM)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(src)
if(FUZZY_INCLUDE_TESTS)
    add_subdirectory(tests)
endif()
if(FUZZY_INCLUDE_EXAMPLES)
    add_subdirectory(examples)
endif()
