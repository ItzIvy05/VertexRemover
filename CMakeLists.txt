cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED ENV{COMMONLIB_SSE_FOLDER})
  message(FATAL_ERROR "Missing COMMONLIB_SSE_FOLDER environment variable")
endif()

set(AUTHOR_NAME "Ivy")
set(PRODUCT_NAME "VertexRemover")
project(${PRODUCT_NAME} VERSION 1.0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(cmake/lib/copyOutputs.cmake)
include(cmake/lib/autoCollectSources.cmake)

add_src_folder(src)
add_include_folder(include)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)
list(APPEND sources ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${headers} ${sources})

set(ENABLE_SKYRIM_SE ON CACHE BOOL " " FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL " " FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL " " FORCE)
set(BUILD_TESTS OFF CACHE BOOL " " FORCE)

add_subdirectory("$ENV{COMMONLIB_SSE_FOLDER}" ${CMAKE_CURRENT_BINARY_DIR}/_deps/clib-build)
get_target_property(COMMONLIB_SRC_DIR CommonLibSSE SOURCE_DIR)
include(${COMMONLIB_SRC_DIR}/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(${PROJECT_NAME} AUTHOR ${AUTHOR_NAME} SOURCES ${headers} ${sources})

file(READ "${CMAKE_CURRENT_BINARY_DIR}/__${PROJECT_NAME}Plugin.cpp" PLUGIN_DECLARATION)
string(REPLACE "SKSE::StructCompatibility::Independent" "static_cast<SKSE::StructCompatibility>(SKSE::PluginVersionData::kVersionIndependentEx_NoStructUse | SKSE::PluginVersionData::kVersionIndependentEx_AddressLibraryV5)" PLUGIN_DECLARATION "${PLUGIN_DECLARATION}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/__${PROJECT_NAME}Plugin.cpp" "${PLUGIN_DECLARATION}")

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE include/PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
  copyOutputs("$ENV{SKYRIM_MODS_FOLDER}/${PRODUCT_NAME}")
endif()
