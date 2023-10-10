set(TEST_APP test_${PROJECT_NAME})

set(TEST_HEADERS test_base.hpp
                 test_connection.hpp
)

set(TEST_SOURCES test_server.cpp
                 test_client.cpp
                 test_tasks.cpp
                 test_connection.cpp
                 test_messages.cpp
)

add_executable(${TEST_APP} test_app.cpp
                           ${TEST_HEADERS}
                           ${TEST_SOURCES}
)

target_link_libraries(${TEST_APP} PRIVATE native_streaming
                                          gtest
)

set_target_properties(${TEST_APP} PROPERTIES DEBUG_POSTFIX _debug)
target_include_directories(${TEST_APP} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_features(${TEST_APP} PRIVATE cxx_std_17)

add_test(NAME ${TEST_APP}
    COMMAND $<TARGET_FILE_NAME:${TEST_APP}>
    WORKING_DIRECTORY bin
)
