cmake_minimum_required(VERSION 3.16)
project(XCORE407I_FIRMWARE C CXX ASM)

# Ensure try-compile does not attempt to link full hosted executable (newlib _exit issue)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Try to locate ARM embedded toolchain if not explicitly provided
if (NOT CMAKE_C_COMPILER MATCHES "arm-none-eabi-gcc")
  find_program(ARM_GCC arm-none-eabi-gcc)
  find_program(ARM_GPP arm-none-eabi-g++)
  find_program(ARM_ASM arm-none-eabi-gcc)
  if (ARM_GCC AND ARM_GPP)
    message(STATUS "Using ARM embedded toolchain: ${ARM_GCC}")
    set(CMAKE_C_COMPILER ${ARM_GCC} CACHE FILEPATH "" FORCE)
    set(CMAKE_CXX_COMPILER ${ARM_GPP} CACHE FILEPATH "" FORCE)
    set(CMAKE_ASM_COMPILER ${ARM_ASM} CACHE FILEPATH "" FORCE)
  else()
    message(FATAL_ERROR "arm-none-eabi-gcc toolchain not found. Please install gcc-arm-none-eabi.")
  endif()
endif()

# Locate objcopy for post-build binary generation
if (NOT CMAKE_OBJCOPY)
  find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy)
  if (NOT CMAKE_OBJCOPY)
    find_program(CMAKE_OBJCOPY objcopy)
  endif()
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Absolute project root (two levels up from this file)
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
get_filename_component(PROJECT_ROOT ${PROJECT_ROOT} REALPATH)
message(STATUS "Project root: ${PROJECT_ROOT}")

set(REMORA_SRC_DIR ${PROJECT_ROOT}/Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I)
set(ST_ROOT        ${PROJECT_ROOT}/st)
set(CUBE_ROOT      ${ST_ROOT}/STM32CubeF4)
set(HAL_CONF_DIR   ${ST_ROOT}/hal_conf)

set(CUBE_CMSIS_INCLUDE ${CUBE_ROOT}/Drivers/CMSIS/Include)
set(CUBE_DEVICE_INCLUDE ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include)
set(CUBE_HAL_INC ${CUBE_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc)

if (NOT EXISTS ${CUBE_HAL_INC}/stm32f4xx_hal.h)
  message(FATAL_ERROR "STM32CubeF4 HAL headers not found at ${CUBE_HAL_INC}. Expected submodule at ${CUBE_ROOT}. If missing: git submodule add https://github.com/STMicroelectronics/STM32CubeF4.git st/STM32CubeF4 && git submodule update --init --recursive")
endif()

add_executable(${PROJECT_NAME}
  startup_stm32f407xx.s
  ${REMORA_SRC_DIR}/main_xcore407i.cpp
  ${REMORA_SRC_DIR}/system_clock.c
  ${REMORA_SRC_DIR}/ethernet_init.c
  ${REMORA_SRC_DIR}/remora_data.c
  ${REMORA_SRC_DIR}/lwip/ethernetif.c
  ${REMORA_SRC_DIR}/lwip/lwip_port.c
  ${REMORA_SRC_DIR}/lwip/remora_udp.c
  ${REMORA_SRC_DIR}/stepgen.c
  ${REMORA_SRC_DIR}/motion_io.c
  ${REMORA_SRC_DIR}/jog.c
  ${REMORA_SRC_DIR}/encoder.c
  ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
  syscalls.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${PROJECT_ROOT}/Remora-OS6/Firmware/FirmwareSource/Remora-OS6
  ${REMORA_SRC_DIR}
  ${REMORA_SRC_DIR}/lwip
  ${HAL_CONF_DIR}
  ${CUBE_CMSIS_INCLUDE}
  ${CUBE_DEVICE_INCLUDE}
  ${CUBE_HAL_INC}
  ${CUBE_ROOT}/Middlewares/Third_Party/LwIP/src/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lwip_port
)

# HAL sources
set(HAL_SRC_DIR ${CUBE_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src)
set(HAL_SOURCES
  ${HAL_SRC_DIR}/stm32f4xx_hal.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_rcc.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_rcc_ex.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_gpio.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_cortex.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_pwr.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_pwr_ex.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_flash.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_flash_ex.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_dma.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_eth.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_tim.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_tim_ex.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_exti.c
)

target_sources(${PROJECT_NAME} PRIVATE ${HAL_SOURCES})

# Compiler flags (target-scoped to avoid quoting artifacts in flags.make)
set(COMMON_FLAGS
  -mcpu=cortex-m4
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -O2
  -ffunction-sections
  -fdata-sections
  -fno-unwind-tables
  -fno-asynchronous-unwind-tables
  -Wall
  -Wextra
)

target_compile_options(${PROJECT_NAME} PRIVATE ${COMMON_FLAGS})
target_compile_definitions(${PROJECT_NAME} PRIVATE STM32F407xx USE_HAL_DRIVER)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -mcpu=cortex-m4 -mthumb")

# Linker script
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F407IGT_FLASH.ld)

# Linker options
if (NOT CMAKE_OBJCOPY)
  message(WARNING "objcopy not found; .bin generation will be skipped")
endif()

target_link_options(${PROJECT_NAME} PRIVATE -T${LINKER_SCRIPT} -Wl,--gc-sections -Wl,-Map=${PROJECT_NAME}.map -nostartfiles -Wl,--print-memory-usage)

# Post-build binary
if (CMAKE_OBJCOPY)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME} ${PROJECT_NAME}.bin
    COMMENT "Generating binary file"
  )
endif()

# --- lwIP (Cube provided) minimal subset ---
set(LWIP_ROOT ${CUBE_ROOT}/Middlewares/Third_Party/LwIP)
if (NOT EXISTS ${LWIP_ROOT}/src/core/init.c)
  message(FATAL_ERROR "Expected lwIP sources under ${LWIP_ROOT}. Ensure Cube repo has Middlewares/Third_Party/LwIP")
endif()

set(LWIP_CORE_SRC
  ${LWIP_ROOT}/src/core/init.c
  ${LWIP_ROOT}/src/core/mem.c
  ${LWIP_ROOT}/src/core/memp.c
  ${LWIP_ROOT}/src/core/netif.c
  ${LWIP_ROOT}/src/core/pbuf.c
  ${LWIP_ROOT}/src/core/timeouts.c
  ${LWIP_ROOT}/src/core/def.c
  ${LWIP_ROOT}/src/core/ip.c
  ${LWIP_ROOT}/src/core/inet_chksum.c
  # autoip omitted
  ${LWIP_ROOT}/src/core/ipv4/icmp.c
  ${LWIP_ROOT}/src/core/ipv4/ip4_addr.c
  ${LWIP_ROOT}/src/core/ipv4/ip4.c
  ${LWIP_ROOT}/src/core/ipv4/ip4_frag.c
  ${LWIP_ROOT}/src/core/ipv4/etharp.c
  ${LWIP_ROOT}/src/core/udp.c
  ${LWIP_ROOT}/src/netif/ethernet.c
)

add_library(lwipcore STATIC ${LWIP_CORE_SRC} lwip_port/sys_arch.c)
target_include_directories(lwipcore PUBLIC
  ${LWIP_ROOT}/src/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lwip_port
)
target_compile_definitions(lwipcore PUBLIC NO_SYS=1 LWIP_TIMEVAL_PRIVATE=1)

target_link_libraries(${PROJECT_NAME} PRIVATE lwipcore)
