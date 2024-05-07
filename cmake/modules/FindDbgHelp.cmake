# List of possible library names
set(DBGHELP_NAMES dbghelp)

if(MSVC)
    # Try to find library
    find_library(DBGHELP_LIBRARY NAMES ${DBGHELP_NAMES})

    # Try to find include directory
    find_path(DBGHELP_INCLUDE_DIR NAMES dbghelp.h PATH_SUFFIXES include)

    include(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(DbgHelp REQUIRED_VARS DBGHELP_LIBRARY DBGHELP_INCLUDE_DIR)

    if(DbgHelp_FOUND)
        set(DBGHELP_LIBRARIES ${DBGHELP_LIBRARY})
    else()
        message(FATAL_ERROR "DbgHelp not found")
    endif()
endif()