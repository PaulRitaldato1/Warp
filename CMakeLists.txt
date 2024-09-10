
#most up to date version as of project creation
cmake_minimum_required(VERSION 3.3)

#project name
project(Warp)

if(MSVC)
    add_definitions(
        -c
        -w4
    )
else()
    add_definitions(
        -c
        -Wall
        -msse2
    )
endif()

if( CMAKE_BUILD_TYPE STREQUAL "" )
    #Set default if the build type is not specified. CMAKE leaves it empty by default
    set(
        CMAKE_BUILD_TYPE
            "Debug"
            CACHE STRING
                "Choose the type of build, options are: None (CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) \"Debug\" \"Release\" \"DebugableRelease\" \"Shipping\"."
            FORCE
    )
endif( CMAKE_BUILD_TYPE STREQUAL "" )

if( CMAKE_BUILD_TYPE STREQUAL "Release")
add_definitions( -O3 )
endif(CMAKE_BUILD_TYPE STREQUAL "Release")

#headers
file(GLOB_RECURSE HDRS
    ${WARP_SOURCE_DIR}/Warp/*.h
    ${WARP_SOURCE_DIR}/Warp/*.hpp
)

#source
file(GLOB_RECURSE SRCS
    ${WARP_SOURCE_DIR}/Warp/*.cpp
    ${WARP_SOURCE_DIR}/Warp/*.c
)

find_package(OpenGL REQUIRED)

include_directories(${WARP_SOURCE_DIR}/Warp)

