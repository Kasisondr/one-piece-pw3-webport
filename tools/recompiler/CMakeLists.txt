cmake_minimum_required(VERSION 3.20)
project(opw3_recompiler C)

set(upstream "${CMAKE_CURRENT_SOURCE_DIR}/../../external/vitarecomp")
add_subdirectory("${upstream}" upstream)
set(decoder "${upstream}/tools/armrecomp/decode.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${decoder}")
file(READ "${decoder}" decoder_source)
set(marker "    if (tb->rn != 15 || tb->rm == ARM_NO_REG) return 0;")
string(FIND "${decoder_source}" "${marker}" marker_position)
if(marker_position EQUAL -1)
  message(FATAL_ERROR "Pinned armrecomp table decoder changed")
endif()
string(REPLACE "${marker}" "${marker}
    /* Reviewed PCSE00638 scene and asset parser tables. The caller verifies the ELF. */
    if (tb->op == OP_TBH && tb->rm == 0) {
        if (tb->addr == 0x812D001Au) return 21;
        if (tb->addr == 0x81335D84u) return 8;
        if (tb->addr == 0x81335DA2u) return 13;
    }" decoder_source "${decoder_source}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/decode.c" "${decoder_source}")
get_target_property(armrecomp_sources armrecomp SOURCES)
list(REMOVE_ITEM armrecomp_sources decode.c)
set_property(TARGET armrecomp PROPERTY SOURCES "${armrecomp_sources}")
target_sources(armrecomp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/decode.c")
