# FetchContent
include(FetchContent)
set(FETCHCONTENT_QUIET    off)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/external")

function(GitHelper name url tag add_sub inc_dir link_lib)
    FetchContent_Declare(
        ${name}
        GIT_REPOSITORY ${url}
        GIT_TAG        ${tag}
        # GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )

    FetchContent_GetProperties(${name})
    string(TOLOWER ${name} lcName)
    if (NOT ${lcName}_POPULATED)
        FetchContent_Populate(${name})
        set(${lcName}_SOURCE_DIR ${${lcName}_SOURCE_DIR} PARENT_SCOPE)
        set(${lcName}_BINARY_DIR ${${lcName}_BINARY_DIR} PARENT_SCOPE)
        if (add_sub)
            add_subdirectory(${${lcName}_SOURCE_DIR} ${${lcName}_BINARY_DIR} EXCLUDE_FROM_ALL)
        endif()
    endif()

    set(include_directories ${include_directories} ${${lcName}_SOURCE_DIR}/${inc_dir} PARENT_SCOPE)
    set(link_libraries      ${link_libraries} ${link_lib}                             PARENT_SCOPE)
endfunction(GitHelper)
