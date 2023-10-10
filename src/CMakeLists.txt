cmake_minimum_required(VERSION 3.13)

set(SRC_Include common.hpp
                logging.hpp
                async_reader.hpp
                async_writer.hpp
                session.hpp
                server.hpp
                client.hpp
)

prepend_include(${PROJECT_NAME} SRC_Include)

set(SRC_Cpp logging.cpp
            async_reader.cpp
            async_writer.cpp
            session.cpp
            server.cpp
            client.cpp
)

add_library(${PROJECT_NAME} STATIC ${SRC_Include} ${SRC_Cpp})
add_library(daq::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# if target exists it means that boost is being built from source
# because Boost_FOUND is false and Boost_LIBRARIES is empty
if (TARGET Boost::beast)
    set(BOOST_LIBS $<BUILD_INTERFACE:Boost::beast>)
else()
    set(BOOST_LIBS ${Boost_LIBRARIES})
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC spdlog
                                             fmt
                                             ${BOOST_LIBS}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/>
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
