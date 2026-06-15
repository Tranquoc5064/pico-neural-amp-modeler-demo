cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2)
set(PICO_PLATFORM rp2350)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb)  # vendored 0.20.0; SDK copy panics on UAC2

include(cmake/pico_sdk_import.cmake)
project(pico_nam C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# Embed a tone: nam_set_model() runs the nam2c host tool (cmake/nam_model.cmake).
include(cmake/nam_model.cmake)
nam_set_model(example.nam)

# NAM inference engine (NeuralAmpModelerCore, a2_fast path)
set(NAMCORE ${CMAKE_CURRENT_LIST_DIR}/lib/NeuralAmpModelerCore)
file(GLOB NAMCORE_SRC ${NAMCORE}/NAM/*.cpp ${NAMCORE}/NAM/wavenet/*.cpp)
add_library(nam_engine STATIC ${NAMCORE_SRC})
target_include_directories(nam_engine BEFORE PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include/compat)
target_include_directories(nam_engine SYSTEM PUBLIC
  ${NAMCORE} ${NAMCORE}/Dependencies/eigen ${NAMCORE}/Dependencies/nlohmann)
target_compile_definitions(nam_engine PUBLIC
  NAM_SAMPLE_FLOAT=1 NAM_ENABLE_A2_FAST=1 NAM_USE_INLINE_GEMM=1 NAM_SHARED_PTR_ATOMIC_FREE_FUNCS=1)
target_compile_options(nam_engine PRIVATE -O2 -ffast-math -funroll-loops -Wno-unused-parameter)

# whole-archive so the engine's static-initializer JSON parsers survive linking
set(NAM_ENGINE_OPT -Wl,--whole-archive nam_engine -Wl,--no-whole-archive)
set(NAM_HW_LIBS pico_stdlib pico_multicore cmsis_core hardware_clocks hardware_vreg)

add_executable(pico_nam_loopback
  src/main.c src/nam_fx.cpp src/usb_descriptors.c src/led.c ${NAM_MODEL_SOURCE})
target_include_directories(pico_nam_loopback PRIVATE include src)
target_link_libraries(pico_nam_loopback ${NAM_ENGINE_OPT} ${NAM_HW_LIBS} tinyusb_device tinyusb_board)
pico_add_extra_outputs(pico_nam_loopback)

set(NAM_SYS_KHZ 300000 CACHE STRING "Benchmark system clock (kHz)")
set(NAM_KSPLIT 14 CACHE STRING "Dual-core layer split (14 = optimal)")
add_executable(nam_bench src/benchmark.cpp ${NAM_MODEL_SOURCE})
target_include_directories(nam_bench PRIVATE include)
target_compile_definitions(nam_bench PRIVATE TARGET_SYS_KHZ=${NAM_SYS_KHZ} KSPLIT=${NAM_KSPLIT})
target_link_libraries(nam_bench ${NAM_ENGINE_OPT} ${NAM_HW_LIBS})
pico_enable_stdio_usb(nam_bench 1)
pico_add_extra_outputs(nam_bench)
