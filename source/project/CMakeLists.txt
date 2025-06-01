# CMakeLists.txt – Professional Build Configuration

cmake_minimum_required(VERSION 3.10)
project(GameOfLifeMPI
    VERSION 1.0.0
    DESCRIPTION "Conway's Game of Life parallel implementation with OpenMPI."
    LANGUAGES C
)

# --------------------------------------------------------------------------------------------------
# Project-wide Settings
# --------------------------------------------------------------------------------------------------

# Enforce C99 standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Define installation directories
include(GNUInstallDirs)

# --------------------------------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------------------------------

# Enable building of unit tests (requires CTest)
option(ENABLE_TESTS "Build unit and integration tests" ON)

# --------------------------------------------------------------------------------------------------
# Find Dependencies
# --------------------------------------------------------------------------------------------------

# Find MPI (both mpich and OpenMPI)
find_package(MPI REQUIRED)
if(NOT MPI_C_FOUND)
    message(FATAL_ERROR "MPI C compiler not found. Please install OpenMPI or MPICH.")
endif()

# --------------------------------------------------------------------------------------------------
# Include Directories
# --------------------------------------------------------------------------------------------------

# Add include/ to the search path for headers
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${MPI_C_INCLUDE_PATH}
)

# --------------------------------------------------------------------------------------------------
# Source Files and Targets
# --------------------------------------------------------------------------------------------------

# Core library (static) – if in future you want to link it elsewhere
add_library(libgameoflife STATIC
    src/life.c
    src/openMPI.c
    src/utils.c
)

# Public include directories for the library
target_include_directories(libgameoflife PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    ${MPI_C_INCLUDE_PATH}
)

# Link MPI to the library
target_link_libraries(libgameoflife PUBLIC ${MPI_C_LIBRARIES})
target_compile_definitions(libgameoflife PRIVATE USE_MPI)

# Executable target
add_executable(game_of_life
    src/main.c
)

# Link with the library and MPI
target_link_libraries(game_of_life PRIVATE libgameoflife ${MPI_C_LIBRARIES})

# --------------------------------------------------------------------------------------------------
# Compiler Warnings and Optimizations
# --------------------------------------------------------------------------------------------------

# Enable common warnings
if(MSVC)
    target_compile_options(libgameoflife PRIVATE /W4 /WX)
    target_compile_options(game_of_life   PRIVATE /W4 /WX)
else()
    target_compile_options(libgameoflife PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(game_of_life   PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# --------------------------------------------------------------------------------------------------
# Installation Rules
# --------------------------------------------------------------------------------------------------

# Install the executable
install(TARGETS game_of_life
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install the library and headers
install(TARGETS libgameoflife
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# --------------------------------------------------------------------------------------------------
# Testing (optional)
# --------------------------------------------------------------------------------------------------

# if(ENABLE_TESTS)
#     enable_testing()
# 
#     # Add a subdirectory containing test CMakeLists (if exists)
#     # For example, tests/test_life.c
#     add_executable(test_life
#         tests/test_life.c
#     )
#     target_link_libraries(test_life PRIVATE libgameoflife)
#     add_test(NAME LifeRulesTest COMMAND test_life)
# endif()

# --------------------------------------------------------------------------------------------------
# Packaging / Exporting (optional)
# --------------------------------------------------------------------------------------------------

# Generate a Config file for find_package support
# include(CMakePackageConfigHelpers)
# write_basic_package_version_file(
#     "${CMAKE_CURRENT_BINARY_DIR}/GameOfLifeMPIConfigVersion.cmake"
#     VERSION ${PROJECT_VERSION}
#     COMPATIBILITY AnyNewerVersion
# )
# 
# install(EXPORT GameOfLifeMPITargets
#     FILE GameOfLifeMPITargets.cmake
#     NAMESPACE GameOfLifeMPI::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GameOfLifeMPI
# )
# 
# configure_package_config_file(
#     "${CMAKE_SOURCE_DIR}/cmake/GameOfLifeMPIConfig.cmake.in"
#     "${CMAKE_CURRENT_BINARY_DIR}/GameOfLifeMPIConfig.cmake"
#     INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GameOfLifeMPI
# )
# 
# install(FILES
#     "${CMAKE_CURRENT_BINARY_DIR}/GameOfLifeMPIConfig.cmake"
#     "${CMAKE_CURRENT_BINARY_DIR}/GameOfLifeMPIConfigVersion.cmake"
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GameOfLifeMPI
# )

# --------------------------------------------------------------------------------------------------
# Exporting for Consumption by Other Projects
# --------------------------------------------------------------------------------------------------

# export(EXPORT GameOfLifeMPITargets
#     FILE "${CMAKE_CURRENT_BINARY_DIR}/GameOfLifeMPITargets.cmake"
#     NAMESPACE GameOfLifeMPI::
# )

# --------------------------------------------------------------------------------------------------
# End of CMakeLists.txt
# --------------------------------------------------------------------------------------------------
