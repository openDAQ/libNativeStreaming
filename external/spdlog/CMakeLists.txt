set_cmake_folder_context(TARGET_FOLDER_NAME)

if (NOT TARGET spdlog)
    set(spdlog_REQUIREDVERSION "1.8.0")

    find_package(spdlog GLOBAL QUIET ${spdlog_REQUIREDVERSION})
    if(spdlog_FOUND)
        message(STATUS "Found spdlog: ${spdlog_VERSION} ${spdlog_CONFIG}")
    else()
        message(STATUS "Fetching spdlog version ${spdlog_REQUIREDVERSION}")

        include(FetchContent)
        get_custom_fetch_content_params(spdlog FC_PARAMS)

        set(SPDLOG_INSTALL ON CACHE BOOL "" FORCE)
        set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)

        FetchContent_Declare(spdlog
                            GIT_REPOSITORY https://github.com/gabime/spdlog.git
                            GIT_TAG v${spdlog_REQUIREDVERSION}
                            GIT_PROGRESS ON
                            GIT_SHALLOW ON
                            GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
                            ${FC_PARAMS}
        )

        FetchContent_MakeAvailable(spdlog)
    endif()
endif()
