cmake_minimum_required(VERSION 3.16)

# Configure cross toolchain & try-compile BEFORE project()
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Prefer user toolchain if already in cache; otherwise find arm-none-eabi
if(NOT DEFINED CMAKE_C_COMPILER)
  find_program(ARM_GCC arm-none-eabi-gcc)
  if(NOT ARM_GCC)
    message(FATAL_ERROR "arm-none-eabi-gcc not found in PATH")
  endif()
  set(CMAKE_C_COMPILER ${ARM_GCC} CACHE FILEPATH "")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
  find_program(ARM_GPP arm-none-eabi-g++)
  if(NOT ARM_GPP)
    message(FATAL_ERROR "arm-none-eabi-g++ not found in PATH")
  endif()
  set(CMAKE_CXX_COMPILER ${ARM_GPP} CACHE FILEPATH "")
endif()
if(NOT DEFINED CMAKE_ASM_COMPILER)
  set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER} CACHE FILEPATH "")
endif()

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
# NOTE: Do not set CMAKE_EXE_LINKER_FLAGS_INIT with --specs here; it's supplied exactly once via arm_flags

project(XCORE407I_FIRMWARE C CXX ASM)

# Ensure --specs=nosys.specs applied exactly once (avoid duplicate spec rename error)
if (CMAKE_EXE_LINKER_FLAGS MATCHES "nosys.specs")
  string(REGEX REPLACE "(^| )--specs=nosys\\.specs" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nosys.specs" CACHE STRING "" FORCE)

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

# ---------------------------------------------------------------------------
# Board crystal frequency selection (some XCore407I variants are assembled
# with 8 MHz HSE, others with 25 MHz). Expose a cache option so users do NOT
# need to edit source when building for the alternate crystal.
# Usage: cmake -B build -DBOARD_HSE_FREQ_MHZ=25 ..  (default 8)
# Only 8 or 25 are accepted; we derive PLLM so that VCO input = 1 MHz.
# ---------------------------------------------------------------------------
set(BOARD_HSE_FREQ_MHZ 8 CACHE STRING "HSE crystal frequency in MHz (8 or 25)")
if(NOT BOARD_HSE_FREQ_MHZ STREQUAL "8" AND NOT BOARD_HSE_FREQ_MHZ STREQUAL "25")
  message(FATAL_ERROR "BOARD_HSE_FREQ_MHZ must be 8 or 25 (got '${BOARD_HSE_FREQ_MHZ}')")
endif()
add_compile_definitions(BOARD_HSE_FREQ_MHZ=${BOARD_HSE_FREQ_MHZ})
message(STATUS "Configured HSE crystal: ${BOARD_HSE_FREQ_MHZ} MHz")

# Absolute project root (two levels up from this file)
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
get_filename_component(PROJECT_ROOT ${PROJECT_ROOT} REALPATH)
message(STATUS "Project root: ${PROJECT_ROOT}")

set(REMORA_SRC_DIR_DEFAULT ${PROJECT_ROOT}/Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I)
# Allow overriding the Remora target source path from the cache so Windows users can point to an external checkout
set(REMORA_SRC_DIR ${REMORA_SRC_DIR_DEFAULT} CACHE PATH "Path to Remora-OS6 TARGET_XCORE407I sources (folder containing main_xcore407i.cpp)")
# Allow user to force build failure if full Remora sources are not found (avoid silent minimal fallback)
option(FULL_FIRMWARE "Require full Remora firmware sources; fail if missing" OFF)
option(REMORA_AUTO_FETCH "Automatically clone Remora OS6 repository when sources are missing" OFF)
set(REMORA_GIT_URL "https://example.com/Remora-OS6.git" CACHE STRING "Git URL of Remora OS6 repository (set real URL before enabling REMORA_AUTO_FETCH)")
set(ST_ROOT        ${PROJECT_ROOT}/st)
# Allow overriding the STM32CubeF4 location via -DCUBE_ROOT=... (defaults to st/STM32CubeF4)
set(CUBE_ROOT_DEFAULT ${ST_ROOT}/STM32CubeF4)
set(CUBE_ROOT ${CUBE_ROOT_DEFAULT} CACHE PATH "Path to STM32CubeF4 root (contains Drivers/, Middlewares/, etc.)")
set(HAL_CONF_DIR   ${ST_ROOT}/hal_conf)

set(CUBE_CMSIS_INCLUDE ${CUBE_ROOT}/Drivers/CMSIS/Include)
set(CUBE_DEVICE_INCLUDE ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include)
set(CUBE_HAL_INC ${CUBE_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc)

# If the default (st/STM32CubeF4) path is missing, auto-fallback to a sibling STM32CubeF4 folder
if (NOT EXISTS ${CUBE_HAL_INC}/stm32f4xx_hal.h)
  # Fallback 1: project root sibling "STM32CubeF4"
  set(CUBE_ROOT_ALT ${PROJECT_ROOT}/STM32CubeF4)
  if (EXISTS ${CUBE_ROOT_ALT}/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h)
    message(STATUS "CUBE_ROOT default not found; using fallback ${CUBE_ROOT_ALT}")
    set(CUBE_ROOT ${CUBE_ROOT_ALT} CACHE PATH "Path to STM32CubeF4 root (auto-detected fallback)" FORCE)
  else()
    # Fallback 2: parent of PROJECT_ROOT (user workspace root) contains STM32CubeF4
    get_filename_component(PROJECT_PARENT ${PROJECT_ROOT} DIRECTORY)
    set(CUBE_ROOT_PARENT ${PROJECT_PARENT}/STM32CubeF4)
    if (EXISTS ${CUBE_ROOT_PARENT}/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h)
      message(STATUS "CUBE_ROOT default not found; using parent fallback ${CUBE_ROOT_PARENT}")
      set(CUBE_ROOT ${CUBE_ROOT_PARENT} CACHE PATH "Path to STM32CubeF4 root (auto-detected parent fallback)" FORCE)
    endif()
  endif()
  # Recompute include paths if we updated CUBE_ROOT
  set(CUBE_CMSIS_INCLUDE ${CUBE_ROOT}/Drivers/CMSIS/Include)
  set(CUBE_DEVICE_INCLUDE ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include)
  set(CUBE_HAL_INC ${CUBE_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc)
endif()

if (NOT EXISTS ${CUBE_HAL_INC}/stm32f4xx_hal.h)
  message(FATAL_ERROR "STM32CubeF4 HAL headers not found. Tried:\n"
    "  Primary: ${CUBE_ROOT_DEFAULT}/Drivers/STM32F4xx_HAL_Driver/Inc\n"
    "  Fallback: ${PROJECT_ROOT}/STM32CubeF4/Drivers/STM32F4xx_HAL_Driver/Inc\n"
    "Effective CUBE_ROOT=\"${CUBE_ROOT}\"\n"
    "Hint: pass -DCUBE_ROOT=\"C:/path/to/STM32CubeF4\" or place the Cube repo in one of the probed locations.")
endif()

# If not present, try a few auto-detected fallbacks (do not override an existing valid user-provided path)
if (NOT EXISTS ${REMORA_SRC_DIR}/main_xcore407i.cpp)
  set(_REMORA_CANDIDATES
    ${PROJECT_ROOT}/Examples_Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I
    ${PROJECT_ROOT}/../Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I
  )
  foreach(_cand IN LISTS _REMORA_CANDIDATES)
    if (EXISTS ${_cand}/main_xcore407i.cpp)
      message(STATUS "Auto-detected Remora sources at ${_cand}")
      set(REMORA_SRC_DIR ${_cand} CACHE PATH "Path to Remora-OS6 TARGET_XCORE407I sources (auto-detected)" FORCE)
      break()
    endif()
  endforeach()
  # Optional fetch: clone the repository root so that REMORA_SRC_DIR_DEFAULT becomes valid
  if (REMORA_AUTO_FETCH AND NOT EXISTS ${REMORA_SRC_DIR}/main_xcore407i.cpp)
    if (REMORA_GIT_URL STREQUAL "https://example.com/Remora-OS6.git")
      message(FATAL_ERROR "REMORA_AUTO_FETCH=ON but REMORA_GIT_URL still placeholder. Set -DREMORA_GIT_URL=... to real repo URL.")
    endif()
    include(ExternalProject)
    set(REMORA_FETCH_ROOT ${PROJECT_ROOT}/Remora-OS6)
    ExternalProject_Add(remora_fetch
      GIT_REPOSITORY ${REMORA_GIT_URL}
      GIT_TAG main
      UPDATE_DISCONNECTED 1
      SOURCE_DIR ${REMORA_FETCH_ROOT}
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
      LOG_DOWNLOAD 1
    )
    add_custom_target(remora_prepare ALL
      DEPENDS remora_fetch
      COMMENT "Fetching Remora OS6 sources from ${REMORA_GIT_URL}")
    # Mark that after fetch the default path should contain the target folder
    add_custom_command(TARGET remora_prepare POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo "Remora sources fetched. Re-run CMake to pick them up if first configure did not include files.")
  endif()
endif()

# Validate (post-fallback) presence; provide guidance or fail if FULL_FIRMWARE requested
if (NOT EXISTS ${REMORA_SRC_DIR}/main_xcore407i.cpp)
  message(STATUS
    "[INFO] Remora target sources not found at ${REMORA_SRC_DIR}."
    " Provide them or configure with -DREMORA_SRC_DIR=\"C:/path/to/Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I\"."
  )
  if (FULL_FIRMWARE)
    message(FATAL_ERROR "FULL_FIRMWARE=ON but Remora sources are missing. Aborting.")
  endif()
endif()

set(MINIMAL_FIRMWARE 0)
if (EXISTS ${REMORA_SRC_DIR}/main_xcore407i.cpp)
  add_executable(${PROJECT_NAME}
    startup_stm32f407xx.s
    ${REMORA_SRC_DIR}/main_xcore407i.cpp
    ${REMORA_SRC_DIR}/system_clock.c
    ${REMORA_SRC_DIR}/ethernet_init.c
    ${REMORA_SRC_DIR}/debug_uart.c
    ${REMORA_SRC_DIR}/version.c
    ${REMORA_SRC_DIR}/ulpi_init.c
    ${REMORA_SRC_DIR}/build_id.c
    ${REMORA_SRC_DIR}/remora_data.c
    ${REMORA_SRC_DIR}/drivers/drivers_init.c
    ${REMORA_SRC_DIR}/lwip/ethernetif.c
    ${REMORA_SRC_DIR}/lwip/lwip_port.c
    ${REMORA_SRC_DIR}/lwip/remora_udp.c
    ${REMORA_SRC_DIR}/stepgen.c
    ${REMORA_SRC_DIR}/motion_io.c
    ${REMORA_SRC_DIR}/jog.c
    ${REMORA_SRC_DIR}/homing.c
    ${REMORA_SRC_DIR}/config_store.c
    ${REMORA_SRC_DIR}/encoder.c
    ${REMORA_SRC_DIR}/auth.c
    ${REMORA_SRC_DIR}/board_motion_gpio.c
    ${REMORA_SRC_DIR}/usb/usbd_conf.c
    ${REMORA_SRC_DIR}/usb/usbd_desc.c
    ${REMORA_SRC_DIR}/usb/usbd_cdc_if.c
    ${PROJECT_ROOT}/src/net/udp_echo.c
    ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
    syscalls.c
  )
  if (TARGET remora_prepare)
    add_dependencies(${PROJECT_NAME} remora_prepare)
  endif()
else()
  set(MINIMAL_FIRMWARE 1)
  message(WARNING "Building minimal bring-up firmware (REMORA_SRC_DIR not found).")
  # Option to build a minimal USB CDC firmware instead of the Ethernet demo
  option(BOARD_MINIMAL_CDC "Build minimal USB CDC device firmware (overrides Ethernet demo when ON)" OFF)
  if(BOARD_MINIMAL_CDC)
    message(STATUS "Minimal USB CDC firmware enabled (BOARD_MINIMAL_CDC=ON)")
    set(MINIMAL_MAIN ${PROJECT_ROOT}/targets/TARGET_XCORE407I/main_cdc_minimal.c)
  else()
    set(MINIMAL_MAIN ${PROJECT_ROOT}/targets/TARGET_XCORE407I/main_minimal.c)
  endif()

  add_executable(${PROJECT_NAME}
    startup_stm32f407xx.s
    ${MINIMAL_MAIN}
  # NOTE: Do not compile the legacy 25MHz-oriented system_clock.c in targets/TARGET_XCORE407I.
  # The authoritative clock setup (8 MHz HSE -> 168 MHz) is in Remora source: system_clock.c included above.
  # (Removed ${PROJECT_ROOT}/targets/TARGET_XCORE407I/system_clock.c)
    $<$<NOT:$<BOOL:BOARD_MINIMAL_CDC>>:${PROJECT_ROOT}/src/net/ethernetif.c>
    $<$<NOT:$<BOOL:BOARD_MINIMAL_CDC>>:${PROJECT_ROOT}/src/net/udp_echo.c>
    $<$<BOOL:BOARD_MINIMAL_CDC>:${PROJECT_ROOT}/targets/TARGET_XCORE407I/usb/usb_device.c>
    $<$<BOOL:BOARD_MINIMAL_CDC>:${PROJECT_ROOT}/targets/TARGET_XCORE407I/usb/usbd_desc.c>
    $<$<BOOL:BOARD_MINIMAL_CDC>:${PROJECT_ROOT}/targets/TARGET_XCORE407I/usb/usbd_cdc_if.c>
    $<$<BOOL:BOARD_MINIMAL_CDC>:${PROJECT_ROOT}/targets/TARGET_XCORE407I/usb/usbd_conf.c>
    ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
    syscalls.c
  )
endif()

if (MINIMAL_FIRMWARE)
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_ROOT}/src/net
    ${HAL_CONF_DIR}
    ${CUBE_CMSIS_INCLUDE}
    ${CUBE_DEVICE_INCLUDE}
    ${CUBE_HAL_INC}
    ${CUBE_ROOT}/Middlewares/Third_Party/LwIP/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lwip_port
    $<$<BOOL:BOARD_MINIMAL_CDC>:${CUBE_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc>
    $<$<BOOL:BOARD_MINIMAL_CDC>:${CUBE_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc>
    $<$<BOOL:BOARD_MINIMAL_CDC>:${PROJECT_ROOT}/targets/TARGET_XCORE407I/usb>
  )
  if(BOARD_MINIMAL_CDC)
    # Pull in USB core + CDC sources
    set(USBD_ROOT ${CUBE_ROOT}/Middlewares/ST/STM32_USB_Device_Library)
    if (EXISTS ${USBD_ROOT}/Core/Src/usbd_core.c)
      set(USBD_CORE_SRC
        ${USBD_ROOT}/Core/Src/usbd_core.c
        ${USBD_ROOT}/Core/Src/usbd_ctlreq.c
        ${USBD_ROOT}/Core/Src/usbd_ioreq.c
        ${USBD_ROOT}/Class/CDC/Src/usbd_cdc.c
      )
      add_library(usbdcore STATIC ${USBD_CORE_SRC})
      target_include_directories(usbdcore PUBLIC
        ${USBD_ROOT}/Core/Inc
        ${USBD_ROOT}/Class/CDC/Inc
        ${PROJECT_ROOT}/targets/TARGET_XCORE407I/usb
        ${HAL_CONF_DIR}
        ${CUBE_HAL_INC}
        ${CUBE_DEVICE_INCLUDE}
        ${CUBE_CMSIS_INCLUDE}
      )
      target_compile_definitions(usbdcore PUBLIC STM32F407xx USE_HAL_DRIVER)
      target_link_libraries(usbdcore PUBLIC arm_flags)
      target_link_libraries(${PROJECT_NAME} PRIVATE usbdcore)
    else()
      message(FATAL_ERROR "USB Device library not found for BOARD_MINIMAL_CDC mode at ${USBD_ROOT}")
    endif()
  endif()
else()
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_ROOT}/Remora-OS6/Firmware/FirmwareSource/Remora-OS6
    ${REMORA_SRC_DIR}
    ${REMORA_SRC_DIR}/lwip
    ${PROJECT_ROOT}/src/net
    ${HAL_CONF_DIR}
    ${CUBE_CMSIS_INCLUDE}
    ${CUBE_DEVICE_INCLUDE}
    ${CUBE_HAL_INC}
    ${CUBE_ROOT}/Middlewares/Third_Party/LwIP/src/include
    ${CUBE_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    ${CUBE_ROOT}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/lwip_port
  )
endif()

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
  ${HAL_SRC_DIR}/stm32f4xx_hal_pcd.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_pcd_ex.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_tim.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_tim_ex.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_exti.c
  ${HAL_SRC_DIR}/stm32f4xx_hal_uart.c
  ${HAL_SRC_DIR}/stm32f4xx_ll_usb.c
)

target_sources(${PROJECT_NAME} PRIVATE ${HAL_SOURCES})

# HAL is external; don't fail build on benign warnings from vendor code
set_source_files_properties(${HAL_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wno-error;-Wno-unused-parameter")

# Optional abstract driver layer (pin / qei)
option(USE_ABSTRACT_DRIVERS "Enable abstract pin & QEI drivers (working Ethernet profile default)" ON)
option(ENABLE_QEI_DIAG "Enable QEI diagnostic helper (requires USE_ABSTRACT_DRIVERS)" ON)
if (USE_ABSTRACT_DRIVERS AND NOT MINIMAL_FIRMWARE)
  message(STATUS "Abstract drivers enabled (pin, qei)")
  target_sources(${PROJECT_NAME} PRIVATE
    ${REMORA_SRC_DIR}/drivers/pin/pin.cpp
    ${REMORA_SRC_DIR}/drivers/qei/qeiDriver.cpp
    $<$<BOOL:${ENABLE_QEI_DIAG}>:${REMORA_SRC_DIR}/qei_diag.c>
  )
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${REMORA_SRC_DIR}/drivers/pin
    ${REMORA_SRC_DIR}/drivers/qei
  )
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ABSTRACT_DRIVERS=1)
  if(ENABLE_QEI_DIAG)
    message(STATUS "QEI diagnostic enabled")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_QEI_DIAG=1)
  else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_QEI_DIAG=0)
  endif()
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ABSTRACT_DRIVERS=0 ENABLE_QEI_DIAG=0)
endif()

# Common optimization & warning flags (arch/float already in initial flags)
set(COMMON_NONARCH_FLAGS
  -O2
  -ffunction-sections
  -fdata-sections
  -fno-unwind-tables
  -fno-asynchronous-unwind-tables
  -Wall
  -Wextra
)

# Interface target to propagate identical compile & link flags to all consumers
add_library(arm_flags INTERFACE)
option(REMORA_STRICT "Treat warnings as errors" OFF)
target_compile_options(arm_flags INTERFACE
  ${COMMON_NONARCH_FLAGS}
  -mcpu=cortex-m4
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
)

# Apply stricter pedantic flags only to our firmware target (exclude 3rd party libs)
if(REMORA_STRICT)
  target_compile_options(${PROJECT_NAME} PRIVATE -Werror)
endif()
target_compile_options(${PROJECT_NAME} PRIVATE -Wshadow -Wimplicit-fallthrough)
target_link_options(arm_flags INTERFACE
  -mcpu=cortex-m4
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -Wl,--gc-sections
  -Wl,--print-memory-usage
)

target_link_libraries(${PROJECT_NAME} PRIVATE arm_flags)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
option(USE_PG11_TXEN "Use PG11 as RMII TX_EN (else PB11)" ON)
option(ETH_HW_CHECKSUM "Enable MAC hardware checksum offload" OFF)
option(DFU_TRIM_BIN "Produce trimmed DFU binary without padding gaps" ON)
option(ETH_LEGACY_DESC_IN_CCM "Place ETH DMA descriptors in CCM section (legacy experimental)" OFF)
option(ETH_CLEAR_DESC_PTRS_POST_INIT "Clear heth.Init descriptor pointers after HAL init (legacy behavior)" OFF)
option(ETH_DISABLE_PHY_SCAN "Skip dynamic PHY scan and use fixed PHY_ADDRESS" OFF)

target_compile_definitions(${PROJECT_NAME} PRIVATE STM32F407xx USE_HAL_DRIVER)

# Board feature toggles (previously only in header defaults). Expose as CMake options
option(BOARD_ENABLE_ETH "Enable Ethernet RMII (DP83848I)" ON)
option(BOARD_ENABLE_ULPI "Enable USB HS ULPI (conflicts with Ethernet pins PB12/PB13)" OFF)
option(BOARD_ENABLE_NAND "Enable NAND Flash interface" OFF)

if(BOARD_ENABLE_ETH)
  target_compile_definitions(${PROJECT_NAME} PRIVATE BOARD_ENABLE_ETH=1)
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE BOARD_ENABLE_ETH=0)
endif()
if(BOARD_ENABLE_ULPI)
  target_compile_definitions(${PROJECT_NAME} PRIVATE BOARD_ENABLE_ULPI=1)
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE BOARD_ENABLE_ULPI=0)
endif()
if(BOARD_ENABLE_NAND)
  target_compile_definitions(${PROJECT_NAME} PRIVATE BOARD_ENABLE_NAND=1)
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE BOARD_ENABLE_NAND=0)
endif()

if(ETH_LEGACY_DESC_IN_CCM)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ETH_LEGACY_DESC_IN_CCM=1)
endif()
if(ETH_CLEAR_DESC_PTRS_POST_INIT)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ETH_CLEAR_DESC_PTRS_POST_INIT=1)
endif()
if(ETH_DISABLE_PHY_SCAN)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ETH_DISABLE_PHY_SCAN=1)
endif()

# Auto-detect git short hash for firmware build info (fallback to nogit)
find_package(Git QUIET)
set(FW_GIT_HASH "nogit")
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${PROJECT_ROOT}
    OUTPUT_VARIABLE GIT_HASH_OUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(GIT_HASH_OUT MATCHES "^[0-9a-fA-F]+$")
    set(FW_GIT_HASH ${GIT_HASH_OUT})
  endif()
endif()
message(STATUS "Firmware git hash: ${FW_GIT_HASH}")
target_compile_definitions(${PROJECT_NAME} PRIVATE FW_GIT_HASH="${FW_GIT_HASH}")
if (USE_PG11_TXEN)
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_PG11_TXEN=1)
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_PG11_TXEN=0)
endif()
if (ETH_HW_CHECKSUM)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ETH_HW_CHECKSUM=1)
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE ETH_HW_CHECKSUM=0)
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_UART_ENABLE=1)
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -mcpu=cortex-m4 -mthumb")

# Linker script
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F407IGT_FLASH.ld)

# Linker options
if (NOT CMAKE_OBJCOPY)
  message(WARNING "objcopy not found; .bin generation will be skipped")
endif()

target_link_options(${PROJECT_NAME} PRIVATE -T${LINKER_SCRIPT} -nostartfiles -Wl,-Map=${PROJECT_NAME}.map)

# Post-build binary
if (CMAKE_OBJCOPY)
  # Always refer to the full path of the produced ELF using generator expressions for reliability
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMENT "Generating binary file (${PROJECT_NAME}.bin)"
  )
  # Copy ELF with explicit extension (some tools look for .elf pattern)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.elf
    COMMENT "Copying ELF to ${PROJECT_NAME}.elf"
  )
  # Generate Intel HEX so base address is embedded (no manual 0x08000000 entry needed)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMENT "Generating Intel HEX file (${PROJECT_NAME}.hex)"
  )
  if(DFU_TRIM_BIN)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_OBJCOPY} -O binary --remove-section .ccm_data --remove-section ._user_heap_stack $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}_flash.bin
      COMMENT "Generating trimmed DFU binary (${PROJECT_NAME}_flash.bin)"
    )
  endif()
endif()

# Provide a generic trimmed binary even if DFU_TRIM_BIN not explicitly set (for Cube flashing)
if (CMAKE_OBJCOPY AND NOT DFU_TRIM_BIN)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary --remove-section .ccm_data --remove-section ._user_heap_stack $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}_flash.bin
    COMMENT "Generating trimmed flash binary (${PROJECT_NAME}_flash.bin)"
  )
endif()
  # Optional: Windows DFU flash helper using existing PowerShell script
  if (CMAKE_HOST_WIN32)
    set(FLASH_POWERSHELL_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/flash_dfu.ps1)
    add_custom_target(flash_ps
      COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File ${FLASH_POWERSHELL_SCRIPT} -Bin ${CMAKE_BINARY_DIR}/targets/TARGET_XCORE407I/${PROJECT_NAME}_flash.bin
      DEPENDS ${PROJECT_NAME}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Flashing (PowerShell) ${PROJECT_NAME}_flash.bin via DFU"
    )
  endif()

# --- Convenience targets: DFU flash & verify ---
find_program(DFU_UTIL dfu-util)
if(DFU_UTIL)
  add_custom_target(flash_dfu
    COMMAND ${CMAKE_COMMAND} -E echo "[flash_dfu] Invoking helper script"
    COMMAND ${CMAKE_COMMAND} -E env FW_BIN=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin bash ${PROJECT_ROOT}/scripts/flash_dfu.sh ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    DEPENDS ${PROJECT_NAME}
    COMMENT "Flashing ${PROJECT_NAME}.bin to 0x08000000 via dfu-util"
  )

  add_custom_target(verify_flash
    COMMAND ${CMAKE_COMMAND} -E echo "[verify_flash] Starting readback"
    COMMAND ${CMAKE_COMMAND} -E env BIN_SIZE=$<SHELL_PATH:$<TARGET_FILE:${PROJECT_NAME}>>
    COMMAND sh -c "SIZE=\`stat -c%s ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin\`; echo '[verify_flash] SIZE='\"$SIZE\"; dfu-util -a 0 -s 0x08000000:$SIZE -U readback.bin || true"
    COMMAND sh -c "cmp -s ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin readback.bin && echo '[verify_flash] Match OK' || echo '[verify_flash] WARNING: Difference detected'"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Verifying on-device flash matches built binary"
  )
else()
  message(STATUS "dfu-util not found: flash_dfu / verify_flash targets disabled")
endif()

# --- CubeProgrammer flash target (Windows host expected) ---
if (CMAKE_HOST_WIN32)
  # Allow override of CubeProgrammer CLI path via -DSTM32_CUBE_PROG=...; else assume default install location
  if (NOT STM32_CUBE_PROG)
    set(STM32_CUBE_PROG "C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe")
  endif()
  add_custom_target(flash_cube
    COMMAND "${STM32_CUBE_PROG}" -c port=SWD freq=4000 -w ${CMAKE_BINARY_DIR}/targets/TARGET_XCORE407I/${PROJECT_NAME}_flash.bin 0x08000000 -v
    DEPENDS ${PROJECT_NAME}
    COMMENT "Flashing (STM32CubeProgrammer) ${PROJECT_NAME}_flash.bin @0x08000000"
  )
  add_custom_target(flash_cube_erase
    COMMAND "${STM32_CUBE_PROG}" -c port=SWD freq=4000 -e all -w ${CMAKE_BINARY_DIR}/targets/TARGET_XCORE407I/${PROJECT_NAME}_flash.bin 0x08000000 -v
    DEPENDS ${PROJECT_NAME}
    COMMENT "Full erase + Flash (STM32CubeProgrammer) ${PROJECT_NAME}_flash.bin @0x08000000"
  )
  add_custom_target(flash_cube_verify
    COMMAND "${STM32_CUBE_PROG}" -c port=SWD freq=4000 -d 0x08000000 ${CMAKE_BINARY_DIR}/targets/TARGET_XCORE407I/readback.bin 0x10000
    COMMAND ${CMAKE_COMMAND} -E echo "Compare (first 64KB)"
    COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/targets/TARGET_XCORE407I/${PROJECT_NAME}_flash.bin ${CMAKE_BINARY_DIR}/targets/TARGET_XCORE407I/readback.bin || ${CMAKE_COMMAND} -E echo "WARNING: mismatch in first 64KB (lengths or contents)"
    COMMENT "Read back first 64KB for quick verification"
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
  ${LWIP_ROOT}/src/core/ipv4/dhcp.c
)

add_library(lwipcore STATIC ${LWIP_CORE_SRC} lwip_port/sys_arch.c)
target_include_directories(lwipcore PUBLIC
  ${LWIP_ROOT}/src/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lwip_port
)
target_compile_definitions(lwipcore PUBLIC NO_SYS=1 LWIP_TIMEVAL_PRIVATE=1)
if (ETH_HW_CHECKSUM)
  target_compile_definitions(lwipcore PUBLIC ETH_HW_CHECKSUM=1)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE lwipcore)
target_link_libraries(lwipcore PUBLIC arm_flags)

# --- USB Device (Cube) core and CDC class sources ---
set(USBD_ROOT ${CUBE_ROOT}/Middlewares/ST/STM32_USB_Device_Library)
if (NOT MINIMAL_FIRMWARE AND EXISTS ${USBD_ROOT}/Core/Src/usbd_core.c)
  set(USBD_CORE_SRC
    ${USBD_ROOT}/Core/Src/usbd_core.c
    ${USBD_ROOT}/Core/Src/usbd_ctlreq.c
    ${USBD_ROOT}/Core/Src/usbd_ioreq.c
  )
  set(USBD_CDC_SRC
    ${USBD_ROOT}/Class/CDC/Src/usbd_cdc.c
  )
  add_library(usbdcore STATIC ${USBD_CORE_SRC} ${USBD_CDC_SRC})
  target_include_directories(usbdcore PUBLIC
    ${USBD_ROOT}/Core/Inc
    ${USBD_ROOT}/Class/CDC/Inc
    ${REMORA_SRC_DIR}/usb
    ${HAL_CONF_DIR}
    ${CUBE_HAL_INC}
    ${CUBE_DEVICE_INCLUDE}
    ${CUBE_CMSIS_INCLUDE}
  )
  target_compile_definitions(usbdcore PUBLIC STM32F407xx USE_HAL_DRIVER)
  target_link_libraries(usbdcore PUBLIC arm_flags)
  target_link_libraries(${PROJECT_NAME} PRIVATE usbdcore)
elseif(NOT MINIMAL_FIRMWARE)
  message(FATAL_ERROR "STM32 USB Device library not found at ${USBD_ROOT}. Ensure submodule present.")
endif()
