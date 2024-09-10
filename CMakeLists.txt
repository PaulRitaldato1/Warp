
#most up to date version as of project creation
cmake_minimum_required(VERSION 3.30)

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
    ${Warp_SOURCE_DIR}/Warp/*.h
    ${Warp_SOURCE_DIR}/Warp/*.hpp
)

#source
file(GLOB_RECURSE SRCS
    ${Warp_SOURCE_DIR}/Warp/*.cpp
    ${Warp_SOURCE_DIR}/Warp/*.c
)

add_executable(Warp ${HDRS} ${SRCS})
target_compile_features(Warp PUBLIC cxx_std_20)

include_directories(
    ${Warp_SOURCE_DIR}/Warp
    ${Warp_SOURCE_DIR}/Warp/Rendering
    ${Warp_SOURCE_DIR}/Warp/Engine
)

#link libraries for each platform
if(WIN32)
    target_link_libraries(Warp PUBLIC "d3d12.lib" "dxgi.lib" "d3dcompiler.lib")
#Linux
elseif(UNIX AND NOT APPLE)

#Apple
else()

endif()

if(WIN32)
    string(REPLACE "/" "\\" source_path_windows "${Warp_SOURCE_DIR}/Resources")
    string(REPLACE "/" "\\" build_path_windows "${Warp_BINARY_DIR}/Resources")
    execute_process(COMMAND cmd.exe /c mklink /J "${build_path_windows}" "${source_path_windows}" RESULT_VARIABLE exitcode)
else()
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${WARP_SOURCE_DIR}/Resources ${WARP_BINARY_DIR}/Resources RESULT_VARIABLE exitcode)
endif()

if(NOT ${exitcode} EQUAL 0)
    MESSAGE("SYMLINKING FAILED:  ${exitcode}")
    MESSAGE("FALLING BACK TO COPYING")
endif()

file(GLOB_RECURSE RES RELATIVE ${WARP_SOURCE_DIR}/Resources ${WARP_SOURCE_DIR}/Resources/*.*)
foreach(file IN LISTS RES)
    configure_file(${WARP_SOURCE_DIR}/Resources/${file} ${WARP_BINARY_DIR}/Resources/${file} COPYONLY)    
endforeach(file IN LISTS RES)

if(MSVC_IDE)
    #create virtual folders for VS because they still do this for some reason
    foreach(source IN LISTS SRCS HDRS)
        get_filename_component(source_path "${source}" PATH)
        string(REPLACE "${Warp_SOURCE_DIR}" "" relative_source_path "${source_path}")
        string(REPLACE "/" "\\" source_path_msvc "${relative_source_path}")
        source_group("${source_path_msvc}" FILES "${source}")
    endforeach(source IN LISTS SRCS HDRS)

    #set Warp as the startup project
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Warp)
endif()