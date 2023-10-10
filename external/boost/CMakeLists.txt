set_cmake_folder_context(TARGET_FOLDER_NAME)

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
