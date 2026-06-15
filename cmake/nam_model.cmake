# Embed a NAM A2 capture as a C weight array via the nam2c host tool, which is
# built with the native toolchain (no Python in the build) by ExternalProject.
#
#   nam_set_model(<.nam>)  — extract the A2-Lite sub-model, validate it with the
#                            engine's is_a2_shape, and emit nam_model.c; the path
#                            is returned in NAM_MODEL_SOURCE (caller scope).
include(ExternalProject)

ExternalProject_Add(nam_host_tools
    SOURCE_DIR       ${CMAKE_SOURCE_DIR}/tools
    BINARY_DIR       ${CMAKE_BINARY_DIR}/host-tools
    CMAKE_ARGS       -DCMAKE_BUILD_TYPE=Release
    BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/host-tools/nam2c
    INSTALL_COMMAND  "")

function(nam_set_model nam)
    if(NOT IS_ABSOLUTE ${nam})
        set(nam ${CMAKE_SOURCE_DIR}/${nam})
    endif()
    set(_c ${CMAKE_BINARY_DIR}/generated/nam_model.c)
    add_custom_command(
        OUTPUT  ${_c}
        COMMAND ${CMAKE_BINARY_DIR}/host-tools/nam2c ${nam} ${_c}
        DEPENDS nam_host_tools ${nam}
        COMMENT "Embedding model: ${nam}"
        VERBATIM)
    set(NAM_MODEL_SOURCE ${_c} PARENT_SCOPE)
endfunction()
