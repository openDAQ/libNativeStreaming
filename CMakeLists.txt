cmake_minimum_required(VERSION 3.24)

project(native_streaming VERSION 1.0.7 LANGUAGES CXX)

if (NOT CMAKE_MESSAGE_CONTEXT)
    set(CMAKE_MESSAGE_CONTEXT ${PROJECT_NAME})
    set(CMAKE_MESSAGE_CONTEXT_SHOW ON CACHE BOOL "Show CMake message context")
endif()

get_filename_component(ROOT_DIR ${CMAKE_SOURCE_DIR} REALPATH)

option(NATIVE_STREAMING_ENABLE_TESTS "Enable unit-tests" OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Threads)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake
)

if (WIN32)
    set(MIN_SUPPORTED_WINDOWS 0x0601)
    add_compile_definitions(WIN32_LEAN_AND_MEAN
                            NOMINMAX
                            _WIN32_WINNT=${MIN_SUPPORTED_WINDOWS} # Windows 7 Compat
    )

    message(STATUS "Setting minimum supported API to Windows 7 (${MIN_SUPPORTED_WINDOWS})")
endif()

set(FETCHCONTENT_EXTERNALS_DIR ${ROOT_DIR}/build/__external CACHE PATH "FetchContent folder prefix")

include(Utils)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(external)
add_subdirectory(src)

if (NATIVE_STREAMING_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# install the library and generate export set (note that no
# targets file is generated or installed in this step)
install(
    TARGETS ${PROJECT_NAME} boost_websocket
    EXPORT ${PROJECT_NAME}
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

# install the public headers
install(
    DIRECTORY include/${PROJECT_NAME}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# install a targets file for the generated export set
install(
    EXPORT ${PROJECT_NAME}
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE daq::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${${PROJECT_NAME}_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)
