cmake_minimum_required(VERSION 3.20)
project(opw3_webport C)
set(CMAKE_C_STANDARD 11)
set(RECOMP_DIR "${CMAKE_SOURCE_DIR}/generated/fbdemo" CACHE PATH "armrecomp output directory")
set(MODULE_ELF "${CMAKE_SOURCE_DIR}/generated/fbdemo/module.elf" CACHE FILEPATH "Matching decrypted ELF")
set(PORT_NAME "vita-demo" CACHE STRING "Output module basename")
if(EMSCRIPTEN)
  add_compile_options(-pthread -msimd128 -msse2)
  add_link_options(-pthread -msimd128 -msse2)
endif()
add_subdirectory(external/vitarecomp)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fix_codegen.py"
  "${RECOMP_DIR}" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/lwmutex.inc" "${CMAKE_SOURCE_DIR}/src/io_positioned.inc" "${CMAKE_SOURCE_DIR}/src/io_web_stream.inc" "${CMAKE_SOURCE_DIR}/src/atrac9_frame.inc" "${CMAKE_SOURCE_DIR}/src/gxp_hlsl_branch.inc" "${CMAKE_SOURCE_DIR}/tools/configure_runtime.py")
set(runtime_overlays
  "${CMAKE_BINARY_DIR}/hle_threadmgr_port.c" "${CMAKE_BINARY_DIR}/sched_port.c"
  "${CMAKE_BINARY_DIR}/hle_io_port.c" "${CMAKE_BINARY_DIR}/atrac9_port.c"
  "${CMAKE_BINARY_DIR}/hle_stubs_port.c" "${CMAKE_BINARY_DIR}/hle_savedata_port.c"
  "${CMAKE_BINARY_DIR}/gxp_hlsl_port.c")
if(NOT EMSCRIPTEN)
  list(APPEND runtime_overlays "${CMAKE_BINARY_DIR}/mem_port.c")
endif()
if(EMSCRIPTEN AND PORT_NAME STREQUAL "opw3")
  list(APPEND runtime_overlays "${CMAKE_BINARY_DIR}/hle_avplayer_port.c")
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/configure_runtime.py"
  ${runtime_overlays} COMMAND_ERROR_IS_FATAL ANY)
get_target_property(runtime_sources vitarecomp SOURCES)
list(REMOVE_ITEM runtime_sources src/hle_threadmgr.c src/sched.c src/hle_io.c src/atrac9.c src/hle_stubs.c src/hle_savedata.c src/gxp_hlsl.c)
if(EMSCRIPTEN AND PORT_NAME STREQUAL "opw3")
  list(REMOVE_ITEM runtime_sources src/hle_avplayer.c)
endif()
set_property(TARGET vitarecomp PROPERTY SOURCES "${runtime_sources}")
if(EMSCRIPTEN AND PORT_NAME STREQUAL "opw3")
  target_sources(vitarecomp PRIVATE "${CMAKE_BINARY_DIR}/hle_avplayer_port.c")
endif()
target_sources(vitarecomp PRIVATE "${CMAKE_BINARY_DIR}/hle_threadmgr_port.c")
target_sources(vitarecomp PRIVATE "${CMAKE_BINARY_DIR}/sched_port.c" "${CMAKE_SOURCE_DIR}/src/hle_ult_runtime.c")
target_sources(vitarecomp PRIVATE "${CMAKE_BINARY_DIR}/hle_io_port.c")
target_sources(vitarecomp PRIVATE "${CMAKE_BINARY_DIR}/atrac9_port.c" "${CMAKE_SOURCE_DIR}/src/hle_audiodec.c")
target_sources(vitarecomp PRIVATE "${CMAKE_BINARY_DIR}/hle_stubs_port.c" "${CMAKE_SOURCE_DIR}/src/hle_np_offline.c")
target_sources(vitarecomp PRIVATE "${CMAKE_BINARY_DIR}/hle_savedata_port.c")
target_sources(vitarecomp PRIVATE "${CMAKE_BINARY_DIR}/gxp_hlsl_port.c")
target_include_directories(vitarecomp PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_sources(vitarecomp PRIVATE "${CMAKE_SOURCE_DIR}/src/hle_gxm_ext.c")
target_sources(vitarecomp PRIVATE "${CMAKE_SOURCE_DIR}/src/hle_display_ext.c")
target_sources(vitarecomp PRIVATE "${CMAKE_SOURCE_DIR}/src/hle_platform_ext.c")
target_include_directories(vitarecomp PRIVATE "${CMAKE_SOURCE_DIR}/external/vitarecomp/src")
if(EMSCRIPTEN)
  # Keep upstream untouched. Replace only its no-window platform adapter.
  get_target_property(host_sources vitahost SOURCES)
  list(REMOVE_ITEM host_sources window_none.c)
  set_property(TARGET vitahost PROPERTY SOURCES "${host_sources}")
  target_sources(vitahost PRIVATE "${CMAKE_SOURCE_DIR}/src/window_web.c")
endif()
if(NOT EMSCRIPTEN)
  get_target_property(runtime_sources vitarecomp SOURCES)
  list(REMOVE_ITEM runtime_sources src/mem.c)
  set_property(TARGET vitarecomp PROPERTY SOURCES "${runtime_sources}")
  target_sources(vitarecomp PRIVATE "${CMAKE_BINARY_DIR}/mem_port.c")
  enable_testing()
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fix_codegen.py" --fixture
    "${CMAKE_BINARY_DIR}/ldrd_fixture.inc" COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fix_codegen.py" "${RECOMP_DIR}" --game-fixture
    "${CMAKE_BINARY_DIR}/game_codegen_fixture.inc" COMMAND_ERROR_IS_FATAL ANY)
  add_executable(test_lwmutex tests/lwmutex.c)
  target_link_libraries(test_lwmutex PRIVATE vitarecomp)
  target_compile_options(test_lwmutex PRIVATE $<$<C_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-UNDEBUG>)
  add_test(NAME lwmutex COMMAND test_lwmutex)
  foreach(test_name ult positioned_io codegen audiodec np_offline)
    add_executable(test_${test_name} tests/${test_name}.c)
    target_link_libraries(test_${test_name} PRIVATE vitarecomp)
    target_include_directories(test_${test_name} PRIVATE "${CMAKE_SOURCE_DIR}/external/vitarecomp/src" "${CMAKE_BINARY_DIR}")
    target_compile_options(test_${test_name} PRIVATE $<$<C_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME ${test_name} COMMAND test_${test_name})
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 20)
  endforeach()
endif()
if(NOT EXISTS "${MODULE_ELF}")
  message(FATAL_ERROR "Missing matching ELF: run tools/build.py demo or prepare first")
endif()
vitarecomp_add_game(port RECOMP_DIR "${RECOMP_DIR}" SOURCES "${CMAKE_SOURCE_DIR}/external/vitarecomp/host/main.c")
if(MSVC)
  target_link_options(port PRIVATE "/MAP:${CMAKE_BINARY_DIR}/port.map")
endif()
if(EMSCRIPTEN)
  add_executable(web_io_probe tests/web_io.c)
  target_link_libraries(web_io_probe PRIVATE vitarecomp)
  target_compile_options(web_io_probe PRIVATE -UNDEBUG)
  set_target_properties(web_io_probe PROPERTIES OUTPUT_NAME "io-probe" SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/web/runtime")
  target_link_options(web_io_probe PRIVATE
    -O2 -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,worker
    -sPROXY_TO_PTHREAD=1 -sPTHREAD_POOL_SIZE=1 -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=33554432 -sSTACK_SIZE=2097152 -sDEFAULT_PTHREAD_STACK_SIZE=2097152
    -sEXIT_RUNTIME=1 -sFORCE_FILESYSTEM=1 -lidbfs.js "-sEXPORTED_RUNTIME_METHODS=['callMain','FS','IDBFS']")
  set_target_properties(port PROPERTIES OUTPUT_NAME "${PORT_NAME}" SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/web/runtime")
  target_link_options(port PRIVATE
    -O2 -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createVitaPort
    -sENVIRONMENT=web,worker -sPROXY_TO_PTHREAD=1 -sPTHREAD_POOL_SIZE=8
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=1140850688 -sMAXIMUM_MEMORY=2147483648
    -sSTACK_SIZE=2097152 -sDEFAULT_PTHREAD_STACK_SIZE=2097152
    -sEXIT_RUNTIME=1 -sFORCE_FILESYSTEM=1 -lidbfs.js
    "-sEXPORTED_RUNTIME_METHODS=['callMain','FS','IDBFS']"
    "--preload-file" "${MODULE_ELF}@/module.elf")
endif()
