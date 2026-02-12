cmake_minimum_required(VERSION 3.13)
project(SDL3MigrationCheck)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Find LLVM and Clang
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_INCLUDE_DIRS}")
find_package(Clang REQUIRED CONFIG)
message(STATUS "Found Clang in: ${CLANG_INCLUDE_DIRS}")

# Add LLVM definitions and include directories
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the plugin as a shared library
add_library(SDL3MigrationCheck SHARED
    SDL3MigrationCheck.cpp
)

# Link against clang libraries
# target_link_libraries(SDL3MigrationCheck)

# Don't add 'lib' prefix to the output
set_target_properties(SDL3MigrationCheck PROPERTIES
    PREFIX ""
)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings if needed
    set_target_properties(SDL3MigrationCheck PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

