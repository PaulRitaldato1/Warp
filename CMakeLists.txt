
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
        /wd4100  # unreferenced formal parameter — suppress until engine is complete
    )
    message("Adding MSVC /MP and /W4")
else()
    add_definitions(
        -Wall
        -msse2
        -Wno-unused-parameter  # suppress until engine is complete
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

include(FetchContent)

# Shared library (WarpEngine.so) requires all static deps to be PIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(
    DirectXMath
    GIT_REPOSITORY https://github.com/microsoft/DirectXMath.git
    GIT_TAG        oct2024
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG        v0.7.2
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    tinyddsloader
    GIT_REPOSITORY https://github.com/benikabocha/tinyddsloader.git
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  "_none"  # prevents MakeAvailable from processing its CMakeLists
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  "_none"
)

FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(DirectXMath fastgltf VulkanMemoryAllocator tinyddsloader stb)

# DirectXMath includes sal.h which is a Windows-only SAL annotation header.
# Provide a no-op stub for non-Windows builds. configure_file runs on every
# CMake configure so it survives fresh build-directory cleans.
if(NOT WIN32)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sal.h
        ${directxmath_SOURCE_DIR}/Inc/sal.h
        COPYONLY
    )
endif()

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
target_compile_definitions(WarpEngine PRIVATE "WARP_EXPORT")
target_precompile_headers(WarpEngine PRIVATE "Warp/Engine/Common/CommonTypes.h")

add_executable(Warp "Warp/EntryPoint/EntryPoint.h" ${TESTBED_SRCS} ${TESTBED_HDRS})
target_compile_features(Warp PRIVATE cxx_std_20)
target_compile_definitions(Warp PUBLIC)
target_precompile_headers(Warp PRIVATE "Warp/Engine/Common/CommonTypes.h")

#TODO: Make a separate application so I can hot reload the user code and the engine code
# add_executable(Application ${ENTRY_SRCS} ${ENTRY_HDRS})
# target_compile_features(Application PRIVATE cxx_std_20)
# target_precompile_headers(Application PRIVATE "Warp/Engine/Common/CommonTypes.h")

#link libraries for each platform
target_link_libraries(WarpEngine PUBLIC  DirectXMath)
target_link_libraries(WarpEngine PRIVATE fastgltf::fastgltf)
target_include_directories(WarpEngine PUBLIC  ${directxmath_SOURCE_DIR}/Inc)
target_include_directories(WarpEngine SYSTEM PRIVATE ${tinyddsloader_SOURCE_DIR})
target_include_directories(WarpEngine SYSTEM PRIVATE ${stb_SOURCE_DIR})

if(WIN32)
    find_package(Vulkan REQUIRED)
    # Suppress Windows min/max macros and reduce header bloat.
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
    # Derive the Vulkan SDK lib directory from the found vulkan-1.lib path.
    get_filename_component(_VK_LIB_DIR "${Vulkan_LIBRARY}" DIRECTORY)
    target_link_libraries(WarpEngine PRIVATE "d3d12.lib" "dxgi.lib" "dxcompiler.lib" "Gdi32.lib" "synchronization.lib" Vulkan::Vulkan)
    # shaderc — use the combined static lib so no extra DLLs are needed.
    target_link_libraries(WarpEngine PRIVATE "$<IF:$<CONFIG:Debug>,${_VK_LIB_DIR}/shaderc_combinedd.lib,${_VK_LIB_DIR}/shaderc_combined.lib>")
    target_link_libraries(Warp PUBLIC WarpEngine)
#Linux
elseif(UNIX AND NOT APPLE)
    find_package(Vulkan REQUIRED)
    target_link_libraries(WarpEngine PRIVATE X11 Vulkan::Vulkan shaderc_shared)
    target_link_libraries(Warp PUBLIC WarpEngine)

#Apple
else()

endif()

# VMA — header-only, needed by Vulkan backend on all platforms.
# Mark as SYSTEM to suppress third-party warnings (nullability, unused function, etc.).
target_link_libraries(WarpEngine PRIVATE GPUOpen::VulkanMemoryAllocator)
get_target_property(VMA_INCLUDE_DIRS GPUOpen::VulkanMemoryAllocator INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(WarpEngine SYSTEM PRIVATE ${VMA_INCLUDE_DIRS})

file(CREATE_LINK
    "${Warp_SOURCE_DIR}/Resources"
    "${Warp_BINARY_DIR}/Resources"
    RESULT symlink_result
    SYMBOLIC
    COPY_ON_ERROR
)
if(NOT symlink_result EQUAL 0)
    message(WARNING "Resources link/copy failed: ${symlink_result}")
endif()

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
