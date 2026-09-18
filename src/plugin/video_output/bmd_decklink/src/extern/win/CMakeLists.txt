if(NOT CMAKE_C_COMPILER_LOADED)
    enable_language(C)
endif()

find_program(MIDL_EXE midl REQUIRED)

set(IDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/DeckLinkAPI.idl)
set(GEN_DIR  ${CMAKE_CURRENT_BINARY_DIR}/idl_generated)
file(MAKE_DIRECTORY ${GEN_DIR})

set(GEN_H       ${GEN_DIR}/DeckLinkAPI.h)
set(GEN_IID_C   ${GEN_DIR}/DeckLinkAPI_i.c)

add_custom_command(
OUTPUT  ${GEN_H} ${GEN_IID_C} ${GEN_PROXY_C} ${GEN_DLL_C}
COMMAND ${MIDL_EXE}
        /nologo
        /env x64
        /h DeckLinkAPI.h
        /iid DeckLinkAPI_i.c
        /out ${GEN_DIR}
        ${IDL_FILE}
DEPENDS ${IDL_FILE}
VERBATIM
)

add_custom_target(decklink_idl_gen DEPENDS ${GEN_H} ${GEN_IID_C})

add_library(decklink_idl STATIC
    ${GEN_IID_C}
)
add_dependencies(decklink_idl decklink_idl_gen)
target_include_directories(decklink_idl PUBLIC ${GEN_DIR})
