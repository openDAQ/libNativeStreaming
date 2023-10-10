set(CMAKE_FOLDER "external")

list(APPEND CMAKE_MESSAGE_CONTEXT external)

add_subdirectory(fmt)
add_subdirectory(spdlog)
add_subdirectory(boost)

if (NATIVE_STREAMING_ENABLE_TESTS)
    add_subdirectory(gtest)
endif()
