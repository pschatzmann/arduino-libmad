cmake_minimum_required(VERSION 3.16)

# set the project name
project(arduino_libmad)

option(BUILD_TESTS "Build all tests automatically" OFF)

# lots of warnings and all warnings as errors
## add_compile_options(-Wall -Wextra )
set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SRC_LIST_C CONFIGURE_DEPENDS  "${PROJECT_SOURCE_DIR}/src/*.c" )

# define libraries
add_library (arduino_libmad ${SRC_LIST_C})

# prevent compile errors
target_compile_options(arduino_libmad PRIVATE -DUSE_DEFAULT_STDLIB )

# define location for header files
target_include_directories(arduino_libmad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/libMAD-mp3 ${CMAKE_CURRENT_SOURCE_DIR}/src/libMAD-aac )

# build examples
if(BUILD_TESTS)
    add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/mp3_write")
endif()