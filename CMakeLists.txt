cmake_minimum_required(VERSION 3.19)
project(LCore)

set(CMAKE_CXX_STANDARD 20)

# If MSVC, enable /Zc:__cplusplus
if (MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

# Required module
find_package(PkgConfig REQUIRED)
set(ADDITIONAL_LIBS "")

# Linux: liburing
if (UNIX AND NOT APPLE)
    pkg_check_modules(LIBURING REQUIRED liburing)
    include_directories(${LIBURING_INCLUDE_DIRS})
    link_directories(${LIBURING_LIBRARY_DIRS})
    add_definitions(${LIBURING_CFLAGS_OTHER})
    set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${LIBURING_LIBRARIES})
endif()

# LCore options

option(LCORE_ENABLE_RECORDSTACK "Enable record stack" ON)
option(LCORE_ENABLE_ASSERT "Enable assert" ON)

if (NOT DEFINED LCORE_NAMESPACE)
    set(LCORE_NAMESPACE "lcore" CACHE STRING "Namespace name")
endif()

if (LCORE_NAMESPACE STREQUAL "")
    set(LCORE_NAMESPACE_BEGIN "")
    set(LCORE_NAMESPACE_END "")
else()
    set(LCORE_NAMESPACE_BEGIN "namespace ${LCORE_NAMESPACE} {")
    set(LCORE_NAMESPACE_END "}")
endif()

set(LCORE_DEBUG OFF)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LCORE_DEBUG ON)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lcore/config.h
)

FILE(GLOB_RECURSE SRC_FILES src/*.c*)
FILE(GLOB_RECURSE INC_FILES include/*.h)

include_directories(include)
add_library(lcore ${SRC_FILES} ${INC_FILES})
set_target_properties(lcore PROPERTIES 
    POSITION_INDEPENDENT_CODE ON
)
link_libraries(lcore ${ADDITIONAL_LIBS})

if (LCORE_DEBUG)
    # Enable all warnings and treat warnings as errors
    message(STATUS "Debug build: Enabling all warnings and treating warnings as errors")
    if (MSVC)
        target_compile_options(lcore PRIVATE /W4 /WX)
    else()
        target_compile_options(lcore PRIVATE -Wall -Wextra -Werror -Wpedantic)
    endif()
endif ()

# add postfix if debug
set_target_properties(lcore PROPERTIES DEBUG_POSTFIX "d")

option(LCORE_ENABLE_TEST "Enable test for lcore" OFF)

if (LCORE_ENABLE_TEST)
    find_package(GTest REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()

# install the library
include(GNUInstallDirs)

install(TARGETS lcore
    EXPORT LCoreTargets  # Provide target export for find_package
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}  # Static library
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}  # Dynamic library
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  # Executables
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}  # Header files
)

# Install header files
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    # FILES_MATCHING PATTERN "*.h"
    PATTERN "*.in" EXCLUDE  # Exclude config.h.in
)

# Export the targets for use with find_package
install(EXPORT LCoreTargets
    FILE LCoreTargets.cmake
    NAMESPACE LCore::  # Target namespace
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCore  # Installation directory for the export file
)

# Generate the LCoreConfig.cmake file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/LCoreConfig.cmake.in  # Template file
    ${CMAKE_CURRENT_BINARY_DIR}/LCoreConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCore
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

# Generate the version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/LCoreConfigVersion.cmake
    VERSION 0.1
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/LCoreConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/LCoreConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCore
)
