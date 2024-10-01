
#most up to date version as of project creation
cmake_minimum_required(VERSION 3.16)
#project name
project(Warp)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_definitions(
        /EHsc
        /Zc:preprocessor
        /MP
        /W4
    )
    message("Adding MSVC /MP and /W4")
else()
    add_definitions(
        -c
        -Wall
        -msse2
    )
    message("Adding Unix -c, -Wall, -msse2")
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
    add_definitions( -O2 )
endif(CMAKE_BUILD_TYPE STREQUAL "Release")

#Engine headers
file(GLOB_RECURSE ENGINE_HDRS CONFIGURE_DEPENDS
    ${Warp_SOURCE_DIR}/Warp/Engine/*.h
    ${Warp_SOURCE_DIR}/Warp/Engine/*.hpp
)

#Engine source
file(GLOB_RECURSE ENGINE_SRCS CONFIGURE_DEPENDS
    ${Warp_SOURCE_DIR}/Warp/Engine/*.cpp
    ${Warp_SOURCE_DIR}/Warp/Engine/*.c
)

#TestBed Sources
file(GLOB_RECURSE TESTBED_SRCS CONFIGURE_DEPENDS
    ${Warp_SOURCE_DIR}/Warp/TestBed/*.cpp
    ${Warp_SOURCE_DIR}/Warp/TestBed/*.c
)

#TestBed headers
file(GLOB_RECURSE TESTBED_HDRS CONFIGURE_DEPENDS
    ${Warp_SOURCE_DIR}/Warp/TestBed/*.h
    ${Warp_SOURCE_DIR}/Warp/TestBed/*.hpp
)

#EntryPoint Sources
# file(GLOB_RECURSE ENTRY_SRCS
#     ${Warp_SOURCE_DIR}/Warp/EntryPoint/*.cpp
#     ${Warp_SOURCE_DIR}/Warp/EntryPoint/*.c
# )

# #EntryPoint headers
# file(GLOB_RECURSE ENTRY_HDRS
#     ${Warp_SOURCE_DIR}/Warp/EntryPoint/*.h
#     ${Warp_SOURCE_DIR}/Warp/EntryPoint/*.hpp
# )

include_directories(
    ${Warp_SOURCE_DIR}/Warp
    ${Warp_SOURCE_DIR}/Warp/Engine/Rendering
    ${Warp_SOURCE_DIR}/Warp/Engine
    )

#TODO
#we also have 'WARP_ASSERTIONS_ENABLED', 'LOG_WARN_ENABLED', 'LOG_DEBUG_ENABLED', 'LOG_INFO_ENABLED', and 'WARP_RELEASE'
#that we need to add depending on the build type
add_library(WarpEngine SHARED ${ENGINE_SRCS} ${ENGINE_HDRS})
target_compile_features(WarpEngine PRIVATE cxx_std_20)
target_compile_definitions(WarpEngine PUBLIC "WARP_EXPORT")
target_precompile_headers(WarpEngine PRIVATE "Warp/Engine/Common/CommonTypes.h")

add_executable(Warp "Warp/EntryPoint/EntryPoint.h" ${TESTBED_SRCS} ${TESTBED_HDRS})
target_compile_features(Warp PRIVATE cxx_std_20)
target_compile_definitions(Warp PUBLIC "WARP_EXPORT")
target_precompile_headers(Warp PRIVATE "Warp/Engine/Common/CommonTypes.h")

#TODO: Make a separate application so I can hot reload the user code and the engine code
# add_executable(Application ${ENTRY_SRCS} ${ENTRY_HDRS})
# target_compile_features(Application PRIVATE cxx_std_20)
# target_precompile_headers(Application PRIVATE "Warp/Engine/Common/CommonTypes.h")

#link libraries for each platform
if(WIN32)
    target_link_libraries(WarpEngine PRIVATE "d3d12.lib" "dxgi.lib" "d3dcompiler.lib" "Gdi32.lib")
    target_link_libraries(Warp PUBLIC WarpEngine)
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
    foreach(source IN LISTS TESTBED_SRCS TESTBED_HDRS)
        get_filename_component(source_path "${source}" PATH)
        string(REPLACE "${Warp_SOURCE_DIR}" "" relative_source_path "${source_path}")
        string(REPLACE "/" "\\" source_path_msvc "${relative_source_path}")
        source_group("${source_path_msvc}" FILES "${source}")
    endforeach(source IN LISTS TESTBEd_SRCS TESTBED_HDRS)

     foreach(source IN LISTS ENGINE_SRCS ENGINE_HDRS)
        get_filename_component(source_path "${source}" PATH)
        string(REPLACE "${Warp_SOURCE_DIR}" "" relative_source_path "${source_path}")
        string(REPLACE "/" "\\" source_path_msvc "${relative_source_path}")
        source_group("${source_path_msvc}" FILES "${source}")
    endforeach(source IN LISTS ENGINE_SRCS ENGINE_HDRS)

    foreach(source IN LISTS ENTRY_SRCS ENTRY_HDRS)
        get_filename_component(source_path "${source}" PATH)
        string(REPLACE "${Warp_SOURCE_DIR}" "" relative_source_path "${source_path}")
        string(REPLACE "/" "\\" source_path_msvc "${relative_source_path}")
        source_group("${source_path_msvc}" FILES "${source}")
    endforeach(source IN LISTS ENGINE_SRCS ENGINE_HDRS)

    #set Warp as the startup project
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT WarpEngine)
endif()
