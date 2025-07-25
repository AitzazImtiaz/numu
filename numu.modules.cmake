cmake_minimum_required(VERSION 3.15)

option(NUMU_BUILD_ALL_MODULES "Build all available modules" OFF)
option(NUMU_BUILD_TESTS "Build module tests" ON)

set(NUMU_CORE_MODULES
    core
    arithmetic
    parsing
)

set(NUMU_STANDARD_MODULES
    calculus.differential
    calculus.integral
    calculus.limit
    linear.basic
    linear.decomposition
)

function(numu_add_module MODULE_NAME)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/modules/${MODULE_NAME})
    list(APPEND NUMU_ACTIVE_MODULES ${MODULE_NAME})
endfunction()

foreach(MODULE ${NUMU_CORE_MODULES})
    numu_add_module(${MODULE})
endforeach()

if(NUMU_BUILD_ALL_MODULES)
    foreach(MODULE ${NUMU_STANDARD_MODULES})
        numu_add_module(${MODULE})
    endforeach()
else()
    foreach(MODULE ${NUMU_STANDARD_MODULES})
        if(${NUMU_MODULE_${MODULE}})
            numu_add_module(${MODULE})
        endif()
    endforeach()
endif()

if(NUMU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test/modules)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numu/config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/numu/config.hpp
)

target_compile_definitions(numu_core
    PUBLIC NUMU_MODULES="${NUMU_ACTIVE_MODULES}"
)
