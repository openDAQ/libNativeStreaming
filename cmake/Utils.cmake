if (NOT COMMAND set_cmake_folder_context)
    function(set_cmake_folder_context OUTFOLDER)
        get_current_folder_name(TARGET_FOLDER_NAME)

        if (ARGC GREATER 1)
            list(APPEND CMAKE_MESSAGE_CONTEXT ${ARGV1})
        else()
            list(APPEND CMAKE_MESSAGE_CONTEXT ${TARGET_FOLDER_NAME})
        endif()

        set(CMAKE_MESSAGE_CONTEXT ${CMAKE_MESSAGE_CONTEXT} PARENT_SCOPE)
        if (ARGC GREATER 1)
            set(CMAKE_FOLDER "${CMAKE_FOLDER}/${ARGV1}" PARENT_SCOPE)
        else()
            set(CMAKE_FOLDER "${CMAKE_FOLDER}/${TARGET_FOLDER_NAME}" PARENT_SCOPE)
        endif()
        set(${OUTFOLDER} ${TARGET_FOLDER_NAME} PARENT_SCOPE)
    endfunction()
endif()

if (NOT COMMAND get_current_folder_name)
    function(get_current_folder_name OUTFOLDER)
        get_filename_component(FOLDER ${CMAKE_CURRENT_SOURCE_DIR} NAME)
        set(${OUTFOLDER} ${FOLDER} PARENT_SCOPE)
    endfunction()
endif()

if (NOT COMMAND prepend_include)
    function(prepend_include SUBFOLDER SOURCE_FILES)
        foreach( SOURCE_FILE ${${SOURCE_FILES}} )
          set( MODIFIED ${MODIFIED} "../include/${SUBFOLDER}/${SOURCE_FILE}" )
        endforeach()
        set( ${SOURCE_FILES} ${MODIFIED} PARENT_SCOPE )
    endfunction()
endif()

if (NOT COMMAND get_custom_fetch_content_params)
    function(get_custom_fetch_content_params LIBRARY_NAME OUTPARAM)
        set(FC_SOURCE_DIR ${FETCHCONTENT_EXTERNALS_DIR}/src)
        set(FC_SUBBUILD_DIR ${FETCHCONTENT_EXTERNALS_DIR}/subbuild/${CMAKE_GENERATOR}/${CMAKE_CXX_COMPILER_ID})

        if (CMAKE_GENERATOR_PLATFORM)
            set(FC_SUBBUILD_DIR ${FC_SUBBUILD_DIR}/${CMAKE_GENERATOR_PLATFORM})
        endif()

        set(${OUTPARAM}
            DOWNLOAD_DIR ${FC_SOURCE_DIR}
            SOURCE_DIR ${FC_SOURCE_DIR}/${LIBRARY_NAME}
            SUBBUILD_DIR ${FC_SUBBUILD_DIR}/${LIBRARY_NAME}
            PARENT_SCOPE
        )
    endfunction()
endif()
