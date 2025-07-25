cmake_minimum_required(VERSION 3.15)
project(numu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

option(BUILD_SHARED_LIBS "Build shared library" ON)
option(NUMU_BUILD_CLI "Build command-line interface" ON)
option(NUMU_BUILD_TESTS "Build tests" ON)

find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

add_subdirectory(src/core)
add_subdirectory(src/lib)

include(numu.modules.cmake)

if(NUMU_BUILD_CLI)
    add_subdirectory(src/cli)
endif()

if(NUMU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

install(DIRECTORY include/numu
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS numu_core
    EXPORT numu-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/numu-config.cmake.in
    ${CMAKE_BINARY_DIR}/numu-config.cmake
    INSTALL_DESTINATION lib/cmake/numu
)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/numu-config-version.cmake
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion
)

install(EXPORT numu-targets
    FILE numu-targets.cmake
    DESTINATION lib/cmake/numu
)

install(FILES
    ${CMAKE_BINARY_DIR}/numu-config.cmake
    ${CMAKE_BINARY_DIR}/numu-config-version.cmake
    DESTINATION lib/cmake/numu
)
