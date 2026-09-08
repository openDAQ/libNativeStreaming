set(TEST_APP test_${PROJECT_NAME})

set(TEST_HEADERS test_base.hpp
                 test_connection.hpp
                 test_secrets.hpp
)

set(TEST_SOURCES test_server.cpp
                 test_client.cpp
                 test_tasks.cpp
                 test_connection.cpp
                 test_messages.cpp
                 test_base64.cpp
                 test_authentication_object.cpp
                 test_authentication.cpp
)

if (NATIVE_STREAMING_ENABLE_TLS)
    list(APPEND TEST_SOURCES test_tls.cpp)
endif()

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

if (NATIVE_STREAMING_ENABLE_TLS)
    # the TLS tests read their certificates and keys relative to the working directory, which is the
    # directory the test executable lives in
    add_custom_command(TARGET ${TEST_APP} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/secrets"
                "$<TARGET_FILE_DIR:${TEST_APP}>/secrets"
        COMMENT "Copying the TLS test fixtures next to the test executable"
    )
endif()

add_test(NAME ${TEST_APP}
    COMMAND $<TARGET_FILE_NAME:${TEST_APP}>
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_APP}>
)
