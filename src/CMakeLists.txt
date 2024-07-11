cmake_minimum_required(VERSION 3.13)

set(SRC_Include common.hpp
                logging.hpp
                async_reader.hpp
                async_writer.hpp
                session.hpp
                server.hpp
                client.hpp
                authentication.hpp
                utils/boost_compatibility_utils.hpp
                utils/base64.hpp
)

prepend_include(${PROJECT_NAME} SRC_Include)

set(SRC_Cpp logging.cpp
            async_reader.cpp
            async_writer.cpp
            session.cpp
            server.cpp
            client.cpp
            authentication.cpp
            utils/boost_compatibility_utils.cpp
            utils/base64.cpp
)

if(MSVC)
    if(${MSVC_VERSION} LESS 1920)
        # msvc-17 has a compiler bug when cmpiling functions defined in boost_compatibility_utils
        # this is why we compile them using cxx_std_14
        set_source_files_properties(
            utils/boost_compatibility_utils.cpp
            PROPERTIES COMPILE_FLAGS /std:c++14
        )
    endif()
endif()

add_library(${PROJECT_NAME} STATIC ${SRC_Include} ${SRC_Cpp})
add_library(daq::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC spdlog
                                             fmt
                                             daq::native_streaming::boost_websocket
)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/>
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()
