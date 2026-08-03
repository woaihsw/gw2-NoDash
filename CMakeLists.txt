cmake_minimum_required(VERSION 3.20)

project(gw2_NoDash LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "gw2-NoDash supports Windows only")
endif()

add_library(gw2_NoDash SHARED
    src/addon.cpp
    src/config.cpp
    src/hooks.cpp
    src/resolver.cpp
)

target_compile_features(gw2_NoDash PRIVATE cxx_std_20)
target_include_directories(gw2_NoDash PRIVATE include)
target_compile_definitions(gw2_NoDash PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_link_libraries(gw2_NoDash PRIVATE psapi)

if(MSVC)
    set_property(TARGET gw2_NoDash PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_options(gw2_NoDash PRIVATE
        /W4 /permissive- /EHsc /utf-8 /Zi
    )
    target_link_options(gw2_NoDash PRIVATE
        /DEBUG
        $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>
        $<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>
    )
endif()

set_target_properties(gw2_NoDash PROPERTIES
    OUTPUT_NAME "gw2-NoDash"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
