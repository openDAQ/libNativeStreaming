set_cmake_folder_context(TARGET_FOLDER_NAME)

add_library(boost_websocket INTERFACE)
add_library(daq::native_streaming::boost_websocket ALIAS boost_websocket)
target_compile_definitions(boost_websocket INTERFACE BOOST_ALL_NO_LIB)

# using only asio and beast
# handle Boost depencies here only if not already handled by parent project
if (NOT TARGET Boost::beast)
    set(Boost_MINVERSION "1.71.0")
    set(Boost_REQUIREDVERSION "1.82.0")

    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_LIBS ON)

    find_package(Boost ${Boost_MINVERSION} COMPONENTS system)

    if (Boost_FOUND)
        message(STATUS "Boost ${Boost_VERSION_STRING} at: ${Boost_INCLUDE_DIRS} | ${Boost_LIBRARY_DIRS}")

        target_link_libraries(boost_websocket INTERFACE ${Boost_LIBRARIES})
        target_include_directories(boost_websocket INTERFACE
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            $<BUILD_INTERFACE:${Boost_INCLUDE_DIRS}>
        )
    else (Boost_FOUND)
        message(STATUS "Fetching Boost ${Boost_REQUIREDVERSION}...")
        FetchContent_Declare(Boost
                             URL      https://github.com/boostorg/boost/releases/download/boost-${Boost_REQUIREDVERSION}/boost-${Boost_REQUIREDVERSION}.tar.xz
                             URL_HASH SHA256=fd60da30be908eff945735ac7d4d9addc7f7725b1ff6fcdcaede5262d511d21e
                             ${FC_PARAMS}
        )

        set(NEEDED_LIBRARIES asio
                             beast
        )

        set(BOOST_INCLUDE_LIBRARIES "${NEEDED_LIBRARIES}"
            CACHE STRING
            "List of libraries to build (default: all but excluded and incompatible)"
            FORCE
        )

        FetchContent_MakeAvailable(Boost)
    endif (Boost_FOUND)
endif ()

if (TARGET Boost::beast)
    target_link_libraries(boost_websocket INTERFACE $<BUILD_INTERFACE:Boost::beast>)
endif()
