# Cross-compilation toolchain file for STM32F407 (arm-none-eabi-gcc)
# Use with: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-gcc-stm32.cmake -B build -S .

cmake_minimum_required(VERSION 3.16)

# Identify this as a generic embedded system (no OS / no executables to run on host)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Prevent CMake from trying to run test executables during try_compile.
# This forces static library creation for compiler checks.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Locate the cross compiler (allow user override via env or cache)
find_program(ARM_NONE_EABI_GCC arm-none-eabi-gcc)
find_program(ARM_NONE_EABI_GXX arm-none-eabi-g++)
find_program(ARM_NONE_EABI_AS arm-none-eabi-as)
find_program(ARM_NONE_EABI_AR arm-none-eabi-ar)
find_program(ARM_NONE_EABI_OBJCOPY arm-none-eabi-objcopy)
find_program(ARM_NONE_EABI_OBJDUMP arm-none-eabi-objdump)
find_program(ARM_NONE_EABI_SIZE arm-none-eabi-size)

if(NOT ARM_NONE_EABI_GCC)
  message(FATAL_ERROR "arm-none-eabi-gcc not found in PATH")
endif()

set(CMAKE_C_COMPILER   ${ARM_NONE_EABI_GCC})
set(CMAKE_CXX_COMPILER ${ARM_NONE_EABI_GXX})
set(CMAKE_ASM_COMPILER ${ARM_NONE_EABI_GCC})

# Force-skip compiler "working" link tests (we know cross tools exist)
set(CMAKE_C_COMPILER_WORKS TRUE CACHE INTERNAL "")
set(CMAKE_CXX_COMPILER_WORKS TRUE CACHE INTERNAL "")

# Basic CPU / FPU flags (adjust if softfp desired)
set(MCPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

# Common warning / optimization baseline - can be extended per target
set(COMMON_C_FLAGS "${MCPU_FLAGS} -ffunction-sections -fdata-sections -fno-common -Wall -Wextra -Wundef -Wshadow -Wdouble-promotion")
set(COMMON_C_DEFS "-DSTM32F407xx -DUSE_HAL_DRIVER")

# Provide initialization for build types if not defined
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Avoid linking against host system libraries during try_compile
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib")

# Apply flags to C and C++
set(CMAKE_C_FLAGS_INIT   "${COMMON_C_FLAGS} ${COMMON_C_DEFS}")
set(CMAKE_CXX_FLAGS_INIT "${COMMON_C_FLAGS} ${COMMON_C_DEFS} -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS_INIT "${MCPU_FLAGS}")

# Provide size tool variable for later use
set(SIZE_TOOL ${ARM_NONE_EABI_SIZE} CACHE FILEPATH "Size tool")
set(OBJCOPY_TOOL ${ARM_NONE_EABI_OBJCOPY} CACHE FILEPATH "Objcopy tool")

# Suppress attempts to link standard system startup files during tests
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

# Mark as an embedded bare-metal environment (no default linker flags)
set(CMAKE_EXE_LINKER_FLAGS "${MCPU_FLAGS} -Wl,--gc-sections")

# Provide a hint to users
message(STATUS "Using STM32 arm-none-eabi toolchain; try-compile set to STATIC_LIBRARY")
