set_cmake_folder_context(TARGET_FOLDER_NAME)

add_library(boost_websocket INTERFACE)
add_library(daq::native_streaming::boost_websocket ALIAS boost_websocket)
target_compile_definitions(boost_websocket INTERFACE BOOST_ALL_NO_LIB)

# handle Boost dependencies here only if not already handled by parent project
if (NOT TARGET Boost::beast)
    opendaq_append_required_boost_components(
        asio
        beast
    )
    opendaq_complete_boost_dependency(
        VERSION     "1.92.0"
        URL         "https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.gz"
        URL_HASH    "SHA256=f51707c27359a0df0cac1beada86de31bb5eed5e8285592dadec384df99c2984"
    )
endif()

if (TARGET Boost::beast)
    target_link_libraries(boost_websocket INTERFACE $<BUILD_INTERFACE:Boost::beast>)
endif()
