set(CMAKE_FOLDER "external")

list(APPEND CMAKE_MESSAGE_CONTEXT external)

# GoogleTest uses include_directories(), which the openDAQ CMake utilities reject in MODERN mode.
opendaq_get_cmake_mode(_NATIVE_STREAMING_CMAKE_MODE_SAVED)
opendaq_set_cmake_mode(ANCIENT)

add_subdirectory(fmt)
add_subdirectory(spdlog)
add_subdirectory(boost)

if (NATIVE_STREAMING_ENABLE_TESTS)
    add_subdirectory(gtest)
endif()

if (_NATIVE_STREAMING_CMAKE_MODE_SAVED)
    opendaq_set_cmake_mode(${_NATIVE_STREAMING_CMAKE_MODE_SAVED})
endif()
